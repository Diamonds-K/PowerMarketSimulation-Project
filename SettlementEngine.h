#ifndef SETTLEMENT_ENGINE_H
#define SETTLEMENT_ENGINE_H

#include <vector>
#include "DataModels.h"

// ============================================================
// 结算引擎（SettlementEngine）
// 职责：根据单时段 ClearingResult 完成结算，把成交电量、收入、
//       账单回填到 Generator / Consumer 对象内部，并回写明细
//       到 res（genIncome / consBill）。
// 约束：
//   1. doSettlement 函数开头必须先做参数合法性校验：
//      res.timeSlot 不在 0~95 直接 return（不修改任何对象），
//      校验不能留到测试阶段。
//   2. 回填覆盖全部主体（成交 + 未成交）：未成交主体的
//      clearedQuantity、income/bill 显式赋值为 0。
//   3. 引擎无内部状态，与 ClearingEngine::doClearing 配对使用，
//      每次调用只处理一个时段，支持连续循环调用 96 次。
//   4. 不依赖任何 Qt 头文件。
// ============================================================
class SettlementEngine
{
public:
    // 执行单个交易时段的结算
    // 入参：res         该时段出清结果（结算后回写 genIncome/consBill 等明细）
    //       generators  发电机组集合（回填该时段 clearedQuantity / income）
    //       consumers   用户集合（回填该时段 clearedQuantity / bill）
    // 返回：void（结算结果体现在 res 与各主体对象中）
    // 实现要求：
    //   1) 函数开头校验 res.timeSlot ∈ [0,95]，非法直接 return；
    //   2) 按统一出清价结算：发电收入 = 成交电量 × clearPrice，
    //      用户账单 = 成交电量 × clearPrice；全部主体遍历回填，
    //      未成交主体 clearedQuantity / income / bill 显式置 0；
    //   3) 主体内部结果容器未初始化时先扩容/初始化（防下标越界）。
    void doSettlement(ClearingResult& res,
                      std::vector<Generator>& generators,
                      std::vector<Consumer>& consumers);
};

#endif // SETTLEMENT_ENGINE_H
