# 📚 Doris Go SDK - 示例集合

高质量的生产级示例，展示 Doris Go SDK 的强大功能和最佳实践。

## 🚀 快速运行

```bash
# 运行单个示例
go run cmd/examples/main.go basic       # 基础并发示例 (5 workers)
go run cmd/examples/main.go single      # 大批量加载 (10万条记录)
go run cmd/examples/main.go concurrent  # 高并发加载 (100万条记录, 10 workers)
go run cmd/examples/main.go json        # JSON 数据加载 (5万条记录)

# 运行所有示例
go run cmd/examples/main.go all
```

## 📊 示例概览

| 示例 | 数据量 | 格式 | 并发数 | 适用场景 | 预计耗时 |
|------|--------|------|--------|----------|----------|
| `basic` | 25条 | CSV | 5 | 学习和开发 | <1秒 |
| `single` | 10万条 | CSV | 1 | 大批量单线程加载 | 2-5秒 |
| `concurrent` | 100万条 | CSV | 10 | 高吞吐生产环境 | 10-30秒 |
| `json` | 5万条 | JSON Lines | 1 | 结构化数据加载 | 2-4秒 |

## 🎯 新版 API 特性展示

### 1. 简洁的配置构造

所有示例都使用新版直接构造配置的方式：

```go
// ✅ 新版 API - 直观清晰
config := &doris.Config{
    Endpoints:   []string{"http://10.16.10.6:8630"},
    User:        "root",
    Password:    "123456",
    Database:    "test",
    Table:       "orders",
    Format:      doris.DefaultCSVFormat(),
    Retry:       doris.DefaultRetry(),
    GroupCommit: doris.ASYNC,
}
```

### 2. 智能重试机制

```go
// 默认重试（推荐）- 6次重试，60秒总时长限制
Retry: doris.DefaultRetry()

// 自定义重试
Retry: doris.NewRetry(3, 2000)  // 3次重试，2秒基础间隔

// 示例中的重试配置对比
production_single_batch_example.go:    doris.NewRetry(3, 2000)
production_concurrent_example.go:      doris.NewRetry(5, 1000)  
production_json_example.go:            doris.NewRetry(3, 2000)
concurrent_load_example.go:            doris.DefaultRetry()
```

### 3. 格式配置对比

```go
// CSV 格式
Format: doris.DefaultCSVFormat()              // 默认：逗号分隔
Format: &doris.CSVFormat{                     // 自定义分隔符
    ColumnSeparator: "|",
    LineDelimiter:   "\n",
}

// JSON 格式  
Format: doris.DefaultJSONFormat()             // JSON Lines
Format: &doris.JSONFormat{Type: doris.JSONArray}  // JSON Array
```

## 🗃️ 统一数据架构

所有示例使用一致的 **orders** 表结构，确保兼容性：

```sql
create table `orders`
(
    OrderID     varchar(200),
    CustomerID  varchar(200),
    ProductName varchar(200),
    Category    varchar(200),
    Brand       varchar(200),
    Quantity    varchar(200),
    UnitPrice   varchar(200),
    TotalAmount varchar(200),
    Status      varchar(200),
    OrderDate   varchar(200),
    Region      varchar(200)
) duplicate key(OrderID)
distributed by hash(OrderID) buckets 10
properties("replication_num"="1");
```

## 🔧 环境配置

### 1. 系统要求

```bash
# Go 版本
go version  # 需要 Go 1.19+

# 依赖安装
go mod tidy
```

### 2. Doris 环境设置

```sql
-- 创建数据库
CREATE DATABASE IF NOT EXISTS test;

-- 使用数据库
USE test;

-- 创建订单表（如上面的表结构）
```

### 3. 网络连接

确保可以访问 Doris FE 节点：

```bash
curl http://10.16.10.6:8630/api/test/orders/_stream_load \
  -u root:123456 \
  -H "label:test_connection" \
  -T /dev/null
```

## 🔍 示例详解

### 1. 基础并发示例 (`basic`)

**文件**: `concurrent_load_example.go`

**特点**:
- 5个并发 worker
- 每个 worker 处理独立的数据
- 完整的错误处理和日志记录
- 线程安全演示

**关键代码**:
```go
func workerFunction(workerID int, client *doris.DorisLoadClient, wg *sync.WaitGroup) {
    defer wg.Done()
    
    // 创建上下文日志器
    workerLogger := doris.NewContextLogger(fmt.Sprintf("Worker-%d", workerID))
    
    // 生成独立数据
    data := GenerateSimpleOrderCSV(workerID)
    
    // 执行加载
    response, err := client.Load(doris.StringReader(data))
    
    // 处理结果...
}
```

### 2. 大批量单线程示例 (`single`)

**文件**: `production_single_batch_example.go`

**特点**:
- 10万条记录批量加载
- 内存效率优化
- 详细性能指标记录
- 生产级错误处理

**配置亮点**:
```go
config := &doris.Config{
    Endpoints:   []string{"http://10.16.10.6:8630"},
    LabelPrefix: "prod_batch",
    Format:      doris.DefaultCSVFormat(),
    Retry:       doris.NewRetry(3, 2000),  // 3次重试
    GroupCommit: doris.ASYNC,               // 异步模式
}
```

### 3. 高并发生产示例 (`concurrent`)

**文件**: `production_concurrent_example.go`

**特点**:
- 100万条记录
- 10个并发 worker
- 原子统计操作
- 进度监控
- 结果聚合分析

**并发模式**:
```go
const (
    TOTAL_RECORDS     = 1000000  // 100万条记录
    NUM_WORKERS       = 10       // 10个worker  
    RECORDS_PER_WORKER = 100000  // 每个worker 10万条
)

// 全局统计（线程安全）
type GlobalStats struct {
    TotalProcessed int64
    TotalFailed    int64
    mutex          sync.RWMutex
}
```

### 4. JSON 数据示例 (`json`)

**文件**: `production_json_example.go`

**特点**:
- 5万条 JSON 记录
- JSON Lines 格式
- 结构化数据处理
- JSON 特定优化

**JSON 配置**:
```go
Format: &doris.JSONFormat{Type: doris.JSONObjectLine}  // JSON Lines
```

## 📈 性能基准

### 数据生成性能

| 生成器类型 | 速度 | 内存使用 | 适用场景 |
|------------|------|----------|----------|
| `GenerateSimpleOrderCSV` | ~960k records/sec | 低 | 测试和开发 |
| `GenerateOrderCSV` | ~850k records/sec | 中 | 生产环境 |
| `GenerateOrderJSON` | ~855k records/sec | 中 | JSON 数据 |

### 加载性能参考

| 示例 | 吞吐量 | 网络开销 | CPU 使用率 | 内存峰值 |
|------|--------|----------|------------|----------|
| Single | 2-5万条/秒 | 低 | 中 | 20-50MB |
| Concurrent | 3-10万条/秒 | 中 | 高 | 100-200MB |
| JSON | 1.2-2.5万条/秒 | 中 | 中 | 30-60MB |

## 🔧 日志和调试

### 日志级别控制

```go
// 开发环境 - 查看详细信息
doris.SetLogLevel(doris.LogLevelDebug)

// 生产环境 - 只显示重要信息
doris.SetLogLevel(doris.LogLevelInfo)

// 性能测试 - 减少日志输出
doris.SetLogLevel(doris.LogLevelError)
```

### 并发场景日志

```go
// 为每个 worker 创建独立的日志上下文
logger := doris.NewContextLogger("Worker-1")
logger.Infof("开始处理批次 %d", batchID)
logger.Warnf("检测到重试，尝试次数: %d", retryCount)
logger.Errorf("处理失败: %v", err)
```

### 输出示例

```bash
# 成功加载的输出
✅ Worker-1 completed in 2.34s
📋 Worker-1: Label=prod_concurrent_test_orders_1699123456789_retry_0_abc123, Rows=100000

# 并发统计输出
🎉 All 10 workers completed in 15.67s!
📊 Total records processed: 1000000
📈 Overall throughput: 63,776 records/sec
💥 Failed workers: 0
⏱️  Average worker time: 12.34s
```

## 🛠️ 配置最佳实践

### 1. 重试策略

```go
// 网络不稳定环境
Retry: &doris.Retry{
    MaxRetryTimes:  5,      // 更多重试次数
    BaseIntervalMs: 2000,   // 较长的基础间隔
    MaxTotalTimeMs: 120000, // 2分钟总时长
}

// 稳定网络环境
Retry: doris.DefaultRetry()  // 默认配置即可

// 快速失败场景
Retry: &doris.Retry{
    MaxRetryTimes:  2,      // 快速失败
    BaseIntervalMs: 500,    // 短间隔
    MaxTotalTimeMs: 10000,  // 10秒总时长
}
```

### 2. Group Commit 选择

```go
GroupCommit: doris.ASYNC,  // 高吞吐量，推荐用于批量导入
GroupCommit: doris.SYNC,   // 立即可见，用于实时需求
GroupCommit: doris.OFF,    // 传统模式，用于需要自定义 Label 的场景
```

### 3. 并发参数调优

```go
// CPU 密集型
numWorkers := runtime.NumCPU()

// I/O 密集型  
numWorkers := runtime.NumCPU() * 2

// 高并发网络
numWorkers := 10-20  // 根据网络带宽调整
```

## ⚠️ 注意事项

### 1. 并发安全

```go
// ✅ 正确：每个 goroutine 使用独立数据
for i := 0; i < numWorkers; i++ {
    go func(workerID int) {
        data := generateWorkerData(workerID)  // 独立数据源
        response, err := client.Load(doris.StringReader(data))
    }(i)
}

// ❌ 错误：共享 Reader 导致竞争条件
file, _ := os.Open("data.csv")
for i := 0; i < 10; i++ {
    go func() {
        client.Load(file)  // 多个 goroutine 共享同一个 Reader！
    }()
}
```

### 2. 内存管理

```go
// 大批量数据分块处理
const BATCH_SIZE = 100000  // 10万条一批

for i := 0; i < totalRecords; i += BATCH_SIZE {
    end := i + BATCH_SIZE
    if end > totalRecords {
        end = totalRecords
    }
    
    batch := generateBatch(i, end)
    response, err := client.Load(doris.StringReader(batch))
    
    // 及时释放内存
    batch = nil
    runtime.GC()
}
```

### 3. 错误处理

```go
// 完整的错误处理模式
response, err := client.Load(data)

// 1. 系统级错误
if err != nil {
    log.Errorf("系统错误: %v", err)
    return
}

// 2. 业务级错误
if response.Status != doris.SUCCESS {
    log.Errorf("加载失败: %s", response.ErrorMessage)
    if response.Resp.ErrorURL != "" {
        log.Errorf("详细错误: %s", response.Resp.ErrorURL)
    }
    return
}

// 3. 成功处理
log.Infof("✅ 成功加载 %d 行", response.Resp.NumberLoadedRows)
```

## 🔍 故障排查

### 常见问题

1. **连接超时**
   ```go
   config.Options = map[string]string{"timeout": "7200"}
   ```

2. **数据格式错误**
   ```go
   config.Options = map[string]string{"strict_mode": "true"}
   ```

3. **内存不足**
   - 减少批次大小 (`BATCH_SIZE`)
   - 减少并发数 (`NUM_WORKERS`)
   - 增加系统内存

4. **网络不稳定**
   - 增加重试次数和总时长
   - 检查网络连接
   - 使用更保守的并发设置

### 调试技巧

```bash
# 1. 查看详细日志
export DORIS_SDK_LOG_LEVEL=debug
go run cmd/examples/main.go basic

# 2. 性能分析
go run -race cmd/examples/main.go concurrent  # 竞争检测
go run cmd/examples/main.go single > performance.log  # 性能日志

# 3. 内存分析
go run cmd/examples/main.go concurrent -memprofile=mem.prof
go tool pprof mem.prof
```

## 📁 文件结构

```
examples/
├── cmd/examples/main.go                    # 统一入口点
├── concurrent_load_example.go              # 基础并发示例
├── production_single_batch_example.go      # 大批量单线程
├── production_concurrent_example.go        # 高并发生产级
├── production_json_example.go              # JSON 数据处理
├── simple_config_example.go                # 简单配置示例
├── format_usage_example.go                 # 格式使用示例
├── label_removal_demo.go                   # Label 删除日志演示
├── data_generator.go                       # 统一数据生成器
└── README.md                               # 本文档
```

## 🚀 下一步

1. **运行基础示例**: `go run cmd/examples/main.go basic`
2. **阅读源码**: 查看 `concurrent_load_example.go` 了解并发模式
3. **自定义配置**: 修改示例中的配置适配你的环境
4. **生产部署**: 参考 `production_*` 示例进行生产环境部署

## 🤝 贡献

欢迎提交新的示例或改进现有示例！请确保：

- 遵循统一的代码风格
- 包含详细的注释和错误处理
- 使用 `orders` 表结构保持一致性
- 添加适当的性能测试和基准

---

**💡 提示**: 这些示例展示了 Doris Go SDK 的强大功能，从简单的学习案例到复杂的生产级应用。选择最适合你需求的示例作为起点！ 