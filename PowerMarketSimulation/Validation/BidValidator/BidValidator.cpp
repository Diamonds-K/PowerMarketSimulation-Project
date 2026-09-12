#include "Validation/BidValidator/BidValidator.h"

#include <sstream>

namespace pms {

namespace {

constexpr double kEpsilon = 1e-9;
constexpr int kMaxSegments = 10;

std::string formatDouble(double value) {
    std::ostringstream oss;
    oss.precision(6);
    oss << value;
    return oss.str();
}

ValidationIssue issue(ValidationIssue::Severity severity,
                      const std::string& code,
                      const std::string& target,
                      const std::string& message) {
    ValidationIssue result;
    result.severity = severity;
    result.code = code;
    result.target = target;
    result.message = message;
    return result;
}

} // namespace

bool ValidationReport::ok() const {
    for (const auto& item : issues_) {
        if (item.severity == ValidationIssue::Severity::Error) {
            return false;
        }
    }
    return true;
}

void ValidationReport::addIssue(const ValidationIssue& item) {
    issues_.push_back(item);
}

void ValidationReport::merge(const ValidationReport& other) {
    issues_.insert(issues_.end(), other.issues_.begin(), other.issues_.end());
}

const std::vector<ValidationIssue>& ValidationReport::issues() const {
    return issues_;
}

std::vector<std::string> ValidationReport::messages() const {
    std::vector<std::string> result;
    result.reserve(issues_.size());
    for (const auto& item : issues_) {
        result.push_back(item.target + ": " + item.message);
    }
    return result;
}

ValidationReport BidValidator::validate(const MarketInput& input) {
    ValidationReport report;

    // 每个时段至少需要一台机组和一个用户。
    if (input.generators().empty()) {
        report.addIssue(issue(ValidationIssue::Severity::Error,
                              "NO_GENERATOR",
                              "MarketInput",
                              "至少需要一台机组"));
    }
    if (input.consumers().empty()) {
        report.addIssue(issue(ValidationIssue::Severity::Error,
                              "NO_CONSUMER",
                              "MarketInput",
                              "至少需要一个用户"));
    }

    for (const auto& generator : input.generators()) {
        report.merge(validateGenerator(generator));
    }
    for (const auto& consumer : input.consumers()) {
        report.merge(validateConsumer(consumer));
    }
    return report;
}

ValidationReport BidValidator::validateGenerator(const Generator& generator) {
    ValidationReport report;
    const std::string target = "Generator:" + generator.id();

    if (generator.pMinMw() < -kEpsilon) {
        report.addIssue(issue(ValidationIssue::Severity::Error,
                              "PMIN_NEGATIVE",
                              target,
                              "Pmin 不能为负"));
    }
    if (generator.pMaxMw() <= generator.pMinMw()) {
        report.addIssue(issue(ValidationIssue::Severity::Error,
                              "RANGE_INVALID",
                              target,
                              "Pmax 必须大于 Pmin"));
    }

    const double availableCapacity = generator.pMaxMw() - generator.pMinMw();
    report.merge(validateBidSheet(generator.bidSheet(), availableCapacity, true));
    return report;
}

ValidationReport BidValidator::validateConsumer(const Consumer& consumer) {
    ValidationReport report;
    const std::string target = "Consumer:" + consumer.id();

    if (consumer.fixedDemandMw() < -kEpsilon) {
        report.addIssue(issue(ValidationIssue::Severity::Error,
                              "DEMAND_NEGATIVE",
                              target,
                              "固定需求不能为负"));
    }

    report.merge(validateBidSheet(consumer.bidSheet(), 0.0, false));
    return report;
}

ValidationReport BidValidator::validateBidSheet(const BidSheet& bidSheet,
                                                double availableCapacityMw,
                                                bool generatorSide) {
    ValidationReport report;
    const std::string target = "BidSheet:" + bidSheet.ownerId();

    // 二次模式只检查系数；分段模式检查段数和单调性。
    if (bidSheet.mode() == BidSheet::Mode::Quadratic) {
        if (generatorSide && bidSheet.quadraticA() <= 0.0) {
            report.addIssue(issue(ValidationIssue::Severity::Error,
                                  "QUADRATIC_A_NONPOSITIVE",
                                  target,
                                  "二次项系数 a 必须大于 0"));
        }
        return report;
    }

    const auto& segments = bidSheet.segments();
    if (segments.empty()) {
        report.addIssue(issue(ValidationIssue::Severity::Error,
                              "NO_SEGMENT",
                              target,
                              "分段报价至少需要一段"));
    } else if (static_cast<int>(segments.size()) > kMaxSegments) {
        report.addIssue(issue(ValidationIssue::Severity::Error,
                              "TOO_MANY_SEGMENTS",
                              target,
                              "分段报价不能超过 10 段"));
    }

    for (const auto& segment : segments) {
        if (segment.quantityMw() <= kEpsilon) {
            report.addIssue(issue(ValidationIssue::Severity::Error,
                                  "NON_POSITIVE_QUANTITY",
                                  target,
                                  "段电量必须大于 0"));
        }
    }

    for (size_t i = 1; i < segments.size(); ++i) {
        const double previous = segments[i - 1].priceYuanPerMwh();
        const double current = segments[i].priceYuanPerMwh();
        if (generatorSide) {
            // 发电侧价格必须单调不减。
            if (current + kEpsilon < previous) {
                report.addIssue(issue(ValidationIssue::Severity::Error,
                                      "NON_MONOTONE_SELL",
                                      target,
                                      "发电段价格必须单调不减"));
                break;
            }
        } else {
            // 用户侧价格必须单调不增。
            if (current - kEpsilon > previous) {
                report.addIssue(issue(ValidationIssue::Severity::Error,
                                      "NON_MONOTONE_BUY",
                                      target,
                                      "用户段价格必须单调不增"));
                break;
            }
        }
    }

    if (generatorSide) {
        const double totalIncrement = bidSheet.totalIncrementMw();
        if (totalIncrement > availableCapacityMw + kEpsilon) {
            report.addIssue(issue(ValidationIssue::Severity::Error,
                                  "CAPACITY_EXCEEDED",
                                  target,
                                  "累计增量电量 " + formatDouble(totalIncrement) +
                                      " 超过 Pmax-Pmin=" + formatDouble(availableCapacityMw)));
        }
    }

    return report;
}

} // namespace pms
