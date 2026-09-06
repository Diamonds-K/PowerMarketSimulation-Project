#ifndef TRADING_CENTER_H
#define TRADING_CENTER_H

#include <vector>
#include "DataModels.h"
#include "ClearingEngine.h"
#include "SettlementEngine.h"

// ============================================================
// 交易中心（TradingCenter）
// 职责：独立调度类，严禁继承 MarketParticipant。
//       内部持有市场主体的完整对象实例（Generator / Consumer），
//       持有 ClearingEngine、SettlementEngine 实例，编排
//       「出清 -> 结算」流程，支持单时段仿真与完整 96 时段仿真。
// 说明：引擎无内部状态，可对 0~95 各时段循环调用，各时段数据
//       相互隔离；出清只读计算、结算负责主体回填。
// ============================================================
class TradingCenter
{
public:
    // 添加一个发电机组（值拷贝存入内部容器，重复 id 由调用方保证）
    // 入参：gen 待添加的发电机组
    void addGenerator(const Generator& gen);

    // 添加一个用户（值拷贝存入内部容器，重复 id 由调用方保证）
    // 入参：cons 待添加的用户
    void addConsumer(const Consumer& cons);

    // 执行单个交易时段仿真（内部先出清、后结算）
    // 入参：timeSlot 时段编号（0~95）
    // 返回：该时段完整的出清结算结果（同时回填各主体数据）
    // 说明：可连续调用 96 次完成全日前仿真；结果不写入 allTimeSlotResult
    ClearingResult runSingleTimeSlot(int timeSlot);

    // 执行完整 96 时段日前市场仿真
    // 说明：内部循环 timeSlot = 0~95 调用 runSingleTimeSlot，
    //       各时段结果存入 allTimeSlotResult（下标与时段号一一对应）
    void runWholeDaySimulation();

    // 获取全部 96 时段出清结算结果（只读访问）
    // 说明：仿真完成后 allTimeSlotResult[timeSlot] 对应时段结果；
    //       尚未仿真时返回空容器
    const std::vector<ClearingResult>& getAllTimeSlotResults() const;

    // 获取内部发电机组容器（只读，供外部查询/校验）
    const std::vector<Generator>& getGenerators() const;

    // 获取内部用户容器（只读，供外部查询/校验）
    const std::vector<Consumer>& getConsumers() const;

private:
    std::vector<Generator> generators;              // 发电机组完整对象集合
    std::vector<Consumer> consumers;                // 用户完整对象集合
    std::vector<ClearingResult> allTimeSlotResult;  // 全部96时段出清结算结果

    ClearingEngine clearingEngine;                  // 出清引擎实例
    SettlementEngine settlementEngine;              // 结算引擎实例
};

#endif // TRADING_CENTER_H
