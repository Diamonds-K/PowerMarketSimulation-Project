#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#include "Clearing/QuadraticClearing/QuadraticClearing.h"
#include "Model/BidSheet/BidSheet.h"
#include "Model/Consumer/Consumer.h"
#include "Model/Generator/Generator.h"
#include "Model/MarketInput/MarketInput.h"

using namespace pms;

namespace {

bool near(double lhs, double rhs, double tolerance = 1e-3) {
    return std::fabs(lhs - rhs) <= tolerance;
}

Generator makeGenerator(const std::string& id,
                        double pMin,
                        double pMax,
                        double a,
                        double b,
                        double c) {
    Generator generator(id, pMin, pMax);
    BidSheet sheet;
    sheet.setMode(BidSheet::Mode::Quadratic);
    sheet.setOwnerId(id);
    sheet.setQuadraticCoefficients(a, b, c);
    generator.setBidSheet(sheet);
    return generator;
}

MarketInput makeQuadraticInput(double demand) {
    const auto generator1 = makeGenerator("G1", 20.0, 80.0, 0.05, 10.0, 0.0);
    const auto generator2 = makeGenerator("G2", 10.0, 60.0, 0.02, 20.0, 0.0);

    MarketInput input(0, MarketMode::Quadratic);
    input.addGenerator(generator1);
    input.addGenerator(generator2);
    input.addConsumer(Consumer("C1", demand));
    return input;
}

void testFreeUnitWithLowerBound() {
    const MarketInput input = makeQuadraticInput(80.0);
    QuadraticClearing clearing;
    const MarketResult result = clearing.clear(input);

    assert(result.feasible());
    assert(near(result.clearingPriceYuanPerMwh(), 17.0));
    assert(near(result.generatorResults()[0].outputMw, 70.0));
    assert(near(result.generatorResults()[1].outputMw, 10.0));
    assert(near(result.clearingVolumeMw(), 80.0));
    assert(near(result.generatorResults()[0].marginalCostYuanPerMwh,
                result.clearingPriceYuanPerMwh()));
}

void testUpperBoundUnit() {
    const MarketInput input = makeQuadraticInput(100.0);
    QuadraticClearing clearing;
    const MarketResult result = clearing.clear(input);

    assert(result.feasible());
    assert(near(result.clearingPriceYuanPerMwh(), 20.8));
    assert(near(result.generatorResults()[0].outputMw, 80.0));
    assert(near(result.generatorResults()[1].outputMw, 20.0));
    assert(near(result.clearingVolumeMw(), 100.0));
    assert(result.generatorResults()[0].marginalCostYuanPerMwh <=
           result.clearingPriceYuanPerMwh() + 1e-6);
}

void testAllAtLowerBound() {
    const MarketInput input = makeQuadraticInput(30.0);
    QuadraticClearing clearing;
    const MarketResult result = clearing.clear(input);

    assert(result.feasible());
    assert(near(result.generatorResults()[0].outputMw, 20.0));
    assert(near(result.generatorResults()[1].outputMw, 10.0));
    assert(near(result.clearingVolumeMw(), 30.0));
    assert(result.clearingPriceYuanPerMwh() >= 10.0);
    assert(result.clearingPriceYuanPerMwh() <= 12.5);
}

void testAllAtUpperBound() {
    const MarketInput input = makeQuadraticInput(140.0);
    QuadraticClearing clearing;
    const MarketResult result = clearing.clear(input);

    assert(result.feasible());
    assert(near(result.generatorResults()[0].outputMw, 80.0));
    assert(near(result.generatorResults()[1].outputMw, 60.0));
    assert(near(result.clearingVolumeMw(), 140.0));
    assert(result.clearingPriceYuanPerMwh() >= 22.3);
    assert(result.clearingPriceYuanPerMwh() <= 23.5);
}

void testInfeasibleDemand() {
    const MarketInput input = makeQuadraticInput(10.0);
    QuadraticClearing clearing;
    const MarketResult result = clearing.clear(input);

    assert(!result.feasible());
}

void testZeroFixedDemandFallsBackToDeclaredDemand() {
    const auto generator = makeGenerator("G1", 0.0, 50.0, 1.0, 10.0, 0.0);

    Consumer consumer("C1", 0.0);
    BidSheet demandSheet;
    demandSheet.setMode(BidSheet::Mode::Piecewise);
    demandSheet.setOwnerId("C1");
    demandSheet.addSegment(BidSegment(1, 25.0, 100.0));
    consumer.setBidSheet(demandSheet);

    MarketInput input(0, MarketMode::Quadratic);
    input.addGenerator(generator);
    input.addConsumer(consumer);

    QuadraticClearing clearing;
    const MarketResult result = clearing.clear(input);

    assert(result.feasible());
    assert(near(result.clearingVolumeMw(), 25.0));
    assert(near(result.clearingPriceYuanPerMwh(), 60.0));
    assert(near(result.consumerResults()[0].clearedDemandMw, 25.0));
}

void testFittedQuadraticMatchesDeclaredDemand() {
    std::vector<BidSegment> segments;
    const double prices[] = {
        364.8, 440.1, 515.4, 590.6, 665.9,
        741.2, 816.5, 891.8, 967.0, 1042.32
    };
    for (int index = 0; index < 9; ++index) {
        segments.emplace_back(index + 1, 2.5, prices[index]);
    }
    segments.emplace_back(10, 4.5, prices[9]);

    const BidSheet::QuadraticFit fit =
        BidSheet::fitLadderToQuadratic(0.0, 100.0, segments);
    const auto generator =
        makeGenerator("G1", 0.0, 100.0, fit.a, fit.b, fit.c);

    Consumer consumer("C1", 0.0);
    BidSheet demandSheet;
    demandSheet.setMode(BidSheet::Mode::Piecewise);
    demandSheet.setOwnerId("C1");
    demandSheet.addSegment(BidSegment(1, 25.0, 1576.84));
    consumer.setBidSheet(demandSheet);

    MarketInput input(0, MarketMode::Quadratic);
    input.addGenerator(generator);
    input.addConsumer(consumer);

    QuadraticClearing clearing;
    const MarketResult result = clearing.clear(input);

    assert(result.feasible());
    assert(near(result.clearingVolumeMw(), 25.0));
    assert(near(result.clearingPriceYuanPerMwh(), 1042.32));
}

} // namespace

int main() {
    testFreeUnitWithLowerBound();
    testUpperBoundUnit();
    testAllAtLowerBound();
    testAllAtUpperBound();
    testInfeasibleDemand();
    testZeroFixedDemandFallsBackToDeclaredDemand();
    testFittedQuadraticMatchesDeclaredDemand();

    std::cout << "OptimizationTest passed" << std::endl;
    return 0;
}
