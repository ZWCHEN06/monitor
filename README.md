# A股指数ETF监控器

A lightweight command-line tool for monitoring A-share index ETFs' valuation metrics.

## 项目目标

帮助投资者跟踪A股指数ETF的估值状态，包括PE TTM、PB、股息率、历史估值百分位等指标，并通过本地终端进行条件预警。

## 第一版范围

- 上海证券交易所和深圳证券交易所上市的股票型指数ETF
- ETF基础信息
- ETF与跟踪指数映射
- 指数PE TTM
- 指数PB
- 指数股息率
- 历史估值百分位
- 条件筛选
- 本地终端预警
- CSV导出

## 不支持的范围

- 港股、美股
- 场外指数基金
- 债券ETF、商品ETF、跨境ETF
- 杠杆ETF、反向ETF
- 实时行情
- 自动交易
- 图形界面或Web服务
- 投资建议

## 技术栈

- C++20
- CMake + Ninja 构建
- libcurl 网络请求
- SQLite3 数据存储
- nlohmann/json JSON解析
- 命令行界面

## 开发阶段

| 阶段 | 状态 |
|------|------|
| 任务0：项目规则和文档 | 进行中 |
| 任务1：最小可编译项目 | 未开始 |
| 任务2：命令行参数解析 | 未开始 |
| 任务3：领域模型 | 未开始 |
| 任务4：SQLite数据库初始化 | 未开始 |
| 任务5：ETF关注列表 | 未开始 |
| 任务6：数据源接口和Mock | 未开始 |
| 任务7：HTTP客户端 | 未开始 |
| 任务8：JSON和CSV解析工具 | 未开始 |
| 任务9：Mock数据更新流程 | 未开始 |
| 任务10：沪深ETF基础目录 | 未开始 |
| 任务11：ETF与指数映射 | 未开始 |
| 任务12：指数估值数据源 | 未开始 |
| 任务13：基金详情查询 | 未开始 |
| 任务14：历史估值百分位 | 未开始 |
| 任务15：ETF筛选 | 未开始 |
| 任务16：本地预警 | 未开始 |
| 任务17：CSV导出 | 未开始 |
| 任务18：数据质量检查 | 未开始 |
| MVP验收 | 未开始 |

## 构建方式

```bash
# 配置
cmake -S . -B build -G Ninja

# 编译
cmake --build build

# 运行
./build/index_fund_monitor --help
```

（具体构建命令后续任务完善）