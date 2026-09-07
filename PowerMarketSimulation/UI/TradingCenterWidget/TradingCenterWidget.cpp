#include "UI/TradingCenterWidget/TradingCenterWidget.h"

#include "Data/CSVReader/CSVReader.h"
#include "Data/CSVWriter/CSVWriter.h"
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
#include <QMessageBox>
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

std::vector<BidSegment> parseSegmentCsv(const QString &filePath,
                                        QString &errorMessage)
{
    std::vector<BidSegment> segments;
    std::vector<CSVReader::Row> rows;
    std::string readError;
    if (!CSVReader::read(filePath.toStdString(), rows, readError)) {
        errorMessage = QString::fromStdString(readError);
        return segments;
    }

    int segmentNo = 1;
    for (const CSVReader::Row &row : rows) {
        if (row.size() < 2) {
            continue;
        }

        bool powerOk = false;
        bool priceOk = false;
        const double power = QString::fromStdString(row[0]).toDouble(&powerOk);
        const double price = QString::fromStdString(row[1]).toDouble(&priceOk);
        if (!powerOk || !priceOk) {
            continue;
        }
        if (power <= 0.0 || price < 0.0) {
            continue;
        }

        segments.push_back(BidSegment(segmentNo++, power, price));
    }

    if (segments.empty()) {
        errorMessage = QStringLiteral("CSV 中没有读取到有效的“电量,价格”数据。");
    }
    return segments;
}

struct SlotSegments {
    int timeSlot = 0;
    std::vector<BidSegment> segments;
};

std::vector<SlotSegments> parseSlotCsv(const QString &filePath,
                                       QString &errorMessage)
{
    std::vector<SlotSegments> slotGroups;
    std::vector<CSVReader::Row> rows;
    std::string readError;
    if (!CSVReader::read(filePath.toStdString(), rows, readError)) {
        errorMessage = QString::fromStdString(readError);
        return slotGroups;
    }

    for (const CSVReader::Row &row : rows) {
        if (row.size() < 3) {
            continue;
        }

        bool slotOk = false;
        bool powerOk = false;
        bool priceOk = false;
        const int timeSlot = QString::fromStdString(row[0]).toInt(&slotOk);
        const double power = QString::fromStdString(row[1]).toDouble(&powerOk);
        const double price = QString::fromStdString(row[2]).toDouble(&priceOk);
        if (!slotOk || !powerOk || !priceOk) {
            continue;
        }
        if (timeSlot < 1 || timeSlot > 96 || power <= 0.0 || price < 0.0) {
            continue;
        }

        SlotSegments group;
        group.timeSlot = timeSlot;
        group.segments.push_back(BidSegment(1, power, price));
        slotGroups.push_back(group);
    }

    if (slotGroups.empty()) {
        errorMessage = QStringLiteral("CSV 中没有读取到有效的“时段,电量,价格”数据。");
    }
    return slotGroups;
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
                     bool sortAscendingByPrice,
                     double startPower = 0.0)
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

    double cumulativePower = startPower;
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

    auto *pminSeries = new QLineSeries;
    pminSeries->setName(QStringLiteral("Pmin 必发电量"));
    pminSeries->setPen(QPen(QColor(110, 110, 110), 2));
    pminSeries->append(0.0, 0.0);
    pminSeries->append(generator.pMinMw(), 0.0);

    auto *supplySeries = new QLineSeries;
    supplySeries->setName(QStringLiteral("供给曲线"));
    supplySeries->setPen(QPen(QColor(0, 150, 80), 2));
    supplySeries->append(generator.pMinMw(), 0.0);
    appendStepCurve(supplySeries, supplySegments, true, generator.pMinMw());

    auto *demandSeries = new QLineSeries;
    demandSeries->setName(QStringLiteral("需求曲线"));
    demandSeries->setPen(QPen(QColor(220, 50, 50), 2));
    appendStepCurve(demandSeries, demandSegments, false);

    auto *chart = new QChart;
    chart->setTitle(QStringLiteral("分段报价供需曲线"));
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

    const double maxX = std::max(generator.pMinMw() + totalQuantityMw(supplySegments),
                                 totalQuantityMw(demandSegments));
    const double maxY = std::max(maxPriceYuanPerMwh(supplySegments),
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

QChart *createQuadraticChart(const Generator &generator,
                             const Consumer &consumer,
                             double clearingPrice)
{
    const double a = generator.bidSheet().quadraticA();
    const double b = generator.bidSheet().quadraticB();
    const double pMin = generator.pMinMw();
    const double pMax = generator.pMaxMw();
    const double demandMw = consumer.fixedDemandMw();

    auto *marginalCostSeries = new QLineSeries;
    marginalCostSeries->setName(QStringLiteral("边际成本曲线"));
    marginalCostSeries->setPen(QPen(QColor(0, 110, 180), 2));

    constexpr int kSampleCount = 120;
    for (int i = 0; i <= kSampleCount; ++i) {
        const double power = pMin + (pMax - pMin) * static_cast<double>(i) / kSampleCount;
        const double marginalCost = 2.0 * a * power + b;
        marginalCostSeries->append(power, marginalCost);
    }

    const double maxPower = std::max(pMax, demandMw);
    auto *lambdaSeries = new QLineSeries;
    lambdaSeries->setName(QStringLiteral("统一出清价 λ"));
    lambdaSeries->setPen(QPen(QColor(220, 140, 30), 2, Qt::DashLine));
    lambdaSeries->append(0.0, clearingPrice);
    lambdaSeries->append(maxPower, clearingPrice);

    const double minPrice = std::min(2.0 * a * pMin + b, clearingPrice);
    const double maxPrice = std::max(2.0 * a * pMax + b, clearingPrice);
    auto *demandSeries = new QLineSeries;
    demandSeries->setName(QStringLiteral("需求 QD"));
    demandSeries->setPen(QPen(QColor(180, 40, 40), 2, Qt::DashLine));
    demandSeries->append(demandMw, minPrice);
    demandSeries->append(demandMw, maxPrice);

    auto *chart = new QChart;
    chart->setTitle(QStringLiteral("二次曲线模式：边际成本与统一出清价"));
    chart->legend()->setVisible(true);
    chart->addSeries(marginalCostSeries);
    chart->addSeries(lambdaSeries);
    chart->addSeries(demandSeries);

    auto *axisX = new QValueAxis;
    axisX->setTitleText(QStringLiteral("电量 (MW)"));
    axisX->setLabelFormat(QStringLiteral("%.0f"));
    axisX->setRange(0.0, maxPower * 1.05);

    auto *axisY = new QValueAxis;
    axisY->setTitleText(QStringLiteral("价格 (元/MWh)"));
    axisY->setLabelFormat(QStringLiteral("%.2f"));
    axisY->setRange(minPrice * 0.95, maxPrice * 1.05);

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    marginalCostSeries->attachAxis(axisX);
    marginalCostSeries->attachAxis(axisY);
    lambdaSeries->attachAxis(axisX);
    lambdaSeries->attachAxis(axisY);
    demandSeries->attachAxis(axisX);
    demandSeries->attachAxis(axisY);
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
    // 创建顶部数据准备区、时段控制区、中间图表区和底部结果操作区。
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
    chartView_->setMinimumHeight(320);
    rootLayout->addWidget(chartView_, 3);

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
    resultsTable_->setMinimumHeight(140);
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
    // 弹出文件选择框，读取 CSV 后写入发电侧当前/对应时段。
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("导入机组参数"),
        QString(),
        QStringLiteral("CSV 文件 (*.csv);;所有文件 (*)"));

    if (!filePath.isEmpty()) {
        QString errorMessage;
        const std::vector<SlotSegments> slotGroups = parseSlotCsv(filePath, errorMessage);
        if (!slotGroups.empty()) {
            for (const SlotSegments &group : slotGroups) {
                generatorWidget_->setSlotSegments(group.timeSlot - 1, group.segments);
            }
            QMessageBox::information(this,
                                     QStringLiteral("导入成功"),
                                     QStringLiteral("已导入 %1 个时段的发电报价。")
                                         .arg(slotGroups.size()));
            return;
        }

        errorMessage.clear();
        const std::vector<BidSegment> segments = parseSegmentCsv(filePath, errorMessage);
        if (!errorMessage.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("导入失败"), errorMessage);
            return;
        }

        generatorWidget_->setCurrentSlotSegments(segments);
        QMessageBox::information(this,
                                 QStringLiteral("导入成功"),
                                 QStringLiteral("已导入 %1 段发电报价到当前时段。")
                                     .arg(segments.size()));
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
        QString errorMessage;
        const std::vector<SlotSegments> slotGroups = parseSlotCsv(filePath, errorMessage);
        if (!slotGroups.empty()) {
            for (const SlotSegments &group : slotGroups) {
                consumerWidget_->setSlotSegments(group.timeSlot - 1, group.segments);
            }
            QMessageBox::information(this,
                                     QStringLiteral("导入成功"),
                                     QStringLiteral("已导入 %1 个时段的负荷报价。")
                                         .arg(slotGroups.size()));
            return;
        }

        errorMessage.clear();
        const std::vector<BidSegment> segments = parseSegmentCsv(filePath, errorMessage);
        if (!errorMessage.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("导入失败"), errorMessage);
            return;
        }

        consumerWidget_->setCurrentSlotSegments(segments);
        QMessageBox::information(this,
                                 QStringLiteral("导入成功"),
                                 QStringLiteral("已导入 %1 段负荷报价到当前时段。")
                                     .arg(segments.size()));
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

    // 从界面控件构建当前时段的模型输入，并调用交易中心出清。
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

    chartView_->setChart(quadratic ? createQuadraticChart(generator,
                                                          consumer,
                                                          result.clearingPriceYuanPerMwh())
                                   : createPiecewiseChart(generator, consumer));
}

void TradingCenterWidget::runBatchClearAllPeriods()
{
    if (!generatorWidget_ || !consumerWidget_) {
        return;
    }

    const bool quadratic = modeCombo_->currentIndex() == 1;
    const MarketMode mode = quadratic ? MarketMode::Quadratic : MarketMode::Piecewise;

    // 用 96 个时段的已保存数据分别构建输入。
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
    lastResults_ = results;

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
    // 把最近一次 96 时段仿真结果写成 CSV 文件。
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("导出最终出清结果"),
        QStringLiteral("clearing_result.csv"),
        QStringLiteral("CSV 文件 (*.csv);;所有文件 (*)"));

    if (!filePath.isEmpty()) {
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
}

} // namespace pms
