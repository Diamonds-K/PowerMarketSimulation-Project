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

struct ConsumerSlotData {
    double fixedDemandMw = 100.0;
    std::vector<BidSegment> segments;
};

struct ConsumerUnitData {
    QString id = QStringLiteral("C1");
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
    bool importParametersFromCsv(const QString &filePath, QString *errorMessage = nullptr);
    bool importBidsFromCsv(const QString &filePath, QString *errorMessage = nullptr);

    bool addUnit();
    bool removeCurrentUnit();
    int unitCount() const;

signals:
    void timeSlotChanged(int displaySlot);

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
    QLineEdit *fixedDemandEdit_ = nullptr;
    QTableWidget *loadTable_ = nullptr;
    QPushButton *submitButton_ = nullptr;
    QLabel *totalCostLabel_ = nullptr;

    std::vector<ConsumerUnitData> units_;
    int currentUnitIndex_ = 0;
    int currentSlotIndex_ = 0;
    bool syncingUnit_ = false;
    bool syncingTimeSlot_ = false;
};

} // namespace pms
