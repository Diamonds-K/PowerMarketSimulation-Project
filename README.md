# PowerMarketSimulation

基于 C++/Qt 的电力现货市场（日前市场）出清仿真平台。

## 功能

三类市场角色各有独立页面，展示的都是本角色自己的结果：

- **发电侧**：提交分段报价单或二次成本曲线，查看中标出力、统一出清电价、上网收益、报价成本与净利润；
- **用户侧**：提交需求申报，查看申报电量、成交电量、缺额与应交电费（账单）；
- **交易中心**：查看全部市场主体申报总览、执行单时段或全天 96 时段出清、公布出清电价、查看供需曲线与 96 点电价曲线、导出结果、保存结果到 SQLite。

## 算法

- 96 个 15 分钟时段日前市场仿真（`0..95`，界面显示 `1..96`）；
- 模式 1 分段报价：双指针逐段撮合，支持部分成交、供需不平衡与缺额；
- 模式 2 二次曲线报价：KKT 条件 + λ 二分求解，需求刚性 `QD`；
- 统一出清价结算，`总支付 = 总收益` 平衡校验；
- 必发电量 `Pmin` 按用户申报量比例分摊到用户（见 `DESIGN.md` 第 7.2 节）。

## 数据

- CSV 导入：机组参数、用户参数、发电分段报价、用户分段报价；
- CSV 导出：96 时段出清汇总、各主体结算明细（机组收入 / 用户账单）；
- SQLite 持久化：交易中心页可把 96 个时段的申报、出清与结算结果写入 `.db` 文件。

`testdata/` 下提供可直接导入的演示数据：

```text
testdata/generators.csv      机组参数
testdata/consumers.csv       用户参数
testdata/bids_generator.csv  发电分段报价（96 时段示例）
testdata/bids_consumer.csv   用户分段报价（96 时段示例）
```

各文件列定义见 `DESIGN.md` 第 8.1 节。

## 构建

需要 CMake 3.16+、C++17、Qt 6（Widgets + Sql + Charts）。

```bash
cmake -S . -B build
cmake --build build
```

运行：

```bash
./build/PowerMarketSimulation
```

运行测试：

```bash
ctest --test-dir build --output-on-failure
```

测试包含 `ClearingTest`、`SettlementTest`、`OptimizationTest`、`ValidationTest`、`TradingCenterTest` 共 5 个可执行目标。

## 开发约束

所有代码开发前必须先阅读 `ARCHITECTURE.md` 与 `DESIGN.md`：

- 算法、价格与边界规则的修改，先改 `DESIGN.md`，再改代码与测试；
- 新增或修改算法必须同步补充 `Test` 用例；
- UI 只能通过 `TradingCenter` 出清，页面内的市场数据组装统一由 `UI/SimulationRunner` 负责。
