# 🚀 Doris Go SDK

[![Go Version](https://img.shields.io/badge/Go-%3E%3D%201.19-blue.svg)](https://golang.org/)
[![License](https://img.shields.io/badge/License-Apache%202.0-green.svg)](https://opensource.org/licenses/Apache-2.0)
[![Thread Safe](https://img.shields.io/badge/Thread%20Safe-✅-brightgreen.svg)](#-并发安全)

高性能、生产就绪的 Apache Doris Stream Load Go 客户端。简洁的 API 设计，强大的功能支持。

## ✨ 主要特性

- 🎯 **简洁直观** - 直接构造配置，告别繁琐的链式调用
- 🔄 **智能重试** - 双重限制（次数+时长）+ 指数退避算法
- 📊 **多格式支持** - JSON Lines、JSON Array、CSV 等格式
- ⚡ **高性能** - 优化的连接池 + 并发处理（50 并发连接）
- 🛡️ **线程安全** - 客户端可安全地在多个 goroutine 间共享
- 🔍 **详细日志** - 完整的请求追踪和错误诊断
- 📈 **生产级** - 内置最佳实践，支持大规模数据加载

## 📦 快速安装

```bash
go get github.com/selectdb/go-doris-sdk
```


### 基础 CSV 加载

```go
package main

import (
	"fmt"
	"github.com/selectdb/go-doris-sdk"
)

func main() {
	// 🎯 新版 API：直接构造配置
	config := &doris.Config{
		Endpoints:   []string{"http://127.0.0.1:8030"},
		User:        "root",
		Password:    "password",
		Database:    "test_db",
		Table:       "users",
		Format:      doris.DefaultCSVFormat(),
		Retry:       doris.DefaultRetry(),
		GroupCommit: doris.ASYNC,
	}

	// 创建客户端
	client, err := doris.NewLoadClient(config)
	if err != nil {
		panic(err)
	}

	// 加载数据
	data := "1,Alice,25\n2,Bob,30\n3,Charlie,35"
	response, err := client.Load(doris.StringReader(data))
	
	if err != nil {
		fmt.Printf("❌ 加载失败: %v\n", err)
		return
	}

	if response.Status == doris.SUCCESS {
		fmt.Printf("✅ 成功加载 %d 行数据！\n", response.Resp.NumberLoadedRows)
	}
}
```

### JSON 数据加载

```go
config := &doris.Config{
	Endpoints:   []string{"http://127.0.0.1:8030"},
	User:        "root",
	Password:    "password", 
	Database:    "test_db",
	Table:       "users",
	Format:      doris.DefaultJSONFormat(), // JSON Lines 格式
	Retry:       doris.DefaultRetry(),
	GroupCommit: doris.ASYNC,
}

client, _ := doris.NewLoadClient(config)

// JSON Lines 数据
jsonData := `{"id":1,"name":"Alice","age":25}
{"id":2,"name":"Bob","age":30}
{"id":3,"name":"Charlie","age":35}`

response, err := client.Load(doris.StringReader(jsonData))
```

## 🛠️ 配置详解

### 基础配置

```go
config := &doris.Config{
	// 必需字段
	Endpoints: []string{
		"http://fe1:8630",
		"http://fe2:8630",    // 支持多 FE 节点，自动负载均衡
	},
	User:     "your_username",
	Password: "your_password",
	Database: "your_database",
	Table:    "your_table",
	
	// 可选字段
	LabelPrefix: "my_app",           // 标签前缀
	Label:       "custom_label_001", // 自定义标签
	Format:      doris.DefaultCSVFormat(),
	Retry:       doris.DefaultRetry(),
	GroupCommit: doris.ASYNC,
	Options: map[string]string{
		"timeout":           "3600",
		"max_filter_ratio":  "0.1",
		"strict_mode":       "true",
	},
}
```

### 数据格式配置

```go
// 1. 使用默认格式（推荐）
Format: doris.DefaultJSONFormat()  // JSON Lines, read_json_by_line=true
Format: doris.DefaultCSVFormat()   // CSV, 逗号分隔，换行符分割

// 2. 自定义 JSON 格式
Format: &doris.JSONFormat{Type: doris.JSONObjectLine}  // JSON Lines
Format: &doris.JSONFormat{Type: doris.JSONArray}       // JSON Array

// 3. 自定义 CSV 格式  
Format: &doris.CSVFormat{
	ColumnSeparator: "|",     // 管道符分隔
	LineDelimiter:   "\n",    // 换行符
}
```

### 重试策略配置

```go
// 1. 使用默认重试（推荐）
Retry: doris.DefaultRetry()  // 6次重试，总时长60秒
// 重试间隔: [1s, 2s, 4s, 8s, 16s, 32s]

// 2. 自定义重试
Retry: &doris.Retry{
	MaxRetryTimes:  3,      // 最大重试次数
	BaseIntervalMs: 2000,   // 基础间隔 2 秒
	MaxTotalTimeMs: 30000,  // 总时长限制 30 秒
}

// 3. 禁用重试
Retry: nil
```

### Group Commit 模式

```go
GroupCommit: doris.ASYNC,  // 异步模式，最高吞吐量
GroupCommit: doris.SYNC,   // 同步模式，立即可见
GroupCommit: doris.OFF,    // 关闭，使用传统模式
```

> ⚠️ **注意**: 启用 Group Commit 时，所有 Label 配置会被自动忽略并记录警告日志。

## 🔄 并发使用

### 基础并发示例

```go
func worker(id int, client *doris.DorisLoadClient, wg *sync.WaitGroup) {
	defer wg.Done()
	
	// ✅ 每个 worker 使用独立的数据
	data := fmt.Sprintf("%d,Worker_%d,Data", id, id)
	
	response, err := client.Load(doris.StringReader(data))
	if err != nil {
		fmt.Printf("Worker %d 失败: %v\n", id, err)
		return
	}
	
	if response.Status == doris.SUCCESS {
		fmt.Printf("✅ Worker %d 成功加载 %d 行\n", id, response.Resp.NumberLoadedRows)
	}
}

func main() {
	client, _ := doris.NewLoadClient(config)
	
	var wg sync.WaitGroup
	// 🚀 启动 10 个并发 worker
	for i := 0; i < 10; i++ {
		wg.Add(1)
		go worker(i, client, &wg)
	}
	wg.Wait()
}
```

### ⚠️ 并发安全要点

- ✅ **DorisLoadClient 是线程安全的** - 可以在多个 goroutine 间共享
- ❌ **Reader 不应该共享** - 每个 goroutine 应使用独立的数据源

```go
// ✅ 正确的并发模式
for i := 0; i < numWorkers; i++ {
	go func(workerID int) {
		data := generateWorkerData(workerID)  // 独立数据
		response, err := client.Load(doris.StringReader(data))
	}(i)
}

// ❌ 错误的并发模式 - 不要这样做！
file, _ := os.Open("data.csv")
for i := 0; i < 10; i++ {
	go func() {
		client.Load(file)  // ❌ 多个 goroutine 共享同一个 Reader
	}()
}
```

## 📊 响应处理

```go
response, err := client.Load(data)

// 1. 检查系统级错误
if err != nil {
	fmt.Printf("系统错误: %v\n", err)
	return
}

// 2. 检查加载状态
switch response.Status {
case doris.SUCCESS:
	fmt.Printf("✅ 加载成功！\n")
	fmt.Printf("📊 统计信息:\n")
	fmt.Printf("  - 加载行数: %d\n", response.Resp.NumberLoadedRows)
	fmt.Printf("  - 加载字节: %d\n", response.Resp.LoadBytes)
	fmt.Printf("  - 耗时: %d ms\n", response.Resp.LoadTimeMs)
	fmt.Printf("  - 标签: %s\n", response.Resp.Label)
	
case doris.FAILURE:
	fmt.Printf("❌ 加载失败: %s\n", response.ErrorMessage)
	
	// 获取详细错误信息
	if response.Resp.ErrorURL != "" {
		fmt.Printf("🔍 错误详情: %s\n", response.Resp.ErrorURL)
	}
}
```

## 🔍 日志控制

### 基础日志配置

```go
// 设置日志级别
doris.SetLogLevel(doris.LogLevelInfo)   // 生产环境推荐
doris.SetLogLevel(doris.LogLevelDebug)  // 开发调试
doris.SetLogLevel(doris.LogLevelError)  // 只显示错误

// 禁用所有日志
doris.DisableLogging()

// 输出到文件
file, _ := os.OpenFile("doris.log", os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0644)
doris.SetLogOutput(file)
```

### 并发场景日志

```go
// 为每个 worker 创建上下文日志器
logger := doris.NewContextLogger("Worker-1")
logger.Infof("开始处理批次 %d", batchID)
logger.Warnf("检测到重试，尝试次数: %d", retryCount)
```

### 集成第三方日志库

```go
import "github.com/sirupsen/logrus"

logger := logrus.New()
logger.SetLevel(logrus.InfoLevel)

// 集成到 Doris SDK
doris.SetCustomLogFuncs(
	logger.Debugf,  // Debug 级别
	logger.Infof,   // Info 级别  
	logger.Warnf,   // Warn 级别
	logger.Errorf,  // Error 级别
)
```

## 📈 生产级示例

我们提供了完整的生产级示例

```bash
# 运行所有示例
go run cmd/examples/main.go all

# 单个示例
go run cmd/examples/main.go single      # 大批量加载 (10万条)
go run cmd/examples/main.go concurrent  # 并发加载 (100万条, 10 workers)  
go run cmd/examples/main.go json        # JSON 加载 (5万条)
go run cmd/examples/main.go basic       # 基础并发 (5 workers)
```

## 🛠️ 实用工具

### 数据转换助手

```go
// 字符串转 Reader
reader := doris.StringReader("1,Alice,25\n2,Bob,30")

// 字节数组转 Reader  
data := []byte("1,Alice,25\n2,Bob,30")
reader := doris.BytesReader(data)

// 结构体转 JSON Reader
users := []User{{ID: 1, Name: "Alice"}}
reader, err := doris.JSONReader(users)
```

### 默认配置构建器

```go
// 快速创建常用配置
retry := doris.DefaultRetry()        // 6次重试，60秒总时长
jsonFormat := doris.DefaultJSONFormat() // JSON Lines 格式
csvFormat := doris.DefaultCSVFormat()   // 标准 CSV 格式

// 自定义配置
customRetry := doris.NewRetry(3, 1000) // 3次重试，1秒基础间隔
```

## 📚 文档和示例

- 📖 [API 迁移指南](docs/API_MIGRATION_GUIDE.md) - 从旧 API 升级指南
- 🧵 [线程安全分析](docs/THREAD_SAFETY_ANALYSIS.md) - 详细的并发安全说明
- 🔍 [Reader 并发分析](docs/READER_CONCURRENCY_ANALYSIS.md) - Reader 使用最佳实践
- 📝 [示例详解](examples/README.md) - 所有示例的详细说明



## 📄 许可证

本项目采用 [Apache License 2.0](LICENSE) 许可证。
