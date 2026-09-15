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

double Generator::pMinMw() const {
    return pMinMw_;
}

double Generator::pMaxMw() const {
    return pMaxMw_;
}

const BidSheet& Generator::bidSheet() const {
    return bidSheet_;
}

void Generator::setBidSheet(const BidSheet& bidSheet) {
    bidSheet_ = bidSheet;
}

} // namespace pms
