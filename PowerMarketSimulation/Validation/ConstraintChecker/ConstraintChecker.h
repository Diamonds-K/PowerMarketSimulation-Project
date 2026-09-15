#pragma once

#include "Model/MarketInput/MarketInput.h"
#include "Validation/BidValidator/BidValidator.h"

namespace pms {

class ConstraintChecker {
public:
    // 检查总需求与全部机组最小、最大出力之间的可行性。
    static ValidationReport checkFeasibility(const MarketInput& input);
};

} // namespace pms
