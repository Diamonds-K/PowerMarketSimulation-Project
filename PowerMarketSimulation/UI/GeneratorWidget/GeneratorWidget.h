#pragma once

#include <QWidget>

#include "Model/Generator/Generator.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QStackedWidget;
class QTableWidget;

namespace pms {

class GeneratorWidget : public QWidget {
    Q_OBJECT

public:
    explicit GeneratorWidget(QWidget *parent = nullptr);

    Generator buildGenerator(bool quadratic) const;

private:
    QWidget *createLadderPage();
    QWidget *createQuadraticPage();
    void connectSignals();
    QString cellText(int row, int column) const;

    QComboBox *unitCombo_ = nullptr;
    QSpinBox *timeSlotSpinBox_ = nullptr;
    QRadioButton *ladderModeRadio_ = nullptr;
    QRadioButton *quadraticModeRadio_ = nullptr;
    QStackedWidget *modeStack_ = nullptr;
    QTableWidget *ladderTable_ = nullptr;
    QLineEdit *pMinEdit_ = nullptr;
    QLineEdit *pMaxEdit_ = nullptr;
    QLineEdit *aEdit_ = nullptr;
    QLineEdit *bEdit_ = nullptr;
    QLineEdit *cEdit_ = nullptr;
    QPushButton *submitButton_ = nullptr;
    QLabel *outputLabel_ = nullptr;
    QLabel *revenueLabel_ = nullptr;
};

} // namespace pms
