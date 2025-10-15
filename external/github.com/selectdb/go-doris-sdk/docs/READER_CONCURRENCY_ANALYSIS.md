# Reader 并发处理分析

## 📊 概述

分析 Doris Go SDK 中 `io.Reader` 在高并发环境下的处理机制和安全性。

## 🔍 当前实现分析

### Reader 处理策略

```go
// 当前实现的两种策略
if seeker, ok := reader.(io.Seeker); ok {
    // 策略1: Seeker Reader - 直接重置
    getBodyFunc = func() (io.Reader, error) {
        if _, err := seeker.Seek(0, io.SeekStart); err != nil {
            return nil, fmt.Errorf("failed to seek to start: %w", err)
        }
        return reader, nil
    }
} else {
    // 策略2: 非Seeker Reader - 缓存内容
    var buf bytes.Buffer
    if _, err := buf.ReadFrom(reader); err != nil {
        return nil, fmt.Errorf("failed to buffer reader content: %w", err)
    }
    
    getBodyFunc = func() (io.Reader, error) {
        return bytes.NewReader(buf.Bytes()), nil
    }
}
```

## ⚠️ 高并发下的潜在问题

### 1. Seeker Reader 的并发安全问题

**问题描述：**
如果多个 goroutine 共享同一个 `DorisLoadClient` 并传入同一个 Seeker Reader（如 `*os.File`），会存在竞争条件：

```go
// ❌ 危险的使用模式
file, _ := os.Open("data.csv")
defer file.Close()

var wg sync.WaitGroup
for i := 0; i < 10; i++ {
    wg.Add(1)
    go func() {
        defer wg.Done()
        // 多个 goroutine 同时使用同一个 file reader
        response, err := client.Load(file)  // ⚠️ 竞争条件！
    }()
}
```

**问题原因：**
- `file.Seek(0, io.SeekStart)` 不是原子操作
- 一个 goroutine 在 seek 后，另一个 goroutine 可能立即 seek，导致数据不一致
- 文件的读取位置会被多个 goroutine 同时修改

### 2. 非 Seeker Reader 的内存问题

**问题描述：**
对于大文件或数据流，缓存策略可能导致内存问题：

```go
// 大文件场景
bigDataReader := generateLargeData(100 * 1024 * 1024) // 100MB

// 在多个 goroutine 中使用
for i := 0; i < 100; i++ {
    go func() {
        // 每次都会在内存中创建 100MB 的副本
        client.Load(bigDataReader)  // ⚠️ 内存开销大
    }()
}
```

## ✅ 安全的使用模式

### 1. 每个 Goroutine 独立的 Reader

```go
// ✅ 推荐：每个 goroutine 创建独立的 reader
func safeWorker(workerID int, client *doris.DorisLoadClient) {
    // 方式1: 独立文件句柄
    file, err := os.Open("data.csv")
    if err != nil {
        return
    }
    defer file.Close()
    
    response, err := client.Load(file)  // ✅ 安全
    // 处理响应...
}

// 或者方式2: 独立数据生成
func safeWorkerWithData(workerID int, client *doris.DorisLoadClient) {
    data := generateWorkerData(workerID)  // 生成独立数据
    response, err := client.Load(doris.StringReader(data))  // ✅ 安全
    // 处理响应...
}
```

### 2. 数据预分片

```go
// ✅ 推荐：预先分片数据
func processBatchesConcurrently(client *doris.DorisLoadClient, allData []string) {
    var wg sync.WaitGroup
    
    for i, batch := range allData {
        wg.Add(1)
        go func(batchData string, batchID int) {
            defer wg.Done()
            
            // 每个 batch 使用独立的 reader
            reader := strings.NewReader(batchData)
            response, err := client.Load(reader)  // ✅ 安全
            
            // 处理响应...
        }(batch, i)
    }
    
    wg.Wait()
}
```

### 3. 使用 bytes.Reader 或 strings.Reader

```go
// ✅ 推荐：使用内存 reader
func safeWithMemoryReader(client *doris.DorisLoadClient) {
    data := "your,csv,data\n1,2,3\n"
    
    var wg sync.WaitGroup
    for i := 0; i < 10; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            
            // 每次创建新的 strings.Reader
            reader := strings.NewReader(data)
            response, err := client.Load(reader)  // ✅ 安全
            
            // 处理响应...
        }()
    }
    wg.Wait()
}
```

## 🛠️ 改进建议

### 1. 添加并发使用警告

```go
// 在文档中明确说明
// ❌ 不要在多个 goroutine 间共享同一个 Reader
// ✅ 每个 goroutine 应该使用独立的 Reader
```

### 2. 检测并发访问（可选）

```go
// 可以添加运行时检测（开发模式）
func (c *DorisLoadClient) Load(reader io.Reader) (*loader.LoadResponse, error) {
    if c.config.Debug {
        // 检查 reader 是否被并发访问
        if detectConcurrentAccess(reader) {
            log.Warnf("Detected potential concurrent access to reader, this may cause race conditions")
        }
    }
    // ... 现有逻辑
}
```

### 3. 提供并发安全的包装器

```go
// 新增：并发安全的 Reader 包装器
type ConcurrentSafeReader struct {
    dataFunc func() []byte  // 数据生成函数
}

func (r *ConcurrentSafeReader) Read(p []byte) (n int, err error) {
    data := r.dataFunc()
    reader := bytes.NewReader(data)
    return reader.Read(p)
}

// 使用方式
safeReader := &ConcurrentSafeReader{
    dataFunc: func() []byte {
        return generateData()  // 每次调用生成新数据
    },
}

// 多个 goroutine 可以安全使用
for i := 0; i < 10; i++ {
    go func() {
        client.Load(safeReader)  // ✅ 安全
    }()
}
```

## 📈 最佳实践总结

### ✅ 推荐的并发模式

1. **独立 Reader 模式**
   ```go
   // 每个 goroutine 创建独立的 file handle 或 reader
   for i := 0; i < numWorkers; i++ {
       go func(workerID int) {
           file, _ := os.Open(fmt.Sprintf("data_%d.csv", workerID))
           defer file.Close()
           client.Load(file)
       }(i)
   }
   ```

2. **数据生成模式**
   ```go
   // 每个 goroutine 生成独立的数据
   for i := 0; i < numWorkers; i++ {
       go func(workerID int) {
           data := generateWorkerData(workerID)
           client.Load(doris.StringReader(data))
       }(i)
   }
   ```

3. **预分片模式**
   ```go
   // 预先将大数据分片
   batches := splitDataIntoBatches(largeData, numWorkers)
   for i, batch := range batches {
       go func(batchData string) {
           client.Load(strings.NewReader(batchData))
       }(batch)
   }
   ```

### ❌ 避免的反模式

1. **共享文件句柄**
   ```go
   // ❌ 不要这样做
   file, _ := os.Open("data.csv")
   for i := 0; i < 10; i++ {
       go func() {
           client.Load(file)  // 竞争条件！
       }()
   }
   ```

2. **共享自定义 Reader**
   ```go
   // ❌ 如果 customReader 有内部状态，不要共享
   customReader := NewCustomReader()
   for i := 0; i < 10; i++ {
       go func() {
           client.Load(customReader)  // 可能有问题
       }()
   }
   ```

## 📋 结论

### 当前实现的优缺点

#### ✅ 优点
- 正确处理了重试场景中的 Reader 消费问题
- 支持 Seeker 和非 Seeker 两种类型的 Reader
- 重试时能够正确重新读取数据

#### ⚠️ 注意事项
- **单线程使用**：当前实现针对单个 goroutine 中的重试场景优化
- **并发使用需谨慎**：在多 goroutine 环境下，用户需要确保每个 goroutine 使用独立的 Reader
- **文档说明**：需要在文档中明确说明并发使用的注意事项

### 推荐的使用方针

1. **DorisLoadClient 可以安全共享**：客户端本身是线程安全的
2. **Reader 不应共享**：每个 goroutine 应使用独立的 Reader
3. **数据独立性**：确保每个并发操作的数据是独立的

这样的设计既保证了重试机制的正确性，又为高并发使用提供了明确的指导原则。 