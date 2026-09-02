#pragma once

#include <string>

#include "Model/MarketInput/MarketInput.h"
#include "Model/MarketResult/MarketResult.h"

namespace pms {

class ClearingEngine {
public:
    virtual ~ClearingEngine() = default;

    virtual MarketResult clear(const MarketInput& input) = 0;
    virtual std::string modeName() const = 0;
};

} // namespace pms
