#pragma once

#include <vector>

#include <QStringList>
#include <QWidget>

#include "TradingCenter/TradingCenter.h"

class QChartView;
class QComboBox;
class QLabel;
class QPushButton;
class QSplitter;
class QSpinBox;
class QTabWidget;
class QTableWidget;

namespace pms {

class SimulationRunner;

class TradingCenterWidget : public QWidget {
    Q_OBJECT

public:
    explicit TradingCenterWidget(SimulationRunner *runner, QWidget *parent = nullptr);
    void setTimeSlot(int displaySlot);

signals:
    void timeSlotChanged(int displaySlot);
    // 报价模式（分段 / 二次曲线）发生变化，三个页面需要同步刷新。
    void modeChanged(bool quadratic);
    // 某个时段完成出清，发电侧与用户侧页面据此刷新自己的结果。
    void clearingFinished(int slotIndex);

private slots:
    void importGeneratorParameters();
    void importGeneratorBids();
    void importConsumerParameters();
    void importConsumerBids();
    void runSinglePeriodClear();
    void runBatchClearAllPeriods();
    void exportResults();
    void exportSettlementDetails();
    void saveToDatabase();
    void showValidationIssues();
    void onTimeSlotChanged(int timeSlot);
    void onModeChanged(int index);

private:
    void connectSignals();
    void runClearForSlot(int slotIndex);
    void refreshDetail(int slotIndex);
    void refreshBidOverview(int slotIndex);
    void refreshPriceCurve();
    // 当前时段的完整校验问题（用于「查看校验问题」按钮）。
    QStringList currentValidationMessages() const;

    SimulationRunner *runner_ = nullptr;

    QComboBox *modeCombo_ = nullptr;
    QSpinBox *periodSpinBox_ = nullptr;
    QPushButton *importGeneratorParametersButton_ = nullptr;
    QPushButton *importGeneratorBidsButton_ = nullptr;
    QPushButton *importConsumerParametersButton_ = nullptr;
    QPushButton *importConsumerBidsButton_ = nullptr;
    QPushButton *singleClearButton_ = nullptr;
    QPushButton *batchClearButton_ = nullptr;
    QPushButton *exportButton_ = nullptr;
    QPushButton *exportDetailButton_ = nullptr;
    QPushButton *saveDatabaseButton_ = nullptr;
    QPushButton *validationButton_ = nullptr;
    QChartView *chartView_ = nullptr;
    QChartView *priceCurveView_ = nullptr;
    // 可拖动的纵向分隔器：供需曲线与结果区之间可以直接拖高。
    QSplitter *mainSplitter_ = nullptr;
    QLabel *mcpLabel_ = nullptr;
    QLabel *totalVolumeLabel_ = nullptr;
    QTabWidget *resultsTabs_ = nullptr;
    QTableWidget *resultsTable_ = nullptr;
    QTableWidget *detailTable_ = nullptr;
    QTableWidget *bidOverviewTable_ = nullptr;

    bool syncingTimeSlot_ = false;
    std::vector<TimeSlotResult> lastResults_;
};

} // namespace pms
