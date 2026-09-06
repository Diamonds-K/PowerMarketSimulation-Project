#include "UI/ConsumerWidget/ConsumerWidget.h"

#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace pms {

ConsumerWidget::ConsumerWidget(QWidget *parent)
    : QWidget(parent)
{
    for (ConsumerSlotData &slot : slotData_) {
        slot.segments = {
            BidSegment(1, 50.0, 260.0),
            BidSegment(2, 30.0, 240.0)
        };
    }

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(10);

    auto *paramGroup = new QGroupBox(QStringLiteral("用户参数"), this);
    auto *paramLayout = new QHBoxLayout(paramGroup);
    paramLayout->addWidget(new QLabel(QStringLiteral("选择用户："), paramGroup));

    userCombo_ = new QComboBox(paramGroup);
    userCombo_->setEditable(true);
    userCombo_->addItem(QStringLiteral("C1"));
    paramLayout->addWidget(userCombo_);

    paramLayout->addWidget(new QLabel(QStringLiteral("固定需求 QD (MW):"), paramGroup));
    fixedDemandEdit_ = new QLineEdit(QStringLiteral("100"), paramGroup);
    paramLayout->addWidget(fixedDemandEdit_);
    paramLayout->addStretch();
    rootLayout->addWidget(paramGroup);

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
        QStringLiteral("负荷段上限 P (MW)"),
        QStringLiteral("买方报价 C (元/MWh)")
    });
    loadTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    loadTable_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    rootLayout->addWidget(loadTable_, 1);

    auto *bottomLayout = new QHBoxLayout;
    submitButton_ = new QPushButton(QStringLiteral("提交负荷申报"), this);
    totalCostLabel_ = new QLabel(QStringLiteral("出清总电费: 0.00 元"), this);
    bottomLayout->addWidget(submitButton_);
    bottomLayout->addStretch();
    bottomLayout->addWidget(totalCostLabel_);
    rootLayout->addLayout(bottomLayout);

    loadSlot(0);
    connectSignals();
}

void ConsumerWidget::connectSignals()
{
    connect(submitButton_, &QPushButton::clicked,
            this, &ConsumerWidget::submitCurrentSlot);
    connect(timeSlotSpinBox_, &QSpinBox::valueChanged,
            this, &ConsumerWidget::onTimeSlotChanged);
}

void ConsumerWidget::submitCurrentSlot()
{
    saveCurrentToSlot(currentSlotIndex_);
    totalCostLabel_->setText(QStringLiteral("已保存第 %1 时段申报")
                                 .arg(currentSlotIndex_ + 1));
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

void ConsumerWidget::onTimeSlotChanged(int displaySlot)
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

void ConsumerWidget::saveCurrentToSlot(int slotIndex)
{
    ConsumerSlotData &data = slotData_[static_cast<std::size_t>(slotIndex)];
    data.id = userCombo_->currentText().trimmed().isEmpty()
                  ? QStringLiteral("C1")
                  : userCombo_->currentText().trimmed();
    data.fixedDemandMw = fixedDemandEdit_->text().toDouble();

    data.segments.clear();
    for (int row = 0; row < loadTable_->rowCount(); ++row) {
        const double quantity = cellText(row, 0).toDouble();
        const double price = cellText(row, 1).toDouble();
        if (quantity > 0.0 && price >= 0.0) {
            data.segments.push_back(BidSegment(row + 1, quantity, price));
        }
    }
}

void ConsumerWidget::loadSlot(int slotIndex)
{
    const ConsumerSlotData &data = slotData_[static_cast<std::size_t>(slotIndex)];
    userCombo_->setCurrentText(data.id);
    fixedDemandEdit_->setText(QString::number(data.fixedDemandMw));

    for (int row = 0; row < loadTable_->rowCount(); ++row) {
        loadTable_->setItem(row, 0, new QTableWidgetItem(QString()));
        loadTable_->setItem(row, 1, new QTableWidgetItem(QString()));
    }

    for (int row = 0; row < static_cast<int>(data.segments.size()); ++row) {
        if (row >= loadTable_->rowCount()) {
            break;
        }
        loadTable_->setItem(row, 0,
            new QTableWidgetItem(QString::number(data.segments[static_cast<std::size_t>(row)].quantityMw())));
        loadTable_->setItem(row, 1,
            new QTableWidgetItem(QString::number(data.segments[static_cast<std::size_t>(row)].priceYuanPerMwh())));
    }
}

Consumer ConsumerWidget::buildConsumer(bool quadratic) const
{
    ConsumerSlotData data;
    data.id = userCombo_->currentText().trimmed().isEmpty()
                  ? QStringLiteral("C1")
                  : userCombo_->currentText().trimmed();
    data.fixedDemandMw = fixedDemandEdit_->text().toDouble();

    for (int row = 0; row < loadTable_->rowCount(); ++row) {
        const double quantity = cellText(row, 0).toDouble();
        const double price = cellText(row, 1).toDouble();
        if (quantity > 0.0 && price >= 0.0) {
            data.segments.push_back(BidSegment(row + 1, quantity, price));
        }
    }
    return buildFromData(data, quadratic);
}

Consumer ConsumerWidget::buildConsumerForSlot(int slotIndex, bool quadratic) const
{
    return buildFromData(slotData_[static_cast<std::size_t>(slotIndex)], quadratic);
}

Consumer ConsumerWidget::buildFromData(const ConsumerSlotData &data, bool quadratic) const
{
    Consumer consumer(data.id.toStdString(), data.fixedDemandMw);

    BidSheet sheet;
    sheet.setOwnerId(data.id.toStdString());
    if (quadratic) {
        sheet.setMode(BidSheet::Mode::Quadratic);
    } else {
        sheet.setMode(BidSheet::Mode::Piecewise);
        for (const BidSegment &segment : data.segments) {
            sheet.addSegment(segment);
        }
    }

    consumer.setBidSheet(sheet);
    return consumer;
}

QString ConsumerWidget::cellText(int row, int column) const
{
    const QTableWidgetItem *item = loadTable_->item(row, column);
    return item ? item->text() : QString();
}

} // namespace pms
