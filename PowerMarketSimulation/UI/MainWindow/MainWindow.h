#pragma once

#include <QMainWindow>

class QTabWidget;

namespace pms {

class GeneratorWidget;
class ConsumerWidget;
class TradingCenterWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    QTabWidget* tabs_ = nullptr;
    GeneratorWidget* generatorWidget_ = nullptr;
    ConsumerWidget* consumerWidget_ = nullptr;
    TradingCenterWidget* tradingCenterWidget_ = nullptr;
};

} // namespace pms
