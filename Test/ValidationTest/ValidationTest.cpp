#include <cassert>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "Model/Consumer/Consumer.h"
#include "Model/Generator/Generator.h"
#include "Model/MarketInput/MarketInput.h"
#include "TradingCenter/TradingCenter.h"
#include "Validation/BidValidator/BidValidator.h"
#include "Validation/ConstraintChecker/ConstraintChecker.h"

using namespace pms;

namespace {

bool hasCode(const ValidationReport& report, const std::string& code) {
    for (const auto& item : report.issues()) {
        if (item.code == code) {
            return true;
        }
    }
    return false;
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

Consumer makePiecewiseConsumer(const std::string& id, double fixedDemand,
                               const std::vector<std::pair<double, double>>& segments) {
    Consumer consumer(id, fixedDemand);
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

void testEmptyMarketInputIsRejected() {
    const MarketInput input(0, MarketMode::Piecewise);
    const ValidationReport report = BidValidator::validate(input);

    assert(!report.ok());
    assert(hasCode(report, "NO_GENERATOR"));
    assert(hasCode(report, "NO_CONSUMER"));
}

void testSegmentStructureRules() {
    // 发电侧价格非单调（200 -> 150）。
    const auto nonMonotoneSell =
        makePiecewiseGenerator("G1", 0.0, 100.0, {{10.0, 200.0}, {10.0, 150.0}});
    const ValidationReport sellReport = BidValidator::validateGenerator(nonMonotoneSell);
    assert(!sellReport.ok());
    assert(hasCode(sellReport, "NON_MONOTONE_SELL"));

    // 用户侧价格非单调（200 -> 260）。
    const auto nonMonotoneBuy =
        makePiecewiseConsumer("C1", 0.0, {{10.0, 200.0}, {10.0, 260.0}});
    const ValidationReport buyReport = BidValidator::validateConsumer(nonMonotoneBuy);
    assert(!buyReport.ok());
    assert(hasCode(buyReport, "NON_MONOTONE_BUY"));

    // 非正段电量。
    const auto zeroQuantity =
        makePiecewiseGenerator("G1", 0.0, 100.0, {{0.0, 200.0}});
    const ValidationReport quantityReport = BidValidator::validateGenerator(zeroQuantity);
    assert(!quantityReport.ok());
    assert(hasCode(quantityReport, "NON_POSITIVE_QUANTITY"));

    // 段数超过 10 段。
    std::vector<std::pair<double, double>> manySegments;
    for (int i = 0; i < 11; ++i) {
        manySegments.push_back({1.0, 100.0 + i});
    }
    const auto tooManySegments = makePiecewiseGenerator("G1", 0.0, 100.0, manySegments);
    const ValidationReport segmentCountReport =
        BidValidator::validateGenerator(tooManySegments);
    assert(!segmentCountReport.ok());
    assert(hasCode(segmentCountReport, "TOO_MANY_SEGMENTS"));

    // 累计增量电量超过 Pmax-Pmin。
    const auto capacityExceeded =
        makePiecewiseGenerator("G1", 20.0, 50.0, {{20.0, 200.0}, {20.0, 250.0}});
    const ValidationReport capacityReport = BidValidator::validateGenerator(capacityExceeded);
    assert(!capacityReport.ok());
    assert(hasCode(capacityReport, "CAPACITY_EXCEEDED"));

    // 空报价单。
    const auto noSegments = makePiecewiseGenerator("G1", 0.0, 100.0, {});
    const ValidationReport emptyReport = BidValidator::validateGenerator(noSegments);
    assert(!emptyReport.ok());
    assert(hasCode(emptyReport, "NO_SEGMENT"));
}

void testParameterRules() {
    const auto invalidRange = makePiecewiseGenerator("G1", 60.0, 50.0, {{10.0, 200.0}});
    assert(hasCode(BidValidator::validateGenerator(invalidRange), "RANGE_INVALID"));

    const auto negativePMin = makePiecewiseGenerator("G1", -5.0, 50.0, {{10.0, 200.0}});
    assert(hasCode(BidValidator::validateGenerator(negativePMin), "PMIN_NEGATIVE"));

    const auto negativeDemand = makePiecewiseConsumer("C1", -10.0, {{10.0, 200.0}});
    assert(hasCode(BidValidator::validateConsumer(negativeDemand), "DEMAND_NEGATIVE"));

    const auto invalidQuadratic = makeQuadraticGenerator("G1", 10.0, 50.0, 0.0, 10.0, 0.0);
    assert(hasCode(BidValidator::validateGenerator(invalidQuadratic),
                   "QUADRATIC_A_NONPOSITIVE"));
}

void testValidationIssueIsLocatable() {
    const auto generator = makePiecewiseGenerator("G7", 0.0, 100.0, {{10.0, 300.0}, {10.0, 200.0}});
    const ValidationReport report = BidValidator::validateGenerator(generator);

    assert(!report.ok());
    const auto messages = report.messages();
    assert(!messages.empty());
    // 错误信息必须能定位到具体主体。
    assert(messages.front().find("G7") != std::string::npos);
}

void testFeasibilityChecks() {
    // 分段模式：申报需求低于 ΣPmin。
    MarketInput belowPMin(0, MarketMode::Piecewise);
    belowPMin.addGenerator(makePiecewiseGenerator("G1", 50.0, 100.0, {{10.0, 200.0}}));
    belowPMin.addConsumer(makePiecewiseConsumer("C1", 0.0, {{20.0, 300.0}}));
    assert(hasCode(ConstraintChecker::checkFeasibility(belowPMin), "DEMAND_BELOW_PMIN"));

    // 分段模式：申报需求高于 ΣPmin 时通过。
    MarketInput abovePMin(0, MarketMode::Piecewise);
    abovePMin.addGenerator(makePiecewiseGenerator("G1", 50.0, 100.0, {{10.0, 200.0}}));
    abovePMin.addConsumer(makePiecewiseConsumer("C1", 0.0, {{80.0, 300.0}}));
    assert(ConstraintChecker::checkFeasibility(abovePMin).ok());

    // 二次模式：QD 高于 ΣPmax。
    MarketInput tooHigh(0, MarketMode::Quadratic);
    tooHigh.addGenerator(makeQuadraticGenerator("G1", 10.0, 50.0, 0.05, 10.0, 0.0));
    tooHigh.addConsumer(Consumer("C1", 80.0));
    assert(hasCode(ConstraintChecker::checkFeasibility(tooHigh), "DEMAND_OUT_OF_RANGE"));

    // 二次模式：QD 低于 ΣPmin。
    MarketInput tooLow(0, MarketMode::Quadratic);
    tooLow.addGenerator(makeQuadraticGenerator("G1", 30.0, 50.0, 0.05, 10.0, 0.0));
    tooLow.addConsumer(Consumer("C1", 10.0));
    assert(hasCode(ConstraintChecker::checkFeasibility(tooLow), "DEMAND_OUT_OF_RANGE"));
}

void testTradingCenterSubmitMergesReports() {
    // 同时存在结构问题（非单调）与可行性问题（需求低于 ΣPmin）。
    MarketInput input(0, MarketMode::Piecewise);
    input.addGenerator(makePiecewiseGenerator("G1", 50.0, 100.0, {{10.0, 300.0}, {10.0, 200.0}}));
    input.addConsumer(makePiecewiseConsumer("C1", 0.0, {{20.0, 400.0}}));

    TradingCenter tradingCenter;
    const ValidationReport report = tradingCenter.submit(input);

    assert(!report.ok());
    assert(hasCode(report, "NON_MONOTONE_SELL"));
    assert(hasCode(report, "DEMAND_BELOW_PMIN"));
    assert(report.messages().size() >= 2);

    // 校验失败时出清必须直接返回不可行，且明细与输入等长。
    const MarketResult result = tradingCenter.clear(input);
    assert(!result.feasible());
    assert(result.generatorResults().size() == 1);
    assert(result.consumerResults().size() == 1);
}

void testQuadraticModeDoesNotRequireConsumerSegments() {
    // 二次曲线模式：用户侧只申报固定需求 QD，不报分段也应当通过校验。
    MarketInput quadratic(0, MarketMode::Quadratic);
    quadratic.addGenerator(makeQuadraticGenerator("G1", 10.0, 100.0, 0.05, 10.0, 0.0));
    quadratic.addConsumer(Consumer("C1", 70.0));
    assert(BidValidator::validate(quadratic).ok());
    assert(ConstraintChecker::checkFeasibility(quadratic).ok());

    TradingCenter tradingCenter;
    const MarketResult result = tradingCenter.clear(quadratic);
    assert(result.feasible());
    assert(result.consumerResults().size() == 1);
    assert(result.consumerResults()[0].declaredDemandMw == 70.0);
    assert(result.consumerResults()[0].clearedDemandMw == 70.0);

    // 分段模式仍然必须提交分段报价。
    MarketInput piecewise(0, MarketMode::Piecewise);
    piecewise.addGenerator(makePiecewiseGenerator("G1", 10.0, 100.0, {{40.0, 200.0}}));
    piecewise.addConsumer(Consumer("C1", 70.0));
    assert(hasCode(BidValidator::validate(piecewise), "NO_SEGMENT"));
}

} // namespace

int main() {
    testEmptyMarketInputIsRejected();
    testSegmentStructureRules();
    testParameterRules();
    testValidationIssueIsLocatable();
    testFeasibilityChecks();
    testTradingCenterSubmitMergesReports();
    testQuadraticModeDoesNotRequireConsumerSegments();

    std::cout << "ValidationTest passed" << std::endl;
    return 0;
}
