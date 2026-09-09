#include <cassert>
#include <cmath>
#include <iostream>
#include <utility>
#include <vector>

#include "Clearing/PiecewiseClearing/PiecewiseClearing.h"
#include "Clearing/QuadraticClearing/QuadraticClearing.h"
#include "Model/Consumer/Consumer.h"
#include "Model/Generator/Generator.h"
#include "Model/MarketInput/MarketInput.h"

using namespace pms;

namespace {

bool near(double lhs, double rhs, double tolerance = 1e-6) {
    return std::fabs(lhs - rhs) <= tolerance;
}

Generator makeGenerator(const std::string& id,
                        double pMin,
                        double pMax,
                        const std::vector<std::pair<double, double>>& segments) {
    Generator generator(id, pMin, pMax);
    BidSheet sheet;
    sheet.setMode(BidSheet::Mode::Piecewise);
    sheet.setOwnerId(id);
    int segmentNo = 1;
    for (const auto& segment : segments) {
        sheet.addSegment(BidSegment(segmentNo++, segment.first, segment.second));
    }
    generator.setBidSheet(sheet);
    return generator;
}

Consumer makeConsumer(const std::string& id,
                      const std::vector<std::pair<double, double>>& segments) {
    Consumer consumer(id, 0.0);
    BidSheet sheet;
    sheet.setMode(BidSheet::Mode::Piecewise);
    sheet.setOwnerId(id);
    int segmentNo = 1;
    for (const auto& segment : segments) {
        sheet.addSegment(BidSegment(segmentNo++, segment.first, segment.second));
    }
    consumer.setBidSheet(sheet);
    return consumer;
}

MarketInput makePiecewiseInput(const std::vector<Generator>& generators,
                               const std::vector<Consumer>& consumers,
                               int timeSlot = 0) {
    MarketInput input(timeSlot, MarketMode::Piecewise);
    for (const auto& generator : generators) {
        input.addGenerator(generator);
    }
    for (const auto& consumer : consumers) {
        input.addConsumer(consumer);
    }
    return input;
}

void testSimpleMatching() {
    const auto generator =
        makeGenerator("G1", 20.0, 100.0, {{20.0, 200.0}, {30.0, 250.0}, {30.0, 300.0}});
    const auto consumer = makeConsumer("C1", {{50.0, 260.0}, {30.0, 240.0}});
    const auto input = makePiecewiseInput({generator}, {consumer});

    PiecewiseClearing clearing;
    const MarketResult result = clearing.clear(input);

    assert(result.feasible());
    assert(near(result.clearingPriceYuanPerMwh(), 250.0));
    assert(near(result.clearingVolumeMw(), 50.0));
    assert(near(result.shortageMw(), 30.0));
    assert(result.generatorResults().size() == 1);
    assert(near(result.generatorResults()[0].outputMw, 50.0));
    assert(result.generatorResults()[0].matchedSegments.size() == 2);
    assert(result.consumerResults().size() == 1);
    assert(near(result.consumerResults()[0].clearedDemandMw, 50.0));
}

void testGeneratorCapacityLimit() {
    const auto generator = makeGenerator("G1", 10.0, 50.0, {{40.0, 100.0}});
    const auto consumer = makeConsumer("C1", {{60.0, 150.0}});
    const auto input = makePiecewiseInput({generator}, {consumer});

    PiecewiseClearing clearing;
    const MarketResult result = clearing.clear(input);

    assert(result.feasible());
    assert(near(result.clearingPriceYuanPerMwh(), 100.0));
    assert(near(result.clearingVolumeMw(), 50.0));
    assert(near(result.shortageMw(), 10.0));
    assert(near(result.generatorResults()[0].outputMw, 50.0));
}

void testNoTradeUsesLowestSellPrice() {
    const auto generator = makeGenerator("G1", 5.0, 50.0, {{10.0, 200.0}});
    const auto consumer = makeConsumer("C1", {{10.0, 150.0}});
    const auto input = makePiecewiseInput({generator}, {consumer});

    PiecewiseClearing clearing;
    const MarketResult result = clearing.clear(input);

    assert(result.feasible());
    assert(near(result.clearingPriceYuanPerMwh(), 200.0));
    assert(near(result.clearingVolumeMw(), 5.0));
    assert(near(result.shortageMw(), 5.0));
    assert(result.generatorResults()[0].matchedSegments.empty());
}

void testDemandBelowSumPMinIsInfeasible() {
    const auto generator = makeGenerator("G1", 50.0, 100.0, {{10.0, 200.0}});
    const auto consumer = makeConsumer("C1", {{20.0, 300.0}});
    const auto input = makePiecewiseInput({generator}, {consumer});

    PiecewiseClearing clearing;
    const MarketResult result = clearing.clear(input);

    assert(!result.feasible());
}

void testMultiGeneratorSortingAndDemandLimit() {
    const auto generator1 = makeGenerator("G1", 10.0, 40.0, {{30.0, 300.0}});
    const auto generator2 = makeGenerator("G2", 20.0, 60.0, {{40.0, 200.0}});
    const auto consumer = makeConsumer("C1", {{80.0, 400.0}});
    const auto input = makePiecewiseInput({generator1, generator2}, {consumer});

    PiecewiseClearing clearing;
    const MarketResult result = clearing.clear(input);

    assert(result.feasible());
    assert(near(result.clearingPriceYuanPerMwh(), 300.0));
    assert(near(result.clearingVolumeMw(), 80.0));
    assert(near(result.shortageMw(), 0.0));
    assert(result.generatorResults().size() == 2);
    assert(near(result.generatorResults()[0].outputMw, 20.0));
    assert(near(result.generatorResults()[1].outputMw, 60.0));
}

void testMultiGeneratorMultiConsumerClearing() {
    const auto generator1 =
        makeGenerator("G1", 20.0, 100.0, {{20.0, 200.0}, {30.0, 250.0}, {30.0, 300.0}});
    const auto generator2 =
        makeGenerator("G2", 10.0, 60.0, {{20.0, 220.0}, {30.0, 260.0}});
    const auto consumer1 = makeConsumer("C1", {{40.0, 270.0}, {20.0, 250.0}});
    const auto consumer2 = makeConsumer("C2", {{30.0, 260.0}, {20.0, 230.0}});
    const auto input = makePiecewiseInput(
        {generator1, generator2}, {consumer1, consumer2});

    PiecewiseClearing clearing;
    const MarketResult result = clearing.clear(input);

    assert(result.feasible());
    assert(near(result.clearingPriceYuanPerMwh(), 250.0));
    assert(near(result.clearingVolumeMw(), 90.0));
    assert(near(result.shortageMw(), 20.0));
    assert(result.generatorResults().size() == 2);
    assert(near(result.generatorResults()[0].outputMw, 60.0));
    assert(near(result.generatorResults()[1].outputMw, 30.0));
    assert(result.consumerResults().size() == 2);

    double clearedDemand = 0.0;
    for (const ConsumerResult &consumerResult : result.consumerResults()) {
        clearedDemand += consumerResult.clearedDemandMw;
    }
    assert(near(clearedDemand, 90.0));
}

void testLadderQuadraticConsistency() {
    const auto generator =
        makeGenerator("G1", 20.0, 100.0, {{20.0, 200.0}, {30.0, 250.0}, {30.0, 300.0}});
    const auto consumer = makeConsumer("C1", {{70.0, 300.0}});
    const auto input = makePiecewiseInput({generator}, {consumer});

    PiecewiseClearing piecewise;
    const MarketResult piecewiseResult = piecewise.clear(input);
    assert(piecewiseResult.feasible());
    assert(near(piecewiseResult.clearingPriceYuanPerMwh(), 250.0));
    assert(near(piecewiseResult.clearingVolumeMw(), 70.0));
    assert(near(piecewiseResult.generatorResults()[0].outputMw, 70.0));

    Generator quadraticGenerator("G1", 20.0, 100.0);
    BidSheet quadraticSheet;
    quadraticSheet.setMode(BidSheet::Mode::Quadratic);
    quadraticSheet.setOwnerId("G1");
    quadraticSheet.setQuadraticCoefficients(5.0 / 6.0, 400.0 / 3.0, 0.0);
    quadraticGenerator.setBidSheet(quadraticSheet);

    MarketInput quadraticInput(0, MarketMode::Quadratic);
    quadraticInput.addGenerator(quadraticGenerator);
    quadraticInput.addConsumer(Consumer("C1", 70.0));

    QuadraticClearing quadratic;
    const MarketResult quadraticResult = quadratic.clear(quadraticInput);
    assert(quadraticResult.feasible());
    assert(near(quadraticResult.clearingPriceYuanPerMwh(), 250.0));
    assert(near(quadraticResult.clearingVolumeMw(), 70.0));
    assert(near(quadraticResult.generatorResults()[0].outputMw, 70.0));
}

} // namespace

int main() {
    testSimpleMatching();
    testGeneratorCapacityLimit();
    testNoTradeUsesLowestSellPrice();
    testDemandBelowSumPMinIsInfeasible();
    testMultiGeneratorSortingAndDemandLimit();
    testMultiGeneratorMultiConsumerClearing();
    testLadderQuadraticConsistency();

    std::cout << "ClearingTest passed" << std::endl;
    return 0;
}
