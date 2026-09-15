#include "Model/Consumer/Consumer.h"

#include <utility>

namespace pms {

Consumer::Consumer(std::string id, double fixedDemandMw)
    : id_(std::move(id)),
      fixedDemandMw_(fixedDemandMw) {}

const std::string& Consumer::id() const {
    return id_;
}

void Consumer::setId(const std::string& id) {
    id_ = id;
}

double Consumer::fixedDemandMw() const {
    return fixedDemandMw_;
}

void Consumer::setFixedDemandMw(double fixedDemandMw) {
    fixedDemandMw_ = fixedDemandMw;
}

double Consumer::effectiveDemandMw() const {
    // 逐时段申报优先：本时段存在申报段时，刚性需求取该时段申报总量。
    const double declaredDemandMw = bidSheet_.totalIncrementMw();
    if (declaredDemandMw > 0.0) {
        return declaredDemandMw;
    }
    // 本时段没有任何申报段时，回退到用户参数中的固定需求。
    return fixedDemandMw_;
}

const BidSheet& Consumer::bidSheet() const {
    return bidSheet_;
}

BidSheet& Consumer::bidSheet() {
    return bidSheet_;
}

void Consumer::setBidSheet(const BidSheet& bidSheet) {
    bidSheet_ = bidSheet;
}

} // namespace pms
