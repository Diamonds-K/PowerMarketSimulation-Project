#include "UI/MainWindow/MainWindow.h"

#include <QTabWidget>

#include "UI/ConsumerWidget/ConsumerWidget.h"
#include "UI/GeneratorWidget/GeneratorWidget.h"
#include "UI/SimulationRunner/SimulationRunner.h"
#include "UI/TradingCenterWidget/TradingCenterWidget.h"

namespace pms {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("电力现货市场出清仿真平台"));

    tabs_ = new QTabWidget(this);
    generatorWidget_ = new GeneratorWidget(this);
    consumerWidget_ = new ConsumerWidget(this);
    runner_ = std::make_unique<SimulationRunner>(generatorWidget_, consumerWidget_);
    tradingCenterWidget_ = new TradingCenterWidget(runner_.get(), this);

    // 三个页面共用同一个出清编排助手：发电侧看收入、用户侧看账单、交易中心看全场。
    generatorWidget_->setRunner(runner_.get());
    consumerWidget_->setRunner(runner_.get());

    tabs_->addTab(generatorWidget_, QStringLiteral("发电侧报价"));
    tabs_->addTab(consumerWidget_, QStringLiteral("用户侧需求"));
    tabs_->addTab(tradingCenterWidget_, QStringLiteral("交易中心"));

    setCentralWidget(tabs_);
    // 交易中心页有两张图（供需曲线 + 96 点电价曲线），默认窗口给足竖向空间；
    // 页内还有可拖动的分隔条，需要时能进一步把图拉高。
    resize(1360, 940);

    connect(generatorWidget_, &GeneratorWidget::timeSlotChanged,
            this, &MainWindow::syncTimeSlot);
    connect(consumerWidget_, &ConsumerWidget::timeSlotChanged,
            this, &MainWindow::syncTimeSlot);
    connect(tradingCenterWidget_, &TradingCenterWidget::timeSlotChanged,
            this, &MainWindow::syncTimeSlot);
    connect(tradingCenterWidget_, &TradingCenterWidget::modeChanged,
            this, [this](bool quadratic) {
                runner_->setQuadraticMode(quadratic);
                generatorWidget_->refreshClearingResult();
                consumerWidget_->refreshBill();
            });
    connect(tradingCenterWidget_, &TradingCenterWidget::clearingFinished,
            this, [this](int) {
                generatorWidget_->refreshClearingResult();
                consumerWidget_->refreshBill();
            });
}

MainWindow::~MainWindow() = default;

void MainWindow::syncTimeSlot(int displaySlot)
{
    generatorWidget_->setTimeSlot(displaySlot);
    consumerWidget_->setTimeSlot(displaySlot);
    tradingCenterWidget_->setTimeSlot(displaySlot);
}

} // namespace pms
