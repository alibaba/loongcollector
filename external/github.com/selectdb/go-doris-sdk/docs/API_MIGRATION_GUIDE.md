# API 迁移指南

## 概述

本指南帮助你从旧版 API 迁移到新版 Doris SDK API。新版本采用了更简洁、更直观的设计哲学。

## 主要变化

### 1. 配置方式变更

#### ❌ 旧版API（链式调用）
```go
setting := doris.NewLoadSetting().
    AddFeNodes("http://localhost:8630").
    SetUser("root").
    SetPassword("password").
    Database("test_db").
    Table("test_table").
    JsonFormat(doris.JsonObjectLine).
    Retry(doris.NewDefaultRetry()).
    BatchMode(doris.ASYNC)

client, err := doris.NewLoadClient(setting)
```

#### ✅ 新版API（直接构造）
```go
config := &doris.Config{
    Endpoints:   []string{"http://localhost:8630"},
    User:        "root",
    Password:    "password",
    Database:    "test_db",
    Table:       "test_table",
    Format:      doris.DefaultJSONFormat(),
    Retry:       doris.DefaultRetry(),
    GroupCommit: doris.ASYNC,
}

client, err := doris.NewLoadClient(config)
```

### 2. 格式配置变更

#### ❌ 旧版本
```go
// JSON格式
.JsonFormat(doris.JsonObjectLine)

// CSV格式  
.CsvFormat(",", "\\n")
```

#### ✅ 新版本
```go
// 使用默认格式
Format: doris.DefaultJSONFormat()  // JSONObjectLine
Format: doris.DefaultCSVFormat()   // "," 分隔，"\n" 换行

// 自定义格式
Format: &doris.JSONFormat{Type: doris.JSONArray}
Format: &doris.CSVFormat{
    ColumnSeparator: "|",
    LineDelimiter:   "\n",
}
```

### 3. 重试配置增强

#### ❌ 旧版本
```go
Retry(doris.NewRetry(5, 1000))  // 只有次数和间隔
```

#### ✅ 新版本
```go
// 使用默认配置（推荐）
Retry: doris.DefaultRetry()  // 6次重试，60秒总时长限制

// 自定义配置
Retry: &doris.Retry{
    MaxRetryTimes:  3,      // 最大重试次数
    BaseIntervalMs: 2000,   // 基础间隔（毫秒）
    MaxTotalTimeMs: 30000,  // 总时长限制（毫秒）
}
```

### 4. 方法变更

#### ❌ 移除的方法
```go
// 这些方法已被移除
client.StreamLoad(reader)     // 使用 Load() 替代
doris.NewLoadSetting()        // 直接构造 Config 结构体
doris.NewJsonFormat()         // 使用 DefaultJSONFormat() 或直接构造
doris.NewCsvFormat()          // 使用 DefaultCSVFormat() 或直接构造
```

#### ✅ 保留的方法
```go
// 主要加载方法
response, err := client.Load(reader)

// 便捷函数
doris.DefaultJSONFormat()
doris.DefaultCSVFormat()
doris.DefaultRetry()
```

## 完整迁移示例

### 迁移前
```go
package main

import (
    "strings"
    "github.com/selectdb/go-doris-sdk"
)

func oldExample() {
    // 旧版配置
    setting := doris.NewLoadSetting().
        AddFeNodes("http://localhost:8630").
        SetUser("root").
        SetPassword("password").
        Database("test_db").
        Table("test_table").
        JsonFormat(doris.JsonObjectLine).
        Retry(doris.NewRetry(3, 1000)).
        BatchMode(doris.ASYNC)

    client, err := doris.NewLoadClient(setting)
    if err != nil {
        return
    }

    data := `{"id": 1, "name": "test"}`
    
    // 旧版加载方法
    jsonBytes, err := client.StreamLoad(strings.NewReader(data))
    if err != nil {
        return
    }
    
    // 需要手动解析JSON
    // ...
}
```

### 迁移后
```go
package main

import (
    "strings"
    "github.com/selectdb/go-doris-sdk"
)

func newExample() {
    // 新版配置
    config := &doris.Config{
        Endpoints:   []string{"http://localhost:8630"},
        User:        "root",
        Password:    "password",
        Database:    "test_db",
        Table:       "test_table",
        Format:      doris.DefaultJSONFormat(),
        Retry:       doris.DefaultRetry(),
        GroupCommit: doris.ASYNC,
    }

    client, err := doris.NewLoadClient(config)
    if err != nil {
        return
    }

    data := `{"id": 1, "name": "test"}`
    
    // 新版加载方法
    response, err := client.Load(strings.NewReader(data))
    if err != nil {
        return
    }
    
    // 直接访问结构化响应
    if response.Status == doris.SUCCESS {
        fmt.Printf("加载成功：%d 行\n", response.Resp.NumberLoadedRows)
    }
}
```

## 迁移检查清单

### ✅ 必要步骤

1. **更新配置创建**
   - [ ] 移除 `NewLoadSetting()` 调用
   - [ ] 改用直接 `&doris.Config{}` 构造
   - [ ] 更新字段名：`AddFeNodes` → `Endpoints`

2. **更新格式配置**
   - [ ] 替换 `.JsonFormat()` → `Format: doris.DefaultJSONFormat()`
   - [ ] 替换 `.CsvFormat()` → `Format: doris.DefaultCSVFormat()`
   - [ ] 或使用直接构造：`&doris.JSONFormat{Type: ...}`

3. **更新重试配置**
   - [ ] 为现有 `Retry` 配置添加 `MaxTotalTimeMs` 字段
   - [ ] 或改用 `doris.DefaultRetry()`

4. **更新方法调用**
   - [ ] 替换 `StreamLoad()` → `Load()`
   - [ ] 更新响应处理逻辑（从 `[]byte` 到 `LoadResponse`）

5. **更新字段名**
   - [ ] `BatchMode` → `GroupCommit`

### 🔍 验证步骤

1. **编译检查**
   ```bash
   go build ./...
   ```

2. **功能测试**
   - [ ] 验证配置创建
   - [ ] 验证数据加载
   - [ ] 验证错误处理
   - [ ] 验证重试逻辑

## 新功能亮点

### 1. 智能重试策略
- **双重限制**：既限制重试次数，又限制总时长
- **动态退避**：根据剩余时间自动调整退避间隔
- **Reader复用**：支持Seeker和非Seeker的Reader重试

### 2. 更好的错误处理
- **结构化响应**：直接访问 `LoadResponse` 字段
- **类型安全**：编译时验证，减少运行时错误

### 3. 简化的API
- **直观配置**：所有选项一目了然
- **减少方法**：移除冗余的API表面积
- **一致性**：统一的命名和使用模式

## ⚠️ 并发使用注意事项

### Reader 的线程安全

**✅ 安全的并发模式：**
```go
// 每个 goroutine 使用独立的 Reader
for i := 0; i < numWorkers; i++ {
    go func(workerID int) {
        data := generateWorkerData(workerID)
        response, err := client.Load(doris.StringReader(data))  // ✅ 安全
        // 处理响应...
    }(i)
}
```

**❌ 危险的并发模式：**
```go
// 不要在多个 goroutine 间共享同一个 Reader
file, _ := os.Open("data.csv")
for i := 0; i < 10; i++ {
    go func() {
        response, err := client.Load(file)  // ❌ 竞争条件！
    }()
}
```

**关键原则：**
- ✅ `DorisLoadClient` 可以在多个 goroutine 间安全共享
- ❌ `io.Reader` 不应在多个 goroutine 间共享
- ✅ 每个 goroutine 应使用独立的数据源

## 获取帮助

如果在迁移过程中遇到问题：

1. **查看示例**：`examples/` 目录包含更新后的完整示例
2. **参考文档**：`docs/` 目录包含详细的技术文档
3. **并发安全指南**：`docs/READER_CONCURRENCY_ANALYSIS.md` 详细分析并发使用
4. **运行测试**：`go test ./...` 验证功能正常

## 向后兼容性

当前版本保留了一些向后兼容的类型别名：

```go
// 这些仍然可用，但推荐使用新的名称
type LoadSetting = Config        // 推荐直接使用 Config
type BatchMode = GroupCommitMode // 推荐使用 GroupCommitMode
```

建议在方便的时候迁移到新的名称以获得更好的代码清晰度。 