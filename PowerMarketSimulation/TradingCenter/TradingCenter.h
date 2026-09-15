#pragma once

#include <memory>
#include <vector>

#include "Clearing/ClearingEngine/ClearingEngine.h"
#include "Clearing/PiecewiseClearing/PiecewiseClearing.h"
#include "Clearing/QuadraticClearing/QuadraticClearing.h"
#include "Model/MarketInput/MarketInput.h"
#include "Model/MarketResult/MarketResult.h"
#include "Model/SettlementResult/SettlementResult.h"
#include "Settlement/SettlementEngine/SettlementEngine.h"
#include "Validation/BidValidator/BidValidator.h"

namespace pms {

// 一个时段的完整仿真结果：出清结果与结算结果。
struct TimeSlotResult {
    MarketResult market;
    SettlementResult settlement;
};

class TradingCenter {
public:
    // 创建交易中心，并初始化分段报价与二次曲线两套出清引擎。
    TradingCenter();

    // 校验单个时段的申报数据。
    ValidationReport submit(const MarketInput& input) const;

    // 先校验，再根据报价模式选择分段或二次出清算法。
    MarketResult clear(const MarketInput& input) const;

    // 根据出清结果计算结算结果。
    SettlementResult settle(const MarketInput& input, const MarketResult& result) const;

    // 一次运行 96 个时段的日前市场仿真。
    std::vector<TimeSlotResult> runDayAheadSimulation(
        const std::vector<MarketInput>& inputs) const;

private:
    // 根据市场模式返回对应的出清引擎。
    ClearingEngine& engineFor(const MarketInput& input) const;

    std::unique_ptr<ClearingEngine> piecewiseEngine_;
    std::unique_ptr<ClearingEngine> quadraticEngine_;
    SettlementEngine settlementEngine_;
};

} // namespace pms
