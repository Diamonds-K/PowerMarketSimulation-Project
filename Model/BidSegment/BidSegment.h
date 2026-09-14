#pragma once

namespace pms {

class BidSegment {
public:
    BidSegment() = default;
    BidSegment(int segmentNo, double quantityMw, double priceYuanPerMwh);

    int segmentNo() const;
    void setSegmentNo(int segmentNo);

    double quantityMw() const;
    void setQuantityMw(double quantityMw);

    double priceYuanPerMwh() const;
    void setPriceYuanPerMwh(double priceYuanPerMwh);

private:
    int segmentNo_ = 0;
    double quantityMw_ = 0.0;
    double priceYuanPerMwh_ = 0.0;
};

} // namespace pms
