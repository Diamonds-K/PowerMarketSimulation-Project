#pragma once

#include <string>
#include <vector>

#include "Model/BidSegment/BidSegment.h"

namespace pms {

class BidSheet {
public:
    // 报价模式：分段阶梯报价或二次成本曲线。
    enum class Mode {
        Piecewise,
        Quadratic
    };

    // 由阶梯报价拟合得到的二次成本系数。
    struct QuadraticFit {
        double a = 0.05;
        double b = 10.0;
        double c = 0.0;
    };

    BidSheet() = default;

    // 根据发电侧阶梯报价拟合 aP^2+bP+c 形式的二次成本曲线。
    static QuadraticFit fitLadderToQuadratic(
        double pMin,
        double pMax,
        const std::vector<BidSegment>& segments);

    // 返回当前报价模式。
    Mode mode() const;

    // 设置报价模式。
    void setMode(Mode mode);

    // 返回报价主体标识。
    const std::string& ownerId() const;

    // 设置报价主体标识。
    void setOwnerId(const std::string& ownerId);

    // 返回全部报价段。
    const std::vector<BidSegment>& segments() const;

    // 向报价单追加一个报价段。
    void addSegment(const BidSegment& segment);

    // 返回二次项系数 a。
    double quadraticA() const;

    // 返回一次项系数 b。
    double quadraticB() const;

    // 返回常数项系数 c。
    double quadraticC() const;

    // 设置二次成本曲线系数。
    void setQuadraticCoefficients(double a, double b, double c);

    // 返回全部报价段电量之和，单位为 MW。
    double totalIncrementMw() const;

private:
    Mode mode_ = Mode::Piecewise;
    std::string ownerId_;
    std::vector<BidSegment> segments_;
    double quadraticA_ = 0.0;
    double quadraticB_ = 0.0;
    double quadraticC_ = 0.0;
};

} // namespace pms
