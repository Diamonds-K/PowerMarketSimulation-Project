#pragma once

#include "Model/MarketInput/MarketInput.h"
#include "Model/MarketResult/MarketResult.h"

namespace pms {

class ClearingEngine {
public:
    // 通过基类指针销毁具体出清算法。
    virtual ~ClearingEngine() = default;

    // 使用统一的市场输入接口执行一个时段的出清计算。
    virtual MarketResult clear(const MarketInput& input) = 0;
};

} // namespace pms
