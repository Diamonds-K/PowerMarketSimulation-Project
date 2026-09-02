#include "Model/MarketInput/MarketInput.h"

namespace pms {

MarketInput::MarketInput(int timeSlot, MarketMode mode)
    : timeSlot_(timeSlot),
      mode_(mode) {}

int MarketInput::timeSlot() const {
    return timeSlot_;
}

void MarketInput::setTimeSlot(int timeSlot) {
    timeSlot_ = timeSlot;
}

MarketMode MarketInput::mode() const {
    return mode_;
}

void MarketInput::setMode(MarketMode mode) {
    mode_ = mode;
}

const std::vector<Generator>& MarketInput::generators() const {
    return generators_;
}

std::vector<Generator>& MarketInput::generators() {
    return generators_;
}

void MarketInput::addGenerator(const Generator& generator) {
    generators_.push_back(generator);
}

const std::vector<Consumer>& MarketInput::consumers() const {
    return consumers_;
}

std::vector<Consumer>& MarketInput::consumers() {
    return consumers_;
}

void MarketInput::addConsumer(const Consumer& consumer) {
    consumers_.push_back(consumer);
}

double MarketInput::totalFixedDemandMw() const {
    double total = 0.0;
    for (const auto& consumer : consumers_) {
        total += consumer.fixedDemandMw();
    }
    return total;
}

double MarketInput::totalDeclaredDemandMw() const {
    double total = 0.0;
    for (const auto& consumer : consumers_) {
        total += consumer.bidSheet().totalIncrementMw();
    }
    return total;
}

} // namespace pms
