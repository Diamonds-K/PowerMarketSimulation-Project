#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "Model/Consumer/Consumer.h"
#include "Model/Generator/Generator.h"
#include "Model/MarketInput/MarketInput.h"
#include "TradingCenter/TradingCenter.h"

using namespace pms;

namespace {

constexpr double kPMin = 10.0;
constexpr double kPMax = 100.0;

bool near(double lhs, double rhs, double tolerance = 1e-6) {
    return std::fabs(lhs - rhs) <= tolerance;
}

Generator makeGenerator(const std::string& id) {
    Generator generator(id, kPMin, kPMax);
    BidSheet sheet;
    sheet.setMode(BidSheet::Mode::Piecewise);
    sheet.setOwnerId(id);
    sheet.addSegment(BidSegment(1, 30.0, 150.0));
    sheet.addSegment(BidSegment(2, 30.0, 200.0));
    sheet.addSegment(BidSegment(3, 30.0, 250.0));
    generator.setBidSheet(sheet);
    return generator;
}

Generator makeQuadraticGenerator(const std::string& id) {
    Generator generator(id, kPMin, kPMax);
    BidSheet sheet;
    sheet.setMode(BidSheet::Mode::Quadratic);
    sheet.setOwnerId(id);
    sheet.setQuadraticCoefficients(0.05, 10.0, 0.0);
    generator.setBidSheet(sheet);
    return generator;
}

Consumer makeConsumer(const std::string& id, double declaredDemand) {
    Consumer consumer(id, declaredDemand);
    BidSheet sheet;
    sheet.setMode(BidSheet::Mode::Piecewise);
    sheet.setOwnerId(id);
    sheet.addSegment(BidSegment(1, declaredDemand, 300.0));
    consumer.setBidSheet(sheet);
    return consumer;
}

std::vector<MarketInput> makePiecewiseDay(bool withInfeasibleLastSlot) {
    std::vector<MarketInput> inputs;
    inputs.reserve(96);
    for (int slot = 0; slot < 96; ++slot) {
        const double demand =
            (withInfeasibleLastSlot && slot == 95) ? 5.0 : 50.0 + (slot % 30);
        MarketInput input(slot, MarketMode::Piecewise);
        input.addGenerator(makeGenerator("G1"));
        input.addConsumer(makeConsumer("C1", demand));
        inputs.push_back(input);
    }
    return inputs;
}

std::vector<MarketInput> makeQuadraticDay() {
    std::vector<MarketInput> inputs;
    inputs.reserve(96);
    for (int slot = 0; slot < 96; ++slot) {
        MarketInput input(slot, MarketMode::Quadratic);
        input.addGenerator(makeQuadraticGenerator("G1"));
        input.addConsumer(Consumer("C1", 40.0 + (slot % 40)));
        inputs.push_back(input);
    }
    return inputs;
}

void testPiecewiseDayAheadSimulation() {
    TradingCenter tradingCenter;
    const std::vector<TimeSlotResult> results =
        tradingCenter.runDayAheadSimulation(makePiecewiseDay(false));

    assert(results.size() == 96);
    for (std::size_t i = 0; i < results.size(); ++i) {
        const TimeSlotResult& slot = results[i];
        assert(slot.input.timeSlot() == static_cast<int>(i));
        assert(slot.market.timeSlot() == static_cast<int>(i));
        assert(slot.market.feasible());

        // 结算必须与出清结果一致：平衡误差为零，且用户成交等于出清电量。
        assert(near(slot.settlement.balanceYuan(), 0.0, 1e-6));
        assert(slot.settlement.generatorSettlements().size() == 1);
        assert(slot.settlement.consumerSettlements().size() == 1);
        assert(near(slot.settlement.consumerSettlements()[0].clearedDemandMw,
                    slot.market.clearingVolumeMw()));

        // 机组出力必须在 [Pmin, Pmax] 内，且不超出申报需求。
        const double output = slot.market.generatorResults()[0].outputMw;
        assert(output >= kPMin - 1e-9);
        assert(output <= kPMax + 1e-9);
        assert(slot.market.clearingVolumeMw() <=
               slot.input.totalDeclaredDemandMw() + 1e-9);
        assert(near(slot.market.shortageMw(),
                    slot.input.totalDeclaredDemandMw() -
                        slot.market.clearingVolumeMw(),
                    1e-9));
    }
}

void testInfeasibleSlotKeepsInputShape() {
    TradingCenter tradingCenter;
    const std::vector<TimeSlotResult> results =
        tradingCenter.runDayAheadSimulation(makePiecewiseDay(true));

    assert(results.size() == 96);
    const TimeSlotResult& last = results.back();

    assert(!last.market.feasible());
    assert(!last.market.message().empty());
    // 不可行时段仍需返回等长明细，且不产生结算。
    assert(last.market.generatorResults().size() == last.input.generators().size());
    assert(last.market.consumerResults().size() == last.input.consumers().size());
    assert(near(last.market.generatorResults()[0].outputMw, 0.0));
    assert(last.settlement.generatorSettlements().empty());
    assert(near(last.settlement.totalPaymentYuan(), 0.0));

    // 其余时段仍然正常出清。
    assert(results[0].market.feasible());
    assert(results[94].market.feasible());
}

void testQuadraticDayAheadSimulation() {
    TradingCenter tradingCenter;
    const std::vector<TimeSlotResult> results =
        tradingCenter.runDayAheadSimulation(makeQuadraticDay());

    assert(results.size() == 96);
    for (std::size_t i = 0; i < results.size(); ++i) {
        const TimeSlotResult& slot = results[i];
        assert(slot.market.feasible());
        // 二次模式需求刚性：出清电量必须等于固定需求 QD。
        // 容差取 1e-4：λ 二分的停机条件是 |S-QD|<=1e-6 或 |hi-lo|<=1e-6，
        // 后者对应的出力误差可达 dλ/(2a)，与 DESIGN 第 10 节的容差口径一致。
        assert(near(slot.market.clearingVolumeMw(),
                    slot.input.totalFixedDemandMw(),
                    1e-4));
        assert(slot.market.clearingPriceYuanPerMwh() > 0.0);
        assert(near(slot.settlement.balanceYuan(), 0.0, 1e-4));
    }
}

void testMixedModeBatchUsesEnginePerSlot() {
    std::vector<MarketInput> inputs;

    MarketInput piecewise0(0, MarketMode::Piecewise);
    piecewise0.addGenerator(makeGenerator("G1"));
    piecewise0.addConsumer(makeConsumer("C1", 70.0));
    inputs.push_back(piecewise0);

    MarketInput quadratic1(1, MarketMode::Quadratic);
    quadratic1.addGenerator(makeQuadraticGenerator("G1"));
    quadratic1.addConsumer(Consumer("C1", 70.0));
    inputs.push_back(quadratic1);

    MarketInput piecewise2(2, MarketMode::Piecewise);
    piecewise2.addGenerator(makeGenerator("G1"));
    piecewise2.addConsumer(makeConsumer("C1", 70.0));
    inputs.push_back(piecewise2);

    TradingCenter tradingCenter;
    const std::vector<TimeSlotResult> results =
        tradingCenter.runDayAheadSimulation(inputs);

    assert(results.size() == 3);
    // 分段模式（需求 70，ΣPmin=10）：增量成交 60 用完，最后成交段为 200 元/MWh。
    assert(near(results[0].market.clearingPriceYuanPerMwh(), 200.0));
    // 二次模式：λ 由 KKT 条件解得（a=0.05, b=10, P=70 => λ=17）。
    assert(near(results[1].market.clearingPriceYuanPerMwh(), 17.0, 1e-3));
    assert(near(results[2].market.clearingPriceYuanPerMwh(), 200.0));
}

} // namespace

int main() {
    testPiecewiseDayAheadSimulation();
    testInfeasibleSlotKeepsInputShape();
    testQuadraticDayAheadSimulation();
    testMixedModeBatchUsesEnginePerSlot();

    std::cout << "TradingCenterTest passed" << std::endl;
    return 0;
}
