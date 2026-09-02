#pragma once

#include <string>
#include <vector>

#include "Clearing/ClearingEngine/ClearingEngine.h"

namespace pms {

class QuadraticClearing : public ClearingEngine {
public:
    MarketResult clear(const MarketInput& input) override;
    std::string modeName() const override;

    static constexpr double kTolerance = 1e-6;
    static constexpr int kMaxIterations = 200;

private:
    struct Unit {
        const Generator* generator = nullptr;
        double a = 0.0;
        double b = 0.0;
        double pMin = 0.0;
        double pMax = 0.0;
    };

    static double outputForLambda(const Unit& unit, double lambda);
    static double totalOutput(const std::vector<Unit>& units, double lambda);
    static MarketResult buildFailure(int timeSlot, const std::string& message);
};

} // namespace pms
