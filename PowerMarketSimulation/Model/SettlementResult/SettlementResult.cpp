#include "Model/SettlementResult/SettlementResult.h"

namespace pms {

int SettlementResult::timeSlot() const {
    return timeSlot_;
}

void SettlementResult::setTimeSlot(int timeSlot) {
    timeSlot_ = timeSlot;
}

double SettlementResult::clearingPriceYuanPerMwh() const {
    return clearingPriceYuanPerMwh_;
}

void SettlementResult::setClearingPriceYuanPerMwh(double price) {
    clearingPriceYuanPerMwh_ = price;
}

double SettlementResult::totalRevenueYuan() const {
    return totalRevenueYuan_;
}

void SettlementResult::setTotalRevenueYuan(double value) {
    totalRevenueYuan_ = value;
}

double SettlementResult::totalCostYuan() const {
    return totalCostYuan_;
}

void SettlementResult::setTotalCostYuan(double value) {
    totalCostYuan_ = value;
}

double SettlementResult::totalProfitYuan() const {
    return totalProfitYuan_;
}

void SettlementResult::setTotalProfitYuan(double value) {
    totalProfitYuan_ = value;
}

double SettlementResult::totalPaymentYuan() const {
    return totalPaymentYuan_;
}

void SettlementResult::setTotalPaymentYuan(double value) {
    totalPaymentYuan_ = value;
}

double SettlementResult::balanceYuan() const {
    return totalPaymentYuan_ - totalRevenueYuan_;
}

const std::vector<GeneratorSettlement>& SettlementResult::generatorSettlements() const {
    return generatorSettlements_;
}

void SettlementResult::addGeneratorSettlement(const GeneratorSettlement& settlement) {
    generatorSettlements_.push_back(settlement);
}

const std::vector<ConsumerSettlement>& SettlementResult::consumerSettlements() const {
    return consumerSettlements_;
}

void SettlementResult::addConsumerSettlement(const ConsumerSettlement& settlement) {
    consumerSettlements_.push_back(settlement);
}

} // namespace pms
