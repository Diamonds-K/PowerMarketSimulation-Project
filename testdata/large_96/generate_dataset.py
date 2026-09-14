#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成 96 时段大规模测试数据（可通过交易中心页的四个按钮直接导入）。

数据特点：
  * 6 台机组（煤电 2 台、燃气 2 台、水电 1 台、新能源 1 台）+ 5 个用户；
  * 96 个时段的负荷形状、开机可用容量、报价段数（2~10 段）与段价各不相同；
  * 价格曲线呈现"凌晨低谷 / 早高峰 / 午间回落 / 晚间尖峰"的双峰形态；
  * 埋入 5 类边界场景（见 README.md 的"特殊时段"表）。

脚本最后会对生成结果做一次自检（单调性、段数上限、容量上限、最小需求），
保证数据能通过平台自身的报价校验。随机数使用固定种子，重复运行结果一致。
"""

import csv
import math
import os
import random

SEED = 20260914
SLOTS = 96
SLOT_HOURS = 0.25
OUT_DIR = os.path.dirname(os.path.abspath(__file__))

# id, Pmin, Pmax, 首段边际成本, 每段价格递增步长, 最少段数, 最多段数, 类型
GENERATORS = [
    ("G1_COAL1", 180.0, 600.0, 152.0, 17.0, 4, 6, "base"),
    ("G2_COAL2", 150.0, 500.0, 176.0, 19.0, 4, 6, "base"),
    ("G3_GAS1", 60.0, 300.0, 238.0, 25.0, 3, 5, "mid"),
    ("G4_GAS2", 40.0, 220.0, 305.0, 30.0, 3, 5, "peak"),
    ("G5_HYDRO", 20.0, 180.0, 118.0, 11.0, 2, 4, "hydro"),
    ("G6_RENEW", 0.0, 260.0, 16.0, 4.0, 2, 5, "renew"),
]

# id, 固定需求 QD（仅二次曲线模式使用）, 报价基准水平（元/MWh）, 负荷特性
CONSUMERS = [
    ("C1_STEEL", 320.0, 372.0, "flat"),
    ("C2_DATACENTER", 220.0, 690.0, "flat"),
    ("C3_COMMERCIAL", 260.0, 520.0, "midday"),
    ("C4_RESIDENTIAL", 300.0, 605.0, "evening"),
    ("C5_EV", 180.0, 448.0, "night"),
]

SUM_PMIN = sum(item[1] for item in GENERATORS)
MAX_SEGMENTS = 10

# 特殊时段（0 基编号），见 README.md
SLOT_DEMAND_BELOW_PMIN = (19, 20)   # 05:00 / 05:15，申报需求低于 ΣPmin → 不可行
SLOT_SUPPLY_SHORTAGE = (77, 78)     # 19:15 / 19:30，机组检修 + 需求抬升 → 缺额
SLOT_TIE_PRICE = 40                 # 10:00，G1 末段与 G2 首段同价
SLOT_LOW_PRICE = 60                 # 15:00，整体报价下移 → 低出清价
SLOT_TEN_SEGMENTS = 84              # 21:00，G2 报满 10 段


def load_shape(hour):
    """24 小时系统负荷形状：凌晨低谷、早高峰、午间平台、晚间尖峰。"""
    evening_peak = 0.34 * math.exp(-((hour - 19.0) ** 2) / 7.0)
    morning_ramp = 0.11 * math.exp(-((hour - 9.5) ** 2) / 5.0)
    night_valley = -0.17 * math.exp(-((hour - 4.0) ** 2) / 9.0)
    return 0.62 + evening_peak + morning_ramp + night_valley


def consumer_shape(profile, hour):
    """各类用户的负荷特性（彼此错峰，保证每时段结构都在变）。"""
    if profile == "midday":
        return 0.55 + 0.75 * math.exp(-((hour - 14.0) ** 2) / 12.0)
    if profile == "evening":
        return 0.45 + 1.05 * math.exp(-((hour - 19.5) ** 2) / 6.0)
    if profile == "night":
        return (0.55
                + 0.95 * math.exp(-((hour - 2.5) ** 2) / 14.0)
                + 0.45 * math.exp(-((hour - 20.5) ** 2) / 6.0))
    return 1.0


def generator_availability(kind, hour, load, rng):
    """机组在该时段的可用出力比例（检修、来水、风光出力都在变）。"""
    noise = 1.0 + rng.uniform(-0.04, 0.04)
    if kind == "base":
        # 晚间高峰煤电机组降出力（模拟检修/降容），逼出高峰电价
        derate = 0.20 * max(0.0, load - 0.72)
        return min(1.0, (0.90 - derate) * noise)
    if kind == "mid":
        return min(1.0, (0.70 + 0.35 * (load - 0.55)) * noise)
    if kind == "peak":
        return min(1.0, max(0.03, (0.25 + 1.15 * (load - 0.62)) * noise))
    if kind == "hydro":
        wet = 0.78 + 0.22 * math.sin(2.0 * math.pi * (hour - 3.0) / 24.0)
        evening_limit = 1.0 - 0.30 * math.exp(-((hour - 19.5) ** 2) / 8.0)
        return min(1.0, max(0.25, wet * evening_limit * noise))
    if kind == "renew":
        solar = math.exp(-((hour - 12.5) ** 2) / 9.0)
        wind = 0.45 + 0.40 * math.sin(2.0 * math.pi * (hour - 2.0) / 24.0)
        return min(1.0, max(0.03, (0.65 * wind + 0.75 * solar) * noise))
    return 1.0


def split_quantity(total, parts, rng):
    """把总量切成 parts 段（前段偏大），返回段电量，和精确等于 total。"""
    weights = [(0.42 - 0.045 * i) * (1.0 + rng.uniform(-0.12, 0.12))
               for i in range(parts)]
    weights = [max(0.08, w) for w in weights]
    scale = total / sum(weights)
    values = [w * scale for w in weights]

    # 段电量下限，保证不会出现 0 或负段
    min_segment = min(0.5, total / (2.0 * parts))
    for index, value in enumerate(values):
        if value < min_segment:
            deficit = min_segment - value
            values[index] = min_segment
            donor = max(range(parts), key=lambda k: values[k])
            if donor != index:
                values[donor] -= deficit

    # 四舍五入后把误差全部丢给最后一段，保证总和精确
    rounded = [round(v, 2) for v in values[:-1]]
    last = round(total - sum(rounded), 2)
    if last <= 0.0:
        rounded = [round(total / parts, 2) for _ in range(parts - 1)]
        last = round(total - sum(rounded), 2)
    return rounded + [last]


def build_slot(rng, slot):
    hour = slot * SLOT_HOURS
    load = load_shape(hour) * (1.0 + rng.uniform(-0.02, 0.02))
    fuel = 1.0 + 0.18 * (load - 0.62) + rng.uniform(-0.03, 0.03)
    total_demand = 500.0 + 1050.0 * load

    # ---- 用户侧：按各自负荷特性分摊系统需求，报价随供需紧张程度抬升 ----
    weights = [consumer_shape(profile, hour) for _, _, _, profile in CONSUMERS]
    weight_sum = sum(weights)
    consumers = []
    for (cid, _qd, base_wtp, _profile), weight in zip(CONSUMERS, weights):
        demand = total_demand * weight / weight_sum * (1.0 + rng.uniform(-0.05, 0.05))
        segment_count = rng.choice([2, 3, 4, 4, 5, 5, 6])
        step = 22.0 + 10.0 * rng.random()
        wtp = base_wtp * (1.0 + 0.25 * (load - 0.62) + rng.uniform(-0.04, 0.04))
        prices = []
        for index in range(segment_count):
            price = wtp - step * index * (0.9 + 0.2 * rng.random())
            prices.append(round(max(15.0, price), 2))
        quantities = split_quantity(demand, segment_count, rng)
        consumers.append((cid, quantities, prices))

    # ---- 发电侧：可用容量、段数、价格阶梯随负荷与燃料价格变化 ----
    generators = []
    for gid, pmin, pmax, base_mc, step, n_min, n_max, kind in GENERATORS:
        capacity = pmax - pmin
        availability = generator_availability(kind, hour, load, rng)
        # 留 0.05 MW 余量，避免四舍五入后累计电量超过 Pmax-Pmin 被校验拒绝
        offer = min(capacity * availability, capacity - 0.05)
        segment_count = rng.randint(n_min, n_max)
        if kind == "renew" and availability < 0.25:
            segment_count = max(1, segment_count - 2)
        scarcity = 1.0 + 0.55 * max(0.0, load - 0.72)
        first_price = base_mc * fuel * scarcity * (1.0 + rng.uniform(-0.03, 0.03))
        prices = []
        price = max(5.0, first_price)
        for _ in range(segment_count):
            prices.append(round(price, 2))
            price += step * (0.75 + 0.5 * rng.random())
        quantities = split_quantity(offer, segment_count, rng)
        generators.append((gid, quantities, prices))

    return generators, consumers, total_demand


def apply_special_slots(rng, slot, generators, consumers, total_demand):
    """对特殊时段做定向改造。"""
    if slot in SLOT_DEMAND_BELOW_PMIN:
        # 深度低谷：总申报需求低于 ΣPmin → 出清判定不可行
        target = 380.0 if slot == SLOT_DEMAND_BELOW_PMIN[0] else 430.0
        scale = target / total_demand
        consumers = [(cid, split_quantity(sum(quantities) * scale, len(quantities), rng), prices)
                     for cid, quantities, prices in consumers]
        total_demand = target

    if slot in SLOT_SUPPLY_SHORTAGE:
        # 晚高峰叠加检修：需求抬高、G3 只剩 1 MW、新能源几乎无出力 → 缺额
        scale = 1.55
        consumers = [(cid, split_quantity(sum(quantities) * scale, len(quantities), rng), prices)
                     for cid, quantities, prices in consumers]
        total_demand *= scale
        patched = []
        for gid, quantities, prices in generators:
            if gid == "G3_GAS1":
                quantities, prices = [1.0], [round(prices[0] + 120.0, 2)]
            elif gid == "G6_RENEW":
                quantities, prices = [2.0], [round(prices[0], 2)]
            patched.append((gid, quantities, prices))
        generators = patched

    if slot == SLOT_TIE_PRICE:
        # 同价段：G1 最贵一段与 G2 最便宜一段价格相同（考验同价成交顺序）
        tie = 300.0
        patched = []
        for gid, quantities, prices in generators:
            if gid == "G1_COAL1":
                prices = sorted([round(min(p, tie), 2) for p in prices])
                prices[-1] = round(tie, 2)
            elif gid == "G2_COAL2":
                # 从同价开始逐段递增，保持单调不减
                prices = [round(tie + 20.0 * i, 2) for i in range(len(prices))]
            patched.append((gid, quantities, prices))
        generators = patched

    if slot == SLOT_LOW_PRICE:
        # 供大于求：全部机组报价整体下移 → 低出清价
        generators = [(gid, quantities, [round(max(5.0, p * 0.78), 2) for p in prices])
                      for gid, quantities, prices in generators]

    if slot == SLOT_TEN_SEGMENTS:
        # 10 段上限：G2 报满 10 段
        patched = []
        for gid, quantities, prices in generators:
            if gid == "G2_COAL2":
                offer = sum(quantities)
                first_price = prices[0]
                quantities = split_quantity(offer, MAX_SEGMENTS, rng)
                prices = [round(first_price + 14.0 * i, 2) for i in range(MAX_SEGMENTS)]
            patched.append((gid, quantities, prices))
        generators = patched

    return generators, consumers, total_demand


def self_check(generator_bids, consumer_bids):
    """按平台校验规则自检：单调性、段数、容量、段电量为正。"""
    problems = []
    capacity = {gid: pmax - pmin for gid, pmin, pmax, *_ in GENERATORS}

    grouped = {}
    for gid, slot, _seg, quantity, price in generator_bids:
        grouped.setdefault((gid, slot), []).append((quantity, price))
    for (gid, slot), rows in grouped.items():
        if len(rows) > MAX_SEGMENTS:
            problems.append(f"{gid} 时段 {slot} 段数 {len(rows)} 超过 {MAX_SEGMENTS}")
        if sum(q for q, _ in rows) > capacity[gid] + 1e-9:
            problems.append(f"{gid} 时段 {slot} 累计电量 {sum(q for q, _ in rows):.2f} "
                            f"超过 Pmax-Pmin={capacity[gid]:.2f}")
        for quantity, price in rows:
            if quantity <= 0 or price < 0:
                problems.append(f"{gid} 时段 {slot} 存在非正电量或负价格")
        for previous, current in zip(rows, rows[1:]):
            if current[1] + 1e-9 < previous[1]:
                problems.append(f"{gid} 时段 {slot} 发电段价格非单调不减")
                break

    grouped.clear()
    for cid, slot, _seg, quantity, price in consumer_bids:
        grouped.setdefault((cid, slot), []).append((quantity, price))
    for (cid, slot), rows in grouped.items():
        if len(rows) > MAX_SEGMENTS:
            problems.append(f"{cid} 时段 {slot} 段数 {len(rows)} 超过 {MAX_SEGMENTS}")
        for quantity, price in rows:
            if quantity <= 0 or price < 0:
                problems.append(f"{cid} 时段 {slot} 存在非正电量或负价格")
        for previous, current in zip(rows, rows[1:]):
            if current[1] - 1e-9 > previous[1]:
                problems.append(f"{cid} 时段 {slot} 用户段价格非单调不增")
                break

    return problems


def main():
    rng = random.Random(SEED)
    generator_bids = []
    consumer_bids = []
    slot_demand = []

    for slot in range(SLOTS):
        generators, consumers, total_demand = build_slot(rng, slot)
        generators, consumers, total_demand = apply_special_slots(
            rng, slot, generators, consumers, total_demand)

        slot_demand.append(total_demand)
        for gid, quantities, prices in generators:
            for index, (quantity, price) in enumerate(zip(quantities, prices), start=1):
                generator_bids.append((gid, slot + 1, index, round(quantity, 2), round(price, 2)))
        for cid, quantities, prices in consumers:
            for index, (quantity, price) in enumerate(zip(quantities, prices), start=1):
                consumer_bids.append((cid, slot + 1, index, round(quantity, 2), round(price, 2)))

    problems = self_check(generator_bids, consumer_bids)
    if problems:
        print("自检发现问题：")
        for item in problems[:20]:
            print("  -", item)
    else:
        print("自检通过：单调性 / 段数上限 / 容量上限 / 段电量为正 全部满足")

    def write_csv(name, header, rows):
        path = os.path.join(OUT_DIR, name)
        with open(path, "w", newline="", encoding="utf-8") as handle:
            writer = csv.writer(handle, lineterminator="\n")
            writer.writerow(header)
            writer.writerows(rows)
        print(f"写入 {name}: {len(rows)} 行")

    write_csv("generators.csv",
              ["generator_id", "p_min_mw", "p_max_mw", "mode"],
              [(gid, f"{pmin:.0f}", f"{pmax:.0f}", "piecewise")
               for gid, pmin, pmax, *_ in GENERATORS])

    write_csv("consumers.csv",
              ["consumer_id", "fixed_demand_mw", "mode"],
              [(cid, f"{qd:.0f}", "piecewise") for cid, qd, _, _ in CONSUMERS])

    write_csv("bids_generator.csv",
              ["generator_id", "time_slot", "segment_no", "quantity_mw", "price_yuan_per_mwh"],
              [(gid, slot, seg, f"{quantity:.2f}", f"{price:.2f}")
               for gid, slot, seg, quantity, price in generator_bids])

    write_csv("bids_consumer.csv",
              ["consumer_id", "time_slot", "segment_no", "quantity_mw", "price_yuan_per_mwh"],
              [(cid, slot, seg, f"{quantity:.2f}", f"{price:.2f}")
               for cid, slot, seg, quantity, price in consumer_bids])

    print(f"系统总申报需求范围: {min(slot_demand):.1f} ~ {max(slot_demand):.1f} MW"
          f"（ΣPmin = {SUM_PMIN:.0f} MW）")
    print("特殊时段（界面显示编号）：不可行 %s / 缺额 %s / 同价 %d / 低价 %d / 10段 %d"
          % ([s + 1 for s in SLOT_DEMAND_BELOW_PMIN],
             [s + 1 for s in SLOT_SUPPLY_SHORTAGE],
             SLOT_TIE_PRICE + 1, SLOT_LOW_PRICE + 1, SLOT_TEN_SEGMENTS + 1))


if __name__ == "__main__":
    main()
