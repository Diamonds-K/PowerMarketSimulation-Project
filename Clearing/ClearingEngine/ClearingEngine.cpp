#include "Clearing/ClearingEngine/ClearingEngine.h"

namespace pms {

namespace {

// 用户申报总量口径：二次模式取固定需求 QD，分段模式取各段增量电量之和。
double declaredDemandOf(const Consumer& consumer, MarketMode mode) {
    if (mode == MarketMode::Quadratic) {
        return consumer.fixedDemandMw();
    }
    return consumer.bidSheet().totalIncrementMw();
}

} // namespace

MarketResult makeInfeasibleResult(const MarketInput& input, const std::string& message) {
    MarketResult result;
    result.setTimeSlot(input.timeSlot());
    result.setFeasible(false);
    result.setMessage(message);

    for (const auto& generator : input.generators()) {
        GeneratorResult item;
        item.generatorId = generator.id();
        result.addGeneratorResult(item);
    }
    for (const auto& consumer : input.consumers()) {
        ConsumerResult item;
        item.consumerId = consumer.id();
        item.declaredDemandMw = declaredDemandOf(consumer, input.mode());
        result.addConsumerResult(item);
    }
    return result;
}

} // namespace pms
