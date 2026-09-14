#pragma once

#include <memory>
#include <string>
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

// 一个时段的完整仿真结果：原始输入、出清结果、结算结果。
struct TimeSlotResult {
    MarketInput input;
    MarketResult market;
    SettlementResult settlement;
};

class TradingCenter {
public:
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

    void setPiecewiseEngine(std::unique_ptr<ClearingEngine> engine);
    void setQuadraticEngine(std::unique_ptr<ClearingEngine> engine);

private:
    ClearingEngine& engineFor(const MarketInput& input) const;

    std::unique_ptr<ClearingEngine> piecewiseEngine_;
    std::unique_ptr<ClearingEngine> quadraticEngine_;
    SettlementEngine settlementEngine_;
};

} // namespace pms
