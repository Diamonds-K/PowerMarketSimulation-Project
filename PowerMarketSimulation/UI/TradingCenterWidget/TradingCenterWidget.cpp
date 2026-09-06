#include "UI/TradingCenterWidget/TradingCenterWidget.h"

#include "UI/ConsumerWidget/ConsumerWidget.h"
#include "UI/GeneratorWidget/GeneratorWidget.h"

#include <QAbstractItemView>
#include <QChart>
#include <QChartView>
#include <QColor>
#include <QComboBox>
#include <QDebug>
#include <QFileDialog>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineSeries>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QValueAxis>
#include <QVBoxLayout>

#include <algorithm>
#include <numeric>
#include <vector>

namespace pms {
namespace {

QString formatNumber(double value, int precision = 2)
{
    return QString::number(value, 'f', precision);
}

double totalQuantityMw(const std::vector<BidSegment> &segments)
{
    return std::accumulate(segments.begin(), segments.end(), 0.0,
                           [](double sum, const BidSegment &segment) {
                               return sum + segment.quantityMw();
                           });
}

double maxPriceYuanPerMwh(const std::vector<BidSegment> &segments)
{
    if (segments.empty()) {
        return 0.0;
    }

    const auto it = std::max_element(segments.begin(), segments.end(),
                                     [](const BidSegment &lhs, const BidSegment &rhs) {
                                         return lhs.priceYuanPerMwh() < rhs.priceYuanPerMwh();
                                     });
    return it->priceYuanPerMwh();
}

void appendStepCurve(QLineSeries *series,
                     const std::vector<BidSegment> &segments,
                     bool sortAscendingByPrice)
{
    std::vector<BidSegment> sortedSegments = segments;
    if (sortAscendingByPrice) {
        std::sort(sortedSegments.begin(), sortedSegments.end(),
                  [](const BidSegment &lhs, const BidSegment &rhs) {
                      return lhs.priceYuanPerMwh() < rhs.priceYuanPerMwh();
                  });
    } else {
        std::sort(sortedSegments.begin(), sortedSegments.end(),
                  [](const BidSegment &lhs, const BidSegment &rhs) {
                      return lhs.priceYuanPerMwh() > rhs.priceYuanPerMwh();
                  });
    }

    double cumulativePower = 0.0;
    for (const BidSegment &segment : sortedSegments) {
        series->append(cumulativePower, segment.priceYuanPerMwh());
        cumulativePower += segment.quantityMw();
        series->append(cumulativePower, segment.priceYuanPerMwh());
    }
}

QChart *createPiecewiseChart(const Generator &generator, const Consumer &consumer)
{
    const std::vector<BidSegment> supplySegments = generator.bidSheet().segments();
    const std::vector<BidSegment> demandSegments = consumer.bidSheet().segments();

    auto *supplySeries = new QLineSeries;
    supplySeries->setName(QStringLiteral("供给曲线"));
    supplySeries->setPen(QPen(QColor(0, 150, 80), 2));
    appendStepCurve(supplySeries, supplySegments, true);

    auto *demandSeries = new QLineSeries;
    demandSeries->setName(QStringLiteral("需求曲线"));
    demandSeries->setPen(QPen(QColor(220, 50, 50), 2));
    appendStepCurve(demandSeries, demandSegments, false);

    auto *chart = new QChart;
    chart->setTitle(QStringLiteral("分段报价供需曲线"));
    chart->legend()->setVisible(true);
    chart->addSeries(supplySeries);
    chart->addSeries(demandSeries);

    auto *axisX = new QValueAxis;
    axisX->setTitleText(QStringLiteral("电量 (MW)"));
    axisX->setLabelFormat(QStringLiteral("%.0f"));

    auto *axisY = new QValueAxis;
    axisY->setTitleText(QStringLiteral("价格 (元/MWh)"));
    axisY->setLabelFormat(QStringLiteral("%.2f"));

    const double maxX = std::max(totalQuantityMw(supplySegments),
                                 totalQuantityMw(demandSegments));
    const double maxY = std::max(maxPriceYuanPerMwh(supplySegments),
                                 maxPriceYuanPerMwh(demandSegments));
    axisX->setRange(0.0, maxX > 0.0 ? maxX * 1.05 : 1.0);
    axisY->setRange(0.0, maxY > 0.0 ? maxY * 1.1 : 1.0);

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    supplySeries->attachAxis(axisX);
    supplySeries->attachAxis(axisY);
    demandSeries->attachAxis(axisX);
    demandSeries->attachAxis(axisY);

    return chart;
}

QChart *createQuadraticChart()
{
    auto *chart = new QChart;
    chart->setTitle(QStringLiteral("二次曲线模式：边际成本曲线待扩展"));

    auto *axisX = new QValueAxis;
    axisX->setTitleText(QStringLiteral("电量 (MW)"));
    axisX->setRange(0.0, 1.0);

    auto *axisY = new QValueAxis;
    axisY->setTitleText(QStringLiteral("价格 (元/MWh)"));
    axisY->setRange(0.0, 1.0);

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    return chart;
}

} // namespace

TradingCenterWidget::TradingCenterWidget(GeneratorWidget *generatorWidget,
                                         ConsumerWidget *consumerWidget,
                                         QWidget *parent)
    : QWidget(parent),
      generatorWidget_(generatorWidget),
      consumerWidget_(consumerWidget)
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(10);

    auto *dataGroup = new QGroupBox(QStringLiteral("数据准备"), this);
    auto *dataLayout = new QHBoxLayout(dataGroup);
    importGeneratorButton_ = new QPushButton(QStringLiteral("导入机组参数(CSV)"), dataGroup);
    importLoadButton_ = new QPushButton(QStringLiteral("导入负荷数据(CSV)"), dataGroup);
    dataLayout->addWidget(importGeneratorButton_);
    dataLayout->addWidget(importLoadButton_);
    dataLayout->addStretch();

    auto *timeGroup = new QGroupBox(QStringLiteral("时段控制"), this);
    auto *timeLayout = new QHBoxLayout(timeGroup);
    timeLayout->addWidget(new QLabel(QStringLiteral("当前时段:"), timeGroup));

    periodSpinBox_ = new QSpinBox(timeGroup);
    periodSpinBox_->setRange(1, 96);
    periodSpinBox_->setValue(1);
    periodSpinBox_->setSuffix(QStringLiteral(" 时段"));
    timeLayout->addWidget(periodSpinBox_);

    modeCombo_ = new QComboBox(timeGroup);
    modeCombo_->addItem(QStringLiteral("分段报价"));
    modeCombo_->addItem(QStringLiteral("二次曲线"));
    timeLayout->addWidget(new QLabel(QStringLiteral("报价模式:"), timeGroup));
    timeLayout->addWidget(modeCombo_);
    timeLayout->addStretch();

    auto *topLayout = new QHBoxLayout;
    topLayout->addWidget(dataGroup, 1);
    topLayout->addWidget(timeGroup);
    rootLayout->addLayout(topLayout);

    auto *chart = new QChart;
    chart->setTitle(QStringLiteral("供需曲线"));
    chartView_ = new QChartView(chart, this);
    chartView_->setRenderHint(QPainter::Antialiasing);
    chartView_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    rootLayout->addWidget(chartView_, 1);

    auto *resultsGroup = new QGroupBox(QStringLiteral("96 时段出清结果"), this);
    auto *resultsLayout = new QVBoxLayout(resultsGroup);
    resultsTable_ = new QTableWidget(0, 8, resultsGroup);
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
    resultsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resultsTable_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    resultsLayout->addWidget(resultsTable_);
    rootLayout->addWidget(resultsGroup, 2);

    auto *resultGroup = new QGroupBox(QStringLiteral("出清结果"), this);
    auto *resultLayout = new QVBoxLayout(resultGroup);
    mcpLabel_ = new QLabel(QStringLiteral("统一出清电价 (MCP): 0.00 元/MWh"), resultGroup);
    totalVolumeLabel_ = new QLabel(QStringLiteral("总出清电量: 0.00 MW"), resultGroup);

    auto applyBoldFont = [](QLabel *label) {
        QFont boldFont = label->font();
        boldFont.setBold(true);
        boldFont.setPointSize(11);
        label->setFont(boldFont);
    };
    applyBoldFont(mcpLabel_);
    applyBoldFont(totalVolumeLabel_);
    resultLayout->addWidget(mcpLabel_);
    resultLayout->addWidget(totalVolumeLabel_);

    auto *operationGroup = new QGroupBox(QStringLiteral("执行操作"), this);
    auto *operationLayout = new QHBoxLayout(operationGroup);
    singleClearButton_ = new QPushButton(QStringLiteral("当前时段单次出清"), operationGroup);
    batchClearButton_ = new QPushButton(QStringLiteral("连续出清全天96时段"), operationGroup);
    exportButton_ = new QPushButton(QStringLiteral("导出最终出清结果"), operationGroup);
    operationLayout->addWidget(singleClearButton_);
    operationLayout->addWidget(batchClearButton_);
    operationLayout->addWidget(exportButton_);

    auto *bottomLayout = new QHBoxLayout;
    bottomLayout->addWidget(resultGroup, 1);
    bottomLayout->addWidget(operationGroup, 2);
    rootLayout->addLayout(bottomLayout);

    connectSignals();
}

void TradingCenterWidget::connectSignals()
{
    connect(importGeneratorButton_, &QPushButton::clicked,
            this, &TradingCenterWidget::importGeneratorParameters);
    connect(importLoadButton_, &QPushButton::clicked,
            this, &TradingCenterWidget::importLoadData);
    connect(singleClearButton_, &QPushButton::clicked,
            this, &TradingCenterWidget::runSinglePeriodClear);
    connect(batchClearButton_, &QPushButton::clicked,
            this, &TradingCenterWidget::runBatchClearAllPeriods);
    connect(exportButton_, &QPushButton::clicked,
            this, &TradingCenterWidget::exportResults);
    connect(periodSpinBox_, &QSpinBox::valueChanged,
            this, &TradingCenterWidget::onTimeSlotChanged);
}

void TradingCenterWidget::importGeneratorParameters()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("导入机组参数"),
        QString(),
        QStringLiteral("CSV 文件 (*.csv);;所有文件 (*)"));

    if (!filePath.isEmpty()) {
        qDebug() << "Selected generator parameter CSV:" << filePath;
    }
}

void TradingCenterWidget::importLoadData()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("导入负荷数据"),
        QString(),
        QStringLiteral("CSV 文件 (*.csv);;所有文件 (*)"));

    if (!filePath.isEmpty()) {
        qDebug() << "Selected load data CSV:" << filePath;
    }
}

void TradingCenterWidget::runSinglePeriodClear()
{
    runClearForSlot(periodSpinBox_->value() - 1);
}

void TradingCenterWidget::setTimeSlot(int displaySlot)
{
    if (displaySlot < 1 || displaySlot > 96) {
        return;
    }
    if (periodSpinBox_->value() == displaySlot) {
        return;
    }

    syncingTimeSlot_ = true;
    periodSpinBox_->setValue(displaySlot);
    syncingTimeSlot_ = false;
}

void TradingCenterWidget::onTimeSlotChanged(int timeSlot)
{
    runClearForSlot(timeSlot - 1);

    if (!syncingTimeSlot_) {
        emit timeSlotChanged(timeSlot);
    }
}

void TradingCenterWidget::runClearForSlot(int slotIndex)
{
    if (!generatorWidget_ || !consumerWidget_) {
        return;
    }

    const bool quadratic = modeCombo_->currentIndex() == 1;
    Generator generator = generatorWidget_->buildGenerator(quadratic);
    Consumer consumer = consumerWidget_->buildConsumer(quadratic);
    generator.bidSheet().setTimeSlot(slotIndex);
    consumer.bidSheet().setTimeSlot(slotIndex);

    MarketInput input(slotIndex,
                      quadratic ? MarketMode::Quadratic : MarketMode::Piecewise);
    input.addGenerator(generator);
    input.addConsumer(consumer);

    const MarketResult result = tradingCenter_.clear(input);
    mcpLabel_->setText(QStringLiteral("统一出清电价 (MCP): %1 元/MWh")
                           .arg(result.clearingPriceYuanPerMwh(), 0, 'f', 2));
    totalVolumeLabel_->setText(QStringLiteral("总出清电量: %1 MW")
                                   .arg(result.clearingVolumeMw(), 0, 'f', 2));

    if (!result.feasible()) {
        qDebug() << "Clearing not feasible:" << QString::fromStdString(result.message());
    }

    chartView_->setChart(quadratic ? createQuadraticChart()
                                   : createPiecewiseChart(generator, consumer));
}

void TradingCenterWidget::runBatchClearAllPeriods()
{
    if (!generatorWidget_ || !consumerWidget_) {
        return;
    }

    const bool quadratic = modeCombo_->currentIndex() == 1;
    const MarketMode mode = quadratic ? MarketMode::Quadratic : MarketMode::Piecewise;

    std::vector<MarketInput> inputs;
    inputs.reserve(96);
    for (int slot = 0; slot < 96; ++slot) {
        Generator generator = generatorWidget_->buildGeneratorForSlot(slot, quadratic);
        Consumer consumer = consumerWidget_->buildConsumerForSlot(slot, quadratic);
        generator.bidSheet().setTimeSlot(slot);
        consumer.bidSheet().setTimeSlot(slot);

        MarketInput input(slot, mode);
        input.addGenerator(generator);
        input.addConsumer(consumer);
        inputs.push_back(input);
    }

    const std::vector<TimeSlotResult> results = tradingCenter_.runDayAheadSimulation(inputs);

    resultsTable_->setRowCount(static_cast<int>(results.size()));
    for (int row = 0; row < static_cast<int>(results.size()); ++row) {
        const TimeSlotResult &item = results[static_cast<std::size_t>(row)];
        const MarketResult &market = item.market;
        const SettlementResult &settlement = item.settlement;

        resultsTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
        resultsTable_->setItem(row, 1,
            new QTableWidgetItem(formatNumber(market.clearingPriceYuanPerMwh(), 2)));
        resultsTable_->setItem(row, 2,
            new QTableWidgetItem(formatNumber(market.clearingVolumeMw(), 2)));
        resultsTable_->setItem(row, 3,
            new QTableWidgetItem(formatNumber(market.shortageMw(), 2)));
        resultsTable_->setItem(row, 4,
            new QTableWidgetItem(formatNumber(settlement.totalPaymentYuan(), 2)));
        resultsTable_->setItem(row, 5,
            new QTableWidgetItem(formatNumber(settlement.totalRevenueYuan(), 2)));
        resultsTable_->setItem(row, 6,
            new QTableWidgetItem(formatNumber(settlement.balanceYuan(), 4)));
        const QString status = market.feasible()
                                   ? QStringLiteral("成交")
                                   : QString::fromStdString(market.message());
        resultsTable_->setItem(row, 7, new QTableWidgetItem(status));
    }

    qDebug() << "Batch clearing finished:" << results.size() << "slots.";
}

void TradingCenterWidget::exportResults()
{
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("导出最终出清结果"),
        QStringLiteral("clearing_result.csv"),
        QStringLiteral("CSV 文件 (*.csv);;所有文件 (*)"));

    if (!filePath.isEmpty()) {
        qDebug() << "Selected clearing result export path:" << filePath;
    }
}

} // namespace pms
