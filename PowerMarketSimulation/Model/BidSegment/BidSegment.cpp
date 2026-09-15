#include "Model/BidSegment/BidSegment.h"

namespace pms {

BidSegment::BidSegment(int segmentNo, double quantityMw, double priceYuanPerMwh)
    : segmentNo_(segmentNo),
      quantityMw_(quantityMw),
      priceYuanPerMwh_(priceYuanPerMwh) {}

int BidSegment::segmentNo() const {
    return segmentNo_;
}

double BidSegment::quantityMw() const {
    return quantityMw_;
}

double BidSegment::priceYuanPerMwh() const {
    return priceYuanPerMwh_;
}

} // namespace pms
