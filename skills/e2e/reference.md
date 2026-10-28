# E2E 测试详细参考

## 可用步骤速查

> 权威来源：`test/engine/steps.go`

### Given（环境准备）

| 步骤模板 | 说明 |
|----------|------|
| `{docker-compose} environment` | 初始化 docker-compose 环境 |
| `{host} environment` | 初始化主机环境 |
| `{daemonset} environment` | 初始化 K8s 环境 |
| `{name} local config as below` | 写入持续采集配置 |
| `{name} onetime pipeline local config as below` | 写入一次性采集配置 |
| `subcribe data from {sls} with config` | 订阅 SLS 数据源 |
| `loongcollector depends on containers {name}` | 设置容器依赖 |
| `loongcollector container mount {src} to {dst}` | 挂载卷 |
| `loongcollector expose port {host} to {container}` | 暴露端口 |
| `docker-compose boot type {type}` | 设置 boot 类型 |
| `mkdir {path}` | 创建目录 |

### When（触发动作）

| 步骤模板 | 说明 |
|----------|------|
| `start docker-compose {case_name}` | 启动 docker-compose 环境 |
| `begin trigger` | 标记触发开始时间（生成日志前必须调用） |
| `generate {N} regex logs to file {path}, with interval {M}ms` | 生成正则日志 |
| `generate {N} json logs to file {path}, with interval {M}ms` | 生成 JSON 日志 |
| `generate {N} apsara logs to file {path}, with interval {M}ms` | 生成 Apsara 日志 |
| `generate {N} OTLP {logs\|metrics\|traces} via otelgen to endpoint {ep}, protocol {grpc\|http}` | 生成 OTLP 数据 |
| `generate {N} http logs, with interval {M}ms, url: {url}, method: {method}, body:` | 生成 HTTP 日志 |
| `execute {N} commands {cmd} in sequence` | 顺序执行命令 |
| `execute {N} commands {cmd} in parallel` | 并行执行命令 |
| `create the shell script file {name} with the following content` | 创建 shell 脚本 |
| `execute {N} the shell script file {name} in parallel` | 并行执行 shell 脚本 |
| `restart agent` | 重启 Agent |
| `force restart agent` | 强制重启 Agent |

### Then（结果验证）

| 步骤模板 | 说明 |
|----------|------|
| `there is {N} logs` | 精确验证日志数（上限 100） |
| `there is at least {N} logs` | 最少日志数验证 |
| `there is less than {N} logs` | 最多日志数验证 |
| `the log fields match kv` | KV 字段匹配（文档内容跟 `"""..."""`） |
| `the log fields match as below` | 日志字段模式匹配 |
| `the log tags match kv` | Tag KV 匹配 |
| `the log is in order` | 日志顺序验证 |
| `wait {N} seconds` | 等待 N 秒 |
| `otlp collector received at least {N} (logs\|metrics\|traces) from file {path}` | OTel Collector 数据验证 |

> 注意：日志数量验证上限 100。超过 100 用 `When query through` + `Then the log fields match kv` 方式。

---

## 扩展步骤

### 1. 编写函数

在 `test/engine/` 对应子目录下：

```go
func MyVerification(ctx context.Context, expected int) (context.Context, error) {
    // 实现逻辑
    return ctx, nil
}
```

签名要求：第一个参数 `context.Context`，返回 `(context.Context, error)`。

### 2. 注册

在 `test/engine/steps.go` 中：

```go
ctx.Then(`^my verification expects \{(\d+)\}$`, verify.MyVerification)
```

### 3. 使用

```gherkin
Then my verification expects {42}
```

---

## docker-compose.yaml 示例

### OTel Collector（OTLP 测试用）

```yaml
services:
  otel-collector:
    image: otel/opentelemetry-collector-contrib:latest
    hostname: otel-collector
    user: "0:0"
    ports:
      - "4317"
    volumes:
      - ./otel-collector-config.yaml:/etc/otelcol-contrib/config.yaml
      - ./otel-export:/tmp/otel-export
    healthcheck:
      test: ["CMD", "wget", "--spider", "-q", "http://localhost:13133/"]
      interval: 5s
      timeout: 3s
      retries: 5
      start_period: 10s
```

---

## eBPF 进程安全测试示例

```gherkin
@e2e @host @ebpf_input
Scenario: TestEBPFProcessSecurityByNormalStart
  Given {host} environment
  Given subcribe data from {sls} with config
  """
  """
  Given {ebpf_process_security_default} local config as below
  """
  enable: true
  inputs:
    - Type: input_process_security
  """
  When begin trigger
  When execute {1} commands {/bin/echo 1} in sequence
  When query through {* | select * from e2e where call_name = 'execve' and binary = '/bin/echo' and arguments = '1'}
  Then there is {1} logs
```
