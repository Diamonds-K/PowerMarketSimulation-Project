#pragma once

#include <string>
#include <vector>

#include "Model/Consumer/Consumer.h"
#include "Model/Generator/Generator.h"
#include "Model/MarketInput/MarketInput.h"

namespace pms {

// 单条校验问题，包含严重级别、关联对象和错误说明。
struct ValidationIssue {
    // 问题级别。
    enum class Severity {
        Error,
        Warning
    };

    Severity severity = Severity::Error;
    std::string target;
    std::string message;
};

class ValidationReport {
public:
    // 判断报告中是否不存在错误级问题。
    bool ok() const;

    // 追加一条校验问题。
    void addIssue(const ValidationIssue& issue);

    // 合并另一份校验报告。
    void merge(const ValidationReport& other);

    // 将所有问题转换为可直接展示的文本消息。
    std::vector<std::string> messages() const;

private:
    std::vector<ValidationIssue> issues_;
};

class BidValidator {
public:
    // 校验整个时段的市场输入。
    static ValidationReport validate(const MarketInput& input);

    // 校验单台机组的容量参数和报价单。
    static ValidationReport validateGenerator(const Generator& generator);

    // 校验单个用户的固定需求和报价单。
    static ValidationReport validateConsumer(const Consumer& consumer);

    // 校验报价单结构、单调性和容量约束。
    static ValidationReport validateBidSheet(const BidSheet& bidSheet,
                                             double availableCapacityMw,
                                             bool generatorSide);
};

} // namespace pms
