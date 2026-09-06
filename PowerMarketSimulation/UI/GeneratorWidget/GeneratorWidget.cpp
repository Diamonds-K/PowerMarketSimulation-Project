#include "UI/GeneratorWidget/GeneratorWidget.h"

#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace pms {

GeneratorWidget::GeneratorWidget(QWidget *parent)
    : QWidget(parent)
{
    for (GeneratorSlotData &slot : slotData_) {
        slot.segments = {
            BidSegment(1, 20.0, 200.0),
            BidSegment(2, 30.0, 250.0),
            BidSegment(3, 30.0, 300.0)
        };
    }

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(10);

    auto *paramGroup = new QGroupBox(QStringLiteral("机组参数"), this);
    auto *paramLayout = new QHBoxLayout(paramGroup);
    paramLayout->addWidget(new QLabel(QStringLiteral("选择机组："), paramGroup));

    unitCombo_ = new QComboBox(paramGroup);
    unitCombo_->setEditable(true);
    unitCombo_->addItem(QStringLiteral("G1"));
    paramLayout->addWidget(unitCombo_);

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

    loadSlot(0);
    connectSignals();
}

QWidget *GeneratorWidget::createLadderPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    ladderTable_ = new QTableWidget(10, 2, page);
    ladderTable_->setHorizontalHeaderLabels({
        QStringLiteral("出力段上限 P (MW)"),
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
}

void GeneratorWidget::submitCurrentSlot()
{
    saveCurrentToSlot(currentSlotIndex_);
    outputLabel_->setText(QStringLiteral("已保存第 %1 时段申报")
                              .arg(currentSlotIndex_ + 1));
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

void GeneratorWidget::setCurrentSlotSegments(const std::vector<BidSegment> &segments)
{
    for (int row = 0; row < ladderTable_->rowCount(); ++row) {
        ladderTable_->setItem(row, 0, new QTableWidgetItem(QString()));
        ladderTable_->setItem(row, 1, new QTableWidgetItem(QString()));
    }

    for (int row = 0; row < static_cast<int>(segments.size()); ++row) {
        if (row >= ladderTable_->rowCount()) {
            break;
        }
        ladderTable_->setItem(row, 0,
            new QTableWidgetItem(QString::number(segments[static_cast<std::size_t>(row)].quantityMw())));
        ladderTable_->setItem(row, 1,
            new QTableWidgetItem(QString::number(segments[static_cast<std::size_t>(row)].priceYuanPerMwh())));
    }

    saveCurrentToSlot(currentSlotIndex_);
    outputLabel_->setText(QStringLiteral("已导入 %1 段发电报价").arg(segments.size()));
}

void GeneratorWidget::onTimeSlotChanged(int displaySlot)
{
    const int newSlot = displaySlot - 1;
    if (newSlot == currentSlotIndex_) {
        return;
    }

    saveCurrentToSlot(currentSlotIndex_);
    currentSlotIndex_ = newSlot;
    loadSlot(currentSlotIndex_);

    if (!syncingTimeSlot_) {
        emit timeSlotChanged(displaySlot);
    }
}

void GeneratorWidget::saveCurrentToSlot(int slotIndex)
{
    GeneratorSlotData &data = slotData_[static_cast<std::size_t>(slotIndex)];
    data.id = unitCombo_->currentText().trimmed().isEmpty()
                  ? QStringLiteral("G1")
                  : unitCombo_->currentText().trimmed();
    data.pMinMw = pMinEdit_->text().toDouble();
    data.pMaxMw = pMaxEdit_->text().toDouble();
    data.quadraticMode = quadraticModeRadio_->isChecked();
    data.quadraticA = aEdit_->text().toDouble();
    data.quadraticB = bEdit_->text().toDouble();
    data.quadraticC = cEdit_->text().toDouble();

    data.segments.clear();
    for (int row = 0; row < ladderTable_->rowCount(); ++row) {
        const double quantity = cellText(row, 0).toDouble();
        const double price = cellText(row, 1).toDouble();
        if (quantity > 0.0 && price >= 0.0) {
            data.segments.push_back(BidSegment(row + 1, quantity, price));
        }
    }
}

void GeneratorWidget::loadSlot(int slotIndex)
{
    const GeneratorSlotData &data = slotData_[static_cast<std::size_t>(slotIndex)];
    unitCombo_->setCurrentText(data.id);
    pMinEdit_->setText(QString::number(data.pMinMw));
    pMaxEdit_->setText(QString::number(data.pMaxMw));
    aEdit_->setText(QString::number(data.quadraticA));
    bEdit_->setText(QString::number(data.quadraticB));
    cEdit_->setText(QString::number(data.quadraticC));

    if (data.quadraticMode) {
        quadraticModeRadio_->setChecked(true);
    } else {
        ladderModeRadio_->setChecked(true);
    }

    for (int row = 0; row < ladderTable_->rowCount(); ++row) {
        ladderTable_->setItem(row, 0, new QTableWidgetItem(QString()));
        ladderTable_->setItem(row, 1, new QTableWidgetItem(QString()));
    }

    for (int row = 0; row < static_cast<int>(data.segments.size()); ++row) {
        if (row >= ladderTable_->rowCount()) {
            break;
        }
        ladderTable_->setItem(row, 0,
            new QTableWidgetItem(QString::number(data.segments[static_cast<std::size_t>(row)].quantityMw())));
        ladderTable_->setItem(row, 1,
            new QTableWidgetItem(QString::number(data.segments[static_cast<std::size_t>(row)].priceYuanPerMwh())));
    }
}

Generator GeneratorWidget::buildGenerator(bool quadratic) const
{
    GeneratorSlotData data;
    data.id = unitCombo_->currentText().trimmed().isEmpty()
                  ? QStringLiteral("G1")
                  : unitCombo_->currentText().trimmed();
    data.pMinMw = pMinEdit_->text().toDouble();
    data.pMaxMw = pMaxEdit_->text().toDouble();
    data.quadraticA = aEdit_->text().toDouble();
    data.quadraticB = bEdit_->text().toDouble();
    data.quadraticC = cEdit_->text().toDouble();

    for (int row = 0; row < ladderTable_->rowCount(); ++row) {
        const double quantity = cellText(row, 0).toDouble();
        const double price = cellText(row, 1).toDouble();
        if (quantity > 0.0 && price >= 0.0) {
            data.segments.push_back(BidSegment(row + 1, quantity, price));
        }
    }
    return buildFromData(data, quadratic);
}

Generator GeneratorWidget::buildGeneratorForSlot(int slotIndex, bool quadratic) const
{
    return buildFromData(slotData_[static_cast<std::size_t>(slotIndex)], quadratic);
}

Generator GeneratorWidget::buildFromData(const GeneratorSlotData &data, bool quadratic) const
{
    Generator generator(data.id.toStdString(), data.pMinMw, data.pMaxMw);

    BidSheet sheet;
    sheet.setOwnerId(data.id.toStdString());
    if (quadratic) {
        sheet.setMode(BidSheet::Mode::Quadratic);
        sheet.setQuadraticCoefficients(data.quadraticA, data.quadraticB, data.quadraticC);
    } else {
        sheet.setMode(BidSheet::Mode::Piecewise);
        for (const BidSegment &segment : data.segments) {
            sheet.addSegment(segment);
        }
    }

    generator.setBidSheet(sheet);
    return generator;
}

QString GeneratorWidget::cellText(int row, int column) const
{
    const QTableWidgetItem *item = ladderTable_->item(row, column);
    return item ? item->text() : QString();
}

} // namespace pms
