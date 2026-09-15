#pragma once

#include <string>

#include "Model/BidSheet/BidSheet.h"

namespace pms {

class Consumer {
public:
    Consumer() = default;
    Consumer(std::string id, double fixedDemandMw);

    const std::string& id() const;
    void setId(const std::string& id);

    double fixedDemandMw() const;
    void setFixedDemandMw(double fixedDemandMw);

    // 本时段有效需求：优先取 bidSheet 中该时段的申报段电量之和；
    // 该时段没有任何申报段时，回退到 fixedDemandMw。见 DESIGN.md 第 2 节第 7 条。
    double effectiveDemandMw() const;

    const BidSheet& bidSheet() const;
    BidSheet& bidSheet();
    void setBidSheet(const BidSheet& bidSheet);

private:
    std::string id_;
    double fixedDemandMw_ = 0.0;
    BidSheet bidSheet_;
};

} // namespace pms
