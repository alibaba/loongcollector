# 日志控制指南

**✨ 统一API设计** - Doris Stream Load Client 提供完整的日志控制功能，所有功能都通过主包提供，无需导入额外的包。

## 🚀 快速开始

只需导入主包即可使用所有日志功能：

```go
import doris "github.com/bingquanzhao/doris-stream-load-client"

func main() {
    // 设置日志级别
    doris.SetLogLevel(doris.LogLevelInfo)
    
    // 创建客户端并使用
    setting := doris.NewLoadSetting().
        AddFeNodes("http://127.0.0.1:8630").
        SetUser("root").
        SetPassword("password").
        Database("test").
        Table("users")
    
    client, err := doris.NewLoadClient(setting)
    if err != nil {
        // 错误会自动记录（如果启用了ERROR级别）
        return
    }
    
    // 客户端操作会自动产生日志
    response, err := client.Load(doris.StringReader("data"))
}
```

## 📊 日志级别控制

### 设置日志级别

```go
// 显示所有日志（开发环境）
doris.SetLogLevel(doris.LogLevelDebug)

// 显示信息、警告、错误（生产推荐）
doris.SetLogLevel(doris.LogLevelInfo)

// 只显示警告和错误
doris.SetLogLevel(doris.LogLevelWarn)

// 只显示错误
doris.SetLogLevel(doris.LogLevelError)
```

### 完全禁用日志

```go
// 禁用所有SDK日志输出
doris.DisableLogging()
```

## 🔧 日志输出控制

### 输出到文件

```go
import "os"

// 输出到文件
file, err := os.OpenFile("doris-sdk.log", os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0666)
if err == nil {
    doris.SetLogOutput(file)
}

// 记住在程序结束时关闭文件
defer file.Close()
```

### 输出到标准错误

```go
import "os"

// 输出到标准错误（默认是标准输出）
doris.SetLogOutput(os.Stderr)
```

## 🏷️ 上下文日志记录器

在并发场景中，可以创建带上下文的日志记录器来追踪特定的操作：

```go
// 创建带上下文的日志记录器
workerLogger := doris.NewContextLogger("Worker-1")
workerLogger.Infof("Starting to process batch %d", batchID)
workerLogger.Errorf("Failed to process record %d: %v", recordID, err)

// 在并发场景中的完整示例
func workerFunction(workerID int, client *doris.DorisLoadClient) {
    logger := doris.NewContextLogger(fmt.Sprintf("Worker-%d", workerID))
    
    logger.Infof("Worker started")
    response, err := client.Load(data)
    if err != nil {
        logger.Errorf("Load failed: %v", err)
        return
    }
    logger.Infof("Load completed: %d rows", response.Resp.NumberLoadedRows)
}
```

## 🔌 集成自定义日志系统

### 使用 logrus

```go
import "github.com/sirupsen/logrus"

func setupLogrusIntegration() {
    logger := logrus.New()
    logger.SetLevel(logrus.InfoLevel)
    
    // 方法1: 逐个设置
    doris.SetCustomLogFunc(doris.LogLevelError, logger.Errorf)
    doris.SetCustomLogFunc(doris.LogLevelWarn, logger.Warnf)
    doris.SetCustomLogFunc(doris.LogLevelInfo, logger.Infof)
    doris.SetCustomLogFunc(doris.LogLevelDebug, logger.Debugf)
    
    // 方法2: 一次性设置（推荐）
    doris.SetCustomLogFuncs(logger.Debugf, logger.Infof, logger.Warnf, logger.Errorf)
}
```

### 使用 zap

```go
import "go.uber.org/zap"

func setupZapIntegration() {
    logger, _ := zap.NewProduction()
    sugar := logger.Sugar()
    defer logger.Sync()
    
    doris.SetCustomLogFuncs(
        sugar.Debugf,
        sugar.Infof, 
        sugar.Warnf,
        sugar.Errorf,
    )
}
```

### 使用 slog (Go 1.21+)

```go
import (
    "log/slog"
    "fmt"
)

func setupSlogIntegration() {
    logger := slog.Default()
    
    doris.SetCustomLogFuncs(
        func(format string, args ...interface{}) {
            logger.Debug(fmt.Sprintf(format, args...))
        },
        func(format string, args ...interface{}) {
            logger.Info(fmt.Sprintf(format, args...))
        },
        func(format string, args ...interface{}) {
            logger.Warn(fmt.Sprintf(format, args...))
        },
        func(format string, args ...interface{}) {
            logger.Error(fmt.Sprintf(format, args...))
        },
    )
}
```

## 📋 日志输出格式

SDK自动记录详细的操作信息：

```
[2025/06/03 16:19:49.999] [INFO ] [G-1] [concurrent_load_example.go:61] [ConcurrentDemo] Starting concurrent loading demo
[2025/06/03 16:19:49.999] [INFO ] [G-35] [concurrent_load_example.go:29] Starting stream load operation
[2025/06/03 16:19:49.999] [INFO ] [G-35] [concurrent_load_example.go:29] Target: test.orders (endpoint: 10.16.10.6:8630)
[2025/06/03 16:19:49.999] [INFO ] [G-35] [concurrent_load_example.go:29] Label: demo_concurrent_test_orders_1748938789999
[2025/06/03 16:19:50.262] [INFO ] [G-35] [stream_loader.go:63] Stream Load Response: {
    "TxnId": 35063,
    "Label": "group_commit_e847dff4018cb1d3_13ea36b3d5e7c1a6",
    "Status": "Success",
    "NumberLoadedRows": 2,
    "LoadBytes": 197,
    "LoadTimeMs": 11
}
[2025/06/03 16:19:50.263] [INFO ] [G-35] [stream_loader.go:63] Load operation completed successfully
```

日志格式包含：
- **时间戳**: `[2025/06/03 16:19:49.999]` - 毫秒级精度
- **级别**: `[INFO]`, `[WARN]`, `[ERROR]`, `[DEBUG]`
- **Goroutine ID**: `[G-35]` - 用于并发追踪
- **源码位置**: `[stream_loader.go:63]` - 方便调试
- **上下文**: `[ConcurrentDemo]` - 来自ContextLogger
- **消息**: 具体的日志内容

## 🏭 生产环境最佳实践

### 1. 推荐的日志级别

```go
// 开发环境 - 查看所有信息
doris.SetLogLevel(doris.LogLevelDebug)

// 生产环境 - 平衡信息量和性能
doris.SetLogLevel(doris.LogLevelInfo)

// 高负载生产环境 - 只记录关键信息
doris.SetLogLevel(doris.LogLevelError)
```

### 2. 日志文件管理

```go
import (
    "os"
    "path/filepath"
    "time"
)

func setupProductionLogging() {
    // 创建带时间戳的日志文件
    timestamp := time.Now().Format("2006-01-02")
    logFile := filepath.Join("logs", fmt.Sprintf("doris-client-%s.log", timestamp))
    
    // 确保目录存在
    os.MkdirAll("logs", 0755)
    
    // 打开文件
    file, err := os.OpenFile(logFile, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0666)
    if err == nil {
        doris.SetLogOutput(file)
        doris.SetLogLevel(doris.LogLevelInfo)
    }
}
```

### 3. 与监控系统集成

```go
import "your-monitoring-system/logger"

func setupMonitoringIntegration() {
    // 集成到监控系统
    doris.SetCustomLogFunc(doris.LogLevelError, func(format string, args ...interface{}) {
        message := fmt.Sprintf(format, args...)
        
        // 记录到标准日志
        log.Printf("[ERROR] %s", message)
        
        // 发送到监控系统
        monitoring.RecordError("doris-client", message)
        
        // 发送告警（如果是关键错误）
        if strings.Contains(message, "connection failed") {
            alerts.SendAlert("Doris connection failed", message)
        }
    })
    
    doris.SetCustomLogFunc(doris.LogLevelInfo, func(format string, args ...interface{}) {
        message := fmt.Sprintf(format, args...)
        log.Printf("[INFO] %s", message)
        
        // 记录成功的操作指标
        if strings.Contains(message, "Load operation completed successfully") {
            metrics.IncrementCounter("doris.load.success")
        }
    })
}
```

### 4. 性能考虑

```go
// 在高性能场景中，可以禁用调试日志
doris.SetLogLevel(doris.LogLevelWarn)

// 或完全禁用日志记录
doris.DisableLogging()

// 使用异步日志系统减少I/O阻塞
logger := logrus.New()
// 配置异步写入...
doris.SetCustomLogFuncs(logger.Debugf, logger.Infof, logger.Warnf, logger.Errorf)
```

## ⚠️ 注意事项

1. **线程安全**: 所有日志配置函数都是线程安全的，可以在运行时动态调整
2. **性能影响**: Debug级别日志会产生大量输出，生产环境建议使用Info或更高级别
3. **文件句柄**: 使用文件输出时记得在程序结束时关闭文件
4. **上下文传播**: ContextLogger的上下文信息只影响显示格式，不影响日志级别过滤

## 🔍 故障排查

### 日志未显示

1. 检查日志级别设置：
```go
doris.SetLogLevel(doris.LogLevelDebug) // 确保级别足够低
```

2. 检查是否被禁用：
```go
// 重新启用日志
doris.SetLogLevel(doris.LogLevelInfo)
```

3. 检查输出目标：
```go
import "os"
doris.SetLogOutput(os.Stdout) // 确保输出到控制台
```

### 日志过多

```go
// 提高日志级别
doris.SetLogLevel(doris.LogLevelError)

// 或完全禁用
doris.DisableLogging()
```

### 集成问题

确保自定义日志函数正确设置：
```go
// 测试自定义日志函数
doris.SetCustomLogFunc(doris.LogLevelInfo, func(format string, args ...interface{}) {
    fmt.Printf("TEST: "+format+"\n", args...)
})
```

## 📚 更多示例

查看 `examples/` 目录中的生产级示例，了解如何在实际应用中使用日志控制功能。