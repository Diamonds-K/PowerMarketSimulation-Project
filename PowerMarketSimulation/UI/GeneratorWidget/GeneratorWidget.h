#pragma once

#include <array>
#include <string>
#include <vector>

#include <QString>
#include <QWidget>

#include "Model/BidSegment/BidSegment.h"
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
    bool quadraticMode = false;
    double quadraticA = 0.05;
    double quadraticB = 10.0;
    double quadraticC = 0.0;
    std::vector<BidSegment> segments;
};

struct GeneratorUnitData {
    QString id = QStringLiteral("G1");
    double pMinMw = 20.0;
    double pMaxMw = 100.0;
    std::array<GeneratorSlotData, 96> timeSlots;
};

class GeneratorWidget : public QWidget {
    Q_OBJECT

public:
    explicit GeneratorWidget(QWidget *parent = nullptr);

    Generator buildGenerator(bool quadratic) const;
    std::vector<Generator> buildGenerators(bool quadratic) const;
    std::vector<Generator> buildGeneratorsForSlot(int slotIndex, bool quadratic) const;

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
    QWidget *createLadderPage();
    QWidget *createQuadraticPage();
    void connectSignals();
    void saveCurrentToSlot(int unitIndex, int slotIndex);
    void saveCurrent();
    void loadUnit(int unitIndex);
    void loadSlot(int slotIndex);
    void clearSegmentTable();
    Generator buildFromData(const GeneratorUnitData &unit,
                            int slotIndex,
                            bool quadratic) const;
    QString cellText(int row, int column) const;
    bool idExists(const QString &id, int exceptIndex = -1) const;
    QString nextDefaultUnitId() const;

    QComboBox *unitCombo_ = nullptr;
    QPushButton *addUnitButton_ = nullptr;
    QPushButton *removeUnitButton_ = nullptr;
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

    std::vector<GeneratorUnitData> units_;
    int currentUnitIndex_ = 0;
    int currentSlotIndex_ = 0;
    bool syncingUnit_ = false;
    bool syncingTimeSlot_ = false;
};

} // namespace pms
