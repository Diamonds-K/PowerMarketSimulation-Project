#pragma once

#include <string>
#include <vector>

#include "Clearing/ClearingEngine/ClearingEngine.h"

namespace pms {

class QuadraticClearing : public ClearingEngine {
public:
    // 使用 KKT/λ 二分方法求解二次成本曲线下的经济调度。
    MarketResult clear(const MarketInput& input) override;

    // λ 二分求解的收敛容差。
    static constexpr double kTolerance = 1e-6;

    // 为避免数值问题设置的最大二分迭代次数。
    static constexpr int kMaxIterations = 200;

private:
    struct Unit {
        const Generator* generator = nullptr;
        double a = 0.0;
        double b = 0.0;
        double pMin = 0.0;
        double pMax = 0.0;
    };

    // 根据边际价格 λ 计算单台机组在上下限约束内的出力。
    static double outputForLambda(const Unit& unit, double lambda);

    // 汇总全部机组在给定 λ 下的总出力。
    static double totalOutput(const std::vector<Unit>& units, double lambda);

    // 构造包含失败原因的不可行出清结果。
    static MarketResult buildFailure(int timeSlot, const std::string& message);
};

} // namespace pms
