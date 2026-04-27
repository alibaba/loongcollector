---
name: e2e
description: LoongCollector E2E 测试全流程指南：设计、编写、运行和调试。当需要编写新 E2E 测试、运行现有测试、或排查 E2E 测试失败时使用此 skill。
---
# LoongCollector E2E 测试指南

> 详细步骤模板见 [reference.md](reference.md) | 可复用脚本见 [scripts/](scripts/)

## 目录

1. [概览](#1-概览)
2. [设计测试用例](#2-设计测试用例)
3. [编写测试用例](#3-编写测试用例)
4. [本地运行（docker-compose）](#4-本地运行)
5. [调试](#5-调试)
6. [已知陷阱](#6-已知陷阱)

---

## 1 概览

基于 **BDD Godog** 框架，通过 `.feature` 文件描述场景，引擎正则匹配步骤函数并传参。

```
test/e2e/
  test_cases/<case_name>/
    case.feature              # 场景描述
    docker-compose.yaml       # 可选，外部依赖服务
  engine/
    steps.go                  # 所有可用步骤（权威来源）
    setup/ control/ trigger/ verify/ cleanup/
```

**环境 tag**：`@host`、`@k8s`、`@docker-compose`（三选一，加 `@e2e`）

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
- **外部依赖**：需要什么辅助服务（OTel Collector、Kafka 等）

---

## 3 编写测试用例

### 3.1 目录结构

```
test/e2e/test_cases/my_feature/
├── case.feature
├── docker-compose.yaml       # 外部依赖
└── otel-collector-config.yaml  # 如果用 OTel Collector
```

### 3.2 Feature 文件模板

```gherkin
@flusher
Feature: my feature name
  Brief description

  @e2e @docker-compose
  Scenario: TestMyFeatureLogs
    Given {docker-compose} environment
    Given {my-config} local config as below
    """
    enable: true
    inputs:
      - Type: input_forward
        Protocol: OTLP
        Endpoint: "0.0.0.0:4320"
    flushers:
      - Type: flusher_otlp_native
        Endpoint: "otel-collector:4317"
    """
    When start docker-compose {my_feature}
    Then wait {10} seconds
    When generate {1} OTLP {logs} via otelgen to endpoint {loongcollectorC:4320}, protocol {grpc}
    Then wait {5} seconds
    Then otlp collector received at least {1} logs from file {/tmp/otel-export/logs.json}
```

### 3.3 强制规则

- 配置中必须含 `enable: true`
- **只使用** `test/engine/steps.go` 中已注册的步骤
- `wait {N} seconds` 是 **Then** 类型，不是 When
- 命名格式：`Test${功能名}${场景描述}`
- **不要**在持续运行插件的配置中使用 `global.ExcutionTimeout`（见 §6.1）

### 3.4 扩展步骤

如需新步骤，参考 [reference.md §扩展步骤](reference.md) 中的开发和注册流程。

---

## 4 本地运行

### 4.1 前置条件

```bash
docker --version && docker compose version
```

如修改了 C++ 代码，需重新编译并更新镜像。两种方式：

**方式一：完整构建**（慢，但保证一致）
```bash
make e2e_image   # 从源码构建完整 Docker 镜像 aliyun/loongcollector:0.0.1
```

**方式二：增量更新**（快，适合迭代调试）
```bash
cd build && make -sj$(nproc) && cd ..
# 替换镜像中的二进制
docker create --name tmp-lc aliyun/loongcollector:0.0.1
docker cp build/loongcollector tmp-lc:/usr/local/loongcollector/loongcollector
docker commit tmp-lc aliyun/loongcollector:0.0.1
docker rm tmp-lc
```

### 4.2 运行

```bash
cd test/e2e

# 运行整个测试用例（所有 Scenario）
TEST_CASE=flusher_otlp_native go test -v -run "TestE2EOnDockerCompose$" \
  -timeout 600s -count=1 ./...

# 只运行指定 Scenario
TEST_CASE=flusher_otlp_native go test -v \
  -run "TestE2EOnDockerCompose/TestFlusherOTLPNativeLogs$" \
  -timeout 600s -count=1 ./...
```

### 4.3 清理（测试失败后必做）

可以直接运行脚本 `bash .cursor/skills/e2e/scripts/e2e-cleanup.sh`，或手动执行：

```bash
docker rm -f $(docker ps -aq) 2>/dev/null
docker network prune -f
rm -rf test/e2e/config test/e2e/onetime_pipeline_config
sudo rm -rf test/e2e/report
rm -f test/e2e/test_cases/<case>/testcase-compose.yaml
```

---

## 5 调试

```bash
# 1. 查看容器日志
docker ps | grep loongcollectorC
docker exec <id> cat /usr/local/loongcollector/log/loongcollector.LOG

# 2. 检查配置是否加载
docker exec <id> ls /usr/local/loongcollector/conf/continuous_pipeline_config/local/

# 3. 检查端口是否监听
docker exec <id> ss -tlnp | grep <port>

# 4. 手动复现 compose 环境
cd test/e2e/test_cases/<case>
docker compose -f testcase-compose.yaml up -d
docker compose -f testcase-compose.yaml logs -f loongcollectorC
```

---

## 6 已知陷阱

### 6.1 ExcutionTimeout 使配置变为一次性

**绝对不要**在 `input_forward`、`input_file` 等持续插件的配置中使用 `global.ExcutionTimeout`。

它会使 `IsOnetime()` 返回 true，导致 `IsValidNativeInputPlugin(name, true)` 在 onetime 注册表中查找，而大部分 input 只注册了 continuous，结果报 `unsupported input plugin`。

详见 `.cursor/rules/project-knowledge/config-pitfalls.mdc`。

### 6.2 FlusherFile 必须是文件

e2e 模板将 `report/<case>default_flusher.json` bind-mount 到容器。若宿主机路径不存在，Docker 会创建为**目录**。已在 `BootController.Start()` 中自动处理。

### 6.3 测试间残留

多 Scenario 共享进程，`Clean()` 会删除 config/report。异常退出后手动清理（§4.3）。
