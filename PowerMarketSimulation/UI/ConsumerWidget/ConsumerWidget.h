#pragma once

#include <QWidget>

#include "Model/Consumer/Consumer.h"

class QLineEdit;
class QPushButton;
class QTableWidget;

namespace pms {

class ConsumerWidget : public QWidget {
    Q_OBJECT

public:
    explicit ConsumerWidget(QWidget* parent = nullptr);

    Consumer buildConsumer(bool quadratic) const;
    void clearForm();

private slots:
    void addSegmentRow();
    void removeSelectedSegmentRow();

private:
    QString cellText(int row, int column) const;

    QLineEdit* idEdit_ = nullptr;
    QLineEdit* fixedDemandEdit_ = nullptr;
    QTableWidget* segmentsTable_ = nullptr;
};

} // namespace pms
