---
name: e2e
description: LoongCollector E2E 测试全流程指南：设计、编写、运行和调试。当需要编写新 E2E 测试、运行现有测试、或排查 E2E 测试失败时使用此 skill。
---
# LoongCollector E2E 测试指南

> 步骤模板见 [reference.md](reference.md)。手动清理可选 [scripts/e2e-cleanup.sh](scripts/e2e-cleanup.sh)。

## 目录

1. [概览](#1-概览)
2. [设计测试用例](#2-设计测试用例)
3. [编写测试用例](#3-编写测试用例)
4. [本地运行（docker-compose）](#4-本地运行)
5. [调试](#5-调试)
6. [已知陷阱](#6-已知陷阱)

---

## 1 概览

基于 **BDD Godog**，`.feature` 描述场景，`test/engine/steps.go` 为步骤权威来源。

```
test/e2e/test_cases/<case_name>/case.feature
test/engine/          # setup / control / verify / cleanup
```

**环境 tag**：`@e2e` + `@docker-compose`（或 `@host` / `@k8s`）

**CI**：`.github/workflows/e2e.yaml` — 矩阵 job 每个 `TEST_CASE` 单独跑；镜像由 `Dockerfile_edge_linux` 构建后 `docker load`；环境变量 `DOCKER_BUILDKIT=0`、`COMPOSE_DOCKER_CLI_BUILD=0`。

**测试框架**（`test/engine/setup/dockercompose/`）：compose 侧车构建禁用 BuildKit；场景结束后先校验 agent PID 再 `cleanup.All()`；失败时 `TryCopyCoreLogs` 导出到 `test/e2e/report/<case>_log/`。

---

## 2 设计测试用例

编写 feature 文件前，先确定测试矩阵。按以下维度逐项评估是否需要覆盖：

### 2.1 场景维度清单

| 维度 | 典型场景 | 何时需要 |
|------|----------|----------|
| **基础功能** | 单配置、单数据类型端到端 | 必须 |
| **多数据类型** | logs / metrics / traces 分别验证 | 插件支持多类型时 |
| **多配置共存** | 同时加载多个 pipeline 配置 | 涉及端口/资源竞争时 |
| **配置热加载** | 运行中增/删/改配置 | 持续运行的 input 插件 |
| **配置类型变更** | 从 A 类型切换到 B 类型 | 插件支持多协议/格式时 |
| **反压与恢复** | 下游不可达 → 恢复后数据不丢 | flusher 插件 |
| **外部依赖失效** | 依赖服务重启/不可达 | 有外部依赖时 |
| **大数据量** | 高吞吐压力下不 OOM/不丢数据 | 性能敏感路径 |

### 2.2 设计产出

确定要覆盖的场景后，明确每个 Scenario 的：

- **输入**：什么数据、什么格式、多少条
- **流经路径**：input → processor → flusher 的具体插件
- **预期输出**：在哪里验证、验证什么
- **外部依赖**：需要什么辅助服务（OTel Collector、Kafka 等）、`docker-compose.yaml` 侧车

---

## 3 编写测试用例

### 3.1 目录结构

```
test/e2e/test_cases/my_feature/
├── case.feature
├── docker-compose.yaml       # 外部依赖
└── otel-collector-config.yaml  # 若使用 OTel Collector
```

### 3.2 模板（节选）

```gherkin
  @e2e @docker-compose
  Scenario: TestMyFeatureBasic
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {my-case} local config as below
    """
    enable: true
    inputs:
      - Type: input_forward
        ...
    flushers:
      - Type: flusher_grpc
        Address: "host.docker.internal:9000"
    """
    When start docker-compose {my_feature}
    Then there is at least {1} logs
```

### 3.3 强制规则

- 配置含 `enable: true`
- **仅使用** `test/engine/steps.go` 已注册步骤
- `wait {N} seconds` 为 **Then**，不是 When
- Scenario 名：`Test${功能名}${场景描述}`
- 持续采集插件不要用 `global.ExcutionTimeout`（见 §6.1）
- `flusher_prometheus` 的 `Retry.*Delay` 等为 **duration 类型**，YAML 里勿写 `"1s"` 字符串（会 JSON 反序列化失败）

### 3.4 v1 / v2 与 flusher

| 配置 | 说明 |
|------|------|
| 默认 v1 + `flusher_grpc` / `flusher_http` | C++ `ProcessPipelineEventGroup` → Go 转 `LogGroup` → `ReceiveLogGroup` |
| `StructureType: v2` + `flusher_prometheus` | Metric 走 `ReceivePipelineEventGroup`；**不能** v2 + `flusher_grpc`（未实现 FlusherV2） |

---

## 4 本地运行

### 4.1 镜像

**与 CI 一致（完整构建，慢）：**

```bash
make e2edocker    # 产出 aliyun/loongcollector:0.0.1
```

**增量更新镜像（C++/Go 改过后常用）：**

```bash
# 示例：仅替换二进制与 Go 插件（路径按本机 build 输出调整）
docker create --name tmp-lc aliyun/loongcollector:0.0.1
docker cp core/build/loongcollector tmp-lc:/usr/local/loongcollector/loongcollector
docker cp output/libGoPluginBase.so tmp-lc:/usr/local/loongcollector/libGoPluginBase.so
docker commit tmp-lc aliyun/loongcollector:0.0.1
docker rm tmp-lc
```

### 4.2 运行（与 GitHub matrix 一致）

仓库根目录：

```bash
export DOCKER_BUILDKIT=0
export COMPOSE_DOCKER_CLI_BUILD=0

# 单用例（推荐，与 CI 相同）
TEST_CASE=input_static_file ./scripts/e2e.sh e2e
```

或直接 `go test`（可自定义超时）：

```bash
cd test
export DOCKER_BUILDKIT=0 COMPOSE_DOCKER_CLI_BUILD=0
TEST_CASE=input_static_file go test -v -timeout 15m \
  -run '^TestE2EOnDockerCompose$' github.com/alibaba/ilogtail/test/e2e

# 单个 Scenario
TEST_CASE=flusher_http go test -v -timeout 15m \
  -run '^TestE2EOnDockerCompose/TestFlusherHTTP$' github.com/alibaba/ilogtail/test/e2e
```

**全量 42 场景**（本地串跑，非 CI 方式）：

```bash
cd test
go test -v -timeout 90m -run '^TestE2EOnDockerCompose$' github.com/alibaba/ilogtail/test/e2e
```

`./scripts/e2e.sh e2e` 未设 `TEST_CASE` 时会跑 `test/e2e` 下全部 feature，默认 **30m** 超时，全量可能不够。

`make e2e` 等价 `clean e2edocker` + `./scripts/e2e.sh e2e`（会完整重建镜像，很慢）。

### 4.3 清理

- 正常退出：引擎 `cleanup.All()` + compose `ShutDown` 会 down 项目并清理残留 `loongcollectorC`。
- 异常退出或端口占用：可执行 skill 附带脚本（**不会**删除宿主机上无关容器）：

```bash
bash skills/e2e/scripts/e2e-cleanup.sh
bash skills/e2e/scripts/e2e-cleanup.sh input_static_file
```

勿将 `test/e2e/test_cases/**/a.log`、`reader_log_rotate/volume/simple.log` 等**跑测产物**提交进仓库。

---

## 5 调试

```bash
# 框架导出的日志（compose 启动失败或场景结束时会尝试复制）
ls test/e2e/report/<TEST_CASE>_log/

# 容器内实时查看
docker ps -a --filter name=loongcollectorC
docker exec <id> cat /usr/local/loongcollector/log/loongcollector.LOG
docker exec <id> cat /usr/local/loongcollector/log/go_plugin.LOG

# 配置是否加载
docker exec <id> ls /usr/local/loongcollector/conf/continuous_pipeline_config/local/
```

常见失败：

| 现象 | 方向 |
|------|------|
| `agent crash` / `agent PID not found` | 确认已合入 test/engine After 钩子与 PID 采集修复；compose 未启动时看 **When** 步骤原始错误 |
| `compose exited abnormally` + buildx | 确认 `DOCKER_BUILDKIT=0`；侧车 `FROM bash` 需能 `docker pull bash` |
| `plugin does not implement FlusherV2` | 勿对 `flusher_grpc` 单独设 `StructureType: v2` |
| `cannot unmarshal string ... time.Duration` | 检查 flusher HTTP/Prometheus 的 Retry/Timeout 配置格式 |
| 0 条日志 | `go_plugin.LOG` 中 CONFIG_LOAD_ALARM；镜像是否含 `ProcessPipelineEventGroup` |

---

## 6 已知陷阱

### 6.1 ExcutionTimeout

**不要**在 `input_file`、`input_forward` 等持续插件配置里写 `global.ExcutionTimeout`，否则被当作 onetime 配置，易出现 `unsupported input plugin`。详见 `.cursor/rules/project-knowledge/config-pitfalls.mdc`。

### 6.2 default_flusher.json

`report/<case>default_flusher.json` 须为**文件**；宿主机路径不存在时 Docker 可能建成目录。`BootController.Start()` 会尝试自动修复。

### 6.3 测试数据勿污染 git

跑测会改写 `a.log`、`volume/simple.log` 等；提交前 `git checkout -- test/e2e/test_cases/...` 恢复。
