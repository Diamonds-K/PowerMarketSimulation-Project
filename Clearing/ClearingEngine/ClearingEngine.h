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

// 统一构造不可行结果：除了时段与原因，还必须按输入主体数量填充等长的零值明细，
// 保证调用方可以安全地按主体下标取用结果（见 DESIGN.md 第 9 节）。
MarketResult makeInfeasibleResult(const MarketInput& input, const std::string& message);

} // namespace pms
