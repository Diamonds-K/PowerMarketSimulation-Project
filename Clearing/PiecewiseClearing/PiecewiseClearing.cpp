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
        return makeInfeasibleResult(input, "PiecewiseClearing 只能处理分段报价模式");
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
    double totalDemand = 0.0;
    for (const auto& consumer : input.consumers()) {
        ConsumerResult result;
        result.consumerId = consumer.id();
        result.declaredDemandMw = consumer.bidSheet().totalIncrementMw();
        totalDemand += result.declaredDemandMw;
        consumerResults.push_back(result);
    }

    // 用户总申报需求不能低于全部机组的最低出力之和。
    if (totalDemand + kEpsilon < sumPMin) {
        return makeInfeasibleResult(
            input, "用户总申报需求低于 ΣPmin，无法保证所有在线机组最低出力");
    }

    // 步骤 1：ΣPmin 按各用户申报量占比分摊到用户（见 DESIGN.md 7.2）。
    // Pmin 不参与报价竞争，因此不进入卖方队列，也不会扣减用户报价曲线。
    if (totalDemand > kEpsilon) {
        for (ConsumerResult& consumerResult : consumerResults) {
            consumerResult.clearedDemandMw =
                sumPMin * consumerResult.declaredDemandMw / totalDemand;
        }
    }

    std::vector<SegmentRef> generatorSegments = collectGeneratorSegments(input);
    std::vector<ConsumerRef> consumerSegments = collectConsumerSegments(input);

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

    // 步骤 2：双指针撮合增量段。双方报价段都只向前移动，不回退。
    size_t sellIndex = 0;
    size_t buyIndex = 0;
    double matchedIncrementMw = 0.0;
    double lastSellPrice = 0.0;
    bool hasTrade = false;
    // 增量市场最多能吃下「用户申报总量 - ΣPmin」，其余电量已由必发电量覆盖。
    const double incrementCapacityMw = std::max(0.0, totalDemand - sumPMin);

    while (buyIndex < consumerSegments.size() && sellIndex < generatorSegments.size()) {
        SegmentRef& sell = generatorSegments[sellIndex];
        ConsumerRef& buy = consumerSegments[buyIndex];

        // 买方价格低于卖方价格时，无法继续成交。
        if (buy.priceYuanPerMwh + kEpsilon < sell.priceYuanPerMwh) {
            break;
        }

        if (unitCumulative[sell.generatorIndex] + kEpsilon >= unitCapacity[sell.generatorIndex]) {
            ++sellIndex;
            continue;
        }

        const double remainingCapacity = unitCapacity[sell.generatorIndex] -
                                         unitCumulative[sell.generatorIndex];
        const double remainingDemand = incrementCapacityMw - matchedIncrementMw;
        double tradeMw = std::min({buy.quantityMw, sell.quantityMw, remainingCapacity});
        tradeMw = std::min(tradeMw, std::max(0.0, remainingDemand));

        if (tradeMw <= kEpsilon) {
            break;
        }

        // 记录成交段、价格和双方剩余量。
        buy.quantityMw -= tradeMw;
        sell.quantityMw -= tradeMw;
        matchedIncrementMw += tradeMw;

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

        consumerResults[buy.consumerIndex].clearedDemandMw += tradeMw;

        if (buy.quantityMw <= kEpsilon) {
            ++buyIndex;
        }
        if (sell.quantityMw <= kEpsilon) {
            ++sellIndex;
        }
    }

    // 总成交 = ΣPmin + 增量成交。
    const double totalClearedMw = sumPMin + matchedIncrementMw;
    // 统一出清价取最后成交的发电段价格。
    double clearingPrice = hasTrade ? lastSellPrice : 0.0;
    if (!hasTrade) {
        // 无增量成交时，取全场最低发电增量段报价作为结算参考价。
        for (const SegmentRef& segment : generatorSegments) {
            clearingPrice = segment.priceYuanPerMwh;
            break;
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

    std::string message = hasTrade
                              ? "双指针撮合完成（Pmin 按申报量比例分摊）"
                              : "无增量成交，必发电量按申报量比例分摊，按最低发电段价格结算";
    if (shortage > kEpsilon) {
        message += "；供电不足，缺额 " + formatDouble(shortage) + " MW";
    }
    result.setMessage(message);
    return result;
}

std::string PiecewiseClearing::modeName() const {
    return "Piecewise";
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
