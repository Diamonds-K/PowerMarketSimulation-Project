#ifndef DATA_MODELS_H
#define DATA_MODELS_H

#include <string>
#include <vector>
#include <unordered_map>

// ============================================================
// 电力现货日前市场仿真平台 —— 数据模型（已有模型，禁止修改）
// BidSegment / BidSheet / MarketParticipant / Generator /
// Consumer 为既有模型定义；ClearingResult 成员名称、类型、
// 顺序严格按任务给定版本，禁止新增、删除、重命名成员。
// 说明：若本地已有同名模型文件，请以本地版本为准（本头文件
//       仅作为引擎头文件的依赖载体，接口依赖上述类型与成员名）。
// ============================================================

// 最小报价单元：电量-价格对
struct BidSegment
{
    double quantity;   // 申报电量（MW）
    double price;      // 申报价格（元/MWh）
};

// 单主体单时段完整报价单
struct BidSheet
{
    std::vector<BidSegment> segments;   // 分段报价列表

    double getTotalQuantity() const;    // 返回总申报电量（MW）
    bool   validate() const;            // 报价合法性校验，合法返回 true
};

// 市场参与者抽象基类
class MarketParticipant
{
public:
    virtual ~MarketParticipant() = default;

    virtual void initTimeSlotResult() = 0;   // 初始化96时段结果容器（纯虚，子类实现）

    void setBidSheet(int timeSlot, const BidSheet& sheet);   // 设置某时段报价单
    const BidSheet& getBidSheet(int timeSlot) const;         // 获取某时段报价单（下标0~95）
    const std::string& getId() const;                        // 获取主体编号
    void setId(const std::string& newId);                    // 设置主体编号

protected:
    std::string id;                    // 主体唯一编号
    std::vector<BidSheet> bidSheets;   // 96个交易时段报价单，下标0~95
};

// 发电机组（卖方）
class Generator : public MarketParticipant
{
public:
    void initTimeSlotResult() override;   // 初始化96时段结果容器（置0）

    double p_min = 0.0;                   // 机组出力下限（MW）
    double p_max = 0.0;                   // 机组出力上限（MW）
    std::vector<double> clearedQuantity;  // 96时段成交电量（MW）
    std::vector<double> income;           // 96时段发电收入（元）
};

// 用户（买方）
class Consumer : public MarketParticipant
{
public:
    void initTimeSlotResult() override;   // 初始化96时段结果容器（置0）

    std::vector<double> clearedQuantity;  // 96时段成交电量（MW）
    std::vector<double> bill;             // 96时段用电账单（元）
};

// 单时段出清结算结果
struct ClearingResult
{
    int timeSlot{0};                                   // 时段编号 0~95
    double clearPrice{0.0};                            // 统一出清价（元/MWh）
    double totalTradeQty{0.0};                         // 总成交电量（MW）

    // 各主体结算明细（键为主体 id）
    std::unordered_map<std::string, double> genClearedQty;   // 机组 -> 成交电量（MW）
    std::unordered_map<std::string, double> consClearedQty;  // 用户 -> 成交电量（MW）
    std::unordered_map<std::string, double> genIncome;       // 机组 -> 发电收入（元）
    std::unordered_map<std::string, double> consBill;        // 用户 -> 用电账单（元）
};

#endif // DATA_MODELS_H
