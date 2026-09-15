#pragma once

#include <array>
#include <string>
#include <vector>

#include <QString>
#include <QWidget>

#include "Model/BidSegment/BidSegment.h"
#include "Model/Consumer/Consumer.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace pms {

struct ConsumerSlotData {
    double fixedDemandMw = 100.0;
    std::vector<BidSegment> segments;
};

struct ConsumerUnitData {
    QString id = QStringLiteral("C1");
    std::array<ConsumerSlotData, 96> timeSlots;
};

// 用户侧需求录入页面，保存多用户、多时段的需求报价。
class ConsumerWidget : public QWidget {
    Q_OBJECT

public:
    // 创建用户侧需求页面及默认用户。
    explicit ConsumerWidget(QWidget *parent = nullptr);

    // 将全部用户转换为指定 96 时段之一的领域模型数据。
    std::vector<Consumer> buildConsumersForSlot(int slotIndex, bool quadratic) const;

    // 切换到指定显示时段，并载入对应需求。
    void setTimeSlot(int displaySlot);

    // 将当前控件内容写回内存中的用户时段数据。
    void flushCurrentSlot();

    // 从 CSV 导入用户参数并替换当前用户列表。
    bool importParametersFromCsv(const QString &filePath, QString *errorMessage = nullptr);

    // 从 CSV 导入用户侧需求报价。
    bool importBidsFromCsv(const QString &filePath, QString *errorMessage = nullptr);

    // 新增一个默认用户。
    bool addUnit();

    // 删除当前选中的用户。
    bool removeCurrentUnit();

signals:
    // 时段变化时向外发出显示时段编号。
    void timeSlotChanged(int displaySlot);

private slots:
    // 保存并校验当前时段需求，随后刷新成本预览。
    void submitCurrentSlot();

    // 响应用户切换时段并同步数据。
    void onTimeSlotChanged(int displaySlot);

    // 响应用户切换用户并载入对应数据。
    void onUnitIndexChanged(int index);

    // 处理“新增用户”按钮。
    void addUnitClicked();

    // 处理“删除用户”按钮。
    void removeUnitClicked();

private:
    // 连接页面内控件信号。
    void connectSignals();

    // 将当前控件内容保存到指定用户和时段。
    void saveCurrentToSlot(int unitIndex, int slotIndex);

    // 保存当前用户的当前时段数据。
    void saveCurrent();

    // 将指定用户的数据载入界面。
    void loadUnit(int unitIndex);

    // 将指定时段的数据载入界面。
    void loadSlot(int slotIndex);

    // 清空需求报价表格。
    void clearSegmentTable();

    // 将界面缓存数据转换为领域模型对象。
    Consumer buildFromData(const ConsumerUnitData &unit,
                           int slotIndex,
                           bool quadratic) const;

    // 读取表格单元格文本。
    QString cellText(int row, int column) const;

    // 判断用户标识是否已存在。
    bool idExists(const QString &id, int exceptIndex = -1) const;

    // 生成下一个可用的默认用户标识。
    QString nextDefaultUnitId() const;

    QComboBox *userCombo_ = nullptr;
    QPushButton *addUnitButton_ = nullptr;
    QPushButton *removeUnitButton_ = nullptr;
    QSpinBox *timeSlotSpinBox_ = nullptr;
    QLineEdit *fixedDemandEdit_ = nullptr;
    QTableWidget *loadTable_ = nullptr;
    QPushButton *submitButton_ = nullptr;
    QLabel *totalCostLabel_ = nullptr;

    std::vector<ConsumerUnitData> units_;
    int currentUnitIndex_ = 0;
    int currentSlotIndex_ = 0;
    bool syncingUnit_ = false;
    bool syncingTimeSlot_ = false;
};

} // namespace pms
