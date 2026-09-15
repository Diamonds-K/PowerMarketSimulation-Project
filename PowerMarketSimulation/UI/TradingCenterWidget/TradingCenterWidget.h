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

// 交易中心页面，负责 CSV 数据导入、出清执行、结果展示和导出。
class TradingCenterWidget : public QWidget {
    Q_OBJECT

public:
    // 创建交易中心页面并关联发电侧、用户侧数据控件。
    explicit TradingCenterWidget(GeneratorWidget *generatorWidget,
                                 ConsumerWidget *consumerWidget,
                                 QWidget *parent = nullptr);

    // 同步当前时段，并刷新该时段的出清结果。
    void setTimeSlot(int displaySlot);

signals:
    // 当前时段变化时通知主窗口同步其他页面。
    void timeSlotChanged(int displaySlot);

private slots:
    // 导入发电机组参数 CSV。
    void importGeneratorParameters();

    // 导入发电侧报价 CSV。
    void importGeneratorBids();

    // 导入用户参数 CSV。
    void importConsumerParameters();

    // 导入用户侧报价 CSV。
    void importConsumerBids();

    // 对当前时段执行一次出清。
    void runSinglePeriodClear();

    // 连续执行全天 96 个时段的出清。
    void runBatchClearAllPeriods();

    // 将最近一次出清结果导出为 CSV。
    void exportResults();

    // 响应用户切换时段。
    void onTimeSlotChanged(int timeSlot);

private:
    // 连接页面内所有按钮和时段控件信号。
    void connectSignals();

    // 组装单个时段输入并执行出清、结算和图表刷新。
    void runClearForSlot(int slotIndex);

    // 刷新当前时段的主体结算明细表。
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
