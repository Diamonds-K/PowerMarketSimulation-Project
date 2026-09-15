#pragma once

#include <string>

#include "Model/BidSheet/BidSheet.h"

namespace pms {

class Consumer {
public:
    // 创建用户，记录标识和固定需求回退值。
    Consumer(std::string id, double fixedDemandMw);

    // 返回用户标识。
    const std::string& id() const;

    // 返回固定需求，单位为 MW。
    double fixedDemandMw() const;

    // 本时段有效需求：优先取 bidSheet 中该时段的申报段电量之和；
    // 该时段没有任何申报段时，回退到 fixedDemandMw。见 DESIGN.md 第 2 节第 7 条。
    double effectiveDemandMw() const;

    // 返回只读报价单。
    const BidSheet& bidSheet() const;

    // 替换用户报价单。
    void setBidSheet(const BidSheet& bidSheet);

private:
    std::string id_;
    double fixedDemandMw_ = 0.0;
    BidSheet bidSheet_;
};

} // namespace pms
