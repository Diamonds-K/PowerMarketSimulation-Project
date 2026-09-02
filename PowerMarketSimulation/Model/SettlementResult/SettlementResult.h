#pragma once

#include <string>
#include <vector>

namespace pms {

struct GeneratorSettlement {
    std::string generatorId;
    double outputMw = 0.0;
    double revenueYuan = 0.0;
    double costYuan = 0.0;
    double profitYuan = 0.0;
};

struct ConsumerSettlement {
    std::string consumerId;
    double clearedDemandMw = 0.0;
    double paymentYuan = 0.0;
};

class SettlementResult {
public:
    SettlementResult() = default;

    int timeSlot() const;
    void setTimeSlot(int timeSlot);

    double clearingPriceYuanPerMwh() const;
    void setClearingPriceYuanPerMwh(double price);

    double totalRevenueYuan() const;
    void setTotalRevenueYuan(double value);

    double totalCostYuan() const;
    void setTotalCostYuan(double value);

    double totalProfitYuan() const;
    void setTotalProfitYuan(double value);

    double totalPaymentYuan() const;
    void setTotalPaymentYuan(double value);

    double balanceYuan() const;

    const std::vector<GeneratorSettlement>& generatorSettlements() const;
    void addGeneratorSettlement(const GeneratorSettlement& settlement);

    const std::vector<ConsumerSettlement>& consumerSettlements() const;
    void addConsumerSettlement(const ConsumerSettlement& settlement);

private:
    int timeSlot_ = -1;
    double clearingPriceYuanPerMwh_ = 0.0;
    double totalRevenueYuan_ = 0.0;
    double totalCostYuan_ = 0.0;
    double totalProfitYuan_ = 0.0;
    double totalPaymentYuan_ = 0.0;
    std::vector<GeneratorSettlement> generatorSettlements_;
    std::vector<ConsumerSettlement> consumerSettlements_;
};

} // namespace pms
