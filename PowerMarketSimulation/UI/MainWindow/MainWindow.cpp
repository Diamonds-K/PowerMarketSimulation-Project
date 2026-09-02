#include "UI/MainWindow/MainWindow.h"

#include <QTabWidget>

#include "UI/ConsumerWidget/ConsumerWidget.h"
#include "UI/GeneratorWidget/GeneratorWidget.h"
#include "UI/TradingCenterWidget/TradingCenterWidget.h"

namespace pms {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("电力现货市场出清仿真平台"));

    tabs_ = new QTabWidget(this);
    generatorWidget_ = new GeneratorWidget(this);
    consumerWidget_ = new ConsumerWidget(this);
    tradingCenterWidget_ = new TradingCenterWidget(generatorWidget_, consumerWidget_, this);

    tabs_->addTab(generatorWidget_, QStringLiteral("发电侧报价"));
    tabs_->addTab(consumerWidget_, QStringLiteral("用户侧需求"));
    tabs_->addTab(tradingCenterWidget_, QStringLiteral("交易中心"));

    setCentralWidget(tabs_);
    resize(1100, 720);
}

} // namespace pms
