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

    const BidSheet& bidSheet() const;
    BidSheet& bidSheet();
    void setBidSheet(const BidSheet& bidSheet);

private:
    std::string id_;
    double fixedDemandMw_ = 0.0;
    BidSheet bidSheet_;
};

} // namespace pms
