#pragma once

#include "Model/MarketInput/MarketInput.h"
#include "Validation/BidValidator/BidValidator.h"

namespace pms {

class ConstraintChecker {
public:
    static ValidationReport checkFeasibility(const MarketInput& input);
};

} // namespace pms
