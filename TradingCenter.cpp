// ============================================================
// TradingCenter.cpp —— 交易中心实现（第5轮补齐）
// 职责：独立调度类（不继承 MarketParticipant）。
//   1. addGenerator / addConsumer：值拷贝注册主体；
//   2. runSingleTimeSlot：单时段「出清 -> 结算」编排；
//   3. runWholeDaySimulation：循环 0~95 完成 96 时段日前仿真，
//      各时段结果按序存入 allTimeSlotResult；
//   4. 三个只读 getter 供外部查询/校验。
// 注意：出清引擎只读计算、结算引擎负责主体回填，
//       引擎无内部状态，可安全连续调用。
// ============================================================

#include "TradingCenter.h"

namespace
{
constexpr int kTotalTimeSlots = 96;   // 日前市场交易时段总数（0~95）
}  // namespace

// 添加发电机组（值拷贝存入内部容器）
void TradingCenter::addGenerator(const Generator& gen)
{
    generators.push_back(gen);
}

// 添加用户（值拷贝存入内部容器）
void TradingCenter::addConsumer(const Consumer& cons)
{
    consumers.push_back(cons);
}

// 执行单个交易时段仿真：先出清（const 只读计算），后结算（主体回填）
ClearingResult TradingCenter::runSingleTimeSlot(int timeSlot)
{
    // 出清：返回该时段撮合结果（timeSlot/clearPrice/totalTradeQty/成交明细）
    ClearingResult res = clearingEngine.doClearing(timeSlot, generators, consumers);

    // 结算：按统一出清价回填全部主体该时段结果，并写回 res 的
    // genIncome / consBill 明细（引擎内部自带参数合法性校验）
    settlementEngine.doSettlement(res, generators, consumers);

    return res;
}

// 执行完整 96 时段日前市场仿真：结果按序存入 allTimeSlotResult
void TradingCenter::runWholeDaySimulation()
{
    allTimeSlotResult.clear();
    allTimeSlotResult.reserve(kTotalTimeSlots);

    for (int timeSlot = 0; timeSlot < kTotalTimeSlots; ++timeSlot)
    {
        // 各时段独立出清+结算，时段间数据互不干扰
        allTimeSlotResult.push_back(runSingleTimeSlot(timeSlot));
    }
}

// 获取全部时段出清结算结果（只读；仿真前为空容器）
const std::vector<ClearingResult>& TradingCenter::getAllTimeSlotResults() const
{
    return allTimeSlotResult;
}

// 获取内部发电机组容器（只读，供外部查询/校验）
const std::vector<Generator>& TradingCenter::getGenerators() const
{
    return generators;
}

// 获取内部用户容器（只读，供外部查询/校验）
const std::vector<Consumer>& TradingCenter::getConsumers() const
{
    return consumers;
}
