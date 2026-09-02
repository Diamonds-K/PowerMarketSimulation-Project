# PowerMarketSimulation

基于 C++/Qt 的电力现货市场（日前市场）出清仿真平台。

## 功能

- 发电侧分段报价与用户侧需求申报
- 交易中心统一出清与统一价格结算
- 96 个 15 分钟时段日前市场仿真
- 分段报价双指针撮合（模式 1）
- 二次曲线报价 KKT/λ 二分求解（模式 2）
- CSV 导入导出与 SQLite 持久化

## 构建

需要 CMake 3.16+、C++17、Qt 6（Widgets + Sql）。

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

## 开发约束

所有代码开发前必须先阅读 `ARCHITECTURE.md` 与 `DESIGN.md`。
