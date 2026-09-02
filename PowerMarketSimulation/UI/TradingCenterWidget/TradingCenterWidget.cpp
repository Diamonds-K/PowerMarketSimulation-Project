#include "UI/TradingCenterWidget/TradingCenterWidget.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>

#include "UI/ConsumerWidget/ConsumerWidget.h"
#include "UI/GeneratorWidget/GeneratorWidget.h"

namespace pms {

namespace {

constexpr int kTimeSlotCount = 96;

QString formatNumber(double value) {
    return QString::number(value, 'f', 4);
}

} // namespace

TradingCenterWidget::TradingCenterWidget(GeneratorWidget* generatorWidget,
                                         ConsumerWidget* consumerWidget,
                                         QWidget* parent)
    : QWidget(parent),
      generatorWidget_(generatorWidget),
      consumerWidget_(consumerWidget) {
    auto* layout = new QVBoxLayout(this);

    auto* controls = new QHBoxLayout;
    modeCombo_ = new QComboBox(this);
    modeCombo_->addItem(QStringLiteral("分段报价（双指针撮合）"));
    modeCombo_->addItem(QStringLiteral("二次曲线（KKT/λ 二分）"));
    runButton_ = new QPushButton(QStringLiteral("运行 96 时段仿真"), this);
    clearButton_ = new QPushButton(QStringLiteral("清空结果"), this);
    controls->addWidget(modeCombo_);
    controls->addWidget(runButton_);
    controls->addWidget(clearButton_);
    controls->addStretch();
    layout->addLayout(controls);

    resultsTable_ = new QTableWidget(0, 8, this);
    resultsTable_->setHorizontalHeaderLabels({
        QStringLiteral("时段"),
        QStringLiteral("出清价 (元/MWh)"),
        QStringLiteral("出清电量 (MW)"),
        QStringLiteral("缺额 (MW)"),
        QStringLiteral("总支付 (元)"),
        QStringLiteral("总收益 (元)"),
        QStringLiteral("平衡 (元)"),
        QStringLiteral("状态")
    });
    resultsTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(resultsTable_, 1);

    log_ = new QTextEdit(this);
    log_->setReadOnly(true);
    log_->setMaximumHeight(160);
    layout->addWidget(log_);

    connect(runButton_, &QPushButton::clicked, this, &TradingCenterWidget::runSimulation);
    connect(clearButton_, &QPushButton::clicked, this, &TradingCenterWidget::clearResults);
}

void TradingCenterWidget::runSimulation() {
    const bool quadratic = modeCombo_->currentIndex() == 1;
    const Generator generator = generatorWidget_->buildGenerator(quadratic);
    const Consumer consumer = consumerWidget_->buildConsumer(quadratic);

    std::vector<MarketInput> inputs;
    inputs.reserve(kTimeSlotCount);
    for (int slot = 0; slot < kTimeSlotCount; ++slot) {
        MarketInput input(slot,
                          quadratic ? MarketMode::Quadratic : MarketMode::Piecewise);
        input.addGenerator(generator);
        input.addConsumer(consumer);
        inputs.push_back(input);
    }

    const ValidationReport report = tradingCenter_.submit(inputs.front());
    const auto messages = report.messages();
    if (!report.ok()) {
        for (const auto& message : messages) {
            appendLog(QStringLiteral("校验失败: ") + QString::fromStdString(message));
        }
        QMessageBox::warning(this,
                             QStringLiteral("校验失败"),
                             QString::fromStdString(messages.empty() ? "输入不合法" : messages.front()));
        return;
    }
    appendLog(QStringLiteral("校验通过，开始 96 时段仿真..."));

    const std::vector<TimeSlotResult> results = tradingCenter_.runDayAheadSimulation(inputs);
    resultsTable_->setRowCount(kTimeSlotCount);

    for (int slot = 0; slot < kTimeSlotCount; ++slot) {
        const TimeSlotResult& item = results[slot];
        const MarketResult& market = item.market;
        const SettlementResult& settlement = item.settlement;

        resultsTable_->setItem(slot, 0, new QTableWidgetItem(QString::number(slot)));
        resultsTable_->setItem(slot, 1,
            new QTableWidgetItem(formatNumber(market.clearingPriceYuanPerMwh())));
        resultsTable_->setItem(slot, 2,
            new QTableWidgetItem(formatNumber(market.clearingVolumeMw())));
        resultsTable_->setItem(slot, 3,
            new QTableWidgetItem(formatNumber(market.shortageMw())));
        resultsTable_->setItem(slot, 4,
            new QTableWidgetItem(formatNumber(settlement.totalPaymentYuan())));
        resultsTable_->setItem(slot, 5,
            new QTableWidgetItem(formatNumber(settlement.totalRevenueYuan())));
        resultsTable_->setItem(slot, 6,
            new QTableWidgetItem(formatNumber(settlement.balanceYuan())));
        resultsTable_->setItem(slot, 7,
            new QTableWidgetItem(market.feasible()
                                     ? QStringLiteral("成交")
                                     : QStringLiteral("不可行")));
    }

    appendLog(QStringLiteral("仿真完成：")
              + QString::number(kTimeSlotCount) + QStringLiteral(" 个时段已出清。"));
}

void TradingCenterWidget::clearResults() {
    resultsTable_->setRowCount(0);
    log_->clear();
}

void TradingCenterWidget::appendLog(const QString& message) {
    log_->append(message);
}

} // namespace pms
