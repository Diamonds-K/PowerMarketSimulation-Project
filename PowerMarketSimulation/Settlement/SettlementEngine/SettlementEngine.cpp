#include "Settlement/SettlementEngine/SettlementEngine.h"

namespace pms {

SettlementResult SettlementEngine::settle(const MarketInput& input,
                                          const MarketResult& result) const {
    SettlementResult settlement;
    settlement.setTimeSlot(result.timeSlot());
    const double price = result.clearingPriceYuanPerMwh();
    settlement.setClearingPriceYuanPerMwh(price);

    const auto& generatorResults = result.generatorResults();
    const auto& consumerResults = result.consumerResults();

    double totalRevenue = 0.0;
    double totalCost = 0.0;
    double totalPayment = 0.0;

    for (size_t i = 0; i < input.generators().size(); ++i) {
        const Generator& generator = input.generators()[i];
        const double outputMw =
            i < generatorResults.size() ? generatorResults[i].outputMw : 0.0;

        const double revenueYuan = price * outputMw * kSlotDurationHours;

        double costYuan = 0.0;
        if (generator.bidSheet().mode() == BidSheet::Mode::Quadratic) {
            const double a = generator.bidSheet().quadraticA();
            const double b = generator.bidSheet().quadraticB();
            const double c = generator.bidSheet().quadraticC();
            costYuan = (a * outputMw * outputMw + b * outputMw + c) * kSlotDurationHours;
        } else {
            const double baseCostYuan =
                price * generator.pMinMw() * kSlotDurationHours;
            double incrementCostYuan = 0.0;
            if (i < generatorResults.size()) {
                for (const auto& segment : generatorResults[i].matchedSegments) {
                    incrementCostYuan +=
                        segment.priceYuanPerMwh * segment.quantityMw * kSlotDurationHours;
                }
            }
            costYuan = baseCostYuan + incrementCostYuan;
        }

        GeneratorSettlement settlementItem;
        settlementItem.generatorId = generator.id();
        settlementItem.outputMw = outputMw;
        settlementItem.revenueYuan = revenueYuan;
        settlementItem.costYuan = costYuan;
        settlementItem.profitYuan = revenueYuan - costYuan;
        settlement.addGeneratorSettlement(settlementItem);

        totalRevenue += revenueYuan;
        totalCost += costYuan;
    }

    for (size_t i = 0; i < input.consumers().size(); ++i) {
        const Consumer& consumer = input.consumers()[i];
        const double clearedMw =
            i < consumerResults.size() ? consumerResults[i].clearedDemandMw : 0.0;
        const double paymentYuan = price * clearedMw * kSlotDurationHours;

        ConsumerSettlement settlementItem;
        settlementItem.consumerId = consumer.id();
        settlementItem.clearedDemandMw = clearedMw;
        settlementItem.paymentYuan = paymentYuan;
        settlement.addConsumerSettlement(settlementItem);

        totalPayment += paymentYuan;
    }

    settlement.setTotalRevenueYuan(totalRevenue);
    settlement.setTotalCostYuan(totalCost);
    settlement.setTotalProfitYuan(totalRevenue - totalCost);
    settlement.setTotalPaymentYuan(totalPayment);
    return settlement;
}

} // namespace pms
