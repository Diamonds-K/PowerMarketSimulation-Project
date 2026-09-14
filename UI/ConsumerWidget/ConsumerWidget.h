#pragma once

#include <array>
#include <string>
#include <vector>

#include <QString>
#include <QWidget>

#include "Model/BidSegment/BidSegment.h"
#include "Model/Consumer/Consumer.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace pms {

class SimulationRunner;

struct ConsumerSlotData {
    double fixedDemandMw = 100.0;
    std::vector<BidSegment> segments;
};

struct ConsumerUnitData {
    QString id = QStringLiteral("C1");
    QString name;
    std::array<ConsumerSlotData, 96> timeSlots;
};

class ConsumerWidget : public QWidget {
    Q_OBJECT

public:
    explicit ConsumerWidget(QWidget *parent = nullptr);

    Consumer buildConsumer(bool quadratic) const;
    std::vector<Consumer> buildConsumers(bool quadratic) const;
    std::vector<Consumer> buildConsumersForSlot(int slotIndex, bool quadratic) const;

    void setTimeSlot(int displaySlot);
    void flushCurrentSlot();
    // 注入 UI 编排助手后，用户侧才能看到自己的成交电量与账单。
    void setRunner(SimulationRunner *runner);
    bool importParametersFromCsv(const QString &filePath, QString *errorMessage = nullptr);
    bool importBidsFromCsv(const QString &filePath, QString *errorMessage = nullptr);

    bool addUnit();
    bool removeCurrentUnit();
    int unitCount() const;
    QString unitId(int index) const;
    QString unitName(int index) const;

signals:
    void timeSlotChanged(int displaySlot);

public slots:
    // 按当前时段重新出清并刷新本用户的成交电量、缺额与电费。
    void refreshBill();

private slots:
    void submitCurrentSlot();
    void onTimeSlotChanged(int displaySlot);
    void onUnitIndexChanged(int index);
    void addUnitClicked();
    void removeUnitClicked();

private:
    void connectSignals();
    void saveCurrentToSlot(int unitIndex, int slotIndex);
    void saveCurrent();
    void loadUnit(int unitIndex);
    void loadSlot(int slotIndex);
    void clearSegmentTable();
    Consumer buildFromData(const ConsumerUnitData &unit,
                           int slotIndex,
                           bool quadratic) const;
    QString cellText(int row, int column) const;
    bool idExists(const QString &id, int exceptIndex = -1) const;
    QString nextDefaultUnitId() const;

    QComboBox *userCombo_ = nullptr;
    QPushButton *addUnitButton_ = nullptr;
    QPushButton *removeUnitButton_ = nullptr;
    QSpinBox *timeSlotSpinBox_ = nullptr;
    QLineEdit *nameEdit_ = nullptr;
    QLineEdit *fixedDemandEdit_ = nullptr;
    QTableWidget *loadTable_ = nullptr;
    QPushButton *submitButton_ = nullptr;
    QLabel *clearingPriceLabel_ = nullptr;
    QLabel *declaredLabel_ = nullptr;
    QLabel *clearedLabel_ = nullptr;
    QLabel *shortageLabel_ = nullptr;
    QLabel *totalCostLabel_ = nullptr;
    QLabel *billStatusLabel_ = nullptr;

    std::vector<ConsumerUnitData> units_;
    SimulationRunner *runner_ = nullptr;
    int currentUnitIndex_ = 0;
    int currentSlotIndex_ = 0;
    bool syncingUnit_ = false;
    bool syncingTimeSlot_ = false;
};

} // namespace pms
