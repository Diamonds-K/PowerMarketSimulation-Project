#pragma once

#include <string>

#include "Model/BidSheet/BidSheet.h"

namespace pms {

class Generator {
public:
    // 创建发电机组，记录标识、最小出力和最大出力。
    Generator(std::string id, double pMinMw, double pMaxMw);

    // 返回机组标识。
    const std::string& id() const;

    // 返回最小稳定出力，单位为 MW。
    double pMinMw() const;

    // 返回最大出力，单位为 MW。
    double pMaxMw() const;

    // 返回只读报价单。
    const BidSheet& bidSheet() const;

    // 替换机组报价单。
    void setBidSheet(const BidSheet& bidSheet);

private:
    std::string id_;
    double pMinMw_ = 0.0;
    double pMaxMw_ = 0.0;
    BidSheet bidSheet_;
};

} // namespace pms
