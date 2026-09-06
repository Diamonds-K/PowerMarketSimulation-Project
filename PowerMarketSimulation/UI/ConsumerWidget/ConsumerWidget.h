#pragma once

#include <QWidget>

#include "Model/Consumer/Consumer.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace pms {

class ConsumerWidget : public QWidget {
    Q_OBJECT

public:
    explicit ConsumerWidget(QWidget *parent = nullptr);

    Consumer buildConsumer(bool quadratic) const;

private:
    QString cellText(int row, int column) const;

    QComboBox *userCombo_ = nullptr;
    QSpinBox *timeSlotSpinBox_ = nullptr;
    QLineEdit *fixedDemandEdit_ = nullptr;
    QTableWidget *loadTable_ = nullptr;
    QPushButton *submitButton_ = nullptr;
    QLabel *totalCostLabel_ = nullptr;
};

} // namespace pms
