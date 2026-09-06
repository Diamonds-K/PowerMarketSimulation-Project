#pragma once

#include <QWidget>

#include "TradingCenter/TradingCenter.h"

class QChartView;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;

namespace pms {

class ConsumerWidget;
class GeneratorWidget;

class TradingCenterWidget : public QWidget {
    Q_OBJECT

public:
    explicit TradingCenterWidget(GeneratorWidget *generatorWidget,
                                 ConsumerWidget *consumerWidget,
                                 QWidget *parent = nullptr);

private slots:
    void importGeneratorParameters();
    void importLoadData();
    void runSinglePeriodClear();
    void runBatchClearAllPeriods();
    void exportResults();
    void onTimeSlotChanged(int timeSlot);

private:
    void connectSignals();
    void runClearForSlot(int slotIndex);

    GeneratorWidget *generatorWidget_ = nullptr;
    ConsumerWidget *consumerWidget_ = nullptr;

    QComboBox *modeCombo_ = nullptr;
    QSpinBox *periodSpinBox_ = nullptr;
    QPushButton *importGeneratorButton_ = nullptr;
    QPushButton *importLoadButton_ = nullptr;
    QPushButton *singleClearButton_ = nullptr;
    QPushButton *batchClearButton_ = nullptr;
    QPushButton *exportButton_ = nullptr;
    QChartView *chartView_ = nullptr;
    QLabel *mcpLabel_ = nullptr;
    QLabel *totalVolumeLabel_ = nullptr;

    TradingCenter tradingCenter_;
};

} // namespace pms
