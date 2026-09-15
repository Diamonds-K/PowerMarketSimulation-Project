#include "Model/BidSheet/BidSheet.h"

#include <algorithm>

namespace pms {

BidSheet::QuadraticFit BidSheet::fitLadderToQuadratic(
    double pMin,
    double pMax,
    const std::vector<BidSegment>& segments) {
    QuadraticFit result;
    if (segments.empty()) {
        return result;
    }

    // 把每段报价看成该段上边界处的边际成本点，再对 MC=2aP+b 做最小二乘拟合。
    // 只使用真实申报容量内的点，避免把末端报价错误平移到 Pmax。
    std::vector<std::pair<double, double>> points;
    double cumulativePower = 0.0;
    for (const BidSegment& segment : segments) {
        cumulativePower += segment.quantityMw();
        const double power = std::min(pMin + cumulativePower, pMax);
        points.push_back({power, segment.priceYuanPerMwh()});
    }

    if (points.size() < 2) {
        points.push_back({pMin, points.front().second});
    }

    double sumX = 0.0;
    double sumY = 0.0;
    for (const auto& point : points) {
        sumX += point.first;
        sumY += point.second;
    }
    const double meanX = sumX / static_cast<double>(points.size());
    const double meanY = sumY / static_cast<double>(points.size());

    double numerator = 0.0;
    double denominator = 0.0;
    for (const auto& point : points) {
        numerator += (point.first - meanX) * (point.second - meanY);
        denominator += (point.first - meanX) * (point.first - meanX);
    }

    const double slope = denominator > 1e-12 ? numerator / denominator : 0.0;
    constexpr double kMinQuadraticA = 1e-6;
    result.a = std::max(slope / 2.0, kMinQuadraticA);
    result.b = meanY - slope * meanX;
    result.c = 0.0;
    return result;
}

BidSheet::Mode BidSheet::mode() const {
    return mode_;
}

void BidSheet::setMode(Mode mode) {
    mode_ = mode;
}

const std::string& BidSheet::ownerId() const {
    return ownerId_;
}

void BidSheet::setOwnerId(const std::string& ownerId) {
    ownerId_ = ownerId;
}

const std::vector<BidSegment>& BidSheet::segments() const {
    return segments_;
}

void BidSheet::addSegment(const BidSegment& segment) {
    segments_.push_back(segment);
}

double BidSheet::quadraticA() const {
    return quadraticA_;
}

double BidSheet::quadraticB() const {
    return quadraticB_;
}

double BidSheet::quadraticC() const {
    return quadraticC_;
}

void BidSheet::setQuadraticCoefficients(double a, double b, double c) {
    quadraticA_ = a;
    quadraticB_ = b;
    quadraticC_ = c;
}

double BidSheet::totalIncrementMw() const {
    double total = 0.0;
    for (const auto& segment : segments_) {
        total += segment.quantityMw();
    }
    return total;
}

} // namespace pms
