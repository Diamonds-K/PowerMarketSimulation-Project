#include "UI/GeneratorWidget/GeneratorWidget.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>

#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "Data/CSVReader/CSVReader.h"

namespace pms {

namespace {

GeneratorUnitData makeDefaultUnit(const QString &id)
{
    GeneratorUnitData unit;
    unit.id = id;
    unit.pMinMw = 20.0;
    unit.pMaxMw = 100.0;

    for (GeneratorSlotData &slot : unit.timeSlots) {
        slot.quadraticMode = false;
        slot.quadraticA = 0.05;
        slot.quadraticB = 10.0;
        slot.quadraticC = 0.0;
        slot.segments = {
            BidSegment(1, 20.0, 200.0),
            BidSegment(2, 30.0, 250.0),
            BidSegment(3, 30.0, 300.0)
        };
    }
    return unit;
}

bool parsePositiveDouble(const QString &text, double &value)
{
    bool ok = false;
    value = text.toDouble(&ok);
    return ok && value >= 0.0;
}

bool isParamHeader(const std::vector<std::string> &row)
{
    if (row.empty()) {
        return false;
    }
    return row.front() == "generator_id" || row.front() == "consumer_id";
}

bool isBidHeader(const std::vector<std::string> &row)
{
    if (row.empty()) {
        return false;
    }
    return row.front() == "owner_id" || row.front() == "generator_id" ||
           row.front() == "consumer_id";
}

} // namespace

GeneratorWidget::GeneratorWidget(QWidget *parent)
    : QWidget(parent)
{
    units_.push_back(makeDefaultUnit(QStringLiteral("G1")));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(10);

    auto *unitGroup = new QGroupBox(QStringLiteral("机组列表"), this);
    auto *unitLayout = new QHBoxLayout(unitGroup);
    unitLayout->addWidget(new QLabel(QStringLiteral("当前机组："), unitGroup));

    unitCombo_ = new QComboBox(unitGroup);
    unitCombo_->setEditable(true);
    unitLayout->addWidget(unitCombo_, 1);

    addUnitButton_ = new QPushButton(QStringLiteral("新增机组"), unitGroup);
    removeUnitButton_ = new QPushButton(QStringLiteral("删除机组"), unitGroup);
    unitLayout->addWidget(addUnitButton_);
    unitLayout->addWidget(removeUnitButton_);
    rootLayout->addWidget(unitGroup);

    auto *paramGroup = new QGroupBox(QStringLiteral("机组参数"), this);
    auto *paramLayout = new QHBoxLayout(paramGroup);
    paramLayout->addWidget(new QLabel(QStringLiteral("Pmin (MW):"), paramGroup));
    pMinEdit_ = new QLineEdit(QStringLiteral("20"), paramGroup);
    paramLayout->addWidget(pMinEdit_);

    paramLayout->addWidget(new QLabel(QStringLiteral("Pmax (MW):"), paramGroup));
    pMaxEdit_ = new QLineEdit(QStringLiteral("100"), paramGroup);
    paramLayout->addWidget(pMaxEdit_);
    paramLayout->addStretch();
    rootLayout->addWidget(paramGroup);

    auto *modeGroup = new QGroupBox(QStringLiteral("报价模式与时段"), this);
    auto *modeLayout = new QHBoxLayout(modeGroup);
    modeLayout->addWidget(new QLabel(QStringLiteral("当前时段:"), modeGroup));

    timeSlotSpinBox_ = new QSpinBox(modeGroup);
    timeSlotSpinBox_->setRange(1, 96);
    timeSlotSpinBox_->setValue(1);
    modeLayout->addWidget(timeSlotSpinBox_);

    ladderModeRadio_ = new QRadioButton(QStringLiteral("10段阶梯报价"), modeGroup);
    quadraticModeRadio_ = new QRadioButton(QStringLiteral("二次成本曲线"), modeGroup);
    ladderModeRadio_->setChecked(true);
    modeLayout->addWidget(ladderModeRadio_);
    modeLayout->addWidget(quadraticModeRadio_);
    modeLayout->addStretch();
    rootLayout->addWidget(modeGroup);

    modeStack_ = new QStackedWidget(this);
    modeStack_->addWidget(createLadderPage());
    modeStack_->addWidget(createQuadraticPage());
    rootLayout->addWidget(modeStack_, 1);

    auto *bottomLayout = new QHBoxLayout;
    submitButton_ = new QPushButton(QStringLiteral("提交发电申报"), this);
    outputLabel_ = new QLabel(QStringLiteral("中标出力: 0.00 MW"), this);
    revenueLabel_ = new QLabel(QStringLiteral("预估收益: 0.00 元"), this);
    bottomLayout->addWidget(submitButton_);
    bottomLayout->addStretch();
    bottomLayout->addWidget(outputLabel_);
    bottomLayout->addWidget(revenueLabel_);
    rootLayout->addLayout(bottomLayout);

    connectSignals();
    loadUnit(currentUnitIndex_);
}

QWidget *GeneratorWidget::createLadderPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    ladderTable_ = new QTableWidget(10, 2, page);
    ladderTable_->setHorizontalHeaderLabels({
        QStringLiteral("分段电量 ΔP (MW)"),
        QStringLiteral("分段报价 C (元/MWh)")
    });
    ladderTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ladderTable_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(ladderTable_);
    return page;
}

QWidget *GeneratorWidget::createQuadraticPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto createParamRow = [this, page, layout](const QString &labelText, QLineEdit *&edit) {
        auto *rowLayout = new QHBoxLayout;
        rowLayout->addWidget(new QLabel(labelText, page));

        edit = new QLineEdit(page);
        edit->setPlaceholderText(QStringLiteral("请输入数值"));
        edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        rowLayout->addWidget(edit, 1);
        layout->addLayout(rowLayout);
    };

    createParamRow(QStringLiteral("二次项系数 a："), aEdit_);
    createParamRow(QStringLiteral("一次项系数 b："), bEdit_);
    createParamRow(QStringLiteral("常数项系数 c："), cEdit_);
    layout->addStretch();
    return page;
}

void GeneratorWidget::connectSignals()
{
    connect(ladderModeRadio_, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            modeStack_->setCurrentIndex(0);
        }
    });

    connect(quadraticModeRadio_, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            modeStack_->setCurrentIndex(1);
        }
    });

    connect(submitButton_, &QPushButton::clicked,
            this, &GeneratorWidget::submitCurrentSlot);
    connect(timeSlotSpinBox_, &QSpinBox::valueChanged,
            this, &GeneratorWidget::onTimeSlotChanged);
    connect(unitCombo_, &QComboBox::currentIndexChanged,
            this, &GeneratorWidget::onUnitIndexChanged);
    connect(addUnitButton_, &QPushButton::clicked,
            this, &GeneratorWidget::addUnitClicked);
    connect(removeUnitButton_, &QPushButton::clicked,
            this, &GeneratorWidget::removeUnitClicked);
}

void GeneratorWidget::submitCurrentSlot()
{
    saveCurrent();
    outputLabel_->setText(QStringLiteral("已保存第 %1 时段申报")
                              .arg(currentSlotIndex_ + 1));
}

void GeneratorWidget::flushCurrentSlot()
{
    saveCurrent();
}

void GeneratorWidget::setTimeSlot(int displaySlot)
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

void GeneratorWidget::saveCurrentToSlot(int unitIndex, int slotIndex)
{
    if (unitIndex < 0 || unitIndex >= static_cast<int>(units_.size())) {
        return;
    }

    GeneratorUnitData &unit = units_[static_cast<std::size_t>(unitIndex)];
    const QString typedId = unitCombo_->currentText().trimmed();
    if (!typedId.isEmpty()) {
        if (idExists(typedId, unitIndex)) {
            outputLabel_->setText(QStringLiteral("机组 ID 重复，未保存"));
            return;
        }
        unit.id = typedId;
    }

    unit.pMinMw = pMinEdit_->text().toDouble();
    unit.pMaxMw = pMaxEdit_->text().toDouble();

    GeneratorSlotData &slot = unit.timeSlots[static_cast<std::size_t>(slotIndex)];
    slot.quadraticMode = quadraticModeRadio_->isChecked();
    slot.quadraticA = aEdit_->text().toDouble();
    slot.quadraticB = bEdit_->text().toDouble();
    slot.quadraticC = cEdit_->text().toDouble();

    slot.segments.clear();
    for (int row = 0; row < ladderTable_->rowCount(); ++row) {
        const double quantity = cellText(row, 0).toDouble();
        const double price = cellText(row, 1).toDouble();
        if (quantity > 0.0 && price >= 0.0) {
            slot.segments.push_back(BidSegment(row + 1, quantity, price));
        }
    }
}

void GeneratorWidget::saveCurrent()
{
    saveCurrentToSlot(currentUnitIndex_, currentSlotIndex_);
}

void GeneratorWidget::loadUnit(int unitIndex)
{
    if (units_.empty()) {
        return;
    }
    if (unitIndex < 0 || unitIndex >= static_cast<int>(units_.size())) {
        unitIndex = 0;
    }
    currentUnitIndex_ = unitIndex;

    unitCombo_->blockSignals(true);
    unitCombo_->clear();
    for (const GeneratorUnitData &unit : units_) {
        unitCombo_->addItem(unit.id);
    }
    unitCombo_->setCurrentIndex(currentUnitIndex_);
    unitCombo_->blockSignals(false);

    loadSlot(currentSlotIndex_);
}

void GeneratorWidget::loadSlot(int slotIndex)
{
    if (units_.empty() || slotIndex < 0 || slotIndex >= 96) {
        return;
    }
    currentSlotIndex_ = slotIndex;
    const GeneratorUnitData &unit = units_[static_cast<std::size_t>(currentUnitIndex_)];
    const GeneratorSlotData &slot = unit.timeSlots[static_cast<std::size_t>(slotIndex)];

    pMinEdit_->setText(QString::number(unit.pMinMw));
    pMaxEdit_->setText(QString::number(unit.pMaxMw));
    aEdit_->setText(QString::number(slot.quadraticA));
    bEdit_->setText(QString::number(slot.quadraticB));
    cEdit_->setText(QString::number(slot.quadraticC));

    if (slot.quadraticMode) {
        quadraticModeRadio_->setChecked(true);
        modeStack_->setCurrentIndex(1);
    } else {
        ladderModeRadio_->setChecked(true);
        modeStack_->setCurrentIndex(0);
    }

    clearSegmentTable();
    for (int row = 0; row < static_cast<int>(slot.segments.size()); ++row) {
        if (row >= ladderTable_->rowCount()) {
            break;
        }
        ladderTable_->setItem(row, 0,
            new QTableWidgetItem(QString::number(slot.segments[static_cast<std::size_t>(row)].quantityMw())));
        ladderTable_->setItem(row, 1,
            new QTableWidgetItem(QString::number(slot.segments[static_cast<std::size_t>(row)].priceYuanPerMwh())));
    }
}

void GeneratorWidget::clearSegmentTable()
{
    for (int row = 0; row < ladderTable_->rowCount(); ++row) {
        ladderTable_->setItem(row, 0, new QTableWidgetItem(QString()));
        ladderTable_->setItem(row, 1, new QTableWidgetItem(QString()));
    }
}

void GeneratorWidget::onTimeSlotChanged(int displaySlot)
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

void GeneratorWidget::onUnitIndexChanged(int index)
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

void GeneratorWidget::addUnitClicked()
{
    addUnit();
}

void GeneratorWidget::removeUnitClicked()
{
    removeCurrentUnit();
}

bool GeneratorWidget::addUnit()
{
    saveCurrent();
    units_.push_back(makeDefaultUnit(nextDefaultUnitId()));
    loadUnit(static_cast<int>(units_.size()) - 1);
    return true;
}

bool GeneratorWidget::removeCurrentUnit()
{
    if (units_.size() <= 1) {
        QMessageBox::information(this, QStringLiteral("删除机组"),
                                 QStringLiteral("至少需要保留一台机组。"));
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

int GeneratorWidget::unitCount() const
{
    return static_cast<int>(units_.size());
}

Generator GeneratorWidget::buildGenerator(bool quadratic) const
{
    return buildFromData(units_[static_cast<std::size_t>(currentUnitIndex_)],
                         currentSlotIndex_,
                         quadratic);
}

std::vector<Generator> GeneratorWidget::buildGenerators(bool quadratic) const
{
    return buildGeneratorsForSlot(currentSlotIndex_, quadratic);
}

std::vector<Generator> GeneratorWidget::buildGeneratorsForSlot(
    int slotIndex,
    bool quadratic) const
{
    std::vector<Generator> generators;
    generators.reserve(units_.size());
    for (const GeneratorUnitData &unit : units_) {
        generators.push_back(buildFromData(unit, slotIndex, quadratic));
    }
    return generators;
}

Generator GeneratorWidget::buildFromData(const GeneratorUnitData &unit,
                                         int slotIndex,
                                         bool quadratic) const
{
    Generator generator(unit.id.toStdString(), unit.pMinMw, unit.pMaxMw);
    const GeneratorSlotData &slot = unit.timeSlots[static_cast<std::size_t>(slotIndex)];

    BidSheet sheet;
    sheet.setOwnerId(unit.id.toStdString());
    if (quadratic) {
        sheet.setMode(BidSheet::Mode::Quadratic);
        sheet.setQuadraticCoefficients(slot.quadraticA,
                                       slot.quadraticB,
                                       slot.quadraticC);
    } else {
        sheet.setMode(BidSheet::Mode::Piecewise);
        for (const BidSegment &segment : slot.segments) {
            sheet.addSegment(segment);
        }
    }

    generator.setBidSheet(sheet);
    return generator;
}

bool GeneratorWidget::importParametersFromCsv(const QString &filePath,
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

    std::vector<GeneratorUnitData> nextUnits;
    std::set<QString> seenIds;
    QStringList errors;

    for (const CSVReader::Row &row : rows) {
        if (row.empty() || isParamHeader(row)) {
            continue;
        }
        if (row.size() < 4) {
            errors << QStringLiteral("发电参数行至少需要 4 列");
            continue;
        }

        const QString id = QString::fromStdString(row[0]).trimmed();
        double pMin = 0.0;
        double pMax = 0.0;
        if (id.isEmpty() || seenIds.count(id) > 0) {
            errors << QStringLiteral("发电参数存在空或重复 ID: %1").arg(id);
            continue;
        }
        if (!parsePositiveDouble(QString::fromStdString(row[1]), pMin) ||
            !parsePositiveDouble(QString::fromStdString(row[2]), pMax) ||
            pMax <= pMin) {
            errors << QStringLiteral("机组 %1 的 Pmin/Pmax 非法").arg(id);
            continue;
        }

        const QString mode = QString::fromStdString(row[3]).trimmed().toLower();
        if (mode != QStringLiteral("piecewise") &&
            mode != QStringLiteral("quadratic")) {
            errors << QStringLiteral("机组 %1 的模式非法: %2").arg(id, mode);
            continue;
        }

        GeneratorUnitData unit;
        unit.id = id;
        unit.pMinMw = pMin;
        unit.pMaxMw = pMax;
        for (GeneratorSlotData &slot : unit.timeSlots) {
            slot.quadraticMode = (mode == QStringLiteral("quadratic"));
        }
        seenIds.insert(id);
        nextUnits.push_back(unit);
    }

    if (!errors.isEmpty() || nextUnits.empty()) {
        if (errorMessage) {
            *errorMessage = errors.isEmpty()
                                ? QStringLiteral("发电参数 CSV 没有有效数据")
                                : errors.join(QStringLiteral("\n"));
        }
        return false;
    }

    units_ = std::move(nextUnits);
    currentUnitIndex_ = 0;
    currentSlotIndex_ = 0;
    loadUnit(0);
    return true;
}

bool GeneratorWidget::importBidsFromCsv(const QString &filePath,
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
    QStringList errors;

    for (const CSVReader::Row &row : rows) {
        if (row.empty() || isBidHeader(row)) {
            continue;
        }
        if (row.size() < 5) {
            errors << QStringLiteral("发电报价行至少需要 5 列");
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
            errors << QStringLiteral("发电报价包含未知机组: %1").arg(record.id);
            continue;
        }
        if (!slotOk || record.timeSlot < 1 || record.timeSlot > 96 ||
            !segmentOk || record.segmentNo < 1 ||
            record.quantityMw <= 0.0 || record.priceYuanPerMwh < 0.0) {
            errors << QStringLiteral("机组 %1 的报价行数值非法").arg(record.id);
            continue;
        }

        const auto key = std::make_tuple(record.id, record.timeSlot, record.segmentNo);
        if (seen.count(key) > 0) {
            errors << QStringLiteral("机组 %1 时段 %2 段号 %3 重复")
                          .arg(record.id)
                          .arg(record.timeSlot)
                          .arg(record.segmentNo);
            continue;
        }
        seen.insert(key);
        records.push_back(record);
    }

    if (!errors.isEmpty() || records.empty()) {
        if (errorMessage) {
            *errorMessage = errors.isEmpty()
                                ? QStringLiteral("发电报价 CSV 没有有效数据")
                                : errors.join(QStringLiteral("\n"));
        }
        return false;
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
        for (GeneratorUnitData &unit : units_) {
            if (unit.id == id) {
                unit.timeSlots[static_cast<std::size_t>(slot)].segments = entry.second;
            }
        }
    }

    loadSlot(currentSlotIndex_);
    return true;
}

bool GeneratorWidget::idExists(const QString &id, int exceptIndex) const
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

QString GeneratorWidget::nextDefaultUnitId() const
{
    int index = 1;
    while (true) {
        const QString candidate = QStringLiteral("G%1").arg(index);
        if (!idExists(candidate)) {
            return candidate;
        }
        ++index;
    }
}

QString GeneratorWidget::cellText(int row, int column) const
{
    const QTableWidgetItem *item = ladderTable_->item(row, column);
    return item ? item->text() : QString();
}

} // namespace pms
