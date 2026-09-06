// ============================================================
// SettlementEngine.cpp —— 市场结算引擎（第4轮：完整结算逻辑）
// 前置依赖：ClearingResult.genClearedQty / consClearedQty 已由
//           ClearingEngine::doClearing 填入各主体成交电量。
// 本文件职责（doSettlement）：
//   1. 函数开头执行全套参数合法性检查（不依赖后期测试兜底）；
//   2. 遍历全部 Generator / Consumer，无论是否成交一律回填：
//      未成交主体 clearedQuantity、income/bill 显式置 0，
//      不允许跳过、不允许残留旧值；
//   3. 使用统一出清价 res.clearPrice 结算：
//      机组收入 = 成交电量 x clearPrice；
//      用户账单 = 成交电量 x clearPrice；
//   4. unordered_map 按 key 读取一律先 find 防御，缺省按 0 处理；
//   5. 结算金额同时写回 res.genIncome / res.consBill；
//   6. totalTradeQty == 0 时，全部主体收入/账单置 0；
//   7. 不修改 ClearingEngine 代码、无 Qt、无二次曲线。
// ============================================================

#include "SettlementEngine.h"

// 匿名命名空间：翻译单元内部常量，不对外暴露
namespace
{
constexpr int kTotalTimeSlots = 96;   // 日前市场交易时段总数（15分钟 x 96）
}  // namespace

// ============================================================
// 执行单个交易时段的结算
// 入参：res         该时段出清结果（in：timeSlot/clearPrice/totalTradeQty/
//                   genClearedQty/consClearedQty；out：回写 genIncome/consBill）
//       generators  发电机组集合（回填该时段 clearedQuantity / income）
//       consumers   用户集合（回填该时段 clearedQuantity / bill）
// 返回：void（结算结果体现在 res 与各主体对象中）
// 说明：仅处理 res.timeSlot 一个时段，可连续调用 96 次完成全日结算，
//       各时段互不干扰；引擎无内部状态。
// ============================================================
void SettlementEngine::doSettlement(
    ClearingResult& res,
    std::vector<Generator>& generators,
    std::vector<Consumer>& consumers)
{
    // ============================================================
    // 第一步：全套参数合法性检查（前置，校验不能留到测试阶段）
    // ============================================================

    // 时段编号越界（含默认值/无效哨兵 -1）-> 无法定位回填下标，直接返回
    if (res.timeSlot < 0 || res.timeSlot >= kTotalTimeSlots)
    {
        return;
    }

    // 防御：总成交电量为负（异常数据）-> 拒绝落账，直接返回
    if (res.totalTradeQty < 0.0)
    {
        return;
    }

    // 防御：统一出清价为负（异常数据）-> 拒绝落账，直接返回
    if (res.clearPrice < 0.0)
    {
        return;
    }

    // 双侧均无主体：无对象可回填，直接返回（单侧为空仍继续，另一侧需置 0）
    if (generators.empty() && consumers.empty())
    {
        return;
    }

    const int    slot  = res.timeSlot;   // 本次结算的时段下标（0~95）
    const double price = (res.totalTradeQty > 0.0) ? res.clearPrice : 0.0;
    // 规则6：totalTradeQty == 0 -> 结算价归零，
    //        统一使下方 收入/账单 = qty x 0 = 0，覆盖所有主体；
    //        即使防御性场景下 map 残留 qty>0，金额也强制为 0。

    // 工具 lambda 1：按 key 从明细 map 读取电量；
    //               key 不存在（该主体未成交 / 未上榜）一律返回 0（规则4防御）
    auto lookupQty = [](const std::unordered_map<std::string, double>& qtyMap,
                        const std::string& id) -> double
    {
        const auto it = qtyMap.find(id);
        return (it != qtyMap.end()) ? it->second : 0.0;
    };

    // 工具 lambda 2：结果容器未初始化/容量不足时补齐 96 个 0（防下标越界）；
    //               已初始化则不动，避免清掉其它时段已结算的历史数据
    auto ensureCapacity = [](std::vector<double>& v)
    {
        if (v.size() < static_cast<std::size_t>(kTotalTimeSlots))
        {
            v.assign(kTotalTimeSlots, 0.0);
        }
    };

    // ============================================================
    // 第二步：发电机组结算回填（遍历全部机组，含未成交）
    // ============================================================
    for (Generator& gen : generators)
    {
        // 容器防御：不足 96 槽位先补齐，保证下标 slot 可安全写入
        ensureCapacity(gen.clearedQuantity);
        ensureCapacity(gen.income);

        // 成交电量：key 不存在按 0（未成交主体）
        const double qty = lookupQty(res.genClearedQty, gen.getId());

        // 统一出清价结算：机组收入 = 成交电量 x clearPrice
        const double income = qty * price;

        // 回填主体对象该时段结果：
        // 未成交时 qty=0 / income=0，显式赋值覆盖，不保留任何旧值
        gen.clearedQuantity[slot] = qty;
        gen.income[slot]          = income;

        // 结算明细同步写回 ClearingResult（含未成交主体的 0，便于下游读取）
        res.genIncome[gen.getId()] = income;
    }

    // ============================================================
    // 第三步：用户结算回填（遍历全部用户，含未成交）
    // ============================================================
    for (Consumer& cons : consumers)
    {
        // 容器防御：不足 96 槽位先补齐，保证下标 slot 可安全写入
        ensureCapacity(cons.clearedQuantity);
        ensureCapacity(cons.bill);

        // 成交电量：key 不存在按 0（未成交主体）
        const double qty = lookupQty(res.consClearedQty, cons.getId());

        // 统一出清价结算：用户账单 = 成交电量 x clearPrice
        const double bill = qty * price;

        // 回填主体对象该时段结果：
        // 未成交时 qty=0 / bill=0，显式赋值覆盖，不保留任何旧值
        cons.clearedQuantity[slot] = qty;
        cons.bill[slot]            = bill;

        // 结算明细同步写回 ClearingResult（含未成交主体的 0，便于下游读取）
        res.consBill[cons.getId()] = bill;
    }

    // 结算完成说明：
    //   - 全部主体（成交 + 未成交）均已回填，未成交者字段显式为 0；
    //   - 金额口径统一：收入/账单 = 成交电量 x 统一出清价（或 totalTradeQty==0 时全 0）；
    //   - res.genIncome / res.consBill 已补齐全部主体 key。
}
