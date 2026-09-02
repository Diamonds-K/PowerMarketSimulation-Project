#include "Model/BidSegment/BidSegment.h"

namespace pms {

BidSegment::BidSegment(int segmentNo, double quantityMw, double priceYuanPerMwh)
    : segmentNo_(segmentNo),
      quantityMw_(quantityMw),
      priceYuanPerMwh_(priceYuanPerMwh) {}

int BidSegment::segmentNo() const {
    return segmentNo_;
}

void BidSegment::setSegmentNo(int segmentNo) {
    segmentNo_ = segmentNo;
}

double BidSegment::quantityMw() const {
    return quantityMw_;
}

void BidSegment::setQuantityMw(double quantityMw) {
    quantityMw_ = quantityMw;
}

double BidSegment::priceYuanPerMwh() const {
    return priceYuanPerMwh_;
}

void BidSegment::setPriceYuanPerMwh(double priceYuanPerMwh) {
    priceYuanPerMwh_ = priceYuanPerMwh;
}

} // namespace pms
