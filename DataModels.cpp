// ============================================================
// DataModels.cpp —— 数据模型方法实现（第5轮补齐）
// 实现 BidSheet、MarketParticipant、Generator、Consumer 的声明方法。
// 防御语义（与既有模型约定一致）：
//   - setBidSheet：时段越界拒绝写入；容器不足先扩容到 96 槽位；
//   - getBidSheet：时段越界或未设置 -> 返回静态常量空报价单；
//   - initTimeSlotResult：结果容器重置为 96 个 0。
// ============================================================

#include "DataModels.h"

namespace
{
constexpr int kTotalTimeSlots = 96;   // 日前市场交易时段总数（0~95）
}  // namespace

// ============================================================
// BidSheet
// ============================================================

// 总申报电量：累加各分段申报电量
double BidSheet::getTotalQuantity() const
{
    double total = 0.0;
    for (const BidSegment& seg : segments)
    {
        total += seg.quantity;
    }
    return total;
}

// 报价合法性校验：任一分段电量或价格为负即非法
bool BidSheet::validate() const
{
    for (const BidSegment& seg : segments)
    {
        if (seg.quantity < 0.0 || seg.price < 0.0)
        {
            return false;
        }
    }
    return true;
}

// ============================================================
// MarketParticipant
// ============================================================

// 设置某时段报价单；时段越界拒绝写入（防御），容器不足先扩容到 96 槽位
void MarketParticipant::setBidSheet(int timeSlot, const BidSheet& sheet)
{
    if (timeSlot < 0 || timeSlot >= kTotalTimeSlots)
    {
        return;  // 非法时段：拒绝写入
    }
    if (static_cast<int>(bidSheets.size()) <= timeSlot)
    {
        bidSheets.resize(kTotalTimeSlots);   // 首次设置时补齐全部槽位
    }
    bidSheets[timeSlot] = sheet;
}

// 获取某时段报价单；时段越界或尚未设置 -> 返回静态空报价单（防越界）
const BidSheet& MarketParticipant::getBidSheet(int timeSlot) const
{
    static const BidSheet emptySheet;   // 常量空表：安全返回值
    if (timeSlot < 0 || timeSlot >= kTotalTimeSlots)
    {
        return emptySheet;
    }
    if (static_cast<int>(bidSheets.size()) <= timeSlot)
    {
        return emptySheet;   // 该时段尚未设置报价：视为空报价单
    }
    return bidSheets[timeSlot];
}

// 获取主体编号
const std::string& MarketParticipant::getId() const
{
    return id;
}

// 设置主体编号
void MarketParticipant::setId(const std::string& newId)
{
    id = newId;
}

// ============================================================
// Generator / Consumer
// ============================================================

// 初始化机组 96 时段结果容器（成交电量 / 收入全部置 0）
void Generator::initTimeSlotResult()
{
    clearedQuantity.assign(kTotalTimeSlots, 0.0);
    income.assign(kTotalTimeSlots, 0.0);
}

// 初始化用户 96 时段结果容器（成交电量 / 账单全部置 0）
void Consumer::initTimeSlotResult()
{
    clearedQuantity.assign(kTotalTimeSlots, 0.0);
    bill.assign(kTotalTimeSlots, 0.0);
}
