#include "Clearing/PiecewiseClearing/PiecewiseClearing.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace pms {

namespace {

constexpr double kEpsilon = 1e-9;

std::string formatDouble(double value) {
    std::ostringstream oss;
    oss.precision(6);
    oss << value;
    return oss.str();
}

} // namespace

MarketResult PiecewiseClearing::clear(const MarketInput& input) {
    if (input.mode() != MarketMode::Piecewise) {
        return buildFailure(input.timeSlot(), "PiecewiseClearing 只能处理分段报价模式");
    }

    std::vector<GeneratorResult> generatorResults;
    generatorResults.reserve(input.generators().size());
    std::vector<double> unitCapacity(input.generators().size(), 0.0);
    std::vector<double> unitCumulative(input.generators().size(), 0.0);
    double sumPMin = 0.0;

    for (size_t i = 0; i < input.generators().size(); ++i) {
        const Generator& generator = input.generators()[i];
        GeneratorResult result;
        result.generatorId = generator.id();
        result.outputMw = generator.pMinMw();
        generatorResults.push_back(result);
        unitCapacity[i] = generator.pMaxMw() - generator.pMinMw();
        sumPMin += generator.pMinMw();
    }

    std::vector<ConsumerResult> consumerResults;
    consumerResults.reserve(input.consumers().size());
    for (const auto& consumer : input.consumers()) {
        ConsumerResult result;
        result.consumerId = consumer.id();
        consumerResults.push_back(result);
    }

    const double totalDemand = input.totalDeclaredDemandMw();
    if (totalDemand + kEpsilon < sumPMin) {
        return buildFailure(input.timeSlot(),
                            "用户总申报需求低于 ΣPmin，无法保证所有在线机组最低出力");
    }

    std::vector<SegmentRef> generatorSegments = collectGeneratorSegments(input);
    std::vector<ConsumerRef> consumerSegments = collectConsumerSegments(input);

    std::sort(generatorSegments.begin(), generatorSegments.end(),
              [](const SegmentRef& lhs, const SegmentRef& rhs) {
                  if (std::fabs(lhs.priceYuanPerMwh - rhs.priceYuanPerMwh) > kEpsilon) {
                      return lhs.priceYuanPerMwh < rhs.priceYuanPerMwh;
                  }
                  if (lhs.generatorIndex != rhs.generatorIndex) {
                      return lhs.generatorIndex < rhs.generatorIndex;
                  }
                  return lhs.segmentNo < rhs.segmentNo;
              });

    std::sort(consumerSegments.begin(), consumerSegments.end(),
              [](const ConsumerRef& lhs, const ConsumerRef& rhs) {
                  if (std::fabs(lhs.priceYuanPerMwh - rhs.priceYuanPerMwh) > kEpsilon) {
                      return lhs.priceYuanPerMwh > rhs.priceYuanPerMwh;
                  }
                  if (lhs.consumerIndex != rhs.consumerIndex) {
                      return lhs.consumerIndex < rhs.consumerIndex;
                  }
                  return lhs.segmentNo < rhs.segmentNo;
              });

    size_t sellIndex = 0;
    size_t buyIndex = 0;
    double matchedIncrementMw = 0.0;
    double lastSellPrice = 0.0;
    bool hasTrade = false;

    while (buyIndex < consumerSegments.size() && sellIndex < generatorSegments.size()) {
        SegmentRef& sell = generatorSegments[sellIndex];
        ConsumerRef& buy = consumerSegments[buyIndex];

        if (buy.priceYuanPerMwh + kEpsilon < sell.priceYuanPerMwh) {
            break;
        }

        if (unitCumulative[sell.generatorIndex] + kEpsilon >= unitCapacity[sell.generatorIndex]) {
            ++sellIndex;
            continue;
        }

        const double remainingCapacity =
            unitCapacity[sell.generatorIndex] - unitCumulative[sell.generatorIndex];
        const double remainingDemand = totalDemand - (sumPMin + matchedIncrementMw);
        double tradeMw = std::min({buy.quantityMw, sell.quantityMw, remainingCapacity});
        tradeMw = std::min(tradeMw, std::max(0.0, remainingDemand));

        if (tradeMw <= kEpsilon) {
            break;
        }

        buy.quantityMw -= tradeMw;
        sell.quantityMw -= tradeMw;
        unitCumulative[sell.generatorIndex] += tradeMw;
        matchedIncrementMw += tradeMw;

        GeneratorResult& generatorResult = generatorResults[sell.generatorIndex];
        generatorResult.outputMw += tradeMw;
        MatchedSegment matchedSegment;
        matchedSegment.segmentNo = sell.segmentNo;
        matchedSegment.quantityMw = tradeMw;
        matchedSegment.priceYuanPerMwh = sell.priceYuanPerMwh;
        generatorResult.matchedSegments.push_back(matchedSegment);

        consumerResults[buy.consumerIndex].clearedDemandMw += tradeMw;
        lastSellPrice = sell.priceYuanPerMwh;
        hasTrade = true;

        if (buy.quantityMw <= kEpsilon) {
            ++buyIndex;
        }
        if (sell.quantityMw <= kEpsilon) {
            ++sellIndex;
        }
    }

    const double totalDeclared = input.totalDeclaredDemandMw();
    if (totalDeclared > kEpsilon) {
        for (size_t i = 0; i < input.consumers().size(); ++i) {
            const double declared = input.consumers()[i].bidSheet().totalIncrementMw();
            const double share = declared / totalDeclared * sumPMin;
            consumerResults[i].clearedDemandMw += share;
        }
    }

    double totalClearedMw = sumPMin + matchedIncrementMw;
    double clearingPrice = hasTrade ? lastSellPrice : 0.0;
    if (!hasTrade && !generatorSegments.empty()) {
        clearingPrice = generatorSegments.front().priceYuanPerMwh;
    }
    const double shortage = std::max(0.0, totalDemand - totalClearedMw);

    MarketResult result;
    result.setTimeSlot(input.timeSlot());
    result.setFeasible(true);
    result.setClearingPriceYuanPerMwh(clearingPrice);
    result.setClearingVolumeMw(totalClearedMw);
    result.setShortageMw(shortage);
    result.setGeneratorResults(generatorResults);
    result.setConsumerResults(consumerResults);

    std::string message = hasTrade ? "双指针撮合完成" : "无增量成交，按最低发电段价格结算";
    if (shortage > kEpsilon) {
        message += "；供电不足，缺额 " + formatDouble(shortage) + " MW";
    }
    result.setMessage(message);
    return result;
}

std::string PiecewiseClearing::modeName() const {
    return "Piecewise";
}

MarketResult PiecewiseClearing::buildFailure(int timeSlot, const std::string& message) {
    MarketResult result;
    result.setTimeSlot(timeSlot);
    result.setFeasible(false);
    result.setMessage(message);
    return result;
}

std::vector<PiecewiseClearing::SegmentRef> PiecewiseClearing::collectGeneratorSegments(
    const MarketInput& input) {
    std::vector<SegmentRef> segments;
    for (size_t generatorIndex = 0; generatorIndex < input.generators().size(); ++generatorIndex) {
        const BidSheet& sheet = input.generators()[generatorIndex].bidSheet();
        for (const auto& segment : sheet.segments()) {
            SegmentRef ref;
            ref.generatorIndex = static_cast<int>(generatorIndex);
            ref.segmentNo = segment.segmentNo();
            ref.quantityMw = segment.quantityMw();
            ref.priceYuanPerMwh = segment.priceYuanPerMwh();
            segments.push_back(ref);
        }
    }
    return segments;
}

std::vector<PiecewiseClearing::ConsumerRef> PiecewiseClearing::collectConsumerSegments(
    const MarketInput& input) {
    std::vector<ConsumerRef> segments;
    for (size_t consumerIndex = 0; consumerIndex < input.consumers().size(); ++consumerIndex) {
        const BidSheet& sheet = input.consumers()[consumerIndex].bidSheet();
        for (const auto& segment : sheet.segments()) {
            ConsumerRef ref;
            ref.consumerIndex = static_cast<int>(consumerIndex);
            ref.segmentNo = segment.segmentNo();
            ref.quantityMw = segment.quantityMw();
            ref.priceYuanPerMwh = segment.priceYuanPerMwh();
            segments.push_back(ref);
        }
    }
    return segments;
}

} // namespace pms
