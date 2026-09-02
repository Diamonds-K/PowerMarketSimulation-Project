#include "Model/MarketResult/MarketResult.h"

namespace pms {

int MarketResult::timeSlot() const {
    return timeSlot_;
}

void MarketResult::setTimeSlot(int timeSlot) {
    timeSlot_ = timeSlot;
}

double MarketResult::clearingPriceYuanPerMwh() const {
    return clearingPriceYuanPerMwh_;
}

void MarketResult::setClearingPriceYuanPerMwh(double price) {
    clearingPriceYuanPerMwh_ = price;
}

double MarketResult::clearingVolumeMw() const {
    return clearingVolumeMw_;
}

void MarketResult::setClearingVolumeMw(double volume) {
    clearingVolumeMw_ = volume;
}

double MarketResult::shortageMw() const {
    return shortageMw_;
}

void MarketResult::setShortageMw(double shortageMw) {
    shortageMw_ = shortageMw;
}

bool MarketResult::feasible() const {
    return feasible_;
}

void MarketResult::setFeasible(bool feasible) {
    feasible_ = feasible;
}

const std::string& MarketResult::message() const {
    return message_;
}

void MarketResult::setMessage(const std::string& message) {
    message_ = message;
}

const std::vector<GeneratorResult>& MarketResult::generatorResults() const {
    return generatorResults_;
}

void MarketResult::addGeneratorResult(const GeneratorResult& result) {
    generatorResults_.push_back(result);
}

void MarketResult::setGeneratorResults(const std::vector<GeneratorResult>& results) {
    generatorResults_ = results;
}

const std::vector<ConsumerResult>& MarketResult::consumerResults() const {
    return consumerResults_;
}

void MarketResult::addConsumerResult(const ConsumerResult& result) {
    consumerResults_.push_back(result);
}

void MarketResult::setConsumerResults(const std::vector<ConsumerResult>& results) {
    consumerResults_ = results;
}

} // namespace pms
