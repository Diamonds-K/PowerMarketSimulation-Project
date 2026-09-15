#pragma once

#include <vector>

#include "Model/Consumer/Consumer.h"
#include "Model/Generator/Generator.h"

namespace pms {

// 市场出清模式：分段阶梯报价或二次成本曲线。
enum class MarketMode {
    Piecewise,
    Quadratic
};

class MarketInput {
public:
    // 创建一个时段的市场输入。
    MarketInput(int timeSlot, MarketMode mode);

    // 返回时段索引，内部编号范围为 0..95。
    int timeSlot() const;

    // 返回当前市场出清模式。
    MarketMode mode() const;

    // 返回当前时段全部发电机组。
    const std::vector<Generator>& generators() const;

    // 向市场输入添加一台发电机组。
    void addGenerator(const Generator& generator);

    // 返回当前时段全部用户。
    const std::vector<Consumer>& consumers() const;

    // 向市场输入添加一个用户。
    void addConsumer(const Consumer& consumer);

    // 返回用户申报段电量总和。
    double totalDeclaredDemandMw() const;

    // 返回应用固定需求回退规则后的有效总需求。
    double totalEffectiveDemandMw() const;

private:
    int timeSlot_ = -1;
    MarketMode mode_ = MarketMode::Piecewise;
    std::vector<Generator> generators_;
    std::vector<Consumer> consumers_;
};

} // namespace pms
