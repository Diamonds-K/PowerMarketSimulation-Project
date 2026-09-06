#pragma once

#include <array>
#include <vector>

#include <QString>
#include <QWidget>

#include "Model/Consumer/Consumer.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace pms {

struct ConsumerSlotData {
    QString id = QStringLiteral("C1");
    double fixedDemandMw = 100.0;
    bool quadraticMode = false;
    std::vector<BidSegment> segments;
};

class ConsumerWidget : public QWidget {
    Q_OBJECT

public:
    explicit ConsumerWidget(QWidget *parent = nullptr);

    Consumer buildConsumer(bool quadratic) const;
    Consumer buildConsumerForSlot(int slotIndex, bool quadratic) const;
    void setTimeSlot(int displaySlot);

signals:
    void timeSlotChanged(int displaySlot);

private slots:
    void submitCurrentSlot();
    void onTimeSlotChanged(int displaySlot);

private:
    void connectSignals();
    void saveCurrentToSlot(int slotIndex);
    void loadSlot(int slotIndex);
    Consumer buildFromData(const ConsumerSlotData &data, bool quadratic) const;
    QString cellText(int row, int column) const;

    QComboBox *userCombo_ = nullptr;
    QSpinBox *timeSlotSpinBox_ = nullptr;
    QLineEdit *fixedDemandEdit_ = nullptr;
    QTableWidget *loadTable_ = nullptr;
    QPushButton *submitButton_ = nullptr;
    QLabel *totalCostLabel_ = nullptr;

    std::array<ConsumerSlotData, 96> slotData_;
    int currentSlotIndex_ = 0;
    bool syncingTimeSlot_ = false;
};

} // namespace pms
