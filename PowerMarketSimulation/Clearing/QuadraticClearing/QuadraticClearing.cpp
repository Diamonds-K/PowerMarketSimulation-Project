#include "Clearing/QuadraticClearing/QuadraticClearing.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace pms {

namespace {

std::string formatDouble(double value) {
    std::ostringstream oss;
    oss.precision(6);
    oss << value;
    return oss.str();
}

} // namespace

MarketResult QuadraticClearing::clear(const MarketInput& input) {
    if (input.mode() != MarketMode::Quadratic) {
        return buildFailure(input.timeSlot(), "QuadraticClearing 只能处理二次曲线报价模式");
    }

    // 读取每台机组的二次成本参数和容量边界。
    std::vector<Unit> units;
    units.reserve(input.generators().size());
    double sumPMin = 0.0;
    double sumPMax = 0.0;

    for (const auto& generator : input.generators()) {
        Unit unit;
        unit.generator = &generator;
        unit.a = generator.bidSheet().quadraticA();
        unit.b = generator.bidSheet().quadraticB();
        unit.pMin = generator.pMinMw();
        unit.pMax = generator.pMaxMw();

        if (unit.a <= 0.0) {
            return buildFailure(input.timeSlot(), "二次项系数 a 必须大于 0");
        }

        sumPMin += unit.pMin;
        sumPMax += unit.pMax;
        units.push_back(unit);
    }

    const double demand = input.totalFixedDemandMw();
    // 二次模式要求固定总需求位于机组总容量范围内。
    if (demand + kTolerance < sumPMin || demand > sumPMax + kTolerance) {
        return buildFailure(input.timeSlot(),
                            "总需求 QD 超出 [ΣPmin, ΣPmax] 范围，判定不可行");
    }

    double lo = std::numeric_limits<double>::infinity();
    double hi = -std::numeric_limits<double>::infinity();
    // λ 的初始搜索区间由各机组边际成本的最小值和最大值确定。
    for (const auto& unit : units) {
        lo = std::min(lo, 2.0 * unit.a * unit.pMin + unit.b);
        hi = std::max(hi, 2.0 * unit.a * unit.pMax + unit.b);
    }
    lo -= 1.0;
    hi += 1.0;

    double lambda = 0.5 * (lo + hi);
    // 对 λ 进行二分，直到总出力满足需求或区间足够小。
    for (int iteration = 0; iteration < kMaxIterations; ++iteration) {
        lambda = 0.5 * (lo + hi);
        const double supply = totalOutput(units, lambda);
        if (std::fabs(supply - demand) <= kTolerance || (hi - lo) <= kTolerance) {
            break;
        }
        if (supply < demand) {
            lo = lambda;
        } else {
            hi = lambda;
        }
    }

    MarketResult result;
    result.setTimeSlot(input.timeSlot());
    result.setFeasible(true);
    result.setClearingPriceYuanPerMwh(lambda);

    double volume = 0.0;
    // 用最终 λ 计算每台机组的最优出力。
    for (const auto& unit : units) {
        const double output = outputForLambda(unit, lambda);
        GeneratorResult generatorResult;
        generatorResult.generatorId = unit.generator->id();
        generatorResult.outputMw = output;
        generatorResult.marginalCostYuanPerMwh = 2.0 * unit.a * output + unit.b;
        result.addGeneratorResult(generatorResult);
        volume += output;
    }
    result.setClearingVolumeMw(volume);

    for (const auto& consumer : input.consumers()) {
        ConsumerResult consumerResult;
        consumerResult.consumerId = consumer.id();
        consumerResult.clearedDemandMw = consumer.fixedDemandMw();
        result.addConsumerResult(consumerResult);
    }

    result.setMessage("二次曲线模式已按 KKT/λ 二分收敛，Cclear=λ=" + formatDouble(lambda));
    return result;
}

std::string QuadraticClearing::modeName() const {
    return "Quadratic";
}

double QuadraticClearing::outputForLambda(const Unit& unit, double lambda) {
    // 自由机组边际成本等于 λ；碰界机组夹在 Pmin 和 Pmax。
    const double output = (lambda - unit.b) / (2.0 * unit.a);
    return std::clamp(output, unit.pMin, unit.pMax);
}

double QuadraticClearing::totalOutput(const std::vector<Unit>& units, double lambda) {
    double total = 0.0;
    for (const auto& unit : units) {
        total += outputForLambda(unit, lambda);
    }
    return total;
}

MarketResult QuadraticClearing::buildFailure(int timeSlot, const std::string& message) {
    MarketResult result;
    result.setTimeSlot(timeSlot);
    result.setFeasible(false);
    result.setMessage(message);
    return result;
}

} // namespace pms
