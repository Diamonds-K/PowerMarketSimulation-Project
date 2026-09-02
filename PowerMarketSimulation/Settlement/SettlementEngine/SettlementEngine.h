#pragma once

#include "Model/MarketInput/MarketInput.h"
#include "Model/MarketResult/MarketResult.h"
#include "Model/SettlementResult/SettlementResult.h"

namespace pms {

class SettlementEngine {
public:
    static constexpr double kSlotDurationHours = 0.25;

    SettlementResult settle(const MarketInput& input, const MarketResult& result) const;
};

} // namespace pms
