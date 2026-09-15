#pragma once

#include <string>
#include <vector>

namespace pms {

// 分段模式下一次具体成交段及其成交电量、价格。
struct MatchedSegment {
    int segmentNo = 0;
    double quantityMw = 0.0;
    double priceYuanPerMwh = 0.0;
};

// 单个发电机组的出清结果。
struct GeneratorResult {
    std::string generatorId;
    double outputMw = 0.0;
    double marginalCostYuanPerMwh = 0.0;
    std::vector<MatchedSegment> matchedSegments;
};

// 单个用户的出清结果。
struct ConsumerResult {
    std::string consumerId;
    double clearedDemandMw = 0.0;
};

class MarketResult {
public:
    // 创建空的出清结果。
    MarketResult() = default;

    // 返回时段索引。
    int timeSlot() const;

    // 设置时段索引。
    void setTimeSlot(int timeSlot);

    // 返回统一出清价，单位为元/MWh。
    double clearingPriceYuanPerMwh() const;

    // 设置统一出清价。
    void setClearingPriceYuanPerMwh(double price);

    // 返回总成交电量，单位为 MW。
    double clearingVolumeMw() const;

    // 设置总成交电量。
    void setClearingVolumeMw(double volume);

    // 返回未满足的缺额，单位为 MW。
    double shortageMw() const;

    // 设置缺额。
    void setShortageMw(double shortageMw);

    // 判断该时段是否存在可行出清结果。
    bool feasible() const;

    // 设置出清是否可行。
    void setFeasible(bool feasible);

    // 返回出清状态说明或失败原因。
    const std::string& message() const;

    // 设置出清状态说明。
    void setMessage(const std::string& message);

    // 返回全部发电机组的出清结果。
    const std::vector<GeneratorResult>& generatorResults() const;

    // 追加一个发电机组的出清结果。
    void addGeneratorResult(const GeneratorResult& result);

    // 替换全部发电机组的出清结果。
    void setGeneratorResults(const std::vector<GeneratorResult>& results);

    // 返回全部用户的出清结果。
    const std::vector<ConsumerResult>& consumerResults() const;

    // 追加一个用户的出清结果。
    void addConsumerResult(const ConsumerResult& result);

    // 替换全部用户的出清结果。
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
