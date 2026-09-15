#pragma once

#include "Model/MarketInput/MarketInput.h"
#include "Model/MarketResult/MarketResult.h"
#include "Model/SettlementResult/SettlementResult.h"

namespace pms {

class SettlementEngine {
public:
    // 单个时段的时长，单位为小时（15 分钟 = 0.25 小时）。
    static constexpr double kSlotDurationHours = 0.25;

    // 根据市场输入和出清结果计算主体收益、成本、利润与用户支付。
    SettlementResult settle(const MarketInput& input, const MarketResult& result) const;
};

} // namespace pms
