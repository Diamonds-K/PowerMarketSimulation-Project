#include "UI/ConsumerWidget/ConsumerWidget.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>

#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "Data/CSVReader/CSVReader.h"
#include "TradingCenter/TradingCenter.h"
#include "UI/SimulationRunner/SimulationRunner.h"

namespace pms {

namespace {

ConsumerUnitData makeDefaultUnit(const QString &id)
{
    ConsumerUnitData unit;
    unit.id = id;
    unit.name = id;
    for (ConsumerSlotData &slot : unit.timeSlots) {
        slot.fixedDemandMw = 100.0;
        slot.segments = {
            BidSegment(1, 50.0, 260.0),
            BidSegment(2, 30.0, 240.0)
        };
    }
    return unit;
}

bool isParamHeader(const std::vector<std::string> &row)
{
    if (row.empty()) {
        return false;
    }
    return row.front() == "consumer_id" || row.front() == "generator_id";
}

bool isBidHeader(const std::vector<std::string> &row)
{
    if (row.empty()) {
        return false;
    }
    return row.front() == "owner_id" || row.front() == "consumer_id" ||
           row.front() == "generator_id";
}

QString formatNumber(double value, int precision = 2)
{
    return QString::number(value, 'f', precision);
}

} // namespace

ConsumerWidget::ConsumerWidget(QWidget *parent)
    : QWidget(parent)
{
    units_.push_back(makeDefaultUnit(QStringLiteral("C1")));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(10);

    auto *unitGroup = new QGroupBox(QStringLiteral("用户列表"), this);
    auto *unitLayout = new QHBoxLayout(unitGroup);
    unitLayout->addWidget(new QLabel(QStringLiteral("当前用户："), unitGroup));

    userCombo_ = new QComboBox(unitGroup);
    userCombo_->setEditable(true);
    unitLayout->addWidget(userCombo_, 1);

    addUnitButton_ = new QPushButton(QStringLiteral("新增用户"), unitGroup);
    removeUnitButton_ = new QPushButton(QStringLiteral("删除用户"), unitGroup);
    unitLayout->addWidget(addUnitButton_);
    unitLayout->addWidget(removeUnitButton_);
    rootLayout->addWidget(unitGroup);

    auto *paramGroup = new QGroupBox(QStringLiteral("用户参数"), this);
    auto *paramLayout = new QHBoxLayout(paramGroup);
    paramLayout->addWidget(new QLabel(QStringLiteral("用户名称:"), paramGroup));
    nameEdit_ = new QLineEdit(paramGroup);
    nameEdit_->setPlaceholderText(QStringLiteral("如：XX 工业用户"));
    paramLayout->addWidget(nameEdit_);

    paramLayout->addWidget(new QLabel(QStringLiteral("固定需求 QD (MW):"), paramGroup));
    fixedDemandEdit_ = new QLineEdit(QStringLiteral("100"), paramGroup);
    paramLayout->addWidget(fixedDemandEdit_);
    rootLayout->addWidget(paramGroup);

    auto *hintLabel = new QLabel(
        QStringLiteral("提示：二次曲线模式下按 QD 刚性需求出清；分段报价模式下按下方各段电量合计作为申报量，"
                       "QD 不参与分段出清。"),
        this);
    hintLabel->setWordWrap(true);
    rootLayout->addWidget(hintLabel);

    auto *timeGroup = new QGroupBox(QStringLiteral("时段控制"), this);
    auto *timeLayout = new QHBoxLayout(timeGroup);
    timeLayout->addWidget(new QLabel(QStringLiteral("当前时段:"), timeGroup));
    timeSlotSpinBox_ = new QSpinBox(timeGroup);
    timeSlotSpinBox_->setRange(1, 96);
    timeSlotSpinBox_->setValue(1);
    timeLayout->addWidget(timeSlotSpinBox_);
    timeLayout->addStretch();
    rootLayout->addWidget(timeGroup);

    loadTable_ = new QTableWidget(10, 2, this);
    loadTable_->setHorizontalHeaderLabels({
        QStringLiteral("分段电量 ΔQ (MW)"),
        QStringLiteral("买方报价 C (元/MWh)")
    });
    loadTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    loadTable_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    rootLayout->addWidget(loadTable_, 1);

    auto *bottomLayout = new QHBoxLayout;
    submitButton_ = new QPushButton(QStringLiteral("提交负荷申报"), this);
    bottomLayout->addWidget(submitButton_);
    bottomLayout->addStretch();
    rootLayout->addLayout(bottomLayout);

    auto *billGroup = new QGroupBox(QStringLiteral("我的账单（当前时段）"), this);
    auto *billLayout = new QGridLayout(billGroup);
    const auto addBillRow = [&billLayout, billGroup](int row,
                                                     const QString &caption,
                                                     QLabel *&value) {
        billLayout->addWidget(new QLabel(caption, billGroup), row, 0);
        value = new QLabel(QStringLiteral("-"), billGroup);
        QFont boldFont = value->font();
        boldFont.setBold(true);
        value->setFont(boldFont);
        billLayout->addWidget(value, row, 1);
    };
    addBillRow(0, QStringLiteral("统一出清电价 (元/MWh)："), clearingPriceLabel_);
    addBillRow(1, QStringLiteral("申报电量 (MW)："), declaredLabel_);
    addBillRow(2, QStringLiteral("成交电量 (MW)："), clearedLabel_);
    addBillRow(3, QStringLiteral("缺额 (MW)："), shortageLabel_);
    addBillRow(4, QStringLiteral("应交电费 (元)："), totalCostLabel_);
    billStatusLabel_ = new QLabel(
        QStringLiteral("提交申报或切换时段后，自动按当前市场模式出清并刷新账单。"), billGroup);
    billStatusLabel_->setWordWrap(true);
    billLayout->addWidget(billStatusLabel_, 5, 0, 1, 2);
    rootLayout->addWidget(billGroup);

    connectSignals();
    loadUnit(currentUnitIndex_);
}

void ConsumerWidget::connectSignals()
{
    connect(submitButton_, &QPushButton::clicked,
            this, &ConsumerWidget::submitCurrentSlot);
    connect(timeSlotSpinBox_, &QSpinBox::valueChanged,
            this, &ConsumerWidget::onTimeSlotChanged);
    connect(userCombo_, &QComboBox::currentIndexChanged,
            this, &ConsumerWidget::onUnitIndexChanged);
    connect(addUnitButton_, &QPushButton::clicked,
            this, &ConsumerWidget::addUnitClicked);
    connect(removeUnitButton_, &QPushButton::clicked,
            this, &ConsumerWidget::removeUnitClicked);
}

void ConsumerWidget::submitCurrentSlot()
{
    saveCurrent();
    refreshBill();
}

void ConsumerWidget::setRunner(SimulationRunner *runner)
{
    runner_ = runner;
    refreshBill();
}

void ConsumerWidget::refreshBill()
{
    if (units_.empty()) {
        return;
    }

    const ConsumerUnitData &unit = units_[static_cast<std::size_t>(currentUnitIndex_)];
    const ConsumerSlotData &slotData = unit.timeSlots[static_cast<std::size_t>(currentSlotIndex_)];

    if (runner_ == nullptr) {
        billStatusLabel_->setText(
            QStringLiteral("已保存第 %1 时段申报（未连接交易中心，暂无法出清）")
                .arg(currentSlotIndex_ + 1));
        return;
    }

    // 用当前时段、当前市场模式的完整市场数据出清，再取出本用户那一行。
    const TimeSlotResult slot = runner_->runSlot(currentSlotIndex_, runner_->quadraticMode());
    const MarketResult &market = slot.market;

    if (!market.feasible()) {
        clearingPriceLabel_->setText(QStringLiteral("不可行"));
        declaredLabel_->setText(QStringLiteral("-"));
        clearedLabel_->setText(QStringLiteral("-"));
        shortageLabel_->setText(QStringLiteral("-"));
        totalCostLabel_->setText(QStringLiteral("-"));
        billStatusLabel_->setText(QStringLiteral("第 %1 时段出清不可行：%2")
                                      .arg(currentSlotIndex_ + 1)
                                      .arg(QString::fromStdString(market.message())));
        return;
    }

    clearingPriceLabel_->setText(formatNumber(market.clearingPriceYuanPerMwh(), 2) +
                                 QStringLiteral(" 元/MWh"));

    const ConsumerSettlement *mine = nullptr;
    for (const ConsumerSettlement &settlement : slot.settlement.consumerSettlements()) {
        if (settlement.consumerId == unit.id.toStdString()) {
            mine = &settlement;
        }
    }
    if (mine == nullptr) {
        declaredLabel_->setText(QStringLiteral("-"));
        clearedLabel_->setText(QStringLiteral("-"));
        shortageLabel_->setText(QStringLiteral("-"));
        totalCostLabel_->setText(QStringLiteral("-"));
        billStatusLabel_->setText(
            QStringLiteral("未找到本用户的结算明细，请检查用户 ID 是否与申报一致。"));
        return;
    }

    declaredLabel_->setText(formatNumber(mine->declaredDemandMw, 3) + QStringLiteral(" MW"));
    clearedLabel_->setText(formatNumber(mine->clearedDemandMw, 3) + QStringLiteral(" MW"));
    shortageLabel_->setText(formatNumber(mine->shortageMw, 3) + QStringLiteral(" MW"));
    totalCostLabel_->setText(formatNumber(mine->paymentYuan, 2) + QStringLiteral(" 元"));

    QString status =
        QStringLiteral("第 %1 时段 · %2 · 成交电量中含按申报量比例分摊的必发电量（ΣPmin）")
            .arg(currentSlotIndex_ + 1)
            .arg(runner_->quadraticMode() ? QStringLiteral("二次曲线模式")
                                          : QStringLiteral("分段报价模式"));
    if (!runner_->quadraticMode()) {
        double declaredSum = 0.0;
        for (const BidSegment &segment : slotData.segments) {
            declaredSum += segment.quantityMw();
        }
        if (std::fabs(declaredSum - slotData.fixedDemandMw) > 1e-6) {
            status += QStringLiteral("；本时段分段申报合计 %1 MW，与固定需求 QD %2 MW 不一致，"
                                     "分段出清以段电量合计为准。")
                          .arg(formatNumber(declaredSum, 2))
                          .arg(formatNumber(slotData.fixedDemandMw, 2));
        }
    }
    billStatusLabel_->setText(status);
}

void ConsumerWidget::flushCurrentSlot()
{
    saveCurrent();
}

void ConsumerWidget::setTimeSlot(int displaySlot)
{
    if (displaySlot < 1 || displaySlot > 96) {
        return;
    }
    if (timeSlotSpinBox_->value() == displaySlot) {
        return;
    }

    syncingTimeSlot_ = true;
    timeSlotSpinBox_->setValue(displaySlot);
    syncingTimeSlot_ = false;
}

void ConsumerWidget::saveCurrentToSlot(int unitIndex, int slotIndex)
{
    if (unitIndex < 0 || unitIndex >= static_cast<int>(units_.size())) {
        return;
    }

    ConsumerUnitData &unit = units_[static_cast<std::size_t>(unitIndex)];
    const QString typedId = userCombo_->currentText().trimmed();
    if (!typedId.isEmpty()) {
        if (idExists(typedId, unitIndex)) {
            billStatusLabel_->setText(QStringLiteral("用户 ID 重复，未保存"));
            return;
        }
        unit.id = typedId;
    }

    const QString typedName = nameEdit_->text().trimmed();
    unit.name = typedName.isEmpty() ? unit.id : typedName;

    ConsumerSlotData &slot = unit.timeSlots[static_cast<std::size_t>(slotIndex)];
    slot.fixedDemandMw = fixedDemandEdit_->text().toDouble();

    slot.segments.clear();
    for (int row = 0; row < loadTable_->rowCount(); ++row) {
        const double quantity = cellText(row, 0).toDouble();
        const double price = cellText(row, 1).toDouble();
        if (quantity > 0.0 && price >= 0.0) {
            slot.segments.push_back(BidSegment(row + 1, quantity, price));
        }
    }
}

void ConsumerWidget::saveCurrent()
{
    saveCurrentToSlot(currentUnitIndex_, currentSlotIndex_);
}

void ConsumerWidget::loadUnit(int unitIndex)
{
    if (units_.empty()) {
        return;
    }
    if (unitIndex < 0 || unitIndex >= static_cast<int>(units_.size())) {
        unitIndex = 0;
    }
    currentUnitIndex_ = unitIndex;

    userCombo_->blockSignals(true);
    userCombo_->clear();
    for (const ConsumerUnitData &unit : units_) {
        userCombo_->addItem(unit.id);
    }
    userCombo_->setCurrentIndex(currentUnitIndex_);
    userCombo_->blockSignals(false);

    loadSlot(currentSlotIndex_);
}

void ConsumerWidget::loadSlot(int slotIndex)
{
    if (units_.empty() || slotIndex < 0 || slotIndex >= 96) {
        return;
    }
    currentSlotIndex_ = slotIndex;
    const ConsumerUnitData &unit = units_[static_cast<std::size_t>(currentUnitIndex_)];
    const ConsumerSlotData &slot = unit.timeSlots[static_cast<std::size_t>(slotIndex)];

    fixedDemandEdit_->setText(QString::number(slot.fixedDemandMw));
    nameEdit_->setText(unit.name.isEmpty() ? unit.id : unit.name);

    clearSegmentTable();
    for (int row = 0; row < static_cast<int>(slot.segments.size()); ++row) {
        if (row >= loadTable_->rowCount()) {
            break;
        }
        loadTable_->setItem(row, 0,
            new QTableWidgetItem(QString::number(slot.segments[static_cast<std::size_t>(row)].quantityMw())));
        loadTable_->setItem(row, 1,
            new QTableWidgetItem(QString::number(slot.segments[static_cast<std::size_t>(row)].priceYuanPerMwh())));
    }

    // 时段/用户切换后立即刷新本用户的成交电量与账单。
    refreshBill();
}

void ConsumerWidget::clearSegmentTable()
{
    for (int row = 0; row < loadTable_->rowCount(); ++row) {
        loadTable_->setItem(row, 0, new QTableWidgetItem(QString()));
        loadTable_->setItem(row, 1, new QTableWidgetItem(QString()));
    }
}

void ConsumerWidget::onTimeSlotChanged(int displaySlot)
{
    const int newSlot = displaySlot - 1;
    if (newSlot == currentSlotIndex_) {
        return;
    }

    saveCurrent();
    currentSlotIndex_ = newSlot;
    loadSlot(currentSlotIndex_);

    if (!syncingTimeSlot_) {
        emit timeSlotChanged(displaySlot);
    }
}

void ConsumerWidget::onUnitIndexChanged(int index)
{
    if (index < 0 || index >= static_cast<int>(units_.size()) ||
        index == currentUnitIndex_) {
        if (index >= 0 && index < static_cast<int>(units_.size())) {
            loadSlot(currentSlotIndex_);
        }
        return;
    }

    saveCurrent();
    loadUnit(index);
}

void ConsumerWidget::addUnitClicked()
{
    addUnit();
}

void ConsumerWidget::removeUnitClicked()
{
    removeCurrentUnit();
}

bool ConsumerWidget::addUnit()
{
    saveCurrent();
    units_.push_back(makeDefaultUnit(nextDefaultUnitId()));
    loadUnit(static_cast<int>(units_.size()) - 1);
    return true;
}

bool ConsumerWidget::removeCurrentUnit()
{
    if (units_.size() <= 1) {
        QMessageBox::information(this, QStringLiteral("删除用户"),
                                 QStringLiteral("至少需要保留一个用户。"));
        return false;
    }

    saveCurrent();
    units_.erase(units_.begin() + currentUnitIndex_);
    if (currentUnitIndex_ >= static_cast<int>(units_.size())) {
        currentUnitIndex_ = static_cast<int>(units_.size()) - 1;
    }
    loadUnit(currentUnitIndex_);
    return true;
}

int ConsumerWidget::unitCount() const
{
    return static_cast<int>(units_.size());
}

QString ConsumerWidget::unitId(int index) const
{
    if (index < 0 || index >= static_cast<int>(units_.size())) {
        return QString();
    }
    return units_[static_cast<std::size_t>(index)].id;
}

QString ConsumerWidget::unitName(int index) const
{
    if (index < 0 || index >= static_cast<int>(units_.size())) {
        return QString();
    }
    const ConsumerUnitData &unit = units_[static_cast<std::size_t>(index)];
    return unit.name.isEmpty() ? unit.id : unit.name;
}

Consumer ConsumerWidget::buildConsumer(bool quadratic) const
{
    return buildFromData(units_[static_cast<std::size_t>(currentUnitIndex_)],
                         currentSlotIndex_,
                         quadratic);
}

std::vector<Consumer> ConsumerWidget::buildConsumers(bool quadratic) const
{
    return buildConsumersForSlot(currentSlotIndex_, quadratic);
}

std::vector<Consumer> ConsumerWidget::buildConsumersForSlot(
    int slotIndex,
    bool quadratic) const
{
    std::vector<Consumer> consumers;
    consumers.reserve(units_.size());
    for (const ConsumerUnitData &unit : units_) {
        consumers.push_back(buildFromData(unit, slotIndex, quadratic));
    }
    return consumers;
}

Consumer ConsumerWidget::buildFromData(const ConsumerUnitData &unit,
                                       int slotIndex,
                                       bool quadratic) const
{
    const ConsumerSlotData &slot = unit.timeSlots[static_cast<std::size_t>(slotIndex)];
    Consumer consumer(unit.id.toStdString(), slot.fixedDemandMw);

    BidSheet sheet;
    sheet.setOwnerId(unit.id.toStdString());
    if (quadratic) {
        sheet.setMode(BidSheet::Mode::Quadratic);
    } else {
        sheet.setMode(BidSheet::Mode::Piecewise);
        for (const BidSegment &segment : slot.segments) {
            sheet.addSegment(segment);
        }
    }
    consumer.setBidSheet(sheet);
    return consumer;
}

bool ConsumerWidget::importParametersFromCsv(const QString &filePath,
                                             QString *errorMessage)
{
    std::vector<CSVReader::Row> rows;
    std::string readError;
    if (!CSVReader::read(filePath.toStdString(), rows, readError)) {
        if (errorMessage) {
            *errorMessage = QString::fromStdString(readError);
        }
        return false;
    }

    std::vector<ConsumerUnitData> nextUnits;
    std::set<QString> seenIds;
    QStringList errors;

    for (const CSVReader::Row &row : rows) {
        if (row.empty() || isParamHeader(row)) {
            continue;
        }
        if (row.size() < 3) {
            errors << QStringLiteral("用户参数行至少需要 3 列");
            continue;
        }

        const QString id = QString::fromStdString(row[0]).trimmed();
        bool demandOk = false;
        const double fixedDemand = QString::fromStdString(row[1]).toDouble(&demandOk);
        if (id.isEmpty() || seenIds.count(id) > 0) {
            errors << QStringLiteral("用户参数存在空或重复 ID: %1").arg(id);
            continue;
        }
        if (!demandOk || fixedDemand < 0.0) {
            errors << QStringLiteral("用户 %1 的固定需求非法").arg(id);
            continue;
        }

        const QString mode = QString::fromStdString(row[2]).trimmed().toLower();
        if (mode != QStringLiteral("piecewise") &&
            mode != QStringLiteral("quadratic")) {
            errors << QStringLiteral("用户 %1 的模式非法: %2").arg(id, mode);
            continue;
        }

        // 复用默认用户，预填每个时段的阶梯需求，避免“参数导入后表格为空”。
        // 二次需求模式不需要阶梯段，清掉即可。
        ConsumerUnitData unit = makeDefaultUnit(id);
        for (ConsumerSlotData &slot : unit.timeSlots) {
            slot.fixedDemandMw = fixedDemand;
            if (mode == QStringLiteral("quadratic")) {
                slot.segments.clear();
            }
        }
        seenIds.insert(id);
        nextUnits.push_back(unit);
    }

    // 只要存在至少一条合法参数就替换成功；非法行通过 errorMessage 汇总提示。
    if (nextUnits.empty()) {
        if (errorMessage) {
            *errorMessage = errors.isEmpty()
                                ? QStringLiteral("用户参数 CSV 没有有效数据")
                                : errors.join(QStringLiteral("\n"));
        }
        return false;
    }

    if (!errors.isEmpty() && errorMessage) {
        *errorMessage = QStringLiteral("以下用户参数行被跳过：\n") +
                        errors.join(QStringLiteral("\n"));
    }

    units_ = std::move(nextUnits);
    currentUnitIndex_ = 0;
    currentSlotIndex_ = 0;
    loadUnit(0);
    return true;
}

bool ConsumerWidget::importBidsFromCsv(const QString &filePath,
                                       QString *errorMessage)
{
    struct BidRecord {
        QString id;
        int timeSlot = 0;
        int segmentNo = 0;
        double quantityMw = 0.0;
        double priceYuanPerMwh = 0.0;
    };

    std::vector<CSVReader::Row> rows;
    std::string readError;
    if (!CSVReader::read(filePath.toStdString(), rows, readError)) {
        if (errorMessage) {
            *errorMessage = QString::fromStdString(readError);
        }
        return false;
    }

    std::vector<BidRecord> records;
    std::set<std::tuple<QString, int, int>> seen;
    // 同一用户同一时段最多 10 段（与 BidValidator 的段数上限一致）。
    std::map<std::pair<QString, int>, int> segmentCount;
    QStringList errors;

    for (const CSVReader::Row &row : rows) {
        if (row.empty() || isBidHeader(row)) {
            continue;
        }
        if (row.size() < 5) {
            errors << QStringLiteral("用户报价行至少需要 5 列");
            continue;
        }

        BidRecord record;
        bool slotOk = false;
        bool segmentOk = false;
        record.id = QString::fromStdString(row[0]).trimmed();
        record.timeSlot = QString::fromStdString(row[1]).toInt(&slotOk);
        record.segmentNo = QString::fromStdString(row[2]).toInt(&segmentOk);
        record.quantityMw = QString::fromStdString(row[3]).toDouble();
        record.priceYuanPerMwh = QString::fromStdString(row[4]).toDouble();

        if (record.id.isEmpty() || !idExists(record.id)) {
            errors << QStringLiteral("用户报价包含未知用户: %1").arg(record.id);
            continue;
        }
        if (!slotOk || record.timeSlot < 1 || record.timeSlot > 96 ||
            !segmentOk || record.segmentNo < 1 ||
            record.quantityMw <= 0.0 || record.priceYuanPerMwh < 0.0) {
            errors << QStringLiteral("用户 %1 的报价行数值非法").arg(record.id);
            continue;
        }

        const auto key = std::make_tuple(record.id, record.timeSlot, record.segmentNo);
        if (seen.count(key) > 0) {
            errors << QStringLiteral("用户 %1 时段 %2 段号 %3 重复")
                          .arg(record.id)
                          .arg(record.timeSlot)
                          .arg(record.segmentNo);
            continue;
        }
        seen.insert(key);

        const auto countKey = std::make_pair(record.id, record.timeSlot);
        if (segmentCount[countKey] >= 10) {
            errors << QStringLiteral("用户 %1 时段 %2 超过 10 段，已忽略多余段")
                          .arg(record.id)
                          .arg(record.timeSlot);
            continue;
        }
        ++segmentCount[countKey];
        records.push_back(record);
    }

    // 只要存在至少一条合法报价就导入；被跳过的坏行通过 errorMessage 汇总提示。
    if (records.empty()) {
        if (errorMessage) {
            *errorMessage = errors.isEmpty()
                                ? QStringLiteral("用户报价 CSV 没有有效数据")
                                : errors.join(QStringLiteral("\n"));
        }
        return false;
    }

    if (!errors.isEmpty() && errorMessage) {
        *errorMessage = QStringLiteral("以下用户报价行被跳过：\n") +
                        errors.join(QStringLiteral("\n"));
    }

    std::map<std::pair<QString, int>, std::vector<BidSegment>> grouped;
    for (const BidRecord &record : records) {
        const auto key = std::make_pair(record.id, record.timeSlot - 1);
        grouped[key].push_back(BidSegment(record.segmentNo,
                                          record.quantityMw,
                                          record.priceYuanPerMwh));
    }

    for (auto &entry : grouped) {
        std::sort(entry.second.begin(), entry.second.end(),
                  [](const BidSegment &lhs, const BidSegment &rhs) {
                      return lhs.segmentNo() < rhs.segmentNo();
                  });
    }

    for (const auto &entry : grouped) {
        const QString id = entry.first.first;
        const int slot = entry.first.second;
        for (ConsumerUnitData &unit : units_) {
            if (unit.id == id) {
                unit.timeSlots[static_cast<std::size_t>(slot)].segments = entry.second;
            }
        }
    }

    loadSlot(currentSlotIndex_);
    return true;
}

bool ConsumerWidget::idExists(const QString &id, int exceptIndex) const
{
    for (int i = 0; i < static_cast<int>(units_.size()); ++i) {
        if (i == exceptIndex) {
            continue;
        }
        if (units_[static_cast<std::size_t>(i)].id == id) {
            return true;
        }
    }
    return false;
}

QString ConsumerWidget::nextDefaultUnitId() const
{
    int index = 1;
    while (true) {
        const QString candidate = QStringLiteral("C%1").arg(index);
        if (!idExists(candidate)) {
            return candidate;
        }
        ++index;
    }
}

QString ConsumerWidget::cellText(int row, int column) const
{
    const QTableWidgetItem *item = loadTable_->item(row, column);
    return item ? item->text() : QString();
}

} // namespace pms
