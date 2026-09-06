#include "Settlement/SettlementEngine/SettlementEngine.h"

namespace pms {

SettlementResult SettlementEngine::settle(const MarketInput& input,
                                          const MarketResult& result) const {
    SettlementResult settlement;
    settlement.setTimeSlot(result.timeSlot());
    const double price = result.clearingPriceYuanPerMwh();
    settlement.setClearingPriceYuanPerMwh(price);

    // 统一结算价格：所有成交电量都按同一个出清价结算。
    const auto& generatorResults = result.generatorResults();
    const auto& consumerResults = result.consumerResults();

    double totalRevenue = 0.0;
    double totalCost = 0.0;
    double totalPayment = 0.0;

    // 计算每台机组的收益、成本和利润。
    for (size_t i = 0; i < input.generators().size(); ++i) {
        const Generator& generator = input.generators()[i];
        const double outputMw =
            i < generatorResults.size() ? generatorResults[i].outputMw : 0.0;

        const double revenueYuan = price * outputMw * kSlotDurationHours;

        double costYuan = 0.0;
        if (generator.bidSheet().mode() == BidSheet::Mode::Quadratic) {
            // 二次模式用 aP^2 + bP + c 计算成本。
            const double a = generator.bidSheet().quadraticA();
            const double b = generator.bidSheet().quadraticB();
            const double c = generator.bidSheet().quadraticC();
            costYuan = (a * outputMw * outputMw + b * outputMw + c) * kSlotDurationHours;
        } else {
            // 分段模式成本 = Pmin 按出清价结算 + 各成交段按段价结算。
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

    // 计算每个用户的支付金额。
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
