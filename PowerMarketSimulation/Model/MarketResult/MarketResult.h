#pragma once

#include <string>
#include <vector>

namespace pms {

struct MatchedSegment {
    int segmentNo = 0;
    double quantityMw = 0.0;
    double priceYuanPerMwh = 0.0;
};

struct GeneratorResult {
    std::string generatorId;
    double outputMw = 0.0;
    double marginalCostYuanPerMwh = 0.0;
    std::vector<MatchedSegment> matchedSegments;
};

struct ConsumerResult {
    std::string consumerId;
    double clearedDemandMw = 0.0;
};

class MarketResult {
public:
    MarketResult() = default;

    int timeSlot() const;
    void setTimeSlot(int timeSlot);

    double clearingPriceYuanPerMwh() const;
    void setClearingPriceYuanPerMwh(double price);

    double clearingVolumeMw() const;
    void setClearingVolumeMw(double volume);

    double shortageMw() const;
    void setShortageMw(double shortageMw);

    bool feasible() const;
    void setFeasible(bool feasible);

    const std::string& message() const;
    void setMessage(const std::string& message);

    const std::vector<GeneratorResult>& generatorResults() const;
    void addGeneratorResult(const GeneratorResult& result);
    void setGeneratorResults(const std::vector<GeneratorResult>& results);

    const std::vector<ConsumerResult>& consumerResults() const;
    void addConsumerResult(const ConsumerResult& result);
    void setConsumerResults(const std::vector<ConsumerResult>& results);

private:
    int timeSlot_ = -1;
    double clearingPriceYuanPerMwh_ = 0.0;
    double clearingVolumeMw_ = 0.0;
    double shortageMw_ = 0.0;
    bool feasible_ = false;
    std::string message_;
    std::vector<GeneratorResult> generatorResults_;
    std::vector<ConsumerResult> consumerResults_;
};

} // namespace pms
