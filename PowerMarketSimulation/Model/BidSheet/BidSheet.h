#pragma once

#include <string>
#include <vector>

#include "Model/BidSegment/BidSegment.h"

namespace pms {

class BidSheet {
public:
    enum class Mode {
        Piecewise,
        Quadratic
    };

    BidSheet() = default;

    Mode mode() const;
    void setMode(Mode mode);

    const std::string& ownerId() const;
    void setOwnerId(const std::string& ownerId);

    int timeSlot() const;
    void setTimeSlot(int timeSlot);

    const std::vector<BidSegment>& segments() const;
    void addSegment(const BidSegment& segment);
    void setSegments(const std::vector<BidSegment>& segments);
    void clearSegments();

    double quadraticA() const;
    double quadraticB() const;
    double quadraticC() const;
    void setQuadraticCoefficients(double a, double b, double c);

    double totalIncrementMw() const;

private:
    Mode mode_ = Mode::Piecewise;
    std::string ownerId_;
    int timeSlot_ = -1;
    std::vector<BidSegment> segments_;
    double quadraticA_ = 0.0;
    double quadraticB_ = 0.0;
    double quadraticC_ = 0.0;
};

} // namespace pms
