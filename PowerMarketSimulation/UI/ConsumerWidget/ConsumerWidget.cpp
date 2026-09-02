#include "UI/ConsumerWidget/ConsumerWidget.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace pms {

ConsumerWidget::ConsumerWidget(QWidget* parent)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);

    auto* form = new QFormLayout;
    idEdit_ = new QLineEdit(QStringLiteral("C1"), this);
    fixedDemandEdit_ = new QLineEdit(QStringLiteral("100"), this);
    form->addRow(QStringLiteral("用户编号"), idEdit_);
    form->addRow(QStringLiteral("固定需求 QD (MW)"), fixedDemandEdit_);
    layout->addLayout(form);

    segmentsTable_ = new QTableWidget(0, 3, this);
    segmentsTable_->setHorizontalHeaderLabels(
        {QStringLiteral("段号"), QStringLiteral("电量 (MW)"), QStringLiteral("价格 (元/MWh)")});
    segmentsTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(segmentsTable_);

    auto* buttons = new QHBoxLayout;
    auto* addButton = new QPushButton(QStringLiteral("增加需求段"), this);
    auto* removeButton = new QPushButton(QStringLiteral("删除所选段"), this);
    auto* clearButton = new QPushButton(QStringLiteral("清空"), this);
    connect(addButton, &QPushButton::clicked, this, &ConsumerWidget::addSegmentRow);
    connect(removeButton, &QPushButton::clicked, this, &ConsumerWidget::removeSelectedSegmentRow);
    connect(clearButton, &QPushButton::clicked, this, &ConsumerWidget::clearForm);
    buttons->addWidget(addButton);
    buttons->addWidget(removeButton);
    buttons->addWidget(clearButton);
    buttons->addStretch();
    layout->addLayout(buttons);

    addSegmentRow();
    addSegmentRow();

    segmentsTable_->item(0, 1)->setText(QStringLiteral("50"));
    segmentsTable_->item(0, 2)->setText(QStringLiteral("260"));
    segmentsTable_->item(1, 1)->setText(QStringLiteral("30"));
    segmentsTable_->item(1, 2)->setText(QStringLiteral("240"));
}

Consumer ConsumerWidget::buildConsumer(bool quadratic) const {
    Consumer consumer(idEdit_->text().trimmed().toStdString(),
                      fixedDemandEdit_->text().toDouble());

    BidSheet sheet;
    sheet.setOwnerId(idEdit_->text().trimmed().toStdString());
    if (quadratic) {
        sheet.setMode(BidSheet::Mode::Quadratic);
    } else {
        sheet.setMode(BidSheet::Mode::Piecewise);
        for (int row = 0; row < segmentsTable_->rowCount(); ++row) {
            const double quantity = cellText(row, 1).toDouble();
            const double price = cellText(row, 2).toDouble();
            sheet.addSegment(BidSegment(row + 1, quantity, price));
        }
    }
    consumer.setBidSheet(sheet);
    return consumer;
}

void ConsumerWidget::clearForm() {
    segmentsTable_->setRowCount(0);
    addSegmentRow();
}

void ConsumerWidget::addSegmentRow() {
    const int row = segmentsTable_->rowCount();
    segmentsTable_->insertRow(row);
    auto* noItem = new QTableWidgetItem(QString::number(row + 1));
    noItem->setFlags(noItem->flags() & ~Qt::ItemIsEditable);
    segmentsTable_->setItem(row, 0, noItem);
    segmentsTable_->setItem(row, 1, new QTableWidgetItem(QStringLiteral("0")));
    segmentsTable_->setItem(row, 2, new QTableWidgetItem(QStringLiteral("0")));
}

void ConsumerWidget::removeSelectedSegmentRow() {
    const int row = segmentsTable_->currentRow();
    if (row >= 0) {
        segmentsTable_->removeRow(row);
    }
}

QString ConsumerWidget::cellText(int row, int column) const {
    const QTableWidgetItem* item = segmentsTable_->item(row, column);
    return item ? item->text() : QString();
}

} // namespace pms
