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

struct TimeSlotResult {
    MarketInput input;
    MarketResult market;
    SettlementResult settlement;
};

class TradingCenter {
public:
    TradingCenter();

    ValidationReport submit(const MarketInput& input) const;
    MarketResult clear(const MarketInput& input) const;
    SettlementResult settle(const MarketInput& input, const MarketResult& result) const;

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
