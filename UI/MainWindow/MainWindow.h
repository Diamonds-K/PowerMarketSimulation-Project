#pragma once

#include <memory>

#include <QMainWindow>

class QTabWidget;

namespace pms {

class GeneratorWidget;
class ConsumerWidget;
class TradingCenterWidget;
class SimulationRunner;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void syncTimeSlot(int displaySlot);

private:
    QTabWidget* tabs_ = nullptr;
    GeneratorWidget* generatorWidget_ = nullptr;
    ConsumerWidget* consumerWidget_ = nullptr;
    TradingCenterWidget* tradingCenterWidget_ = nullptr;
    // UI 层共用的编排助手：三个页面都通过它出清，保证结果口径一致。
    std::unique_ptr<SimulationRunner> runner_;
};

} // namespace pms
