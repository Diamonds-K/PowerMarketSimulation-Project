#pragma once

#include <string>
#include <vector>

namespace pms {

// 单个发电机组的结算明细。
struct GeneratorSettlement {
    std::string generatorId;
    double outputMw = 0.0;
    double revenueYuan = 0.0;
    double costYuan = 0.0;
    double profitYuan = 0.0;
};

// 单个用户的结算明细。
struct ConsumerSettlement {
    std::string consumerId;
    double clearedDemandMw = 0.0;
    double paymentYuan = 0.0;
};

class SettlementResult {
public:
    // 创建空的结算结果。
    SettlementResult() = default;

    // 返回全部机组的总收益。
    double totalRevenueYuan() const;

    // 设置全部机组的总收益。
    void setTotalRevenueYuan(double value);

    // 返回全部用户的总支付。
    double totalPaymentYuan() const;

    // 设置全部用户的总支付。
    void setTotalPaymentYuan(double value);

    // 返回总支付与总收益之差，用于结算平衡校验。
    double balanceYuan() const;

    // 返回全部发电机组的结算明细。
    const std::vector<GeneratorSettlement>& generatorSettlements() const;

    // 追加一条发电机组结算明细。
    void addGeneratorSettlement(const GeneratorSettlement& settlement);

    // 返回全部用户的结算明细。
    const std::vector<ConsumerSettlement>& consumerSettlements() const;

    // 追加一条用户结算明细。
    void addConsumerSettlement(const ConsumerSettlement& settlement);

private:
    double totalRevenueYuan_ = 0.0;
    double totalPaymentYuan_ = 0.0;
    std::vector<GeneratorSettlement> generatorSettlements_;
    std::vector<ConsumerSettlement> consumerSettlements_;
};

} // namespace pms
