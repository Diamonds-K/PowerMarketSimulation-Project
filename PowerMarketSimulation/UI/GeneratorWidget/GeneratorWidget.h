#pragma once

#include <array>
#include <string>
#include <vector>

#include <QString>
#include <QWidget>

#include "Model/BidSegment/BidSegment.h"
#include "Model/Generator/Generator.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QStackedWidget;
class QTableWidget;

namespace pms {

struct GeneratorSlotData {
    bool quadraticMode = false;
    double quadraticA = 0.05;
    double quadraticB = 10.0;
    double quadraticC = 0.0;
    std::vector<BidSegment> segments;
};

struct GeneratorUnitData {
    QString id = QStringLiteral("G1");
    double pMinMw = 20.0;
    double pMaxMw = 100.0;
    std::array<GeneratorSlotData, 96> timeSlots;
};

// 发电侧报价录入页面，保存多机组、多时段的分段或二次报价。
class GeneratorWidget : public QWidget {
    Q_OBJECT

public:
    // 创建发电侧报价页面及默认机组。
    explicit GeneratorWidget(QWidget *parent = nullptr);

    // 将全部机组转换为指定 96 时段之一的领域模型数据。
    std::vector<Generator> buildGeneratorsForSlot(int slotIndex, bool quadratic) const;

    // 切换到指定显示时段，并载入对应报价。
    void setTimeSlot(int displaySlot);

    // 将当前控件内容写回内存中的机组时段数据。
    void flushCurrentSlot();

    // 从 CSV 导入发电机组参数并替换当前机组列表。
    bool importParametersFromCsv(const QString &filePath, QString *errorMessage = nullptr);

    // 从 CSV 导入发电侧分段报价。
    bool importBidsFromCsv(const QString &filePath, QString *errorMessage = nullptr);

    // 新增一台默认机组。
    bool addUnit();

    // 删除当前选中的机组。
    bool removeCurrentUnit();

signals:
    // 时段变化时向外发出显示时段编号。
    void timeSlotChanged(int displaySlot);

private slots:
    // 保存并校验当前时段报价，随后刷新收益预览。
    void submitCurrentSlot();

    // 响应用户切换时段并同步数据。
    void onTimeSlotChanged(int displaySlot);

    // 响应用户切换机组并载入对应数据。
    void onUnitIndexChanged(int index);

    // 处理“新增机组”按钮。
    void addUnitClicked();

    // 处理“删除机组”按钮。
    void removeUnitClicked();

private:
    // 创建分段阶梯报价编辑区。
    QWidget *createLadderPage();

    // 创建二次成本曲线参数编辑区。
    QWidget *createQuadraticPage();

    // 连接页面内控件信号。
    void connectSignals();

    // 将当前控件内容保存到指定机组和时段。
    void saveCurrentToSlot(int unitIndex, int slotIndex);

    // 保存当前机组的当前时段数据。
    void saveCurrent();

    // 将指定机组的数据载入界面。
    void loadUnit(int unitIndex);

    // 将指定时段的数据载入界面。
    void loadSlot(int slotIndex);

    // 清空分段报价表格。
    void clearSegmentTable();

    // 将界面缓存数据转换为领域模型对象。
    Generator buildFromData(const GeneratorUnitData &unit,
                            int slotIndex,
                            bool quadratic) const;

    // 读取表格单元格文本。
    QString cellText(int row, int column) const;

    // 判断机组标识是否已存在。
    bool idExists(const QString &id, int exceptIndex = -1) const;

    // 生成下一个可用的默认机组标识。
    QString nextDefaultUnitId() const;

    QComboBox *unitCombo_ = nullptr;
    QPushButton *addUnitButton_ = nullptr;
    QPushButton *removeUnitButton_ = nullptr;
    QSpinBox *timeSlotSpinBox_ = nullptr;
    QRadioButton *ladderModeRadio_ = nullptr;
    QRadioButton *quadraticModeRadio_ = nullptr;
    QStackedWidget *modeStack_ = nullptr;
    QTableWidget *ladderTable_ = nullptr;
    QLineEdit *pMinEdit_ = nullptr;
    QLineEdit *pMaxEdit_ = nullptr;
    QLineEdit *aEdit_ = nullptr;
    QLineEdit *bEdit_ = nullptr;
    QLineEdit *cEdit_ = nullptr;
    QPushButton *submitButton_ = nullptr;
    QLabel *outputLabel_ = nullptr;
    QLabel *revenueLabel_ = nullptr;

    std::vector<GeneratorUnitData> units_;
    int currentUnitIndex_ = 0;
    int currentSlotIndex_ = 0;
    bool syncingUnit_ = false;
    bool syncingTimeSlot_ = false;
};

} // namespace pms
