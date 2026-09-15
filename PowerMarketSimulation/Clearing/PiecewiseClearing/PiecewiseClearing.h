#pragma once

#include <string>
#include <vector>

#include "Clearing/ClearingEngine/ClearingEngine.h"

namespace pms {

class PiecewiseClearing : public ClearingEngine {
public:
    // 执行分段报价双指针撮合，并返回统一出清价与成交结果。
    MarketResult clear(const MarketInput& input) override;

private:
    struct SegmentRef {
        int generatorIndex = -1;
        int segmentNo = 0;
        double quantityMw = 0.0;
        double priceYuanPerMwh = 0.0;
        bool isPMin = false;
    };

    struct ConsumerRef {
        int consumerIndex = -1;
        int segmentNo = 0;
        double quantityMw = 0.0;
        double priceYuanPerMwh = 0.0;
    };

    // 构造包含失败原因的不可行出清结果。
    static MarketResult buildFailure(int timeSlot, const std::string& message);

    // 汇总全部发电机组的可撮合增量报价段。
    static std::vector<SegmentRef> collectGeneratorSegments(const MarketInput& input);

    // 汇总全部用户的需求报价段。
    static std::vector<ConsumerRef> collectConsumerSegments(const MarketInput& input);
};

} // namespace pms
