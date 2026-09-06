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

    ladderTable_->setItem(0, 0, new QTableWidgetItem(QStringLiteral("20")));
    ladderTable_->setItem(0, 1, new QTableWidgetItem(QStringLiteral("200")));
    ladderTable_->setItem(1, 0, new QTableWidgetItem(QStringLiteral("30")));
    ladderTable_->setItem(1, 1, new QTableWidgetItem(QStringLiteral("250")));
    ladderTable_->setItem(2, 0, new QTableWidgetItem(QStringLiteral("30")));
    ladderTable_->setItem(2, 1, new QTableWidgetItem(QStringLiteral("300")));

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
    aEdit_->setText(QStringLiteral("0.05"));
    bEdit_->setText(QStringLiteral("10"));
    cEdit_->setText(QStringLiteral("0"));
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
}

Generator GeneratorWidget::buildGenerator(bool quadratic) const
{
    const QString id = unitCombo_->currentText().trimmed().isEmpty()
                           ? QStringLiteral("G1")
                           : unitCombo_->currentText().trimmed();

    Generator generator(id.toStdString(),
                        pMinEdit_->text().toDouble(),
                        pMaxEdit_->text().toDouble());

    BidSheet sheet;
    sheet.setOwnerId(id.toStdString());
    if (quadratic) {
        sheet.setMode(BidSheet::Mode::Quadratic);
        sheet.setQuadraticCoefficients(aEdit_->text().toDouble(),
                                       bEdit_->text().toDouble(),
                                       cEdit_->text().toDouble());
    } else {
        sheet.setMode(BidSheet::Mode::Piecewise);
        for (int row = 0; row < ladderTable_->rowCount(); ++row) {
            const double quantity = cellText(row, 0).toDouble();
            const double price = cellText(row, 1).toDouble();
            if (quantity > 0.0 && price >= 0.0) {
                sheet.addSegment(BidSegment(row + 1, quantity, price));
            }
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
