#pragma once

#include <string>
#include <vector>

#include "Clearing/ClearingEngine/ClearingEngine.h"

namespace pms {

class PiecewiseClearing : public ClearingEngine {
public:
    MarketResult clear(const MarketInput& input) override;
    std::string modeName() const override;

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

    static MarketResult buildFailure(int timeSlot, const std::string& message);
    static std::vector<SegmentRef> collectGeneratorSegments(const MarketInput& input);
    static std::vector<ConsumerRef> collectConsumerSegments(const MarketInput& input);
};

} // namespace pms
