#pragma once

#include <QWidget>

#include "Model/Generator/Generator.h"

class QLineEdit;
class QPushButton;
class QTableWidget;

namespace pms {

class GeneratorWidget : public QWidget {
    Q_OBJECT

public:
    explicit GeneratorWidget(QWidget* parent = nullptr);

    Generator buildGenerator(bool quadratic) const;
    void clearForm();

private slots:
    void addSegmentRow();
    void removeSelectedSegmentRow();

private:
    QString cellText(int row, int column) const;

    QLineEdit* idEdit_ = nullptr;
    QLineEdit* pMinEdit_ = nullptr;
    QLineEdit* pMaxEdit_ = nullptr;
    QLineEdit* quadraticAEdit_ = nullptr;
    QLineEdit* quadraticBEdit_ = nullptr;
    QLineEdit* quadraticCEdit_ = nullptr;
    QTableWidget* segmentsTable_ = nullptr;
};

} // namespace pms
