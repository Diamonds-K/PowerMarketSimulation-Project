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

    // 每台机组初始出力至少为 Pmin，随后再叠加增量成交。
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
    // 用户总申报需求不能低于全部机组的最低出力之和。
    if (totalDemand + kEpsilon < sumPMin) {
        return buildFailure(input.timeSlot(),
                            "用户总申报需求低于 ΣPmin，无法保证所有在线机组最低出力");
    }

    std::vector<SegmentRef> generatorSegments = collectGeneratorSegments(input);
    std::vector<ConsumerRef> consumerSegments = collectConsumerSegments(input);

    // 将 Pmin 作为价格为 0 的必发段插入卖方队列。
    for (size_t i = 0; i < input.generators().size(); ++i) {
        const double pMin = input.generators()[i].pMinMw();
        if (pMin > kEpsilon) {
            SegmentRef pminSegment;
            pminSegment.generatorIndex = static_cast<int>(i);
            pminSegment.segmentNo = 0;
            pminSegment.quantityMw = pMin;
            pminSegment.priceYuanPerMwh = 0.0;
            pminSegment.isPMin = true;
            generatorSegments.push_back(pminSegment);
        }
    }

    // 卖方价格从低到高排列。
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

    // 买方价格从高到低排列。
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

    // 双指针撮合：双方报价段都只向前移动，不回退。
    size_t sellIndex = 0;
    size_t buyIndex = 0;
    double matchedDemandMw = 0.0;
    double lastSellPrice = 0.0;
    bool hasTrade = false;

    while (buyIndex < consumerSegments.size() && sellIndex < generatorSegments.size()) {
        SegmentRef& sell = generatorSegments[sellIndex];
        ConsumerRef& buy = consumerSegments[buyIndex];

        // 买方价格低于卖方价格时，无法继续成交。
        if (buy.priceYuanPerMwh + kEpsilon < sell.priceYuanPerMwh) {
            break;
        }

        if (!sell.isPMin &&
            unitCumulative[sell.generatorIndex] + kEpsilon >= unitCapacity[sell.generatorIndex]) {
            ++sellIndex;
            continue;
        }

        const double remainingCapacity = sell.isPMin
                                             ? sell.quantityMw
                                             : unitCapacity[sell.generatorIndex] -
                                                   unitCumulative[sell.generatorIndex];
        const double remainingDemand = totalDemand - matchedDemandMw;
        double tradeMw = std::min({buy.quantityMw, sell.quantityMw, remainingCapacity});
        tradeMw = std::min(tradeMw, std::max(0.0, remainingDemand));

        if (tradeMw <= kEpsilon) {
            break;
        }

        // 记录成交段、价格和双方剩余量。
        buy.quantityMw -= tradeMw;
        sell.quantityMw -= tradeMw;
        matchedDemandMw += tradeMw;

        if (!sell.isPMin) {
            unitCumulative[sell.generatorIndex] += tradeMw;

            GeneratorResult& generatorResult = generatorResults[sell.generatorIndex];
            generatorResult.outputMw += tradeMw;
            MatchedSegment matchedSegment;
            matchedSegment.segmentNo = sell.segmentNo;
            matchedSegment.quantityMw = tradeMw;
            matchedSegment.priceYuanPerMwh = sell.priceYuanPerMwh;
            generatorResult.matchedSegments.push_back(matchedSegment);

            lastSellPrice = sell.priceYuanPerMwh;
            hasTrade = true;
        }

        consumerResults[buy.consumerIndex].clearedDemandMw += tradeMw;

        if (buy.quantityMw <= kEpsilon) {
            ++buyIndex;
        }
        if (sell.quantityMw <= kEpsilon) {
            ++sellIndex;
        }
    }

    double totalClearedMw = matchedDemandMw;
    // 统一出清价取最后成交的发电段价格。
    double clearingPrice = hasTrade ? lastSellPrice : 0.0;
    if (!hasTrade) {
        for (const SegmentRef& segment : generatorSegments) {
            if (!segment.isPMin) {
                clearingPrice = segment.priceYuanPerMwh;
                break;
            }
        }
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
