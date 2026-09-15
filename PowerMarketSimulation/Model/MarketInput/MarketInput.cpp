#include "Model/MarketInput/MarketInput.h"

namespace pms {

MarketInput::MarketInput(int timeSlot, MarketMode mode)
    : timeSlot_(timeSlot),
      mode_(mode) {}

int MarketInput::timeSlot() const {
    return timeSlot_;
}

MarketMode MarketInput::mode() const {
    return mode_;
}

const std::vector<Generator>& MarketInput::generators() const {
    return generators_;
}

void MarketInput::addGenerator(const Generator& generator) {
    generators_.push_back(generator);
}

const std::vector<Consumer>& MarketInput::consumers() const {
    return consumers_;
}

void MarketInput::addConsumer(const Consumer& consumer) {
    consumers_.push_back(consumer);
}

double MarketInput::totalDeclaredDemandMw() const {
    double total = 0.0;
    for (const auto& consumer : consumers_) {
        total += consumer.bidSheet().totalIncrementMw();
    }
    return total;
}

double MarketInput::totalEffectiveDemandMw() const {
    double total = 0.0;
    for (const auto& consumer : consumers_) {
        total += consumer.effectiveDemandMw();
    }
    return total;
}

} // namespace pms
