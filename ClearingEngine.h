#ifndef CLEARING_ENGINE_H
#define CLEARING_ENGINE_H

#include <string>
#include <vector>
#include "DataModels.h"

// ============================================================
// 市场出清引擎（ClearingEngine）
// 职责：对单个交易时段执行市场出清（构建供需曲线 + 求统一出清价
//       与分段成交电量），只计算、不落盘。
// 约束：
//   1. doClearing 输入容器为 const 只读，不修改 Generator /
//      Consumer 对象的任何成员；主体结果回填统一由 SettlementEngine 完成。
//   2. 对外接口 doClearing 函数体内必须先做参数合法性检查
//      （timeSlot 越界 / 集合引用自检等），非法输入直接返回空结果，
//      不抛异常、不越界访问，校验不能留到测试阶段。
//   3. 引擎无内部状态，可连续循环调用 96 次，各时段数据完全隔离。
//   4. 不实现二次曲线优化、不依赖任何 Qt 头文件。
// ============================================================
class ClearingEngine
{
public:
    // 执行单个交易时段的市场出清
    // 入参：timeSlot    时段编号（0~95）
    //       generators  参与出清的发电机组（const 只读）
    //       consumers   参与出清的用户（const 只读）
    // 返回：ClearingResult 该时段出清结果（出清价、总成交电量、各主体成交明细）
    // 实现要求：函数开头校验 timeSlot ∈ [0,95]，非法时返回空结果
    // （timeSlot 置为非法值便于下游识别），且不触碰任何输入对象。
    ClearingResult doClearing(int timeSlot,
                              const std::vector<Generator>& generators,
                              const std::vector<Consumer>& consumers) const;

private:
    // 私有嵌套报价段：报价段 + 归属主体 id
    // 仅类内可见，不对外暴露；用于构建携带主体标识的报价曲线
    struct OwnedBidSegment
    {
        BidSegment seg;        // 报价段（电量-价格）
        std::string ownerId;   // 该报价段归属主体 id（机组/用户）
    };

    // 构建供给曲线
    // 入参：timeSlot     时段编号（0~95）
    //       generators  发电机组集合（const 只读）
    // 返回：携带机组 id 的报价段序列，按价格升序（低 -> 高）
    // 实现要求：仅读取各机组 getBidSheet(timeSlot) 的报价段，不改动机组对象
    std::vector<OwnedBidSegment> buildSupplyCurve(
        int timeSlot,
        const std::vector<Generator>& generators) const;

    // 构建需求曲线
    // 入参：timeSlot    时段编号（0~95）
    //       consumers  用户集合（const 只读）
    // 返回：携带用户 id 的报价段序列，按价格降序（高 -> 低）
    // 实现要求：仅读取各用户 getBidSheet(timeSlot) 的报价段，不改动用户对象
    std::vector<OwnedBidSegment> buildDemandCurve(
        int timeSlot,
        const std::vector<Consumer>& consumers) const;
};

#endif // CLEARING_ENGINE_H
