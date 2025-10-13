# Doris Flusher 测试指南

## 测试概览

本测试套件包含了 Doris flusher 的完整单元测试和集成测试。

### 测试覆盖率

当前测试覆盖率：**39.4%**

主要测试覆盖的功能：
- ✅ 100% - Flusher 创建和初始化
- ✅ 100% - 配置验证
- ✅ 100% - 认证凭据管理
- ✅ 100% - 就绪状态检查
- ⚠️ 7.4% - Flush 方法（需要实际 Doris 实例）

## 运行测试

### 运行所有单元测试

```bash
cd /Users/bingquanzhao/work/loongcollector
go test -v ./plugins/flusher/doris/...
```

### 运行单元测试（跳过集成测试）

```bash
go test -v ./plugins/flusher/doris/... -short
```

### 运行特定测试

```bash
# 测试配置验证
go test -v ./plugins/flusher/doris/... -run TestFlusherDoris_Validate

# 测试认证
go test -v ./plugins/flusher/doris/... -run TestAuthentication_GetUsernamePassword
```

### 查看测试覆盖率

```bash
go test -cover ./plugins/flusher/doris/...
```

### 生成详细覆盖率报告

```bash
go test -coverprofile=coverage.out ./plugins/flusher/doris/...
go tool cover -html=coverage.out
```

### 运行基准测试

```bash
go test -bench=. -benchmem ./plugins/flusher/doris/...
```

## 集成测试

集成测试需要运行的 Doris 实例。默认情况下，集成测试被禁用（函数名以 `Invalid` 开头）。

### 启用集成测试

1. 确保你有一个运行中的 Doris 实例
2. 创建测试数据库和表：

```sql
CREATE DATABASE IF NOT EXISTS test_db;

USE test_db;

CREATE TABLE IF NOT EXISTS test_table (
    `group` INT,
    `message` VARCHAR(500),
    `id` INT,
    `__time__` DATETIME
)
DUPLICATE KEY(`group`)
DISTRIBUTED BY HASH(`group`) BUCKETS 10
PROPERTIES (
    "replication_num" = "1"
);
```

3. 修改测试代码，将 `InvalidTestConnectAndWrite` 改为 `TestConnectAndWrite`

4. 更新测试中的连接参数：

```go
flusher.Addresses = []string{"your-doris-fe:8030"}
flusher.Database = "test_db"
flusher.Table = "test_table"
flusher.Authentication.PlainText = &PlainTextConfig{
    Username: "your-username",
    Password: "your-password",
}
```

5. 运行集成测试：

```bash
go test -v ./plugins/flusher/doris/... -run TestConnectAndWrite
```

## 测试用例说明

### 单元测试

1. **TestNewFlusherDoris** - 测试 flusher 创建
2. **TestFlusherDoris_Description** - 测试描述方法
3. **TestFlusherDoris_Validate** - 测试配置验证
   - 有效配置
   - 空地址列表
   - nil 地址列表
   - 空表名
4. **TestFlusherDoris_IsReady** - 测试就绪状态检查
5. **TestAuthentication_GetUsernamePassword** - 测试认证凭据
   - 有效凭据
   - 空用户名
   - 空密码
   - nil 配置
6. **TestFlusherDoris_Init** - 测试初始化
   - 无效配置
   - 缺少认证信息
7. **TestFlusherDoris_FlushWithoutInit** - 测试未初始化时的 flush 操作

### 集成测试

1. **InvalidTestConnectAndWrite** - 完整的端到端测试
   - 连接 Doris
   - 初始化 flusher
   - 写入测试数据
   - 验证写入成功

### 基准测试

1. **BenchmarkFlusherDoris_MakeTestLogGroupList** - 测试日志组创建性能

当前性能指标（Apple M2）：
```
BenchmarkFlusherDoris_MakeTestLogGroupList-8   	
41990 次迭代
28054 ns/op（每次操作 28 微秒）
25552 B/op（每次操作分配 25KB）
922 allocs/op（每次操作 922 次内存分配）
```

## 测试数据

测试使用 `makeTestLogGroupList()` 函数生成模拟数据：
- 10 个 LogGroup
- 每个 LogGroup 包含 10 条 Log
- 每条 Log 包含字段：group, message, id
- 总共 100 条日志记录

## 最佳实践

1. **始终运行单元测试** - 在提交代码前运行所有单元测试
2. **检查测试覆盖率** - 确保新功能有相应的测试
3. **运行集成测试** - 在发布前运行集成测试验证与实际 Doris 的兼容性
4. **性能基准测试** - 定期运行基准测试，监控性能变化

## 故障排查

### 测试失败

如果测试失败，检查：
1. 本地 SDK 路径是否正确设置在 go.mod 中
2. 依赖是否已正确安装（运行 `go mod tidy`）
3. 测试中的配置是否正确

### 集成测试失败

如果集成测试失败，检查：
1. Doris 实例是否正在运行
2. 连接参数是否正确（地址、端口、用户名、密码）
3. 数据库和表是否已创建
4. 用户是否有写入权限
5. 网络连接是否正常

## 持续集成

在 CI/CD 流程中，建议：

```bash
# 运行单元测试
go test -v -short ./plugins/flusher/doris/...

# 检查测试覆盖率
go test -cover ./plugins/flusher/doris/...

# 运行 lint 检查
golangci-lint run ./plugins/flusher/doris/...
```

## 贡献指南

添加新测试时：
1. 遵循现有测试的命名规范
2. 使用表驱动测试（table-driven tests）处理多个测试场景
3. 为复杂功能添加单元测试和集成测试
4. 更新此文档说明新测试的用途

