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

    loadTable_->setItem(0, 0, new QTableWidgetItem(QStringLiteral("50")));
    loadTable_->setItem(0, 1, new QTableWidgetItem(QStringLiteral("260")));
    loadTable_->setItem(1, 0, new QTableWidgetItem(QStringLiteral("30")));
    loadTable_->setItem(1, 1, new QTableWidgetItem(QStringLiteral("240")));

    auto *bottomLayout = new QHBoxLayout;
    submitButton_ = new QPushButton(QStringLiteral("提交负荷申报"), this);
    totalCostLabel_ = new QLabel(QStringLiteral("出清总电费: 0.00 元"), this);
    bottomLayout->addWidget(submitButton_);
    bottomLayout->addStretch();
    bottomLayout->addWidget(totalCostLabel_);
    rootLayout->addLayout(bottomLayout);
}

Consumer ConsumerWidget::buildConsumer(bool quadratic) const
{
    const QString id = userCombo_->currentText().trimmed().isEmpty()
                           ? QStringLiteral("C1")
                           : userCombo_->currentText().trimmed();

    Consumer consumer(id.toStdString(), fixedDemandEdit_->text().toDouble());

    BidSheet sheet;
    sheet.setOwnerId(id.toStdString());
    if (quadratic) {
        sheet.setMode(BidSheet::Mode::Quadratic);
    } else {
        sheet.setMode(BidSheet::Mode::Piecewise);
        for (int row = 0; row < loadTable_->rowCount(); ++row) {
            const double quantity = cellText(row, 0).toDouble();
            const double price = cellText(row, 1).toDouble();
            if (quantity > 0.0 && price >= 0.0) {
                sheet.addSegment(BidSegment(row + 1, quantity, price));
            }
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
