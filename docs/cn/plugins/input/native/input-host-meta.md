# 主机元信息采集

## 简介

`input_host_meta` 定时采集主机元数据，包括主机、进程实体及其之间的关系。该插件采用全量上报+增量采集的混合模式，既能保证数据完整性，又能高效地追踪进程变化。

## 版本

[Beta](../../stability-level.md)

## 版本说明

* 推荐版本：【待发布】

## 配置参数

### 基础配置

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| Type | string，无默认值（必填） | 插件类型，固定为`input_host_meta`。 |
| EnableProcessEntity | bool, true | 是否启用进程实体采集。设置为 false 可以禁用进程实体采集功能。 |

### 进程实体采集配置

**注意**：以下配置仅在 `EnableProcessEntity = true` 时生效。

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| FullReportInterval | int, 3600 | 全量上报间隔，单位为秒。全量上报会重新采集所有进程信息并更新可变属性（如 ppid、user、state）。 |
| IncrementalInterval | int, 10 | 增量采集间隔，单位为秒。增量采集只检测进程的新增、退出和 PID 复用，不追踪属性变化。 |
| ExcludeKernelThreads | bool, true | 是否排除内核线程。内核线程通常以 `PF_KTHREAD` 标志位标识。 |
| MinRunningTimeSeconds | int, 20 | 最小运行时间过滤，单位为秒。运行时间小于此值的进程将被过滤。设置为 0 表示不过滤。 |
| ProcessWhitelist | string array, [] | 进程白名单，支持正则表达式。匹配进程名（comm）或可执行文件路径（exe）。为空表示不过滤。 |
| ProcessBlacklist | string array, [] | 进程黑名单，支持正则表达式。匹配进程名（comm）或可执行文件路径（exe）。为空表示不过滤。 |

### 采集模式说明

#### 全量上报（Full Report）
- **触发时机**：启动后首次采集、之后每隔 `FullReportInterval` 秒
- **采集内容**：
  - 所有符合过滤条件的进程
  - 更新可变属性：ppid（父进程ID）、user（用户）、state（进程状态）
  - 上报不变属性：pid、ktime、comm、exe、cmdline、args
- **用途**：确保数据完整性，修正可能的数据漂移

#### 增量采集（Incremental Collection）
- **触发时机**：每隔 `IncrementalInterval` 秒
- **检测内容**：
  - 新增进程：系统中出现的新进程
  - 退出进程：从系统中消失的进程
  - PID 复用：相同 PID 但 startTime 不同的进程（旧进程退出，新进程复用了 PID）
- **不追踪**：进程属性变化（ppid、user、state 等可变属性的变化）
- **用途**：高效追踪进程生命周期变化

## 样例

### 基础配置

* 采集配置（使用默认参数）

```yaml
enable: true
inputs:
  - Type: input_host_meta
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

### 禁用进程实体采集

* 采集配置（仅采集其他主机元信息，不采集进程实体）

```yaml
enable: true
inputs:
  - Type: input_host_meta
    EnableProcessEntity: false      # 禁用进程实体采集
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

### 自定义配置

* 采集配置（自定义过滤规则）

```yaml
enable: true
inputs:
  - Type: input_host_meta
    EnableProcessEntity: true       # 启用进程实体采集（默认值）
    FullReportInterval: 7200        # 2小时全量上报一次
    IncrementalInterval: 30         # 30秒增量采集一次
    ExcludeKernelThreads: true      # 排除内核线程
    MinRunningTimeSeconds: 60       # 只采集运行超过60秒的进程
    ProcessWhitelist:
      - "nginx"                     # 只采集 nginx 相关进程
      - "java"
      - "/usr/bin/python.*"
    ProcessBlacklist:
      - ".*test.*"                  # 排除测试进程
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

### 同一 pipeline 多实例配置

* 采集配置（多个实例，但只有第一个启用进程实体采集）

```yaml
enable: true
inputs:
  - Type: input_host_meta           # 第一个实例：启用进程实体采集
    EnableProcessEntity: true       
    FullReportInterval: 3600
    IncrementalInterval: 10
  - Type: input_host_meta           # 第二个实例：禁用进程实体采集
    EnableProcessEntity: false      # 必须设置为 false，否则会注册失败
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

**说明**：同一个 pipeline 中只能有一个启用了进程实体采集的 `input_host_meta` 实例。如果配置多个实例，除第一个外，其他实例必须设置 `EnableProcessEntity = false`。

### 输出说明

#### 进程实体事件

```json
{
  
  // 进程主键（不变）
  "pid": "84450",
  "ktime": "1735348941",
  
  // 进程标识属性（不变）
  "comm": "nginx",
  "binary": "/usr/sbin/nginx",
  "arguments": "/usr/sbin/nginx -g daemon off;",
  
  // 可变属性（全量上报时更新）
  "ppid": "1",
  "user": "www-data",
  "state": "S",
  
  // 运行时计算属性
  "runtime_seconds": "3600"
}
```

#### 字段说明

**进程主键（唯一标识，不变）：**
- `pid`：进程ID
- `ktime`：进程启动时间（Unix时间戳，内核时间）

**进程标识属性（不变）：**
- `comm`：进程名称，来自 `/proc/[pid]/stat` 的 comm 字段
- `binary`：可执行文件路径，来自 `/proc/[pid]/exe` 符号链接
- `arguments`：完整命令行参数，来自 `/proc/[pid]/cmdline`

**可变属性（全量上报时更新）：**
- `ppid`：父进程ID，来自 `/proc/[pid]/stat`
- `user`：进程所属用户，来自 `/proc/[pid]/status`
- `state`：进程状态（R/S/D/Z/T等），来自 `/proc/[pid]/stat`

**运行时计算属性：**
- `runtime_seconds`：进程运行时间（秒），实时计算


## 进程过滤机制

进程过滤按以下顺序执行：

1. **内核线程过滤**（如果 `ExcludeKernelThreads = true`）
   - 检查进程的 `PF_KTHREAD` 标志位（0x00200000）
   - 内核线程通常是系统后台任务，如 `kworker`、`ksoftirqd` 等

2. **运行时间过滤**（如果 `MinRunningTimeSeconds > 0`）
   - 计算进程运行时间：当前时间 - 启动时间
   - 过滤掉短期运行的进程，避免采集瞬间启动又退出的进程

3. **黑名单过滤**（如果 `ProcessBlacklist` 不为空）
   - 使用正则表达式匹配进程名（comm）或可执行文件路径（binary）
   - 匹配成功则过滤掉

4. **白名单过滤**（如果 `ProcessWhitelist` 不为空）
   - 使用正则表达式匹配进程名（comm）或可执行文件路径（binary）
   - 只有匹配成功的进程才会被采集
   - 如果白名单为空，则不进行白名单过滤

## 注意事项

1. **进程实体采集开关**：通过设置 `EnableProcessEntity = false` 可以禁用进程实体采集，此时所有进程实体相关的配置参数（如 `FullReportInterval`、`IncrementalInterval`、过滤配置等）都将被忽略。

2. **同一 pipeline 限制**：每个 pipeline 配置只能有一个启用了进程实体采集的 `input_host_meta` 实例（即 `EnableProcessEntity = true`）。如果在同一个 pipeline 中配置多个启用进程实体采集的实例，第二个及后续实例将被拒绝注册到 ProcessEntityRunner，并记录错误日志。如果需要在同一 pipeline 中配置多个 `input_host_meta` 实例，除第一个外，其他实例应设置 `EnableProcessEntity = false`。

3. **内存占用**：进程缓存会永久保存已采集的进程信息，直到进程退出才清理。对于进程数量特别多的系统（>10000），建议适当调整过滤规则或考虑禁用进程实体采集。

4. **PID 复用检测**：通过比较 PID 和 startTime 来检测 PID 复用。当旧进程退出且新进程复用相同 PID 时，会生成两个事件：
   - 旧进程退出（从缓存中移除）
   - 新进程启动（作为新进程采集）

5. **时钟跳跃处理**：如果检测到系统时钟发生跳跃（>60秒），会自动重置采集调度时间，并记录告警日志。

6. **性能考虑**：
   - 全量上报会读取所有进程的 `/proc/[pid]/stat` 和 `/proc/[pid]/status`，有一定性能开销
   - 增量采集只检测进程列表变化，性能开销较小
   - 建议根据实际需求调整 `FullReportInterval` 和 `IncrementalInterval`
   - 如果不需要进程实体采集，可以设置 `EnableProcessEntity = false` 来完全禁用

7. **正则表达式**：白名单和黑名单支持标准的 C++ 正则表达式语法。无效的正则表达式会被记录警告日志并跳过。
