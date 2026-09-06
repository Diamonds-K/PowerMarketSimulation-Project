#pragma once

#include <array>
#include <vector>

#include <QString>
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

struct GeneratorSlotData {
    QString id = QStringLiteral("G1");
    double pMinMw = 20.0;
    double pMaxMw = 100.0;
    bool quadraticMode = false;
    double quadraticA = 0.05;
    double quadraticB = 10.0;
    double quadraticC = 0.0;
    std::vector<BidSegment> segments;
};

class GeneratorWidget : public QWidget {
    Q_OBJECT

public:
    explicit GeneratorWidget(QWidget *parent = nullptr);

    Generator buildGenerator(bool quadratic) const;
    Generator buildGeneratorForSlot(int slotIndex, bool quadratic) const;
    void setTimeSlot(int displaySlot);
    void setCurrentSlotSegments(const std::vector<BidSegment> &segments);

signals:
    void timeSlotChanged(int displaySlot);

private slots:
    void submitCurrentSlot();
    void onTimeSlotChanged(int displaySlot);

private:
    QWidget *createLadderPage();
    QWidget *createQuadraticPage();
    void connectSignals();
    void saveCurrentToSlot(int slotIndex);
    void loadSlot(int slotIndex);
    Generator buildFromData(const GeneratorSlotData &data, bool quadratic) const;
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

    std::array<GeneratorSlotData, 96> slotData_;
    int currentSlotIndex_ = 0;
    bool syncingTimeSlot_ = false;
};

} // namespace pms
