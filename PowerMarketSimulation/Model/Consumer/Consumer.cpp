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
