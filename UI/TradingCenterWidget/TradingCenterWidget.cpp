#include "UI/TradingCenterWidget/TradingCenterWidget.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <string>
#include <vector>

#include <QAbstractItemView>
#include <QChart>
#include <QChartView>
#include <QColor>
#include <QComboBox>
#include <QFileDialog>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QGraphicsLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineSeries>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QScatterSeries>
#include <QSplitter>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QValueAxis>
#include <QVBoxLayout>

#include "Data/CSVWriter/CSVWriter.h"
#include "Data/SQLiteManager/SQLiteManager.h"
#include "UI/ConsumerWidget/ConsumerWidget.h"
#include "UI/GeneratorWidget/GeneratorWidget.h"
#include "UI/SimulationRunner/SimulationRunner.h"

namespace pms {

namespace {

QString formatNumber(double value, int precision = 2)
{
    return QString::number(value, 'f', precision);
}

// Qt Charts 默认留白偏大，宽屏下图表容易显得扁；收紧边距把高度让给绘图区。
void tightenChart(QChart *chart)
{
    chart->setMargins(QMargins(4, 4, 4, 4));
    chart->setBackgroundRoundness(0);
    if (chart->layout() != nullptr) {
        chart->layout()->setContentsMargins(0, 0, 0, 0);
    }
}

// 机组申报上限：分段模式为 Pmin + 各段增量之和，二次模式为 Pmax。
double generatorDeclaredMw(const Generator &generator, MarketMode mode)
{
    if (mode == MarketMode::Quadratic) {
        return generator.pMaxMw();
    }
    return generator.pMinMw() + generator.bidSheet().totalIncrementMw();
}

void showImportOutcome(QWidget *parent,
                       const QString &successTitle,
                       const QString &successMessage,
                       const QString &warningMessage)
{
    if (warningMessage.isEmpty()) {
        QMessageBox::information(parent, successTitle, successMessage);
    } else {
        QMessageBox::warning(parent, QStringLiteral("部分导入"), warningMessage);
    }
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

// 分段模式供需曲线：Pmin 必发电量 + 供给阶梯 + 需求阶梯 + 出清点。
QChart *createPiecewiseChart(const std::vector<Generator> &generators,
                             const std::vector<Consumer> &consumers,
                             const MarketResult &result)
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
    tightenChart(chart);
    chart->addSeries(pminSeries);
    chart->addSeries(supplySeries);
    chart->addSeries(demandSeries);

    const double maxX = sumPMin + totalQuantityMw(supplyIncrements);
    const double maxY = std::max(maxPriceYuanPerMwh(supplyIncrements),
                                 maxPriceYuanPerMwh(demandSegments));

    // 出清点：水平线为统一出清价，竖直线为出清电量，交点为边际成交位置。
    QLineSeries *priceLine = nullptr;
    QLineSeries *volumeLine = nullptr;
    if (result.feasible()) {
        priceLine = new QLineSeries;
        priceLine->setName(QStringLiteral("统一出清价"));
        priceLine->setPen(QPen(QColor(230, 130, 20), 2, Qt::DashLine));
        priceLine->append(0.0, result.clearingPriceYuanPerMwh());
        priceLine->append(maxX > 0.0 ? maxX : 1.0, result.clearingPriceYuanPerMwh());

        volumeLine = new QLineSeries;
        volumeLine->setName(QStringLiteral("出清电量"));
        volumeLine->setPen(QPen(QColor(90, 90, 200), 2, Qt::DashLine));
        volumeLine->append(result.clearingVolumeMw(), 0.0);
        volumeLine->append(result.clearingVolumeMw(), maxY > 0.0 ? maxY : 1.0);

        chart->addSeries(priceLine);
        chart->addSeries(volumeLine);
    }

    auto *axisX = new QValueAxis;
    axisX->setTitleText(QStringLiteral("电量 (MW)"));
    axisX->setLabelFormat(QStringLiteral("%.0f"));
    axisX->setRange(0.0, maxX > 0.0 ? maxX * 1.05 : 1.0);

    auto *axisY = new QValueAxis;
    axisY->setTitleText(QStringLiteral("价格 (元/MWh)"));
    axisY->setLabelFormat(QStringLiteral("%.2f"));
    axisY->setRange(0.0, maxY > 0.0 ? maxY * 1.1 : 1.0);

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    pminSeries->attachAxis(axisX);
    pminSeries->attachAxis(axisY);
    supplySeries->attachAxis(axisX);
    supplySeries->attachAxis(axisY);
    demandSeries->attachAxis(axisX);
    demandSeries->attachAxis(axisY);
    if (priceLine != nullptr) {
        priceLine->attachAxis(axisX);
        priceLine->attachAxis(axisY);
    }
    if (volumeLine != nullptr) {
        volumeLine->attachAxis(axisX);
        volumeLine->attachAxis(axisY);
    }
    return chart;
}

QChart *createQuadraticChart(const std::vector<Generator> &generators,
                             const std::vector<Consumer> &consumers,
                             const MarketResult &result)
{
    auto *chart = new QChart;
    if (result.feasible()) {
        chart->setTitle(QStringLiteral("二次曲线模式：边际成本、聚合供给与统一出清价"));
    } else {
        chart->setTitle(QStringLiteral("二次曲线模式：不可行 - ") +
                        QString::fromStdString(result.message()));
    }
    chart->legend()->setVisible(true);
    tightenChart(chart);

    double clearingPrice = result.clearingPriceYuanPerMwh();
    double sumPMax = 0.0;
    double minPrice = 0.0;
    double maxPrice = 0.0;
    bool haveUnit = false;
    std::vector<QLineSeries *> chartSeries;
    std::vector<double> unitA;
    std::vector<double> unitB;
    std::vector<double> unitPMin;
    std::vector<double> unitPMax;

    for (const Generator &generator : generators) {
        const double a = generator.bidSheet().quadraticA();
        const double b = generator.bidSheet().quadraticB();
        const double pMin = generator.pMinMw();
        const double pMax = generator.pMaxMw();
        unitA.push_back(a);
        unitB.push_back(b);
        unitPMin.push_back(pMin);
        unitPMax.push_back(pMax);
        sumPMax += pMax;

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
    }

    double demandMw = 0.0;
    for (const Consumer &consumer : consumers) {
        demandMw += consumer.fixedDemandMw();
    }

    if (haveUnit) {
        auto *aggregateSeries = new QLineSeries;
        aggregateSeries->setName(QStringLiteral("聚合供给"));
        aggregateSeries->setPen(QPen(QColor(0, 140, 70), 2));
        constexpr int kPriceSampleCount = 240;
        for (int i = 0; i <= kPriceSampleCount; ++i) {
            const double price =
                minPrice + (maxPrice - minPrice) * static_cast<double>(i) / kPriceSampleCount;
            double totalPower = 0.0;
            for (std::size_t j = 0; j < unitA.size(); ++j) {
                const double power = (price - unitB[j]) / (2.0 * unitA[j]);
                totalPower += std::clamp(power, unitPMin[j], unitPMax[j]);
            }
            aggregateSeries->append(totalPower, price);
        }
        chart->addSeries(aggregateSeries);
        chartSeries.push_back(aggregateSeries);
    }

    const double maxPower = std::max(sumPMax, demandMw);

    auto *demandSeries = new QLineSeries;
    demandSeries->setName(QStringLiteral("需求 QD"));
    demandSeries->setPen(QPen(QColor(180, 40, 40), 2, Qt::DashLine));
    demandSeries->append(demandMw, minPrice);
    demandSeries->append(demandMw, maxPrice);
    chart->addSeries(demandSeries);
    chartSeries.push_back(demandSeries);

    QLineSeries *lambdaSeries = nullptr;
    if (result.feasible()) {
        lambdaSeries = new QLineSeries;
        lambdaSeries->setName(QStringLiteral("统一出清价 λ"));
        lambdaSeries->setPen(QPen(QColor(220, 140, 30), 2, Qt::DashLine));
        lambdaSeries->append(0.0, clearingPrice);
        lambdaSeries->append(maxPower > 0.0 ? maxPower : 1.0, clearingPrice);
        chart->addSeries(lambdaSeries);
        chartSeries.push_back(lambdaSeries);
    }

    auto *dispatchSeries = new QScatterSeries;
    dispatchSeries->setName(QStringLiteral("实际出力点"));
    dispatchSeries->setColor(QColor(255, 80, 0));
    dispatchSeries->setMarkerSize(9.0);
    if (result.feasible() &&
        result.generatorResults().size() == generators.size()) {
        for (std::size_t i = 0; i < generators.size(); ++i) {
            const double output = result.generatorResults()[i].outputMw;
            const double marginalCost = 2.0 * unitA[i] * output + unitB[i];
            dispatchSeries->append(output, marginalCost);
        }
    }
    chart->addSeries(dispatchSeries);

    auto *axisX = new QValueAxis;
    axisX->setTitleText(QStringLiteral("电量 (MW)"));
    axisX->setLabelFormat(QStringLiteral("%.0f"));
    axisX->setRange(0.0, maxPower > 0.0 ? maxPower * 1.05 : 1.0);

    auto *axisY = new QValueAxis;
    axisY->setTitleText(QStringLiteral("价格 (元/MWh)"));
    axisY->setLabelFormat(QStringLiteral("%.2f"));
    double yLow = minPrice;
    double yHigh = maxPrice;
    if (result.feasible()) {
        yLow = std::min(yLow, clearingPrice);
        yHigh = std::max(yHigh, clearingPrice);
    }
    axisY->setRange(yLow > 0.0 ? yLow * 0.95 : -1.0,
                    yHigh > 0.0 ? yHigh * 1.05 : 1.0);

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    for (QLineSeries *series : chartSeries) {
        series->attachAxis(axisX);
        series->attachAxis(axisY);
    }
    dispatchSeries->attachAxis(axisX);
    dispatchSeries->attachAxis(axisY);
    return chart;
}

// 96 点日前市场电价曲线（附出清电量，便于观察量价关系）。
QChart *createPriceCurveChart(const std::vector<TimeSlotResult> &results)
{
    auto *chart = new QChart;
    auto *priceSeries = new QLineSeries;
    priceSeries->setName(QStringLiteral("出清电价 (元/MWh)"));
    priceSeries->setPen(QPen(QColor(200, 60, 40), 2));
    auto *volumeSeries = new QLineSeries;
    volumeSeries->setName(QStringLiteral("出清电量 (MW)"));
    volumeSeries->setPen(QPen(QColor(0, 120, 200), 2));

    double maxPrice = 1.0;
    double maxVolume = 1.0;
    int plotted = 0;
    int infeasible = 0;
    for (std::size_t i = 0; i < results.size(); ++i) {
        const MarketResult &market = results[i].market;
        // 未出清的时段（timeSlot 不匹配）不画点，也不计入不可行。
        if (market.timeSlot() != static_cast<int>(i)) {
            continue;
        }
        if (!market.feasible()) {
            ++infeasible;
            continue;
        }
        const double slot = static_cast<double>(i + 1);
        priceSeries->append(slot, market.clearingPriceYuanPerMwh());
        volumeSeries->append(slot, market.clearingVolumeMw());
        maxPrice = std::max(maxPrice, market.clearingPriceYuanPerMwh());
        maxVolume = std::max(maxVolume, market.clearingVolumeMw());
        ++plotted;
    }

    if (plotted == 0) {
        chart->setTitle(QStringLiteral("96 点日前市场电价曲线（请先运行全天出清）"));
    } else if (plotted + infeasible < 96) {
        chart->setTitle(QStringLiteral("96 点日前市场电价与出清电量曲线（已完成 %1/96 个时段）")
                            .arg(plotted + infeasible));
    } else if (infeasible > 0) {
        chart->setTitle(QStringLiteral("96 点日前市场电价与出清电量曲线（%1 个时段不可行未绘制）")
                            .arg(infeasible));
    } else {
        chart->setTitle(QStringLiteral("96 点日前市场电价与出清电量曲线"));
    }
    chart->legend()->setVisible(true);
    tightenChart(chart);
    chart->addSeries(priceSeries);
    chart->addSeries(volumeSeries);

    auto *axisX = new QValueAxis;
    axisX->setTitleText(QStringLiteral("时段"));
    axisX->setLabelFormat(QStringLiteral("%.0f"));
    axisX->setRange(1.0, 96.0);
    axisX->setTickCount(13);

    auto *axisPrice = new QValueAxis;
    axisPrice->setTitleText(QStringLiteral("出清电价 (元/MWh)"));
    axisPrice->setLabelFormat(QStringLiteral("%.1f"));
    axisPrice->setRange(0.0, maxPrice * 1.1);

    auto *axisVolume = new QValueAxis;
    axisVolume->setTitleText(QStringLiteral("出清电量 (MW)"));
    axisVolume->setLabelFormat(QStringLiteral("%.0f"));
    axisVolume->setRange(0.0, maxVolume * 1.1);

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisPrice, Qt::AlignLeft);
    chart->addAxis(axisVolume, Qt::AlignRight);
    priceSeries->attachAxis(axisX);
    priceSeries->attachAxis(axisPrice);
    volumeSeries->attachAxis(axisX);
    volumeSeries->attachAxis(axisVolume);
    return chart;
}

} // namespace

TradingCenterWidget::TradingCenterWidget(SimulationRunner *runner, QWidget *parent)
    : QWidget(parent),
      runner_(runner)
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

    // 出清结果摘要单独一行：既不把首行撑宽（窗口最小宽度可控），也不占太多竖向空间。
    mcpLabel_ = new QLabel(QStringLiteral("统一出清电价 (MCP): 0.00 元/MWh"), this);
    totalVolumeLabel_ = new QLabel(QStringLiteral("总出清电量: 0.00 MW"), this);
    const auto applyBoldFont = [](QLabel *label) {
        QFont boldFont = label->font();
        boldFont.setBold(true);
        boldFont.setPointSize(11);
        label->setFont(boldFont);
    };
    applyBoldFont(mcpLabel_);
    applyBoldFont(totalVolumeLabel_);
    auto *summaryRow = new QHBoxLayout;
    summaryRow->addWidget(mcpLabel_);
    summaryRow->addSpacing(28);
    summaryRow->addWidget(totalVolumeLabel_);
    summaryRow->addStretch();
    rootLayout->addLayout(summaryRow);

    // 供需曲线与结果区放进纵向分隔器：图表显得扁时，直接拖分隔条把图拉高。
    mainSplitter_ = new QSplitter(Qt::Vertical, this);
    mainSplitter_->setChildrenCollapsible(false);
    mainSplitter_->setHandleWidth(6);

    auto *chart = new QChart;
    chart->setTitle(QStringLiteral("供需曲线"));
    chartView_ = new QChartView(chart, mainSplitter_);
    chartView_->setRenderHint(QPainter::Antialiasing);
    chartView_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    chartView_->setMinimumHeight(180);
    mainSplitter_->addWidget(chartView_);

    auto *resultsGroup = new QGroupBox(QStringLiteral("出清结果"), mainSplitter_);
    auto *resultsLayout = new QVBoxLayout(resultsGroup);
    resultsTabs_ = new QTabWidget(resultsGroup);

    // 页签 1：96 时段汇总表。
    auto *summaryPage = new QWidget(resultsTabs_);
    auto *summaryLayout = new QVBoxLayout(summaryPage);
    summaryLayout->setContentsMargins(0, 0, 0, 0);
    resultsTable_ = new QTableWidget(0, 8, summaryPage);
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
    resultsTable_->setMinimumHeight(120);
    summaryLayout->addWidget(resultsTable_);

    // 页签 2：96 点电价曲线单独占一个页签，能拿到整块高度，不再被压扁。
    auto *curvePage = new QWidget(resultsTabs_);
    auto *curveLayout = new QVBoxLayout(curvePage);
    curveLayout->setContentsMargins(0, 0, 0, 0);

    auto *priceCurve = new QChart;
    priceCurve->setTitle(QStringLiteral("96 点日前市场电价曲线（请先运行全天出清）"));
    priceCurveView_ = new QChartView(priceCurve, curvePage);
    priceCurveView_->setRenderHint(QPainter::Antialiasing);
    priceCurveView_->setMinimumHeight(220);
    priceCurveView_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    curveLayout->addWidget(priceCurveView_);

    // 页签 3：当前时段各主体明细（含申报量 / 成交量 / 缺额）。
    detailTable_ = new QTableWidget(0, 11, resultsTabs_);
    detailTable_->setHorizontalHeaderLabels({
        QStringLiteral("时段"),
        QStringLiteral("类型"),
        QStringLiteral("主体"),
        QStringLiteral("申报量 (MW)"),
        QStringLiteral("成交/出力 (MW)"),
        QStringLiteral("缺额 (MW)"),
        QStringLiteral("边际成本 (元/MWh)"),
        QStringLiteral("收益 (元)"),
        QStringLiteral("成本 (元)"),
        QStringLiteral("利润 (元)"),
        QStringLiteral("支付 (元)")
    });
    detailTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    detailTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    detailTable_->setMinimumHeight(140);

    // 页签 4：申报总览（交易中心查看全部市场主体申报表单）。
    bidOverviewTable_ = new QTableWidget(0, 8, resultsTabs_);
    bidOverviewTable_->setHorizontalHeaderLabels({
        QStringLiteral("时段"),
        QStringLiteral("类型"),
        QStringLiteral("名称 / 主体"),
        QStringLiteral("段号"),
        QStringLiteral("段电量 (MW)"),
        QStringLiteral("段价格 (元/MWh)"),
        QStringLiteral("参数"),
        QStringLiteral("校验")
    });
    bidOverviewTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    bidOverviewTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    bidOverviewTable_->setMinimumHeight(140);

    resultsTabs_->addTab(summaryPage, QStringLiteral("96时段汇总"));
    resultsTabs_->addTab(curvePage, QStringLiteral("96点电价曲线"));
    resultsTabs_->addTab(detailTable_, QStringLiteral("当前时段主体明细"));
    resultsTabs_->addTab(bidOverviewTable_, QStringLiteral("申报总览"));
    resultsLayout->addWidget(resultsTabs_);
    mainSplitter_->addWidget(resultsGroup);
    mainSplitter_->setStretchFactor(0, 1);
    mainSplitter_->setStretchFactor(1, 1);
    rootLayout->addWidget(mainSplitter_, 1);

    auto *operationGroup = new QGroupBox(QStringLiteral("执行操作"), this);
    auto *operationLayout = new QGridLayout(operationGroup);
    singleClearButton_ = new QPushButton(QStringLiteral("当前时段单次出清"), operationGroup);
    batchClearButton_ = new QPushButton(QStringLiteral("连续出清全天96时段"), operationGroup);
    exportButton_ = new QPushButton(QStringLiteral("导出96时段结果"), operationGroup);
    exportDetailButton_ = new QPushButton(QStringLiteral("导出主体结算明细"), operationGroup);
    saveDatabaseButton_ = new QPushButton(QStringLiteral("保存结果到数据库"), operationGroup);
    validationButton_ = new QPushButton(QStringLiteral("查看校验问题"), operationGroup);
    operationLayout->addWidget(singleClearButton_, 0, 0);
    operationLayout->addWidget(batchClearButton_, 0, 1);
    operationLayout->addWidget(validationButton_, 0, 2);
    operationLayout->addWidget(exportButton_, 1, 0);
    operationLayout->addWidget(exportDetailButton_, 1, 1);
    operationLayout->addWidget(saveDatabaseButton_, 1, 2);

    auto *bottomLayout = new QHBoxLayout;
    bottomLayout->addWidget(operationGroup, 1);
    rootLayout->addLayout(bottomLayout);

    connectSignals();
    // 初始比例：供需曲线占上半部分，结果区占下半部分（都可以拖）。
    mainSplitter_->setSizes({430, 380});
    refreshBidOverview(0);
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
    connect(exportDetailButton_, &QPushButton::clicked,
            this, &TradingCenterWidget::exportSettlementDetails);
    connect(saveDatabaseButton_, &QPushButton::clicked,
            this, &TradingCenterWidget::saveToDatabase);
    connect(validationButton_, &QPushButton::clicked,
            this, &TradingCenterWidget::showValidationIssues);
    connect(periodSpinBox_, &QSpinBox::valueChanged,
            this, &TradingCenterWidget::onTimeSlotChanged);
    connect(modeCombo_, &QComboBox::currentIndexChanged,
            this, &TradingCenterWidget::onModeChanged);
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
    if (runner_ != nullptr && runner_->generatorWidget()->importParametersFromCsv(filePath, &errorMessage)) {
        showImportOutcome(this, QStringLiteral("导入成功"),
                          QStringLiteral("发电参数已替换为 CSV 中的机组。"),
                          errorMessage);
        refreshBidOverview(periodSpinBox_->value() - 1);
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
    if (runner_ != nullptr && runner_->generatorWidget()->importBidsFromCsv(filePath, &errorMessage)) {
        showImportOutcome(this, QStringLiteral("导入成功"),
                          QStringLiteral("发电报价已导入。"),
                          errorMessage);
        refreshBidOverview(periodSpinBox_->value() - 1);
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
    if (runner_ != nullptr && runner_->consumerWidget()->importParametersFromCsv(filePath, &errorMessage)) {
        showImportOutcome(this, QStringLiteral("导入成功"),
                          QStringLiteral("用户参数已替换为 CSV 中的用户。"),
                          errorMessage);
        refreshBidOverview(periodSpinBox_->value() - 1);
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
    if (runner_ != nullptr && runner_->consumerWidget()->importBidsFromCsv(filePath, &errorMessage)) {
        showImportOutcome(this, QStringLiteral("导入成功"),
                          QStringLiteral("用户报价已导入。"),
                          errorMessage);
        refreshBidOverview(periodSpinBox_->value() - 1);
    } else {
        QMessageBox::warning(this, QStringLiteral("导入失败"), errorMessage);
    }
}

void TradingCenterWidget::runSinglePeriodClear()
{
    if (runner_ == nullptr) {
        return;
    }

    const int slotIndex = periodSpinBox_->value() - 1;
    runClearForSlot(slotIndex);

    if (!lastResults_.empty() &&
        lastResults_[static_cast<std::size_t>(slotIndex)].market.feasible()) {
        return;
    }

    // 显式点击出清时，把该时段的全部校验问题一次列出来，而不是只提示第一条。
    const QStringList messages = currentValidationMessages();
    const QString detail = messages.isEmpty()
                               ? QString::fromStdString(
                                     lastResults_[static_cast<std::size_t>(slotIndex)].market.message())
                               : messages.join(QStringLiteral("\n"));
    QMessageBox::warning(this, QStringLiteral("出清不可行"), detail);
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

void TradingCenterWidget::onModeChanged(int index)
{
    if (runner_ == nullptr) {
        return;
    }
    const bool quadratic = index == 1;
    runner_->setQuadraticMode(quadratic);

    // 模式切换后按新模式重新出清当前时段（含 96 时段结果的重算由批量出清完成）。
    lastResults_.clear();
    resultsTable_->setRowCount(0);
    runClearForSlot(periodSpinBox_->value() - 1);
    refreshPriceCurve();

    emit modeChanged(quadratic);
}

void TradingCenterWidget::runClearForSlot(int slotIndex)
{
    if (runner_ == nullptr || slotIndex < 0 || slotIndex >= 96) {
        return;
    }

    const bool quadratic = runner_->quadraticMode();
    const TimeSlotResult slot = runner_->runSlot(slotIndex, quadratic);

    if (slot.market.feasible()) {
        mcpLabel_->setText(QStringLiteral("统一出清电价 (MCP): %1 元/MWh")
                               .arg(slot.market.clearingPriceYuanPerMwh(), 0, 'f', 2));
        totalVolumeLabel_->setText(QStringLiteral("总出清电量: %1 MW（缺额 %2 MW）")
                                       .arg(slot.market.clearingVolumeMw(), 0, 'f', 2)
                                       .arg(slot.market.shortageMw(), 0, 'f', 2));
    } else {
        mcpLabel_->setText(QStringLiteral("统一出清电价 (MCP): 不可行"));
        totalVolumeLabel_->setText(
            QStringLiteral("不可行原因: %1").arg(QString::fromStdString(slot.market.message())));
    }

    if (lastResults_.size() != 96) {
        lastResults_.resize(96);
    }
    lastResults_[static_cast<std::size_t>(slotIndex)] = slot;

    chartView_->setChart(quadratic ? createQuadraticChart(slot.input.generators(),
                                                          slot.input.consumers(),
                                                          slot.market)
                                   : createPiecewiseChart(slot.input.generators(),
                                                          slot.input.consumers(),
                                                          slot.market));
    refreshDetail(slotIndex);
    refreshBidOverview(slotIndex);
    refreshPriceCurve();

    emit clearingFinished(slotIndex);
}

void TradingCenterWidget::runBatchClearAllPeriods()
{
    if (runner_ == nullptr) {
        return;
    }

    const bool quadratic = runner_->quadraticMode();
    lastResults_ = runner_->runAll(quadratic);

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

    const int currentSlot = periodSpinBox_->value() - 1;
    refreshDetail(currentSlot);
    refreshBidOverview(currentSlot);
    refreshPriceCurve();

    emit clearingFinished(currentSlot);
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
        for (int column = 3; column < detailTable_->columnCount(); ++column) {
            detailTable_->setItem(0, column, new QTableWidgetItem(QString()));
        }
        return;
    }

    const auto &generators = settlement.generatorSettlements();
    const auto &consumers = settlement.consumerSettlements();
    detailTable_->setRowCount(static_cast<int>(generators.size() + consumers.size()));

    int row = 0;
    for (std::size_t i = 0; i < generators.size(); ++i) {
        const GeneratorSettlement &generator = generators[i];
        double declaredMw = 0.0;
        if (i < item.input.generators().size()) {
            declaredMw = generatorDeclaredMw(item.input.generators()[i], item.input.mode());
        }
        detailTable_->setItem(row, 0, new QTableWidgetItem(QString::number(slotIndex + 1)));
        detailTable_->setItem(row, 1, new QTableWidgetItem(QStringLiteral("发电")));
        detailTable_->setItem(row, 2,
            new QTableWidgetItem(QString::fromStdString(generator.generatorId)));
        detailTable_->setItem(row, 3,
            new QTableWidgetItem(formatNumber(declaredMw, 3)));
        detailTable_->setItem(row, 4,
            new QTableWidgetItem(formatNumber(generator.outputMw, 3)));
        detailTable_->setItem(row, 5, new QTableWidgetItem(QStringLiteral("-")));
        const double marginalCost =
            i < market.generatorResults().size()
                ? market.generatorResults()[i].marginalCostYuanPerMwh
                : 0.0;
        detailTable_->setItem(row, 6,
            new QTableWidgetItem(marginalCost > 0.0 ? formatNumber(marginalCost, 3)
                                                    : QString()));
        detailTable_->setItem(row, 7,
            new QTableWidgetItem(formatNumber(generator.revenueYuan, 3)));
        detailTable_->setItem(row, 8,
            new QTableWidgetItem(formatNumber(generator.costYuan, 3)));
        detailTable_->setItem(row, 9,
            new QTableWidgetItem(formatNumber(generator.profitYuan, 3)));
        detailTable_->setItem(row, 10, new QTableWidgetItem(QString()));
        ++row;
    }

    for (const ConsumerSettlement &consumer : consumers) {
        detailTable_->setItem(row, 0, new QTableWidgetItem(QString::number(slotIndex + 1)));
        detailTable_->setItem(row, 1, new QTableWidgetItem(QStringLiteral("用户")));
        detailTable_->setItem(row, 2,
            new QTableWidgetItem(QString::fromStdString(consumer.consumerId)));
        detailTable_->setItem(row, 3,
            new QTableWidgetItem(formatNumber(consumer.declaredDemandMw, 3)));
        detailTable_->setItem(row, 4,
            new QTableWidgetItem(formatNumber(consumer.clearedDemandMw, 3)));
        detailTable_->setItem(row, 5,
            new QTableWidgetItem(formatNumber(consumer.shortageMw, 3)));
        detailTable_->setItem(row, 6, new QTableWidgetItem(QString()));
        detailTable_->setItem(row, 7, new QTableWidgetItem(QString()));
        detailTable_->setItem(row, 8, new QTableWidgetItem(QString()));
        detailTable_->setItem(row, 9, new QTableWidgetItem(QString()));
        detailTable_->setItem(row, 10,
            new QTableWidgetItem(formatNumber(consumer.paymentYuan, 3)));
        ++row;
    }
}

void TradingCenterWidget::refreshBidOverview(int slotIndex)
{
    if (runner_ == nullptr || slotIndex < 0 || slotIndex >= 96) {
        return;
    }

    const bool quadratic = runner_->quadraticMode();
    const MarketInput input = runner_->buildSlotInput(slotIndex, quadratic);
    const ValidationReport report = runner_->validateSlot(slotIndex, quadratic);

    // 把校验问题按主体归类，便于在对应行标出问题。
    std::map<std::string, QStringList> issuesByTarget;
    for (const ValidationIssue &issue : report.issues()) {
        issuesByTarget[issue.target] << QString::fromStdString(issue.message);
    }

    std::vector<QStringList> rows;
    for (const Generator &generator : input.generators()) {
        const QString id = QString::fromStdString(generator.id());
        const QString name = runner_->generatorName(id);
        const QString subject = name.isEmpty() || name == id
                                    ? id
                                    : QStringLiteral("%1 (%2)").arg(name, id);
        const QString capacity = QStringLiteral("Pmin %1 / Pmax %2")
                                     .arg(formatNumber(generator.pMinMw(), 1),
                                          formatNumber(generator.pMaxMw(), 1));
        const QStringList issues = issuesByTarget["Generator:" + generator.id()];
        const QString status = issues.isEmpty() ? QStringLiteral("通过")
                                                : issues.join(QStringLiteral("；"));

        if (quadratic) {
            const BidSheet &sheet = generator.bidSheet();
            rows.push_back({QString::number(slotIndex + 1),
                            QStringLiteral("发电"),
                            subject,
                            QStringLiteral("—"),
                            QStringLiteral("—"),
                            QStringLiteral("—"),
                            QStringLiteral("a=%1, b=%2, c=%3, %4")
                                .arg(formatNumber(sheet.quadraticA(), 4),
                                     formatNumber(sheet.quadraticB(), 4),
                                     formatNumber(sheet.quadraticC(), 4),
                                     capacity),
                            status});
            continue;
        }

        if (generator.bidSheet().segments().empty()) {
            // 该时段未提交分段报价的机组仍要出现在总览里，便于交易中心发现漏报。
            rows.push_back({QString::number(slotIndex + 1),
                            QStringLiteral("发电"),
                            subject,
                            QStringLiteral("—"),
                            QStringLiteral("—"),
                            QStringLiteral("—"),
                            capacity,
                            status});
            continue;
        }

        for (const BidSegment &segment : generator.bidSheet().segments()) {
            rows.push_back({QString::number(slotIndex + 1),
                            QStringLiteral("发电"),
                            subject,
                            QString::number(segment.segmentNo()),
                            formatNumber(segment.quantityMw(), 3),
                            formatNumber(segment.priceYuanPerMwh(), 2),
                            capacity,
                            status});
        }
    }

    for (const Consumer &consumer : input.consumers()) {
        const QString id = QString::fromStdString(consumer.id());
        const QString name = runner_->consumerName(id);
        const QString subject = name.isEmpty() || name == id
                                    ? id
                                    : QStringLiteral("%1 (%2)").arg(name, id);
        const QString declared = QStringLiteral("QD=%1").arg(formatNumber(consumer.fixedDemandMw(), 2));
        const QStringList issues = issuesByTarget["Consumer:" + consumer.id()];
        const QString status = issues.isEmpty() ? QStringLiteral("通过")
                                                : issues.join(QStringLiteral("；"));

        if (consumer.bidSheet().segments().empty()) {
            rows.push_back({QString::number(slotIndex + 1),
                            QStringLiteral("用户"),
                            subject,
                            QStringLiteral("—"),
                            QStringLiteral("—"),
                            QStringLiteral("—"),
                            declared,
                            status});
            continue;
        }

        for (const BidSegment &segment : consumer.bidSheet().segments()) {
            rows.push_back({QString::number(slotIndex + 1),
                            QStringLiteral("用户"),
                            subject,
                            QString::number(segment.segmentNo()),
                            formatNumber(segment.quantityMw(), 3),
                            formatNumber(segment.priceYuanPerMwh(), 2),
                            declared,
                            status});
        }
    }

    bidOverviewTable_->setRowCount(static_cast<int>(rows.size()));
    for (int row = 0; row < static_cast<int>(rows.size()); ++row) {
        const QStringList &values = rows[static_cast<std::size_t>(row)];
        for (int column = 0; column < bidOverviewTable_->columnCount(); ++column) {
            const QString text = column < values.size() ? values[column] : QString();
            auto *item = new QTableWidgetItem(text);
            if (text != QStringLiteral("通过") && column == bidOverviewTable_->columnCount() - 1) {
                item->setForeground(QColor(190, 40, 40));
            }
            bidOverviewTable_->setItem(row, column, item);
        }
    }
}

void TradingCenterWidget::refreshPriceCurve()
{
    priceCurveView_->setChart(createPriceCurveChart(lastResults_));
}

QStringList TradingCenterWidget::currentValidationMessages() const
{
    QStringList messages;
    if (runner_ == nullptr) {
        return messages;
    }

    SimulationRunner *runner = runner_;
    const ValidationReport report =
        runner->validateSlot(periodSpinBox_->value() - 1, runner->quadraticMode());
    for (const ValidationIssue &issue : report.issues()) {
        const QString prefix =
            issue.severity == ValidationIssue::Severity::Error ? QStringLiteral("错误")
                                                               : QStringLiteral("警告");
        messages << QStringLiteral("[%1] %2: %3")
                        .arg(prefix,
                             QString::fromStdString(issue.target),
                             QString::fromStdString(issue.message));
    }
    return messages;
}

void TradingCenterWidget::showValidationIssues()
{
    const QStringList messages = currentValidationMessages();
    if (messages.isEmpty()) {
        QMessageBox::information(this,
                                 QStringLiteral("校验通过"),
                                 QStringLiteral("第 %1 时段全部市场主体申报校验通过。")
                                     .arg(periodSpinBox_->value()));
        return;
    }
    QMessageBox::warning(this,
                         QStringLiteral("第 %1 时段校验问题").arg(periodSpinBox_->value()),
                         messages.join(QStringLiteral("\n")));
}

void TradingCenterWidget::exportResults()
{
    if (lastResults_.size() != 96) {
        QMessageBox::information(this, QStringLiteral("暂无结果"),
                                 QStringLiteral("请先运行「连续出清全天96时段」。"));
        return;
    }

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

void TradingCenterWidget::exportSettlementDetails()
{
    if (lastResults_.size() != 96) {
        QMessageBox::information(this, QStringLiteral("暂无结果"),
                                 QStringLiteral("请先运行「连续出清全天96时段」。"));
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("导出主体结算明细"),
        QStringLiteral("settlement_detail.csv"),
        QStringLiteral("CSV 文件 (*.csv);;所有文件 (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    std::vector<CSVWriter::Row> rows;
    rows.push_back({
        "time_slot",
        "subject_type",
        "subject_id",
        "declared_mw",
        "cleared_mw",
        "shortage_mw",
        "clearing_price_yuan_per_mwh",
        "revenue_yuan",
        "cost_yuan",
        "profit_yuan",
        "payment_yuan"
    });

    for (const TimeSlotResult &item : lastResults_) {
        if (!item.market.feasible()) {
            continue;
        }
        const std::string slot = std::to_string(item.market.timeSlot() + 1);
        const std::string price =
            formatNumber(item.market.clearingPriceYuanPerMwh(), 4).toStdString();

        for (std::size_t i = 0; i < item.settlement.generatorSettlements().size(); ++i) {
            const GeneratorSettlement &generator =
                item.settlement.generatorSettlements()[i];
            double declaredMw = 0.0;
            if (i < item.input.generators().size()) {
                declaredMw =
                    generatorDeclaredMw(item.input.generators()[i], item.input.mode());
            }
            rows.push_back({
                slot,
                "generator",
                generator.generatorId,
                formatNumber(declaredMw, 4).toStdString(),
                formatNumber(generator.outputMw, 4).toStdString(),
                "",
                price,
                formatNumber(generator.revenueYuan, 4).toStdString(),
                formatNumber(generator.costYuan, 4).toStdString(),
                formatNumber(generator.profitYuan, 4).toStdString(),
                ""
            });
        }

        for (const ConsumerSettlement &consumer : item.settlement.consumerSettlements()) {
            rows.push_back({
                slot,
                "consumer",
                consumer.consumerId,
                formatNumber(consumer.declaredDemandMw, 4).toStdString(),
                formatNumber(consumer.clearedDemandMw, 4).toStdString(),
                formatNumber(consumer.shortageMw, 4).toStdString(),
                price,
                "",
                "",
                "",
                formatNumber(consumer.paymentYuan, 4).toStdString()
            });
        }
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
                             QStringLiteral("已导出各时段机组收入与用户账单明细。"));
}

void TradingCenterWidget::saveToDatabase()
{
    if (lastResults_.size() != 96) {
        QMessageBox::information(this, QStringLiteral("暂无结果"),
                                 QStringLiteral("请先运行「连续出清全天96时段」，再保存到数据库。"));
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("保存出清结果到 SQLite"),
        QStringLiteral("power_market.db"),
        QStringLiteral("SQLite 数据库 (*.db);;所有文件 (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    SQLiteManager manager(filePath);
    QString errorMessage;
    if (!manager.open(&errorMessage) || !manager.initializeSchema(&errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), errorMessage);
        return;
    }

    int saved = 0;
    for (const TimeSlotResult &item : lastResults_) {
        if (!manager.saveMarketInput(item.input, &errorMessage)) {
            QMessageBox::warning(this, QStringLiteral("保存失败"), errorMessage);
            return;
        }
        if (item.market.feasible() &&
            !manager.saveMarketResult(item.market, item.settlement, &errorMessage)) {
            QMessageBox::warning(this, QStringLiteral("保存失败"), errorMessage);
            return;
        }
        ++saved;
    }
    manager.close();

    QMessageBox::information(
        this,
        QStringLiteral("保存成功"),
        QStringLiteral("已把 %1 个时段的申报、出清与结算结果写入：\n%2").arg(saved).arg(filePath));
}

} // namespace pms
