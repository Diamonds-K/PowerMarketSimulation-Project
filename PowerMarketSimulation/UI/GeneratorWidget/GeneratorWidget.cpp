#include "UI/GeneratorWidget/GeneratorWidget.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace pms {

GeneratorWidget::GeneratorWidget(QWidget* parent)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);

    auto* form = new QFormLayout;
    idEdit_ = new QLineEdit(QStringLiteral("G1"), this);
    pMinEdit_ = new QLineEdit(QStringLiteral("20"), this);
    pMaxEdit_ = new QLineEdit(QStringLiteral("100"), this);
    quadraticAEdit_ = new QLineEdit(QStringLiteral("0.05"), this);
    quadraticBEdit_ = new QLineEdit(QStringLiteral("10"), this);
    quadraticCEdit_ = new QLineEdit(QStringLiteral("0"), this);

    form->addRow(QStringLiteral("机组编号"), idEdit_);
    form->addRow(QStringLiteral("Pmin (MW)"), pMinEdit_);
    form->addRow(QStringLiteral("Pmax (MW)"), pMaxEdit_);
    form->addRow(QStringLiteral("二次系数 a"), quadraticAEdit_);
    form->addRow(QStringLiteral("二次系数 b"), quadraticBEdit_);
    form->addRow(QStringLiteral("二次系数 c"), quadraticCEdit_);
    layout->addLayout(form);

    segmentsTable_ = new QTableWidget(0, 3, this);
    segmentsTable_->setHorizontalHeaderLabels(
        {QStringLiteral("段号"), QStringLiteral("电量 (MW)"), QStringLiteral("价格 (元/MWh)")});
    segmentsTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(segmentsTable_);

    auto* buttons = new QHBoxLayout;
    auto* addButton = new QPushButton(QStringLiteral("增加报价段"), this);
    auto* removeButton = new QPushButton(QStringLiteral("删除所选段"), this);
    auto* clearButton = new QPushButton(QStringLiteral("清空"), this);
    connect(addButton, &QPushButton::clicked, this, &GeneratorWidget::addSegmentRow);
    connect(removeButton, &QPushButton::clicked, this, &GeneratorWidget::removeSelectedSegmentRow);
    connect(clearButton, &QPushButton::clicked, this, &GeneratorWidget::clearForm);
    buttons->addWidget(addButton);
    buttons->addWidget(removeButton);
    buttons->addWidget(clearButton);
    buttons->addStretch();
    layout->addLayout(buttons);

    addSegmentRow();
    addSegmentRow();
    addSegmentRow();

    segmentsTable_->item(0, 1)->setText(QStringLiteral("20"));
    segmentsTable_->item(0, 2)->setText(QStringLiteral("200"));
    segmentsTable_->item(1, 1)->setText(QStringLiteral("30"));
    segmentsTable_->item(1, 2)->setText(QStringLiteral("250"));
    segmentsTable_->item(2, 1)->setText(QStringLiteral("30"));
    segmentsTable_->item(2, 2)->setText(QStringLiteral("300"));
}

Generator GeneratorWidget::buildGenerator(bool quadratic) const {
    Generator generator(idEdit_->text().trimmed().toStdString(),
                        pMinEdit_->text().toDouble(),
                        pMaxEdit_->text().toDouble());

    BidSheet sheet;
    sheet.setOwnerId(idEdit_->text().trimmed().toStdString());
    if (quadratic) {
        sheet.setMode(BidSheet::Mode::Quadratic);
        sheet.setQuadraticCoefficients(quadraticAEdit_->text().toDouble(),
                                       quadraticBEdit_->text().toDouble(),
                                       quadraticCEdit_->text().toDouble());
    } else {
        sheet.setMode(BidSheet::Mode::Piecewise);
        for (int row = 0; row < segmentsTable_->rowCount(); ++row) {
            const double quantity = cellText(row, 1).toDouble();
            const double price = cellText(row, 2).toDouble();
            sheet.addSegment(BidSegment(row + 1, quantity, price));
        }
    }
    generator.setBidSheet(sheet);
    return generator;
}

void GeneratorWidget::clearForm() {
    segmentsTable_->setRowCount(0);
    addSegmentRow();
}

void GeneratorWidget::addSegmentRow() {
    const int row = segmentsTable_->rowCount();
    segmentsTable_->insertRow(row);
    auto* noItem = new QTableWidgetItem(QString::number(row + 1));
    noItem->setFlags(noItem->flags() & ~Qt::ItemIsEditable);
    segmentsTable_->setItem(row, 0, noItem);
    segmentsTable_->setItem(row, 1, new QTableWidgetItem(QStringLiteral("0")));
    segmentsTable_->setItem(row, 2, new QTableWidgetItem(QStringLiteral("0")));
}

void GeneratorWidget::removeSelectedSegmentRow() {
    const int row = segmentsTable_->currentRow();
    if (row >= 0) {
        segmentsTable_->removeRow(row);
    }
}

QString GeneratorWidget::cellText(int row, int column) const {
    const QTableWidgetItem* item = segmentsTable_->item(row, column);
    return item ? item->text() : QString();
}

} // namespace pms
