#include "UI/SimulationRunner/SimulationRunner.h"

#include "UI/ConsumerWidget/ConsumerWidget.h"
#include "UI/GeneratorWidget/GeneratorWidget.h"

namespace pms {

SimulationRunner::SimulationRunner(GeneratorWidget* generatorWidget,
                                   ConsumerWidget* consumerWidget)
    : generatorWidget_(generatorWidget),
      consumerWidget_(consumerWidget) {}

MarketInput SimulationRunner::buildSlotInput(int slotIndex, bool quadratic) {
    const MarketMode mode = quadratic ? MarketMode::Quadratic : MarketMode::Piecewise;
    MarketInput input(slotIndex, mode);

    if (generatorWidget_ != nullptr) {
        // 组装前先把编辑区内容写回数据模型，避免「刚输入还没点提交」的漏读。
        generatorWidget_->flushCurrentSlot();
        for (Generator generator : generatorWidget_->buildGeneratorsForSlot(slotIndex, quadratic)) {
            generator.bidSheet().setTimeSlot(slotIndex);
            input.addGenerator(generator);
        }
    }
    if (consumerWidget_ != nullptr) {
        consumerWidget_->flushCurrentSlot();
        for (Consumer consumer : consumerWidget_->buildConsumersForSlot(slotIndex, quadratic)) {
            consumer.bidSheet().setTimeSlot(slotIndex);
            input.addConsumer(consumer);
        }
    }
    return input;
}

ValidationReport SimulationRunner::validateSlot(int slotIndex, bool quadratic) {
    return tradingCenter_.submit(buildSlotInput(slotIndex, quadratic));
}

TimeSlotResult SimulationRunner::runSlot(int slotIndex, bool quadratic) {
    TimeSlotResult slot;
    slot.input = buildSlotInput(slotIndex, quadratic);
    slot.market = tradingCenter_.clear(slot.input);
    if (slot.market.feasible()) {
        slot.settlement = tradingCenter_.settle(slot.input, slot.market);
    }
    return slot;
}

std::vector<TimeSlotResult> SimulationRunner::runAll(bool quadratic) {
    std::vector<MarketInput> inputs;
    inputs.reserve(96);
    for (int slot = 0; slot < 96; ++slot) {
        inputs.push_back(buildSlotInput(slot, quadratic));
    }
    return tradingCenter_.runDayAheadSimulation(inputs);
}

void SimulationRunner::setQuadraticMode(bool quadratic) {
    quadraticMode_ = quadratic;
}

bool SimulationRunner::quadraticMode() const {
    return quadraticMode_;
}

GeneratorWidget* SimulationRunner::generatorWidget() const {
    return generatorWidget_;
}

ConsumerWidget* SimulationRunner::consumerWidget() const {
    return consumerWidget_;
}

QString SimulationRunner::generatorName(const QString& generatorId) const {
    if (generatorWidget_ == nullptr) {
        return QString();
    }
    for (int i = 0; i < generatorWidget_->unitCount(); ++i) {
        if (generatorWidget_->unitId(i) == generatorId) {
            return generatorWidget_->unitName(i);
        }
    }
    return QString();
}

QString SimulationRunner::consumerName(const QString& consumerId) const {
    if (consumerWidget_ == nullptr) {
        return QString();
    }
    for (int i = 0; i < consumerWidget_->unitCount(); ++i) {
        if (consumerWidget_->unitId(i) == consumerId) {
            return consumerWidget_->unitName(i);
        }
    }
    return QString();
}

TradingCenter& SimulationRunner::tradingCenter() {
    return tradingCenter_;
}

} // namespace pms
