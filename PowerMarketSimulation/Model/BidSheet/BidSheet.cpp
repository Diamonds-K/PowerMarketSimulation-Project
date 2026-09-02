#include "Model/BidSheet/BidSheet.h"

namespace pms {

BidSheet::Mode BidSheet::mode() const {
    return mode_;
}

void BidSheet::setMode(Mode mode) {
    mode_ = mode;
}

const std::string& BidSheet::ownerId() const {
    return ownerId_;
}

void BidSheet::setOwnerId(const std::string& ownerId) {
    ownerId_ = ownerId;
}

int BidSheet::timeSlot() const {
    return timeSlot_;
}

void BidSheet::setTimeSlot(int timeSlot) {
    timeSlot_ = timeSlot;
}

const std::vector<BidSegment>& BidSheet::segments() const {
    return segments_;
}

void BidSheet::addSegment(const BidSegment& segment) {
    segments_.push_back(segment);
}

void BidSheet::setSegments(const std::vector<BidSegment>& segments) {
    segments_ = segments;
}

void BidSheet::clearSegments() {
    segments_.clear();
}

double BidSheet::quadraticA() const {
    return quadraticA_;
}

double BidSheet::quadraticB() const {
    return quadraticB_;
}

double BidSheet::quadraticC() const {
    return quadraticC_;
}

void BidSheet::setQuadraticCoefficients(double a, double b, double c) {
    quadraticA_ = a;
    quadraticB_ = b;
    quadraticC_ = c;
}

double BidSheet::totalIncrementMw() const {
    double total = 0.0;
    for (const auto& segment : segments_) {
        total += segment.quantityMw();
    }
    return total;
}

} // namespace pms
