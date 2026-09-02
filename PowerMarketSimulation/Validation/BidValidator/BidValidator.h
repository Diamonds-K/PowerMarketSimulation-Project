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
    static ValidationReport validateGenerator(const Generator& generator);
    static ValidationReport validateConsumer(const Consumer& consumer);
    static ValidationReport validateBidSheet(const BidSheet& bidSheet,
                                             double availableCapacityMw,
                                             bool generatorSide);
};

} // namespace pms
