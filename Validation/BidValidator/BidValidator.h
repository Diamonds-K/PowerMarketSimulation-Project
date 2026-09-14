#pragma once

#include <string>
#include <vector>

#include "Model/Consumer/Consumer.h"
#include "Model/Generator/Generator.h"
#include "Model/MarketInput/MarketInput.h"

namespace pms {

struct ValidationIssue {
    enum class Severity {
        Error,
        Warning
    };

    Severity severity = Severity::Error;
    std::string code;
    std::string target;
    std::string message;
};

class ValidationReport {
public:
    bool ok() const;
    void addIssue(const ValidationIssue& issue);
    void merge(const ValidationReport& other);
    const std::vector<ValidationIssue>& issues() const;
    std::vector<std::string> messages() const;

private:
    std::vector<ValidationIssue> issues_;
};

class BidValidator {
public:
    static ValidationReport validate(const MarketInput& input);
    // quadraticMarket 为 true 时按二次曲线模式校验：用户侧只申报固定需求 QD，
    // 不要求分段报价；发电机侧只校验二次项系数（见 DESIGN.md 第 4.2 节）。
    static ValidationReport validateGenerator(const Generator& generator,
                                              bool quadraticMarket = false);
    static ValidationReport validateConsumer(const Consumer& consumer,
                                             bool quadraticMarket = false);
    static ValidationReport validateBidSheet(const BidSheet& bidSheet,
                                             double availableCapacityMw,
                                             bool generatorSide,
                                             bool quadraticMarket = false);
};

} // namespace pms
