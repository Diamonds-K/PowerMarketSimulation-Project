#pragma once

#include <vector>

#include "Model/Consumer/Consumer.h"
#include "Model/Generator/Generator.h"

namespace pms {

enum class MarketMode {
    Piecewise,
    Quadratic
};

class MarketInput {
public:
    MarketInput() = default;
    MarketInput(int timeSlot, MarketMode mode);

    int timeSlot() const;
    void setTimeSlot(int timeSlot);

    MarketMode mode() const;
    void setMode(MarketMode mode);

    const std::vector<Generator>& generators() const;
    std::vector<Generator>& generators();
    void addGenerator(const Generator& generator);

    const std::vector<Consumer>& consumers() const;
    std::vector<Consumer>& consumers();
    void addConsumer(const Consumer& consumer);

    double totalFixedDemandMw() const;
    double totalDeclaredDemandMw() const;

private:
    int timeSlot_ = -1;
    MarketMode mode_ = MarketMode::Piecewise;
    std::vector<Generator> generators_;
    std::vector<Consumer> consumers_;
};

} // namespace pms
