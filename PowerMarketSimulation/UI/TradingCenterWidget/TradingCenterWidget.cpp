#include "UI/TradingCenterWidget/TradingCenterWidget.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <string>
#include <vector>

#include <QAbstractItemView>
#include <QChart>
#include <QChartView>
#include <QColor>
#include <QComboBox>
#include <QDebug>
#include <QFileDialog>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineSeries>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QValueAxis>
#include <QVBoxLayout>

#include "Data/CSVWriter/CSVWriter.h"
#include "UI/ConsumerWidget/ConsumerWidget.h"
#include "UI/GeneratorWidget/GeneratorWidget.h"

namespace pms {

namespace {

QString formatNumber(double value, int precision = 2)
{
    return QString::number(value, 'f', precision);
}

void appendStepCurve(QLineSeries *series,
                     std::vector<BidSegment> segments,
                     bool sortAscendingByPrice,
                     double startPower = 0.0)
{
    if (sortAscendingByPrice) {
        std::sort(segments.begin(), segments.end(),
                  [](const BidSegment &lhs, const BidSegment &rhs) {
                      return lhs.priceYuanPerMwh() < rhs.priceYuanPerMwh();
                  });
    } else {
        std::sort(segments.begin(), segments.end(),
                  [](const BidSegment &lhs, const BidSegment &rhs) {
                      return lhs.priceYuanPerMwh() > rhs.priceYuanPerMwh();
                  });
    }

    double cumulativePower = startPower;
    for (const BidSegment &segment : segments) {
        series->append(cumulativePower, segment.priceYuanPerMwh());
        cumulativePower += segment.quantityMw();
        series->append(cumulativePower, segment.priceYuanPerMwh());
    }
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

std::vector<BidSegment> collectSegments(const std::vector<Generator> &generators,
                                        const std::vector<Consumer> &consumers)
{
    std::vector<BidSegment> segments;
    for (const Generator &generator : generators) {
        const auto &source = generator.bidSheet().segments();
        segments.insert(segments.end(), source.begin(), source.end());
    }
    for (const Consumer &consumer : consumers) {
        const auto &source = consumer.bidSheet().segments();
        segments.insert(segments.end(), source.begin(), source.end());
    }
    return segments;
}

std::vector<BidSegment> collectSupplySegments(const std::vector<Generator> &generators)
{
    std::vector<BidSegment> segments;
    for (const Generator &generator : generators) {
        const auto &source = generator.bidSheet().segments();
        segments.insert(segments.end(), source.begin(), source.end());
    }
    return segments;
}

std::vector<BidSegment> collectDemandSegments(const std::vector<Consumer> &consumers)
{
    std::vector<BidSegment> segments;
    for (const Consumer &consumer : consumers) {
        const auto &source = consumer.bidSheet().segments();
        segments.insert(segments.end(), source.begin(), source.end());
    }
    return segments;
}

QChart *createPiecewiseChart(const std::vector<Generator> &generators,
                             const std::vector<Consumer> &consumers)
{
    const std::vector<BidSegment> supplyIncrements = collectSupplySegments(generators);
    const std::vector<BidSegment> demandSegments = collectDemandSegments(consumers);
    double sumPMin = 0.0;
    for (const Generator &generator : generators) {
        sumPMin += generator.pMinMw();
    }

    auto *pminSeries = new QLineSeries;
    pminSeries->setName(QStringLiteral("Pmin 必发电量"));
    pminSeries->setPen(QPen(QColor(110, 110, 110), 2));
    pminSeries->append(0.0, 0.0);
    pminSeries->append(sumPMin, 0.0);

    auto *supplySeries = new QLineSeries;
    supplySeries->setName(QStringLiteral("供给曲线"));
    supplySeries->setPen(QPen(QColor(0, 150, 80), 2));
    supplySeries->append(sumPMin, 0.0);
    appendStepCurve(supplySeries, supplyIncrements, true, sumPMin);

    auto *demandSeries = new QLineSeries;
    demandSeries->setName(QStringLiteral("需求曲线"));
    demandSeries->setPen(QPen(QColor(220, 50, 50), 2));
    appendStepCurve(demandSeries, demandSegments, false);

    auto *chart = new QChart;
    chart->setTitle(QStringLiteral("多主体分段报价供需曲线"));
    chart->legend()->setVisible(true);
    chart->addSeries(pminSeries);
    chart->addSeries(supplySeries);
    chart->addSeries(demandSeries);

    auto *axisX = new QValueAxis;
    axisX->setTitleText(QStringLiteral("电量 (MW)"));
    axisX->setLabelFormat(QStringLiteral("%.0f"));

    auto *axisY = new QValueAxis;
    axisY->setTitleText(QStringLiteral("价格 (元/MWh)"));
    axisY->setLabelFormat(QStringLiteral("%.2f"));

    const double maxX = sumPMin + totalQuantityMw(supplyIncrements);
    const double maxY = std::max(maxPriceYuanPerMwh(supplyIncrements),
                                 maxPriceYuanPerMwh(demandSegments));
    axisX->setRange(0.0, maxX > 0.0 ? maxX * 1.05 : 1.0);
    axisY->setRange(0.0, maxY > 0.0 ? maxY * 1.1 : 1.0);

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    pminSeries->attachAxis(axisX);
    pminSeries->attachAxis(axisY);
    supplySeries->attachAxis(axisX);
    supplySeries->attachAxis(axisY);
    demandSeries->attachAxis(axisX);
    demandSeries->attachAxis(axisY);
    return chart;
}

QChart *createQuadraticChart(const std::vector<Generator> &generators,
                             const std::vector<Consumer> &consumers,
                             double clearingPrice)
{
    auto *chart = new QChart;
    chart->setTitle(QStringLiteral("二次曲线模式：边际成本与统一出清价"));
    chart->legend()->setVisible(true);

    double maxPower = 0.0;
    double minPrice = 0.0;
    double maxPrice = 0.0;
    bool haveUnit = false;
    std::vector<QLineSeries *> chartSeries;

    for (const Generator &generator : generators) {
        const double a = generator.bidSheet().quadraticA();
        const double b = generator.bidSheet().quadraticB();
        const double pMin = generator.pMinMw();
        const double pMax = generator.pMaxMw();

        auto *series = new QLineSeries;
        series->setName(QString::fromStdString(generator.id()));
        series->setPen(QPen(QColor(0, 110, 180), 2));
        constexpr int kSampleCount = 120;
        for (int i = 0; i <= kSampleCount; ++i) {
            const double power = pMin + (pMax - pMin) * static_cast<double>(i) / kSampleCount;
            const double marginalCost = 2.0 * a * power + b;
            series->append(power, marginalCost);
            if (!haveUnit) {
                minPrice = marginalCost;
                maxPrice = marginalCost;
                haveUnit = true;
            } else {
                minPrice = std::min(minPrice, marginalCost);
                maxPrice = std::max(maxPrice, marginalCost);
            }
        }
        chart->addSeries(series);
        chartSeries.push_back(series);
        maxPower = std::max(maxPower, pMax);
    }

    double demandMw = 0.0;
    for (const Consumer &consumer : consumers) {
        demandMw += consumer.fixedDemandMw();
    }
    maxPower = std::max(maxPower, demandMw);

    auto *lambdaSeries = new QLineSeries;
    lambdaSeries->setName(QStringLiteral("统一出清价 λ"));
    lambdaSeries->setPen(QPen(QColor(220, 140, 30), 2, Qt::DashLine));
    lambdaSeries->append(0.0, clearingPrice);
    lambdaSeries->append(maxPower, clearingPrice);

    auto *demandSeries = new QLineSeries;
    demandSeries->setName(QStringLiteral("需求 QD"));
    demandSeries->setPen(QPen(QColor(180, 40, 40), 2, Qt::DashLine));
    demandSeries->append(demandMw, minPrice);
    demandSeries->append(demandMw, maxPrice);

    chart->addSeries(lambdaSeries);
    chart->addSeries(demandSeries);
    chartSeries.push_back(lambdaSeries);
    chartSeries.push_back(demandSeries);

    auto *axisX = new QValueAxis;
    axisX->setTitleText(QStringLiteral("电量 (MW)"));
    axisX->setLabelFormat(QStringLiteral("%.0f"));
    axisX->setRange(0.0, maxPower > 0.0 ? maxPower * 1.05 : 1.0);

    auto *axisY = new QValueAxis;
    axisY->setTitleText(QStringLiteral("价格 (元/MWh)"));
    axisY->setLabelFormat(QStringLiteral("%.2f"));
    const double yLow = std::min(minPrice, clearingPrice);
    const double yHigh = std::max(maxPrice, clearingPrice);
    axisY->setRange(yLow > 0.0 ? yLow * 0.95 : -1.0,
                    yHigh > 0.0 ? yHigh * 1.05 : 1.0);

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    for (QLineSeries *series : chartSeries) {
        series->attachAxis(axisX);
        series->attachAxis(axisY);
    }
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
    auto *dataLayout = new QGridLayout(dataGroup);
    importGeneratorParametersButton_ =
        new QPushButton(QStringLiteral("发电参数(CSV)"), dataGroup);
    importGeneratorBidsButton_ =
        new QPushButton(QStringLiteral("发电报价(CSV)"), dataGroup);
    importConsumerParametersButton_ =
        new QPushButton(QStringLiteral("用户参数(CSV)"), dataGroup);
    importConsumerBidsButton_ =
        new QPushButton(QStringLiteral("用户报价(CSV)"), dataGroup);
    dataLayout->addWidget(importGeneratorParametersButton_, 0, 0);
    dataLayout->addWidget(importGeneratorBidsButton_, 0, 1);
    dataLayout->addWidget(importConsumerParametersButton_, 0, 2);
    dataLayout->addWidget(importConsumerBidsButton_, 0, 3);

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
    chartView_->setMinimumHeight(260);
    rootLayout->addWidget(chartView_, 3);

    auto *resultsGroup = new QGroupBox(QStringLiteral("出清结果"), this);
    auto *resultsLayout = new QVBoxLayout(resultsGroup);
    resultsTabs_ = new QTabWidget(resultsGroup);

    resultsTable_ = new QTableWidget(0, 8, resultsTabs_);
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
    resultsTable_->setMinimumHeight(140);

    detailTable_ = new QTableWidget(0, 9, resultsTabs_);
    detailTable_->setHorizontalHeaderLabels({
        QStringLiteral("时段"),
        QStringLiteral("类型"),
        QStringLiteral("主体"),
        QStringLiteral("出力/成交 (MW)"),
        QStringLiteral("边际成本 (元/MWh)"),
        QStringLiteral("收益 (元)"),
        QStringLiteral("成本 (元)"),
        QStringLiteral("利润 (元)"),
        QStringLiteral("支付 (元)")
    });
    detailTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    detailTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    detailTable_->setMinimumHeight(140);

    resultsTabs_->addTab(resultsTable_, QStringLiteral("96时段汇总"));
    resultsTabs_->addTab(detailTable_, QStringLiteral("当前时段主体明细"));
    resultsLayout->addWidget(resultsTabs_);
    rootLayout->addWidget(resultsGroup, 2);

    auto *resultGroup = new QGroupBox(QStringLiteral("当前出清结果"), this);
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
    connect(importGeneratorParametersButton_, &QPushButton::clicked,
            this, &TradingCenterWidget::importGeneratorParameters);
    connect(importGeneratorBidsButton_, &QPushButton::clicked,
            this, &TradingCenterWidget::importGeneratorBids);
    connect(importConsumerParametersButton_, &QPushButton::clicked,
            this, &TradingCenterWidget::importConsumerParameters);
    connect(importConsumerBidsButton_, &QPushButton::clicked,
            this, &TradingCenterWidget::importConsumerBids);
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
        QStringLiteral("导入发电参数"),
        QString(),
        QStringLiteral("CSV 文件 (*.csv);;所有文件 (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (generatorWidget_->importParametersFromCsv(filePath, &errorMessage)) {
        QMessageBox::information(this, QStringLiteral("导入成功"),
                                 QStringLiteral("发电参数已替换为 CSV 中的机组。"));
    } else {
        QMessageBox::warning(this, QStringLiteral("导入失败"), errorMessage);
    }
}

void TradingCenterWidget::importGeneratorBids()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("导入发电报价"),
        QString(),
        QStringLiteral("CSV 文件 (*.csv);;所有文件 (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (generatorWidget_->importBidsFromCsv(filePath, &errorMessage)) {
        QMessageBox::information(this, QStringLiteral("导入成功"),
                                 QStringLiteral("发电报价已导入。"));
    } else {
        QMessageBox::warning(this, QStringLiteral("导入失败"), errorMessage);
    }
}

void TradingCenterWidget::importConsumerParameters()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("导入用户参数"),
        QString(),
        QStringLiteral("CSV 文件 (*.csv);;所有文件 (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (consumerWidget_->importParametersFromCsv(filePath, &errorMessage)) {
        QMessageBox::information(this, QStringLiteral("导入成功"),
                                 QStringLiteral("用户参数已替换为 CSV 中的用户。"));
    } else {
        QMessageBox::warning(this, QStringLiteral("导入失败"), errorMessage);
    }
}

void TradingCenterWidget::importConsumerBids()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("导入用户报价"),
        QString(),
        QStringLiteral("CSV 文件 (*.csv);;所有文件 (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (consumerWidget_->importBidsFromCsv(filePath, &errorMessage)) {
        QMessageBox::information(this, QStringLiteral("导入成功"),
                                 QStringLiteral("用户报价已导入。"));
    } else {
        QMessageBox::warning(this, QStringLiteral("导入失败"), errorMessage);
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

    generatorWidget_->flushCurrentSlot();
    consumerWidget_->flushCurrentSlot();

    const bool quadratic = modeCombo_->currentIndex() == 1;
    const MarketMode mode = quadratic ? MarketMode::Quadratic : MarketMode::Piecewise;
    const std::vector<Generator> generators =
        generatorWidget_->buildGeneratorsForSlot(slotIndex, quadratic);
    const std::vector<Consumer> consumers =
        consumerWidget_->buildConsumersForSlot(slotIndex, quadratic);

    MarketInput input(slotIndex, mode);
    for (Generator generator : generators) {
        generator.bidSheet().setTimeSlot(slotIndex);
        input.addGenerator(generator);
    }
    for (Consumer consumer : consumers) {
        consumer.bidSheet().setTimeSlot(slotIndex);
        input.addConsumer(consumer);
    }

    const MarketResult result = tradingCenter_.clear(input);
    mcpLabel_->setText(QStringLiteral("统一出清电价 (MCP): %1 元/MWh")
                           .arg(result.clearingPriceYuanPerMwh(), 0, 'f', 2));
    totalVolumeLabel_->setText(QStringLiteral("总出清电量: %1 MW")
                                   .arg(result.clearingVolumeMw(), 0, 'f', 2));

    if (lastResults_.size() != 96) {
        lastResults_.resize(96);
    }
    TimeSlotResult &slotResult = lastResults_[static_cast<std::size_t>(slotIndex)];
    slotResult.input = input;
    slotResult.market = result;
    if (result.feasible()) {
        slotResult.settlement = tradingCenter_.settle(input, result);
    }

    chartView_->setChart(quadratic ? createQuadraticChart(generators,
                                                          consumers,
                                                          result.clearingPriceYuanPerMwh())
                                   : createPiecewiseChart(generators, consumers));
    refreshDetail(slotIndex);
}

void TradingCenterWidget::runBatchClearAllPeriods()
{
    if (!generatorWidget_ || !consumerWidget_) {
        return;
    }

    generatorWidget_->flushCurrentSlot();
    consumerWidget_->flushCurrentSlot();

    const bool quadratic = modeCombo_->currentIndex() == 1;
    const MarketMode mode = quadratic ? MarketMode::Quadratic : MarketMode::Piecewise;

    std::vector<MarketInput> inputs;
    inputs.reserve(96);
    for (int slot = 0; slot < 96; ++slot) {
        const std::vector<Generator> generators =
            generatorWidget_->buildGeneratorsForSlot(slot, quadratic);
        const std::vector<Consumer> consumers =
            consumerWidget_->buildConsumersForSlot(slot, quadratic);

        MarketInput input(slot, mode);
        for (Generator generator : generators) {
            generator.bidSheet().setTimeSlot(slot);
            input.addGenerator(generator);
        }
        for (Consumer consumer : consumers) {
            consumer.bidSheet().setTimeSlot(slot);
            input.addConsumer(consumer);
        }
        inputs.push_back(input);
    }

    lastResults_ = tradingCenter_.runDayAheadSimulation(inputs);

    resultsTable_->setRowCount(96);
    for (int row = 0; row < 96; ++row) {
        const TimeSlotResult &item = lastResults_[static_cast<std::size_t>(row)];
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

    refreshDetail(periodSpinBox_->value() - 1);
    qDebug() << "Batch clearing finished:" << lastResults_.size() << "slots.";
}

void TradingCenterWidget::refreshDetail(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= 96 ||
        lastResults_.size() != 96 ||
        lastResults_[static_cast<std::size_t>(slotIndex)].market.timeSlot() != slotIndex) {
        detailTable_->setRowCount(0);
        return;
    }

    const TimeSlotResult &item = lastResults_[static_cast<std::size_t>(slotIndex)];
    const MarketResult &market = item.market;
    const SettlementResult &settlement = item.settlement;

    if (!market.feasible()) {
        detailTable_->setRowCount(1);
        detailTable_->setItem(0, 0, new QTableWidgetItem(QString::number(slotIndex + 1)));
        detailTable_->setItem(0, 1, new QTableWidgetItem(QStringLiteral("不可行")));
        detailTable_->setItem(0, 2,
            new QTableWidgetItem(QString::fromStdString(market.message())));
        return;
    }

    const auto &generators = settlement.generatorSettlements();
    const auto &consumers = settlement.consumerSettlements();
    detailTable_->setRowCount(static_cast<int>(generators.size() + consumers.size()));

    int row = 0;
    for (std::size_t i = 0; i < generators.size(); ++i) {
        const GeneratorSettlement &generator = generators[i];
        detailTable_->setItem(row, 0, new QTableWidgetItem(QString::number(slotIndex + 1)));
        detailTable_->setItem(row, 1, new QTableWidgetItem(QStringLiteral("发电")));
        detailTable_->setItem(row, 2,
            new QTableWidgetItem(QString::fromStdString(generator.generatorId)));
        detailTable_->setItem(row, 3,
            new QTableWidgetItem(formatNumber(generator.outputMw, 3)));
        const double marginalCost =
            i < market.generatorResults().size()
                ? market.generatorResults()[i].marginalCostYuanPerMwh
                : 0.0;
        detailTable_->setItem(row, 4,
            new QTableWidgetItem(marginalCost > 0.0 ? formatNumber(marginalCost, 3)
                                                    : QString()));
        detailTable_->setItem(row, 5,
            new QTableWidgetItem(formatNumber(generator.revenueYuan, 3)));
        detailTable_->setItem(row, 6,
            new QTableWidgetItem(formatNumber(generator.costYuan, 3)));
        detailTable_->setItem(row, 7,
            new QTableWidgetItem(formatNumber(generator.profitYuan, 3)));
        detailTable_->setItem(row, 8, new QTableWidgetItem(QString()));
        ++row;
    }

    for (const ConsumerSettlement &consumer : consumers) {
        detailTable_->setItem(row, 0, new QTableWidgetItem(QString::number(slotIndex + 1)));
        detailTable_->setItem(row, 1, new QTableWidgetItem(QStringLiteral("用户")));
        detailTable_->setItem(row, 2,
            new QTableWidgetItem(QString::fromStdString(consumer.consumerId)));
        detailTable_->setItem(row, 3,
            new QTableWidgetItem(formatNumber(consumer.clearedDemandMw, 3)));
        detailTable_->setItem(row, 4, new QTableWidgetItem(QString()));
        detailTable_->setItem(row, 5, new QTableWidgetItem(QString()));
        detailTable_->setItem(row, 6, new QTableWidgetItem(QString()));
        detailTable_->setItem(row, 7, new QTableWidgetItem(QString()));
        detailTable_->setItem(row, 8,
            new QTableWidgetItem(formatNumber(consumer.paymentYuan, 3)));
        ++row;
    }
}

void TradingCenterWidget::exportResults()
{
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("导出最终出清结果"),
        QStringLiteral("clearing_result.csv"),
        QStringLiteral("CSV 文件 (*.csv);;所有文件 (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    std::vector<CSVWriter::Row> rows;
    rows.push_back({
        "time_slot",
        "clearing_price_yuan_per_mwh",
        "clearing_volume_mw",
        "shortage_mw",
        "total_payment_yuan",
        "total_revenue_yuan",
        "balance_yuan",
        "status"
    });

    for (const TimeSlotResult &item : lastResults_) {
        const MarketResult &market = item.market;
        const SettlementResult &settlement = item.settlement;
        rows.push_back({
            std::to_string(market.timeSlot() + 1),
            formatNumber(market.clearingPriceYuanPerMwh(), 4).toStdString(),
            formatNumber(market.clearingVolumeMw(), 4).toStdString(),
            formatNumber(market.shortageMw(), 4).toStdString(),
            formatNumber(settlement.totalPaymentYuan(), 4).toStdString(),
            formatNumber(settlement.totalRevenueYuan(), 4).toStdString(),
            formatNumber(settlement.balanceYuan(), 6).toStdString(),
            market.feasible() ? "feasible" : "infeasible"
        });
    }

    std::string errorMessage;
    if (!CSVWriter::write(filePath.toStdString(), rows, errorMessage)) {
        QMessageBox::warning(this,
                             QStringLiteral("导出失败"),
                             QString::fromStdString(errorMessage));
        return;
    }

    QMessageBox::information(this,
                             QStringLiteral("导出成功"),
                             QStringLiteral("已导出 %1 个时段的出清结果。")
                                 .arg(lastResults_.size()));
}

} // namespace pms
