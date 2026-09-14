#include "Validation/ConstraintChecker/ConstraintChecker.h"

#include <sstream>

namespace pms {

namespace {

constexpr double kEpsilon = 1e-9;

std::string formatDouble(double value) {
    std::ostringstream oss;
    oss.precision(6);
    oss << value;
    return oss.str();
}

} // namespace

ValidationReport ConstraintChecker::checkFeasibility(const MarketInput& input) {
    ValidationReport report;

    if (input.generators().empty()) {
        return report;
    }

    double sumPMin = 0.0;
    double sumPMax = 0.0;
    // 汇总全部机组的容量上下限。
    for (const auto& generator : input.generators()) {
        sumPMin += generator.pMinMw();
        sumPMax += generator.pMaxMw();
    }

    if (input.mode() == MarketMode::Quadratic) {
        const double demand = input.totalFixedDemandMw();
        // 二次模式要求固定需求在总容量范围内。
        if (demand + kEpsilon < sumPMin || demand > sumPMax + kEpsilon) {
            ValidationIssue item;
            item.severity = ValidationIssue::Severity::Error;
            item.code = "DEMAND_OUT_OF_RANGE";
            item.target = "MarketInput";
            item.message = "总需求 QD=" + formatDouble(demand) +
                           " 必须位于 [ΣPmin, ΣPmax]=[" +
                           formatDouble(sumPMin) + ", " +
                           formatDouble(sumPMax) + "]";
            report.addIssue(item);
        }
        return report;
    }

    const double declaredDemand = input.totalDeclaredDemandMw();
    // 分段模式要求申报需求不低于机组最低出力之和。
    if (declaredDemand + kEpsilon < sumPMin) {
        ValidationIssue item;
        item.severity = ValidationIssue::Severity::Error;
        item.code = "DEMAND_BELOW_PMIN";
        item.target = "MarketInput";
        item.message = "用户总申报需求=" + formatDouble(declaredDemand) +
                       " 低于 ΣPmin=" + formatDouble(sumPMin);
        report.addIssue(item);
    }

    return report;
}

} // namespace pms
