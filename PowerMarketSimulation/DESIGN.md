# 电力现货市场出清仿真平台 设计文档（约束文件）

> 本文件是 `ARCHITECTURE.md` 的配套设计约束。所有算法、价格规则、边界规则、结算规则和测试用例必须与本文一致。

## 1. 目标与范围

平台以日前电力现货市场为简化场景，覆盖以下完整闭环：

```text
市场主体建模 -> 报价提交 -> 报价校验 -> 市场出清 -> 价格形成 -> 统一价格结算 -> 结果展示
```

- 日前市场共 96 个时段，时段编号 `0..95`，每个时段 15 分钟；
- 单市场、无网络约束、统一出清价；
- 所有机组已在线运行；
- 二次曲线模式下需求刚性（固定 `QD`）。

## 2. 统一假设

1. 所有机组在线运行，`Pmin` 为最低稳定运行功率，`Pmax` 为最大出力。
2. 分段报价只申报 `Pmin` 以上的增量段；`Pmin` 不参与报价竞争，但计入最终结算。
3. 用户分段需求按价格单调不增排列。
4. 二次曲线总报价成本为 `Bi(Pi)=ai*Pi^2+bi*Pi+ci`，`ai>0` 保证凸性。
5. `ci` 只影响总成本，不影响出力分配和 `λ`。
6. 分段模式用户申报总需求低于 `ΣPmin` 时不可行；二次模式 `QD` 必须在 `[ΣPmin, ΣPmax]` 内。

## 3. 量纲与结算单位

- 功率/电量：`MW`（时段平均功率）；
- 价格：`元/MWh`；
- 每个时段时长：`k = 0.25 h`；
- 时段电量：`E = P * 0.25`（`MWh`）；
- 时段费用：`F = λ * P * 0.25`（`元`）。

代码中统一使用常量 `kSlotDurationHours = 0.25`。课程资料中的简化式 `T = λ*QD` 按小时能量口径表述，本平台显式乘以 `0.25`，保证 96 时段日总费用正确。

## 4. 数据模型

### 4.1 核心类

| 类 | 关键字段 | 说明 |
| --- | --- | --- |
| `BidSegment` | `segmentNo`、`quantityMw`、`priceYuanPerMwh` | 一段电量与价格 |
| `BidSheet` | `ownerId`、`timeSlot`、`mode`、`segments`、`a/b/c` | 分段或二次报价 |
| `Generator` | `id`、`pMinMw`、`pMaxMw`、`bidSheet` | 发电机组 |
| `Consumer` | `id`、`fixedDemandMw`、`bidSheet` | 用户 |
| `MarketInput` | `timeSlot`、`mode`、`generators`、`consumers` | 单时段市场输入 |
| `MarketResult` | `clearingPrice`、`clearingVolume`、`feasible`、`shortage`、机组/用户结果 | 单时段出清结果 |
| `SettlementResult` | 发电收益/成本/利润、用户支付、总量平衡 | 单时段结算结果 |

### 4.2 报价约束

发电侧分段报价：

```text
C1 <= C2 <= ... <= Cn
段电量 > 0
段数 <= 10
Σ段电量 <= Pmax - Pmin
```

用户侧分段报价：

```text
C1 >= C2 >= ... >= Cn
段电量 > 0
段数 <= 10
```

二次曲线报价：

```text
ai > 0
fixedDemand >= 0
```

## 5. 模式 1：分段报价出清

### 5.1 模型

每台机组的实际出力：

```text
Pi = Pmin_i + Pmarket_i
```

- `Pmin_i`：最低稳定出力，不参与报价竞争，计入最终结算；
- `Pmarket_i`：由双指针撮合得到的增量成交。

### 5.2 双指针撮合

1. 所有机组的 `Pmin` 视为价格接受者的必发电量，先按用户报价从高到低满足需求，再从用户需求段中扣除对应电量。
2. 发电增量段按价格升序排序；同价时按申报顺序（机组顺序、段号）成交。
3. 用户需求段按价格降序排序；同价时按申报顺序（用户顺序、段号）成交。
4. 双指针 `s`（发电）、`b`（用户）同时前进：

```text
若 Cbuy >= Csell：
    Qtrade = min(Qbuy, Qsell, 机组剩余增量空间, 需求剩余量)
    成交并扣除双方剩余量
    更新机组累计增量
    若机组累计增量达到 Pmax - Pmin，跳过该机组剩余段
直到 Cbuy < Csell、一侧报价耗尽或需求满足
```

4. 撮合时以 `总成交 = ΣPmin + 增量成交` 为口径，避免超出用户申报总需求。

### 5.3 价格规则

- 统一出清价 `Cclear` 取**最后成交发电段价格**；
- 用户报价只用于判断是否成交，不参与定价；
- 若最后一段发电段部分成交，价格仍为该段报价；
- 若未发生任何增量成交，`Cclear` 取全场最低发电段报价作为结算参考价；
- 同价段固定按申报顺序成交，不做按比例分摊；
- 所有成交电量（含 `Pmin`）按 `Cclear` 统一结算。

### 5.4 缺额规则

- 用户申报总需求高于可成交总量时，记录缺额 `shortageMw`；
- 已成交部分仍按统一价格结算；
- 用户申报总需求低于 `ΣPmin` 时判定不可行。

### 5.5 伪代码

```text
sort(generatorSegments, byPriceAsc, thenByOrder)
sort(consumerBlocks, byPriceDesc, thenByOrder)

if totalDemand < sumPMin:
    return infeasible("用户总申报需求低于 ΣPmin")

s = b = 0
matchedIncrement = 0
while b < consumers.size() and s < generators.size():
    if consumerPrice[b] < generatorPrice[s]:
        break
    if unitCumulative[gen(s)] >= capacity[gen(s)]:
        s += 1
        continue

    remainingDemand = totalDemand - (sumPMin + matchedIncrement)
    q = min(consumerQty[b], generatorQty[s],
            capacity[gen(s)] - unitCumulative[gen(s)],
            remainingDemand)
    if q <= 0:
        break

    trade(q, generatorSegment[s], consumerBlock[b])
    matchedIncrement += q
    lastSellPrice = generatorPrice[s]

    if consumerQty[b] <= 0: b += 1
    if generatorQty[s] <= 0: s += 1

Cclear = lastSellPrice if matchedIncrement > 0 else lowestSellPrice
shortage = max(0, totalDemand - (sumPMin + matchedIncrement))
```

## 6. 模式 2：二次曲线出清

### 6.1 优化模型

```text
min Σ Bi(Pi) = Σ (ai*Pi^2 + bi*Pi + ci)

s.t.
    Σ Pi = QD
    Pmin_i <= Pi <= Pmax_i
    ai > 0
```

`QD` 为二次模式固定总需求。若 `QD` 在 `[ΣPmin, ΣPmax]` 之外，直接判定不可行。

### 6.2 KKT 条件

边际成本：

```text
MCi(Pi) = 2*ai*Pi + bi
```

- 自由机组：`MCi(Pi) = λ`；
- 上限机组：`Pi = Pmax_i` 且 `MCi(Pmax_i) <= λ`；
- 下限机组：`Pi = Pmin_i` 且 `MCi(Pmin_i) >= λ`。

### 6.3 λ 二分求解

```text
检查可行性：sumPMin <= QD <= sumPMax
lo = min(MCi(Pmin_i)) - 1
hi = max(MCi(Pmax_i)) + 1

repeat:
    lambda = (lo + hi) / 2
    Pi = clamp((lambda - bi) / (2*ai), Pmin_i, Pmax_i)
    S = Σ Pi
    if |S - QD| <= tol or |hi - lo| <= tol:
        break
    if S < QD: lo = lambda
    else:      hi = lambda
```

由于 `ai > 0`，`Pi(λ)` 关于 `λ` 单调不减，二分必然收敛；复杂度为 `O(N * 迭代次数)`。

### 6.4 边界价格规则

- 存在自由机组时：`Cclear = λ`，且唯一；
- 全部机组压界时：`λ` 不唯一。本项目固定返回二分得到的 `λ` 作为出清价，并在结果中保留机组出力与边际成本，供 UI 展示边界状态。

### 6.5 与模式 1 的一致性

若二次曲线是分段报价的精确边际成本表达，两种模式结果应一致；若由拟合得到，允许存在拟合误差。一致性测试见第 10 节。

## 7. 结算规则

### 7.1 公式

统一出清价 `λ`（或 `Cclear`），时段折算 `k = 0.25`：

| 项目 | 公式 |
| --- | --- |
| 发电收益 | `Ri = λ * Pi * k` |
| 发电成本（二次） | `Ci = (ai*Pi^2 + bi*Pi + ci) * k` |
| 发电成本（分段） | `Ci = (λ*Pmin + Σ(段价*段成交电量)) * k` |
| 发电利润 | `πi = Ri - Ci` |
| 用户支付 | `Ti = λ * Qcleared_i * k` |
| 总支付 | `T = Σ Ti` |
| 总收益 | `R = Σ Ri` |
| 平衡校验 | `T - R ≈ 0` |

### 7.2 Pmin 分摊规则（分段模式）

分段模式下，`ΣPmin` 是必须运行电量。为满足“总支付 = 总收益”的统一价格结算平衡：

- 按每个用户申报总量占全部申报总量的比例，将 `ΣPmin` 分摊到各用户；
- 用户结算电量 = 该用户增量成交 + 分摊的 `Pmin` 电量；
- 若总申报量为 0，则不分摊（该场景在可行性校验中被拒绝）。

## 8. 数据层设计

### 8.1 CSV 格式

`CSVReader / CSVWriter` 为通用 CSV 读写，不包含业务逻辑。多主体、多时段、多段报价格式约定如下：

`generators.csv`（发电参数，参数导入后替换当前机组列表）：

```csv
generator_id,p_min_mw,p_max_mw,mode
G1,20,100,piecewise
G2,30,120,piecewise
```

`consumers.csv`（用户参数，参数导入后替换当前用户列表）：

```csv
consumer_id,fixed_demand_mw,mode
C1,60,piecewise
C2,40,piecewise
```

`bids_generator.csv`（发电分段报价；按 `generator_id + time_slot` 分组，同组多行构成一个时段的多段报价）：

```csv
generator_id,time_slot,segment_no,quantity_mw,price_yuan_per_mwh
G1,1,1,20,200
G1,1,2,30,250
G1,1,3,30,300
```

`bids_consumer.csv`（用户分段报价，结构同上）：

```csv
consumer_id,time_slot,segment_no,quantity_mw,price_yuan_per_mwh
C1,1,1,40,270
C1,1,2,20,250
```

约定：

- `time_slot` 使用界面时段编号 `1..96`，程序内部转换为 `0..95`；
- `quantity_mw` 表示 `Pmin` 之上的增量电量，不是累计上限；
- `segment_no` 从 1 开始，同一主体同时段内不允许重复。

二次系数 CSV 格式已预留，暂未接入界面导入：

```csv
owner_id,time_slot,a,b,c
G1,1,0.05,10,0
```

### 8.2 SQLite 表结构

| 表 | 用途 |
| --- | --- |
| `generators` | 机组与容量参数 |
| `consumers` | 用户与固定需求 |
| `bid_segments` | 分段报价 |
| `market_results` | 各时段出清价与电量 |
| `generator_results` | 各时段机组出力与边际成本 |
| `consumer_results` | 各时段用户成交需求 |
| `settlements` | 发电结算明细 |
| `consumer_payments` | 用户支付明细 |

## 9. 交易中心编排流程

```text
UI
 └── TradingCenter::runDayAheadSimulation(inputs[96])
      ├── BidValidator::validate(input)
      ├── ConstraintChecker::checkFeasibility(input)
      ├── ClearingEngine::clear(input)       // Piecewise 或 Quadratic
      ├── SettlementEngine::settle(input, result)
      └── 返回 TimeSlotResult[96]
```

校验失败时，出清返回 `feasible = false` 并携带第一条错误信息，不进入结算。

## 10. 测试清单（验收用例）

| 类别 | 用例 | 验收标准 |
| --- | --- | --- |
| 报价校验 | 非单调段价、负电量、段数超 10、累计增量超 `Pmax-Pmin` | 出清前拒绝并给出原因 |
| 基础撮合 | 单机组、多机组、同价段、供需不平衡 | 成交量正确、价格符合规则、机组不超 `Pmax` |
| 基础撮合 | 需求低于 `ΣPmin` | 判定不可行 |
| 二次求解 | `QD` 越界 | 判定不可行 |
| 二次求解 | 全自由、部分压上限/下限、全压界、`QD=ΣPmin`、`QD=ΣPmax` | 满足 KKT 条件，误差小于容差 |
| 结算 | 统一价格、`Pmin` 分摊 | `总支付 = 总收益`，平衡误差小于容差 |
| 一致性 | 分段报价与等价二次曲线 | 两种模式结果一致或误差可控 |

## 11. 变量速查

| 符号 | 含义 | 单位 |
| --- | --- | --- |
| `Pi` | 第 i 台机组出力 | MW |
| `Pmin_i / Pmax_i` | 机组最小/最大出力 | MW |
| `Pmarket_i` | 机组增量成交 | MW |
| `Cclear / λ` | 统一出清价 | 元/MWh |
| `Bi(Pi)` | 总报价成本函数 | 元 |
| `ai, bi, ci` | 二次系数，`ai>0` | 模型参数 |
| `QD` | 固定总需求 | MW |
| `Cbuy / Csell` | 当前买方/卖方报价 | 元/MWh |
| `Qtrade` | 本次成交量 | MW |
| `k` | 时段时长 | 0.25 h |
| `shortageMw` | 缺额 | MW |
