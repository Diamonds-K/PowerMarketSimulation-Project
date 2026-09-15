#pragma once

#include <vector>

#include <QWidget>

#include "TradingCenter/TradingCenter.h"

class QChartView;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTableWidget;

namespace pms {

class ConsumerWidget;
class GeneratorWidget;

class TradingCenterWidget : public QWidget {
    Q_OBJECT

public:
    explicit TradingCenterWidget(GeneratorWidget *generatorWidget,
                                 ConsumerWidget *consumerWidget,
                                 QWidget *parent = nullptr);
    void setTimeSlot(int displaySlot);

signals:
    void timeSlotChanged(int displaySlot);

private slots:
    void importGeneratorParameters();
    void importGeneratorBids();
    void importConsumerParameters();
    void importConsumerBids();
    void runSinglePeriodClear();
    void runBatchClearAllPeriods();
    void exportResults();
    void onTimeSlotChanged(int timeSlot);

private:
    void connectSignals();
    void runClearForSlot(int slotIndex);
    void refreshDetail(int slotIndex);

    GeneratorWidget *generatorWidget_ = nullptr;
    ConsumerWidget *consumerWidget_ = nullptr;

    QComboBox *modeCombo_ = nullptr;
    QSpinBox *periodSpinBox_ = nullptr;
    QPushButton *importGeneratorParametersButton_ = nullptr;
    QPushButton *importGeneratorBidsButton_ = nullptr;
    QPushButton *importConsumerParametersButton_ = nullptr;
    QPushButton *importConsumerBidsButton_ = nullptr;
    QPushButton *singleClearButton_ = nullptr;
    QPushButton *batchClearButton_ = nullptr;
    QPushButton *exportButton_ = nullptr;
    QChartView *chartView_ = nullptr;
    QLabel *mcpLabel_ = nullptr;
    QLabel *totalVolumeLabel_ = nullptr;
    QTabWidget *resultsTabs_ = nullptr;
    QTableWidget *resultsTable_ = nullptr;
    QTableWidget *detailTable_ = nullptr;

    TradingCenter tradingCenter_;
    bool syncingTimeSlot_ = false;
    std::vector<TimeSlotResult> lastResults_;
};

} // namespace pms
