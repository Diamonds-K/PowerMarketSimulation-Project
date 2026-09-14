#pragma once

#include <vector>

#include <QString>

#include "TradingCenter/TradingCenter.h"

namespace pms {

class GeneratorWidget;
class ConsumerWidget;

// UI 层内部编排助手：唯一负责把发电侧与用户侧的录入组装成 MarketInput，
// 再调用 TradingCenter 完成校验 / 出清 / 结算。三个页面共用同一个实例，
// 保证「发电侧看收入、用户侧看账单、交易中心看全场」用的是同一套结果。
class SimulationRunner {
public:
    SimulationRunner(GeneratorWidget* generatorWidget, ConsumerWidget* consumerWidget);

    // slotIndex 为程序内部时段编号 0..95。
    // 组装前会先把两个页面编辑区的内容写回数据模型，因此不是 const 方法。
    MarketInput buildSlotInput(int slotIndex, bool quadratic);
    ValidationReport validateSlot(int slotIndex, bool quadratic);
    TimeSlotResult runSlot(int slotIndex, bool quadratic);
    std::vector<TimeSlotResult> runAll(bool quadratic);

    // 当前市场模式由交易中心页的「报价模式」下拉框决定。
    void setQuadraticMode(bool quadratic);
    bool quadraticMode() const;

    GeneratorWidget* generatorWidget() const;
    ConsumerWidget* consumerWidget() const;
    // 按主体 ID 取页面里录入的名称，用于申报总览与导出展示。
    QString generatorName(const QString& generatorId) const;
    QString consumerName(const QString& consumerId) const;

    TradingCenter& tradingCenter();

private:
    GeneratorWidget* generatorWidget_ = nullptr;
    ConsumerWidget* consumerWidget_ = nullptr;
    TradingCenter tradingCenter_;
    bool quadraticMode_ = false;
};

} // namespace pms
