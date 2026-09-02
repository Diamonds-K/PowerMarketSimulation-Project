# PowerMarketSimulation 项目架构约束

> 本文件是本项目的**长期架构约束文件**。所有代码开发、代码生成、代码评审和重构都必须首先阅读并遵守本文件与 `DESIGN.md`。

## 1. 项目定位

本项目是一个基于 C++/Qt 的电力现货市场（日前市场）出清仿真平台，核心能力包括：

1. 发电侧分段报价提交；
2. 用户侧需求申报；
3. 交易中心市场出清；
4. 统一价格结算；
5. 96 个 15 分钟时段日前市场仿真；
6. 二次曲线报价优化扩展。

本项目采用**两种可对比的报价模式**：

- 模式 1：分段报价，双指针撮合；
- 模式 2：二次曲线报价，KKT/λ 二分求解。

两种模式共享同一套数据模型、校验框架、结算框架和展示层，不重新设计项目方向。

## 2. 不可变更的架构原则

以下原则是硬约束，任何代码不得违反：

1. **分层固定**：项目采用 `Model / Validation / Clearing / Settlement / TradingCenter / Data / UI / Test` 分层。
2. **统一出清接口**：所有出清算法必须通过 `ClearingEngine::clear(const MarketInput&)` 统一接口调用。
3. **依赖方向**：`UI -> TradingCenter -> (Validation / Clearing / Settlement / Data) -> Model`；不允许反向依赖。
4. **算法层不依赖 Qt**：`Model / Validation / Clearing / Settlement` 不得包含任何 Qt 头文件。
5. **数据层不依赖业务逻辑**：`Data` 层只做数据读写与持久化，不得调用 `Validation / Clearing / Settlement` 中的业务规则。
6. **UI 不直接调用算法**：UI 组件只能调用 `TradingCenter` 业务入口，不能直接实例化或调用 `ClearingEngine / SettlementEngine`。
7. **Model 为纯数据结构**：`Model` 只保存数据，不包含撮合、优化、结算等算法逻辑。
8. **测试即验收**：算法、结算和边界规则必须有对应测试，新增算法必须同步更新 `Test`。
9. **单位与规则显式化**：所有量纲、价格规则、边界规则、同价规则必须在 `DESIGN.md` 中显式定义，代码实现与之一致。

## 3. 目录结构

```
PowerMarketSimulation/
│
├── ARCHITECTURE.md
├── DESIGN.md
├── AGENTS.md
├── README.md
├── CMakeLists.txt
├── main.cpp
│
├── Model/
│   ├── Generator/Generator.h
│   ├── Generator/Generator.cpp
│   ├── Consumer/Consumer.h
│   ├── Consumer/Consumer.cpp
│   ├── BidSegment/BidSegment.h
│   ├── BidSegment/BidSegment.cpp
│   ├── BidSheet/BidSheet.h
│   ├── BidSheet/BidSheet.cpp
│   ├── MarketInput/MarketInput.h
│   ├── MarketInput/MarketInput.cpp
│   ├── MarketResult/MarketResult.h
│   ├── MarketResult/MarketResult.cpp
│   ├── SettlementResult/SettlementResult.h
│   └── SettlementResult/SettlementResult.cpp
│
├── Validation/
│   ├── BidValidator/BidValidator.h
│   ├── BidValidator/BidValidator.cpp
│   ├── ConstraintChecker/ConstraintChecker.h
│   └── ConstraintChecker/ConstraintChecker.cpp
│
├── Clearing/
│   ├── ClearingEngine/ClearingEngine.h
│   ├── ClearingEngine/ClearingEngine.cpp
│   ├── PiecewiseClearing/PiecewiseClearing.h
│   ├── PiecewiseClearing/PiecewiseClearing.cpp
│   ├── QuadraticClearing/QuadraticClearing.h
│   └── QuadraticClearing/QuadraticClearing.cpp
│
├── Settlement/
│   ├── SettlementEngine/SettlementEngine.h
│   └── SettlementEngine/SettlementEngine.cpp
│
├── TradingCenter/
│   ├── TradingCenter.h
│   └── TradingCenter.cpp
│
├── Data/
│   ├── CSVReader/CSVReader.h
│   ├── CSVReader/CSVReader.cpp
│   ├── CSVWriter/CSVWriter.h
│   ├── CSVWriter/CSVWriter.cpp
│   ├── SQLiteManager/SQLiteManager.h
│   └── SQLiteManager/SQLiteManager.cpp
│
├── UI/
│   ├── MainWindow/MainWindow.h
│   ├── MainWindow/MainWindow.cpp
│   ├── GeneratorWidget/GeneratorWidget.h
│   ├── GeneratorWidget/GeneratorWidget.cpp
│   ├── ConsumerWidget/ConsumerWidget.h
│   ├── ConsumerWidget/ConsumerWidget.cpp
│   ├── TradingCenterWidget/TradingCenterWidget.h
│   └── TradingCenterWidget/TradingCenterWidget.cpp
│
└── Test/
    ├── ClearingTest/ClearingTest.cpp
    ├── SettlementTest/SettlementTest.cpp
    └── OptimizationTest/OptimizationTest.cpp
```

> 说明：需求文档中“交易中心”承担“汇总申报、组织出清、发布结果、完成结算”的职责，且 UI 层不得直接调用算法，因此增加顶层 `TradingCenter` 模块作为**应用服务门面**。它不改变既定的七层方向，只是补上 UI 与算法之间的编排层。

## 4. 分层职责

### 4.1 Model（数据模型）

纯数据结构，保存市场主体、报价、市场输入与结果：

- `Generator`：机组 id、`Pmin`、`Pmax`、报价单；
- `Consumer`：用户 id、固定需求、需求报价单；
- `BidSegment`：段号、电量、价格；
- `BidSheet`：某主体某时段的分段报价或二次曲线系数；
- `MarketInput`：一个时段的完整市场输入（机组、用户、模式）；
- `MarketResult`：一个时段的出清结果（价格、电量、机组/用户结果）；
- `SettlementResult`：一个时段的结算结果（收益、成本、利润、支付）。

### 4.2 Validation（校验）

`BidValidator` 负责报价结构校验：段电量、单调性、段数上限、容量范围、二次项系数。

`ConstraintChecker` 负责全局可行性校验：总需求是否位于 `[ΣPmin, ΣPmax]` 内、用户申报需求是否低于 `ΣPmin`。

校验结果使用结构化的 `ValidationReport` 返回，错误必须可定位到具体对象。

### 4.3 Clearing（出清）

- `ClearingEngine`：抽象统一接口；
- `PiecewiseClearing`：分段报价双指针撮合；
- `QuadraticClearing`：二次曲线 KKT/λ 二分求解。

算法模块只接收 `MarketInput`、只返回 `MarketResult`，不依赖 Qt，不输出 UI 内容。

### 4.4 Settlement（结算）

`SettlementEngine` 根据 `MarketInput + MarketResult` 计算发电收益/成本/利润、用户支付、总支付与总收益平衡，输出 `SettlementResult`。

### 4.5 TradingCenter（交易中心服务）

应用服务门面，编排：校验 -> 出清 -> 结算 -> 96 时段仿真。UI 只依赖此模块。

### 4.6 Data（数据层）

- `CSVReader / CSVWriter`：通用 CSV 读写，不包含业务规则；
- `SQLiteManager`：结果与历史数据持久化。

### 4.7 UI（展示层）

- `MainWindow`：主窗口与页签；
- `GeneratorWidget`：发电侧报价录入；
- `ConsumerWidget`：用户侧需求录入；
- `TradingCenterWidget`：运行仿真、展示结果。

UI 不得直接调用算法，只能调用 `TradingCenter`。

### 4.8 Test（测试）

- `ClearingTest`：分段撮合用例；
- `SettlementTest`：结算与平衡用例；
- `OptimizationTest`：二次曲线优化与 KKT 边界用例。

## 5. 依赖矩阵

| 层 | 允许依赖 | 禁止依赖 |
| --- | --- | --- |
| Model | 标准库 | Validation / Clearing / Settlement / Data / UI / Qt |
| Validation | Model、标准库 | Clearing / Settlement / UI / Qt |
| Clearing | Model、标准库 | Validation / UI / Qt |
| Settlement | Model、标准库 | UI / Qt |
| TradingCenter | Model、Validation、Clearing、Settlement | UI / Qt |
| Data | Model、标准库、Qt Sql | Validation / Clearing / Settlement |
| UI | Model、TradingCenter、Qt Widgets | 直接调用 ClearingEngine / SettlementEngine |
| Test | Model、Clearing、Settlement | UI（默认） |

## 6. 统一接口示例

所有出清算法必须保持以下调用形态：

```cpp
pms::MarketInput input;
// 组装 input ...

pms::ClearingEngine& engine = pms::TradingCenter::engineFor(input); // 或直接注入具体引擎
pms::MarketResult result = engine.clear(input);

pms::SettlementResult settlement = pms::SettlementEngine().settle(input, result);
```

UI 中的合法写法只到 `TradingCenter`：

```cpp
pms::TradingCenter tradingCenter;
std::vector<pms::MarketInput> inputs; // 96 个时段
std::vector<pms::TimeSlotResult> results = tradingCenter.runDayAheadSimulation(inputs);
```

## 7. 文件与类映射

| 目录 | 类/模块 | 说明 |
| --- | --- | --- |
| `Model/Generator` | `Generator` | 发电机组 |
| `Model/Consumer` | `Consumer` | 用户 |
| `Model/BidSegment` | `BidSegment` | 报价段 |
| `Model/BidSheet` | `BidSheet` | 申报单 |
| `Model/MarketInput` | `MarketInput`、`MarketMode` | 市场输入 |
| `Model/MarketResult` | `MarketResult`、`GeneratorResult`、`ConsumerResult` | 出清结果 |
| `Model/SettlementResult` | `SettlementResult`、`GeneratorSettlement`、`ConsumerSettlement` | 结算结果 |
| `Validation/BidValidator` | `BidValidator`、`ValidationReport`、`ValidationIssue` | 报价校验 |
| `Validation/ConstraintChecker` | `ConstraintChecker` | 全局可行性 |
| `Clearing/ClearingEngine` | `ClearingEngine` | 统一算法接口 |
| `Clearing/PiecewiseClearing` | `PiecewiseClearing` | 分段撮合 |
| `Clearing/QuadraticClearing` | `QuadraticClearing` | 二次优化 |
| `Settlement/SettlementEngine` | `SettlementEngine` | 结算 |
| `TradingCenter` | `TradingCenter`、`TimeSlotResult` | 交易中心门面 |
| `Data/CSVReader` | `CSVReader` | CSV 读取 |
| `Data/CSVWriter` | `CSVWriter` | CSV 写出 |
| `Data/SQLiteManager` | `SQLiteManager` | SQLite 持久化 |
| `UI/MainWindow` | `MainWindow` | 主窗口 |
| `UI/GeneratorWidget` | `GeneratorWidget` | 发电录入 |
| `UI/ConsumerWidget` | `ConsumerWidget` | 用户录入 |
| `UI/TradingCenterWidget` | `TradingCenterWidget` | 仿真与结果展示 |

## 8. 代码与命名约束

1. 命名空间统一使用 `namespace pms`。
2. 头文件统一使用 `#pragma once`。
3. 文件名、类名、目录名必须与本节映射表一致。
4. `Model / Validation / Clearing / Settlement` 禁止出现 Qt 头文件。
5. 所有 `MarketInput` 校验必须先经过 `BidValidator` 与 `ConstraintChecker`，再进入出清。
6. 新增算法必须实现 `ClearingEngine` 接口，不得绕过统一接口。
7. 新增读写功能必须放入 `Data` 层，UI 不直接操作文件/SQLite。
8. 小数规则、边界规则、同价规则必须在 `DESIGN.md` 中先定义，再写代码。

## 9. 开发流程约束

1. 开发任何功能前，先阅读本文件与 `DESIGN.md`。
2. 涉及算法、价格规则或边界行为的修改，必须先更新 `DESIGN.md`，再修改代码与测试。
3. 新功能必须包含对应测试，测试放入 `Test` 对应目录。
4. 不破坏统一接口 `ClearingEngine::clear(const MarketInput&)`。
5. 不改变分层与依赖方向，不重新设计项目方向。

## 10. 旧文件迁移说明

当前工作区状态：

- `D:\桌面\新建文件夹` 下仅存在课程设计文档（`.docx`、`.pdf`），没有旧 C++ 工程代码；
- `C:\Users\diamo\Documents\ChatGPT\程序设计` 为独立的 JS 演示工程，与本 C++ 项目无关。

因此本次整理**不移动任何旧代码文件**，课程文档保留原位，工程从 `PowerMarketSimulation/` 开始建立。

若后续出现旧代码，迁移规则：

| 旧内容类型 | 迁移目标 |
| --- | --- |
| 市场主体/报价/结果数据类 | `Model` |
| 报价/约束校验 | `Validation` |
| 撮合/优化算法 | `Clearing` |
| 结算计算 | `Settlement` |
| CSV/SQLite 读写 | `Data` |
| 交易中心编排 | `TradingCenter` |
| 窗口与控件 | `UI` |
| 算法/结算测试 | `Test` |

## 11. 禁止事项

- 禁止 UI 直接调用 `ClearingEngine`、`SettlementEngine`、`BidValidator`。
- 禁止算法层包含 Qt 头文件。
- 禁止 `Data` 层依赖 `Validation / Clearing / Settlement` 业务逻辑。
- 禁止在 `Model` 中编写撮合、优化、结算算法。
- 禁止使用新目录名、新分层替代本文件定义的结构。
