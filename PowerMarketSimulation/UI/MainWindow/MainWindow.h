#pragma once

#include <QMainWindow>

class QTabWidget;

namespace pms {

class GeneratorWidget;
class ConsumerWidget;
class TradingCenterWidget;

// 应用主窗口，负责组织发电、用户和交易中心三个功能页。
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    // 创建主窗口和各功能页面。
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    // 将任一处切换的时段同步到其他页面。
    void syncTimeSlot(int displaySlot);

private:
    QTabWidget* tabs_ = nullptr;
    GeneratorWidget* generatorWidget_ = nullptr;
    ConsumerWidget* consumerWidget_ = nullptr;
    TradingCenterWidget* tradingCenterWidget_ = nullptr;
};

} // namespace pms
