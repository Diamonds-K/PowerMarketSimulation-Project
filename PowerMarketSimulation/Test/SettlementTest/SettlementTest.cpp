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
#include "Settlement/SettlementEngine/SettlementEngine.h"

using namespace pms;

namespace {

bool near(double lhs, double rhs, double tolerance = 1e-6) {
    return std::fabs(lhs - rhs) <= tolerance;
}

Generator makePiecewiseGenerator(const std::string& id,
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

Generator makeQuadraticGenerator(const std::string& id,
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

void testPiecewiseSettlement() {
    const auto generator =
        makePiecewiseGenerator("G1", 20.0, 100.0, {{20.0, 200.0}, {30.0, 250.0}, {30.0, 300.0}});
    Consumer consumer("C1", 0.0);
    BidSheet sheet;
    sheet.setMode(BidSheet::Mode::Piecewise);
    sheet.setOwnerId("C1");
    sheet.addSegment(BidSegment(1, 50.0, 260.0));
    sheet.addSegment(BidSegment(2, 30.0, 240.0));
    consumer.setBidSheet(sheet);

    MarketInput input(0, MarketMode::Piecewise);
    input.addGenerator(generator);
    input.addConsumer(consumer);

    PiecewiseClearing clearing;
    const MarketResult result = clearing.clear(input);
    assert(result.feasible());

    SettlementEngine settlementEngine;
    const SettlementResult settlement = settlementEngine.settle(input, result);

    assert(near(settlement.clearingPriceYuanPerMwh(), 250.0));
    assert(near(settlement.totalRevenueYuan(), 0.25 * 250.0 * 50.0));
    assert(near(settlement.totalCostYuan(),
                0.25 * (250.0 * 20.0 + 200.0 * 20.0 + 250.0 * 10.0)));
    assert(near(settlement.totalProfitYuan(), 0.25 * 1000.0));
    assert(near(settlement.totalPaymentYuan(), 0.25 * 250.0 * 50.0));
    assert(near(settlement.balanceYuan(), 0.0));
}

void testQuadraticSettlement() {
    const auto generator1 = makeQuadraticGenerator("G1", 20.0, 80.0, 0.05, 10.0, 0.0);
    const auto generator2 = makeQuadraticGenerator("G2", 10.0, 60.0, 0.02, 20.0, 0.0);
    const auto consumer = Consumer("C1", 100.0);

    MarketInput input(0, MarketMode::Quadratic);
    input.addGenerator(generator1);
    input.addGenerator(generator2);
    input.addConsumer(consumer);

    QuadraticClearing clearing;
    const MarketResult result = clearing.clear(input);
    assert(result.feasible());

    SettlementEngine settlementEngine;
    const SettlementResult settlement = settlementEngine.settle(input, result);

    assert(near(settlement.totalPaymentYuan(), settlement.totalRevenueYuan(), 1e-4));
    assert(near(settlement.balanceYuan(), 0.0, 1e-4));
    assert(settlement.generatorSettlements().size() == 2);
    assert(settlement.consumerSettlements().size() == 1);
    assert(near(settlement.consumerSettlements()[0].paymentYuan,
                settlement.totalPaymentYuan(), 1e-4));
}

} // namespace

int main() {
    testPiecewiseSettlement();
    testQuadraticSettlement();

    std::cout << "SettlementTest passed" << std::endl;
    return 0;
}
