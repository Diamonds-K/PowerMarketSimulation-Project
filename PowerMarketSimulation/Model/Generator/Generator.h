#pragma once

#include <string>

#include "Model/BidSheet/BidSheet.h"

namespace pms {

class Generator {
public:
    Generator() = default;
    Generator(std::string id, double pMinMw, double pMaxMw);

    const std::string& id() const;
    void setId(const std::string& id);

    double pMinMw() const;
    void setPMinMw(double pMinMw);

    double pMaxMw() const;
    void setPMaxMw(double pMaxMw);

    const BidSheet& bidSheet() const;
    BidSheet& bidSheet();
    void setBidSheet(const BidSheet& bidSheet);

private:
    std::string id_;
    double pMinMw_ = 0.0;
    double pMaxMw_ = 0.0;
    BidSheet bidSheet_;
};

} // namespace pms
