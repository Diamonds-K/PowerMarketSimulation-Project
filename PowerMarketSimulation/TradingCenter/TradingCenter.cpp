#include "TradingCenter/TradingCenter.h"

#include <utility>

#include "Validation/ConstraintChecker/ConstraintChecker.h"

namespace pms {

TradingCenter::TradingCenter()
    : piecewiseEngine_(std::make_unique<PiecewiseClearing>()),
      quadraticEngine_(std::make_unique<QuadraticClearing>()) {}

ValidationReport TradingCenter::submit(const MarketInput& input) const {
    ValidationReport report = BidValidator::validate(input);
    report.merge(ConstraintChecker::checkFeasibility(input));
    return report;
}

MarketResult TradingCenter::clear(const MarketInput& input) const {
    const ValidationReport report = submit(input);
    if (!report.ok()) {
        MarketResult result;
        result.setTimeSlot(input.timeSlot());
        result.setFeasible(false);
        const auto messages = report.messages();
        result.setMessage(messages.empty() ? "校验失败" : messages.front());
        return result;
    }
    return engineFor(input).clear(input);
}

SettlementResult TradingCenter::settle(const MarketInput& input,
                                       const MarketResult& result) const {
    return settlementEngine_.settle(input, result);
}

std::vector<TimeSlotResult> TradingCenter::runDayAheadSimulation(
    const std::vector<MarketInput>& inputs) const {
    std::vector<TimeSlotResult> results;
    results.reserve(inputs.size());
    for (const auto& input : inputs) {
        TimeSlotResult slot;
        slot.input = input;
        slot.market = clear(input);
        if (slot.market.feasible()) {
            slot.settlement = settle(input, slot.market);
        }
        results.push_back(slot);
    }
    return results;
}

void TradingCenter::setPiecewiseEngine(std::unique_ptr<ClearingEngine> engine) {
    piecewiseEngine_ = std::move(engine);
}

void TradingCenter::setQuadraticEngine(std::unique_ptr<ClearingEngine> engine) {
    quadraticEngine_ = std::move(engine);
}

ClearingEngine& TradingCenter::engineFor(const MarketInput& input) const {
    if (input.mode() == MarketMode::Quadratic) {
        return *quadraticEngine_;
    }
    return *piecewiseEngine_;
}

} // namespace pms
