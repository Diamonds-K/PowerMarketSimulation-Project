// ============================================================
// ClearingEngine.cpp —— 市场出清引擎：报价预处理 + 双指针供需撮合
// 第2轮已实现：报价段提取、无效段过滤、供需曲线排序。
// 第3轮新增：双指针供需撮合（支持部分成交），填充统一出清价、
//            总成交电量与各主体成交明细（genClearedQty / consClearedQty）。
// 仍不实现：结算回填（genIncome / consBill）——归 SettlementEngine；
//           二次曲线优化、Qt 界面均不涉及。
// 约束落实：
//   1. doClearing 入参 generators / consumers 全程 const 只读，
//      绝不修改 Generator / Consumer 的任何成员；
//   2. 对外接口 doClearing 函数开头即做参数合法性检查；
//   3. 空报价单 / quantity<=0 无效段已在 buildSupplyCurve /
//      buildDemandCurve 阶段过滤完毕；
//   4. 撮合仅填充返回的 ClearingResult，不写任何业务对象。
// ============================================================

#include "ClearingEngine.h"

#include <algorithm>   // std::stable_sort / std::min
#include <iostream>    // 调试打印（后续轮次可精简）

// 匿名命名空间：翻译单元内部常量，不对外暴露
namespace
{
constexpr int   kTotalTimeSlots = 96;    // 日前市场交易时段总数（15分钟 x 96）
constexpr double kEps = 1e-9;            // 电量比较容差：扣减后残余量判零用
}  // namespace

// ============================================================
// 构建供给曲线（发电侧报价序列）
// 规则：
//   1. 遍历全部 Generator，仅读取其 timeSlot 时段的 BidSheet（getBidSheet）；
//   2. 报价单 segments 为空的主体自然跳过（无段可提取）；
//   3. quantity <= 0 的报价段视为无效段，直接丢弃；
//   4. 组装为携带归属机组 id 的 OwnedBidSegment；
//   5. 全部段按 price 升序排序（低 -> 高），同价保持申报先后（稳定排序）。
// 返回：排序后的供给报价序列
// ============================================================
std::vector<ClearingEngine::OwnedBidSegment>
ClearingEngine::buildSupplyCurve(int timeSlot,
                                 const std::vector<Generator>& generators) const
{
    std::vector<OwnedBidSegment> supply;

    // 遍历全部机组（const 只读，仅调用 const 访问器）
    for (const Generator& gen : generators)
    {
        // 取该时段的报价单；segments 为空时循环体不执行，主体自动跳过
        const BidSheet& sheet = gen.getBidSheet(timeSlot);
        for (const BidSegment& seg : sheet.segments)
        {
            // 无效段过滤：仅保留正电量段
            if (seg.quantity <= 0.0)
            {
                continue;
            }

            // 组装报价段 + 归属机组 id
            OwnedBidSegment ob;
            ob.seg = seg;
            ob.ownerId = gen.getId();
            supply.push_back(ob);
        }
    }

    // 供给曲线按价格升序（低 -> 高）；stable_sort 保证同价段保持申报顺序
    std::stable_sort(supply.begin(), supply.end(),
                     [](const OwnedBidSegment& a, const OwnedBidSegment& b)
                     {
                         return a.seg.price < b.seg.price;
                     });

    return supply;
}

// ============================================================
// 构建需求曲线（用户侧报价序列）
// 规则（与供给对称）：
//   1. 遍历全部 Consumer，仅读取其 timeSlot 时段的 BidSheet；
//   2. 空报价单主体自动跳过；
//   3. quantity <= 0 无效段直接丢弃；
//   4. 组装为携带归属用户 id 的 OwnedBidSegment；
//   5. 全部段按 price 降序排序（高 -> 低），同价保持申报先后（稳定排序）。
// 返回：排序后的需求报价序列
// ============================================================
std::vector<ClearingEngine::OwnedBidSegment>
ClearingEngine::buildDemandCurve(int timeSlot,
                                 const std::vector<Consumer>& consumers) const
{
    std::vector<OwnedBidSegment> demand;

    // 遍历全部用户（const 只读，仅调用 const 访问器）
    for (const Consumer& cons : consumers)
    {
        // 取该时段的报价单；segments 为空时循环体不执行，主体自动跳过
        const BidSheet& sheet = cons.getBidSheet(timeSlot);
        for (const BidSegment& seg : sheet.segments)
        {
            // 无效段过滤：仅保留正电量段
            if (seg.quantity <= 0.0)
            {
                continue;
            }

            // 组装报价段 + 归属用户 id
            OwnedBidSegment ob;
            ob.seg = seg;
            ob.ownerId = cons.getId();
            demand.push_back(ob);
        }
    }

    // 需求曲线按价格降序（高 -> 低）；stable_sort 保证同价段保持申报顺序
    std::stable_sort(demand.begin(), demand.end(),
                     [](const OwnedBidSegment& a, const OwnedBidSegment& b)
                     {
                         return a.seg.price > b.seg.price;
                     });

    return demand;
}

// ============================================================
// 执行单个交易时段的市场出清（第3轮：预处理 + 双指针撮合）
// 函数开头执行参数合法性检查（不允许推迟到测试阶段）：
//   1. timeSlot 越界（不在 0~95）-> 返回 timeSlot=-1 的空结果，
//      无效哨兵供下游结算引擎识别并跳过；
//   2. generators / consumers 任一为空（无市场参与者）-> 返回空结果，
//      无需构建曲线。
// 正常路径流程：
//   A. 调用两个私有函数得到排序后的供给（发电报价升序）与
//      需求（用户报价降序）序列，各段携带归属主体 id；
//   B. 双指针逐段撮合（详见函数体内注释）；
//   C. 返回完整 ClearingResult：timeSlot、统一出清价 clearPrice、
//      总成交电量 totalTradeQty、按 ownerId 累加的成交明细
//      genClearedQty / consClearedQty。
// 注意：绝不修改 Generator / Consumer 对象；收入/账单字段
//       genIncome / consBill 本阶段不填，归 SettlementEngine。
// ============================================================
ClearingResult ClearingEngine::doClearing(
    int timeSlot,
    const std::vector<Generator>& generators,
    const std::vector<Consumer>& consumers) const
{
    // ---------- 参数合法性检查（步骤 1：时段编号） ----------
    if (timeSlot < 0 || timeSlot >= kTotalTimeSlots)
    {
        // 无效哨兵结果：timeSlot=-1 标识非法时段，其余字段保持默认
        ClearingResult invalidResult;
        invalidResult.timeSlot = -1;
        return invalidResult;
    }

    // ---------- 参数合法性检查（步骤 2：市场参与者集合） ----------
    if (generators.empty() || consumers.empty())
    {
        // 任一市场侧为空：无撮合对象，直接返回空结果
        // （timeSlot 保持默认 0，无成交时 Q/C 亦为 0）
        return ClearingResult{};
    }

    // ---------- 报价预处理：构建排序后的供需曲线 ----------
    // 提取 + 过滤 + 排序：供给按价升序（低->高），需求按价降序（高->低），
    // 每条 OwnedBidSegment 均携带归属主体 ownerId
    std::vector<OwnedBidSegment> supply = buildSupplyCurve(timeSlot, generators);
    std::vector<OwnedBidSegment> demand = buildDemandCurve(timeSlot, consumers);

    // ---------- 调试打印：价格 + 归属 id（后续轮次可精简） ----------
    std::cout << "[ClearingEngine] timeSlot=" << timeSlot
              << " 供给段数=" << supply.size()
              << " 需求段数=" << demand.size() << "\n";

    std::cout << "  [supply]  ";
    for (const OwnedBidSegment& s : supply)
    {
        std::cout << "(" << s.ownerId << ", p=" << s.seg.price
                  << ", q=" << s.seg.quantity << ") ";
    }
    std::cout << "\n";

    std::cout << "  [demand]  ";
    for (const OwnedBidSegment& d : demand)
    {
        std::cout << "(" << d.ownerId << ", p=" << d.seg.price
                  << ", q=" << d.seg.quantity << ") ";
    }
    std::cout << "\n";

    // ---------- 出清结果装配 ----------
    ClearingResult result;
    result.timeSlot = timeSlot;

    // 输出结果字段先显式清零（与结构体默认一致，语义自明）：
    // 无成交时 clearPrice / totalTradeQty 即保持 0（业务规则 4、5）
    result.clearPrice    = 0.0;
    result.totalTradeQty = 0.0;

    // ============================================================
    // 双指针供需撮合（第3轮核心）
    // 指针含义：
    //   si —— 指向当前待匹配的供给段（发电报价升序，低 -> 高）
    //   di —— 指向当前待匹配的需求段（用户报价降序，高 -> 低）
    // 每次迭代考察 supply[si] 与 demand[di] 这一对报价段：
    //   1) 成交条件判定：用户愿付价 >= 发电要价才可成交；
    //      一旦不满足，说明后续需求报价更低 / 供给报价更高，
    //      不可能再有成交，立即终止撮合（业务规则 3）；
    //   2) 部分成交：本对成交电量取双方段内剩余电量的较小者，
    //      即 tradeQty = min(供给剩余, 需求剩余)（业务规则 2）；
    //   3) 电量扣减：从两端段内剩余量中同步扣掉 tradeQty，
    //      剩余量 > 0 的一段可与下一段继续匹配（跨主体连续成交）；
    //   4) id 累加：按 ownerId 将 tradeQty 分别累加进发电/用户
    //      成交明细 map（业务规则 6）；
    //   5) 指针移动：某段剩余电量耗尽（<= EPS 判零，吸收浮点残余）
    //      即移动对应指针；供给耗尽 / 需求耗尽 -> 循环条件不成立，
    //      自然终止撮合（业务规则 5）；
    //   6) 出清价：每笔成交后更新 clearPrice = 当前发电段报价，
    //      循环结束后即“边际成交发电侧报价”（业务规则 4）。
    // ============================================================
    std::size_t si = 0;   // 供给指针
    std::size_t di = 0;   // 需求指针

    while (si < supply.size() && di < demand.size())
    {
        // ---------- 成交条件判定（不满足立即终止） ----------
        if (demand[di].seg.price < supply[si].seg.price)
        {
            // 用户最高愿付价已低于发电最低要价：边际之外无成交，
            // 双指针后续报价只会更差，立即终止撮合
            break;
        }

        // ---------- 部分成交：电量取双方剩余较小值 ----------
        const double tradeQty =
            std::min(supply[si].seg.quantity, demand[di].seg.quantity);

        // 总成交电量累加
        result.totalTradeQty += tradeQty;

        // 出清价 = 本笔成交的发电侧（供给）报价；
        // 随撮合推进持续更新，最终停留点为边际成交发电侧报价
        result.clearPrice = supply[si].seg.price;

        // 按 ownerId 累加成交电量（同一主体可多段多次成交）
        result.genClearedQty[supply[si].ownerId] += tradeQty;
        result.consClearedQty[demand[di].ownerId] += tradeQty;

        // ---------- 电量扣减（支持部分成交与跨段连续成交） ----------
        supply[si].seg.quantity -= tradeQty;
        demand[di].seg.quantity -= tradeQty;

        // ---------- 指针移动：段内剩余电量耗尽则移过该段 ----------
        // 用 EPS 容差判定“耗尽”，避免浮点减法残留极小量导致死循环
        if (supply[si].seg.quantity <= kEps)
        {
            ++si;   // 供给段耗尽 -> 供给指针前进，匹配下一供给段
        }
        if (demand[di].seg.quantity <= kEps)
        {
            ++di;   // 需求段耗尽 -> 需求指针前进，匹配下一需求段
        }
        // 若某段仍有剩余，指针不动，下一轮继续与对侧新段匹配；
        // si / di 任一越界 -> while 条件不成立，撮合自然终止
    }

    // 循环结束后的状态说明（业务规则 4、5）：
    //   - 一笔都未成交：clearPrice / totalTradeQty 保持 0；
    //   - 有成交：clearPrice = 最后一笔成交的发电侧报价（边际价）；
    //   - genClearedQty / consClearedQty 已按 ownerId 完成累加。
    return result;
}
