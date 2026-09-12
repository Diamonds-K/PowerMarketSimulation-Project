#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#include "Clearing/QuadraticClearing/QuadraticClearing.h"
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

} // namespace

int main() {
    testFreeUnitWithLowerBound();
    testUpperBoundUnit();
    testAllAtLowerBound();
    testAllAtUpperBound();
    testInfeasibleDemand();

    std::cout << "OptimizationTest passed" << std::endl;
    return 0;
}
