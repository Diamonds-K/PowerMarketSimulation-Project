#include "Model/Generator/Generator.h"

#include <utility>

namespace pms {

Generator::Generator(std::string id, double pMinMw, double pMaxMw)
    : id_(std::move(id)),
      pMinMw_(pMinMw),
      pMaxMw_(pMaxMw) {}

const std::string& Generator::id() const {
    return id_;
}

void Generator::setId(const std::string& id) {
    id_ = id;
}

double Generator::pMinMw() const {
    return pMinMw_;
}

void Generator::setPMinMw(double pMinMw) {
    pMinMw_ = pMinMw;
}

double Generator::pMaxMw() const {
    return pMaxMw_;
}

void Generator::setPMaxMw(double pMaxMw) {
    pMaxMw_ = pMaxMw;
}

const BidSheet& Generator::bidSheet() const {
    return bidSheet_;
}

BidSheet& Generator::bidSheet() {
    return bidSheet_;
}

void Generator::setBidSheet(const BidSheet& bidSheet) {
    bidSheet_ = bidSheet;
}

} // namespace pms
