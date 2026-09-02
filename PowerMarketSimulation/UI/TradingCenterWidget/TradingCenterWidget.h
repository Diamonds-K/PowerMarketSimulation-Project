#pragma once

#include <QWidget>

#include "TradingCenter/TradingCenter.h"

class QComboBox;
class QPushButton;
class QTableWidget;
class QTextEdit;

namespace pms {

class ConsumerWidget;
class GeneratorWidget;

class TradingCenterWidget : public QWidget {
    Q_OBJECT

public:
    explicit TradingCenterWidget(GeneratorWidget* generatorWidget,
                                 ConsumerWidget* consumerWidget,
                                 QWidget* parent = nullptr);

private slots:
    void runSimulation();
    void clearResults();

private:
    void appendLog(const QString& message);

    GeneratorWidget* generatorWidget_ = nullptr;
    ConsumerWidget* consumerWidget_ = nullptr;
    QComboBox* modeCombo_ = nullptr;
    QPushButton* runButton_ = nullptr;
    QPushButton* clearButton_ = nullptr;
    QTableWidget* resultsTable_ = nullptr;
    QTextEdit* log_ = nullptr;
    TradingCenter tradingCenter_;
};

} // namespace pms
