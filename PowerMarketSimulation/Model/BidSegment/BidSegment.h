#pragma once

namespace pms {

class BidSegment {
public:
    // 创建一个报价段，记录段号、电量和单价。
    BidSegment(int segmentNo, double quantityMw, double priceYuanPerMwh);

    // 返回段号。
    int segmentNo() const;

    // 返回该段电量，单位为 MW。
    double quantityMw() const;

    // 返回该段报价，单位为元/MWh。
    double priceYuanPerMwh() const;

private:
    int segmentNo_ = 0;
    double quantityMw_ = 0.0;
    double priceYuanPerMwh_ = 0.0;
};

} // namespace pms
