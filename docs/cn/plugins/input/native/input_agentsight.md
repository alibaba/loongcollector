# input-agentsight 插件

## 简介

`input_agentsight` 插件实现对当前 openclaw、hermes 等 agent 工具等采集，支持的大模型供应商包括 OpenAI、Anthropic，以及国内的厂商协议。

## 版本

dev

## 版本说明

* 推荐版本：LoongCollector v3.3.4 及以上

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为 `input_agentsight`  |
|  ProbeConfig  |  object  |  否  |  /  |  AgentSight 探测配置。**整体未配置**时所有子项走默认。  |
|  ProbeConfig.Verbose  |  uint  |  否  |  `0`  |  是否打印 ebpf 的详细日志，`1` 代表开启，`0` 代表关闭  |
|  ProbeConfig.LogPath  |  string  |  否  |  `""`  |  ebpf 日志的输出位置  |
|  ProbeConfig.CmdlineWhitelist  |  array  |  否（**推荐填写**）  |  内置 9 条  |  进程 **agent 筛选白名单**。每一项为对象：`AgentType`（上报字段 `gen_ai.agent.type`）+ `Args`（字符串数组，与进程 cmdline 各参数（即 `argv`）按位置 glob 匹配）。**未配置**且 `CmdlineBlacklist` 也为空时注入「默认 `CmdlineWhitelist`」（见下文）；**填写后只使用用户规则**，不再叠加内置。空数组 `[]` 视为非法配置。  |
|  ProbeConfig.CmdlineBlacklist  |  array  |  否  |  /  |  进程 **agent 筛选黑名单**，每项为 glob 字符串数组（无 `AgentType`）；**命中则排除**，不采集。**优先级高于白名单**。  |
|  ProbeConfig.Https  |  array  |  否  |  内置 4 条  |  HTTPS 加密流量的域名白名单（字符串数组，glob 通配符 `*`，不区分大小写）。访问白名单内域名的进程可被识别为采集目标。未配置时注入默认精确主机名列表，见下文。  |
|  ProbeConfig.Http  |  array  |  否  |  `[]`（关闭）  |  HTTP 明文流量的目标列表（字符串数组）。每项可为 `:端口`、`IP`、`IP:端口` 或域名（如 `model-svc.default.svc`、`*.internal.svc`）。**留空时不采集明文 HTTP 流量**。  |
|  ProbeConfig.StreamModeFormat  |  bool  |  否  |  `true`  |  为 `true` 时，每次 LLM 调用在同一 `PipelineEventGroup` 内输出两条日志（各有 `event.id`）：`event.name=gen_ai.model.request`（请求开始时间戳）与 `gen_ai.model.response`（请求结束时间戳）。为 `false` 时输出单条合并日志，**无** `event.name` / `event.id`。  |
|  ProbeConfig.AutoMessageTrim  |  bool  |  否  |  `true`  |  为 `true` 时**不**输出 `gen_ai.system_instructions`、`gen_ai.tool.definitions` 及全量 `gen_ai.input.messages`（仍输出 `delta` / `hash`）。为 `false` 时输出 system、tools（非空时每次），全量 input 由同 session **前缀 hash 去重**决定。**不影响** `gen_ai.output.messages`。  |

### `AgentType` 取值命名规范

本插件通过 **cmdline 白名单** 设置的是 **agent 类型**，命中后写入日志的 `gen_ai.agent.type` 字段。

**推荐**（非强制）的 `AgentType` 取值约定：

- 仅使用 **小写字母**、**数字**、**连字符** `-`
- 以字母或数字开头/结尾，**不**以 `-` 开头或结尾
- 多个单词用 **单个** 连字符连接，不用空格、下划线或驼峰

| 推荐 | 不推荐 |
| --- | --- |
| `openclaw` | `OpenClaw`、`open_claw` |
| `claude-code` | `Claude Code`、`claude_code` |
| `hermes` | `Hermes` |
| `cosh` | `Cosh` |

`AgentType` 必须是**非空字符串**；具体取值不做硬校验，写什么就上报什么到 `gen_ai.agent.type`。统一遵循上述规范便于跨产品聚合分析。

### 优先级与默认值

#### 黑白名单判定逻辑

本插件有**两条独立判定链**：

- **HTTPS 加密流量** 走 **进程级判定**：由 `CmdlineBlacklist` / `CmdlineWhitelist` / `Https` 决定哪些进程被纳入采集。
- **HTTP 明文流量** 走 **目的地级判定**：由 `Http` 列表按"目的端口 / 目的 IP / 目的域名"过滤要采集的流量，与进程无关。

两条链相互独立、互不影响；同一名单内的多条规则之间为 **OR**（命中任一条即可）。

##### 1. HTTPS 加密流量采集（进程级）

进程是否纳入采集，按下列**固定顺序**判定（未配置项见下文「默认值」）：

1. 命中 `CmdlineBlacklist` → **不采集**（cmdline 黑名单优先）
2. 未命中黑名单，且命中 `CmdlineWhitelist` → **纳入采集**
3. 仍未纳入，且进程访问域名命中 `Https` → **纳入采集**
4. 以上均未命中 → **不采集**

```mermaid
graph TD
    A["进程是否纳入 HTTPS 采集?"] --> B{"命中 CmdlineBlacklist?"}
    B -->|是| N["不采集"]
    B -->|否| C{"命中 CmdlineWhitelist?"}
    C -->|是| Y["纳入采集"]
    C -->|否| D{"访问域名命中 Https?"}
    D -->|是| Y
    D -->|否| N
```

例如：只配置了 cmdline 黑名单、未配 `Https` 时，仍会注入默认 `Https` 列表；只配 `Https`、未配 cmdline 黑白名单时，仍会注入下文的默认 cmdline 白名单。

##### 2. HTTP 明文流量采集（目的地级）

`Http` 是**纯流量白名单**，不区分进程，按目的地 `:端口` / `IP` / `IP:端口` / `域名` 命中即采集：

```mermaid
graph TD
    A["HTTP 明文流量是否采集?"] --> B{"目的地命中 Http?"}
    B -->|是| Y["采集该流量"]
    B -->|否| N["不采集"]
```

`Http` 列表 **为空时不采集任何明文 HTTP 流量**（默认关闭）；非空时仅采集命中目的地的流量。

#### Cmdline 规则优先级和默认注入值

1. **黑名单优先于白名单**：同一进程同时命中黑/白名单时，**黑名单生效**。
2. **多条白名单之间**：**OR**，命中任一条即可。
3. **默认注入条件**：`CmdlineWhitelist` 与 `CmdlineBlacklist` **均未配置**时，注入下表；一旦配置了 **任意一条** 用户 cmdline 白名单或黑名单，则 **不再** 注入默认 cmdline。
4. **空数组拒绝**：显式写 `CmdlineWhitelist: []` 会被视为非法配置；不写该字段才会走默认注入。

**默认 `CmdlineWhitelist`（9 条）**

| AgentType | Args（cmdline 各段 glob） |
| --- | --- |
| `hermes` | `hermes*` |
| `hermes` | `*python*`, `*hermes*` |
| `hermes` | `*python*`, `-m`, `*hermes*` |
| `cosh` | `node*`, `*/usr/bin/co*` |
| `cosh` | `node*`, `*/usr/bin/cosh*` |
| `cosh` | `node*`, `*/usr/bin/copliot*` |
| `cosh` | `node*`, `*copilot-shell*` |
| `openclaw` | `*openclaw-gatewa*` |
| `openclaw` | `node*`, `*openclaw*` |

#### 自定义示例（覆盖默认）

填写后只使用用户规则，不再叠加内置：

```yaml
ProbeConfig:
  CmdlineWhitelist:
    - AgentType: openclaw
      Args: ["node*", "*openclaw*"]
    - AgentType: claude-code
      Args: ["node*", "*claude*"]
```

#### Https 规则优先级和默认注入值

1. **多条 Https 规则之间**：**OR**，命中任一条即可。
2. **默认注入条件**：`Https` 列表 **为空** 时，注入下表；一旦配置了 **任意一条** 用户规则，则 **不再** 注入默认值。

**默认 `Https`（4 条，精确主机名）**

| 域名 |
| --- |
| `api.openai.com` |
| `api.anthropic.com` |
| `dashscope.aliyuncs.com` |
| `dashscope-intl.aliyuncs.com` |

#### Http 规则优先级和默认注入值

1. **多条 Http 条目之间**：**OR**，命中任一条即可，顺序无关。
2. **每一项**可写以下四种形态之一：
   - `:端口`（如 `:8080`）：匹配任意目的 IP + 指定端口。
   - `IP`（如 `10.0.0.1`）：匹配指定目的 IPv4 + 任意端口。
   - `IP:端口`（如 `10.0.0.1:9090`）：精确匹配目的 IPv4 + 端口。
   - `域名`（如 `model-svc.default.svc`、`*.internal.svc`）：在**运行时**根据域名解析结果动态生效。
3. **默认注入条件**：`Http` 列表 **为空** 时不注入任何默认值，明文 HTTP 流量采集**默认关闭**。

#### `gen_ai.agent.type` 的取值规则

`gen_ai.agent.type` **只来自 cmdline 白名单**（用户配置或内置默认）中命中规则的 `AgentType`，与 `Https` / `Http` 列表无直接映射关系。按下列顺序确定：

1. 进程被当前生效的 cmdline 白名单命中 → 取**第一条**命中规则的 `AgentType`。
2. 进程仅靠 `Https` 纳入采集，且 cmdline 未命中（用户已覆盖默认时）→ 二次匹配 **内置默认 9 条**；命中则输出对应类型（如 `openclaw`）。
3. 仍匹配不上 → **不输出** `gen_ai.agent.type`。

「只配 `Https`、不配 cmdline」是允许的：cmdline 走内置默认 9 条 + `Https` 走用户配置，互相独立生效。**`Https` / `Http` 列表中的条目本身不会作为** `gen_ai.agent.type`。

### Cmdline 规则自定义写法

配置里**每一项**是一条白名单规则对象，包含两个字段：

| 字段 | 说明 |
| --- | --- |
| `AgentType` | 命中该规则后，写入日志 `gen_ai.agent.type` 的类型标识（如 `openclaw`）。多条规则可使用相同 `AgentType`。取值规范见上文。 |
| `Args` | 与进程 cmdline 各参数（即 `argv`，与 `/proc/<PID>/cmdline` 一致）按位置做 glob 匹配的字符串数组。 |

先在本机查看真实命令行，再写 glob：

```bash
tr '\0' ' ' < /proc/<PID>/cmdline; echo
```

每一段用 **glob** 匹配，不关心的位置写 `"*"`。

**前缀匹配**：当 `Args` 的段数**少于** cmdline 实际参数数时，只对前 N 段按位置做 glob 匹配，**后面多出来的参数不参与匹配**。例如 `Args: ["node*", "*openclaw*"]` 可命中 `node openclaw.js gateway`（第三段 `gateway` 被忽略）。若需约束后续参数，须在 `Args` 中继续写出对应段。反之，`Args` 段数**多于** cmdline 实际参数数时则不命中。

**示例**（须写在 `ProbeConfig` 下）：

```yaml
ProbeConfig:
  CmdlineWhitelist:
    - AgentType: openclaw
      Args: ["node*", "*openclaw*"]
    - AgentType: hermes
      Args: ["*python*", "*hermes*"]
```

同一进程命中多条规则时，**采集仍生效**；`gen_ai.agent.type` 取**列表中第一条**命中规则的 `AgentType`。若需固定类型，把更具体的规则排在前面，或避免 glob 重叠。

### Https 规则自定义写法

`Https` 里每一项用于匹配进程访问的大模型 API 主机名。**默认注入为精确主机名**；自行配置时也可写 glob（如 `*.anthropic.com`），通配符为 `*`，匹配 **不区分大小写**。示例：

```yaml
Https:
  - "api.openai.com"
  - "dashscope.aliyuncs.com"
  - "*.anthropic.com"
```

### Http 规则自定义写法

`Http` 里每一项可以是 `:端口`、`IP`、`IP:端口` 或域名（含 glob），四种形态可混合书写。命中其中之一即对该明文 HTTP 流量进行采集。示例：

```yaml
Http:
  - ":8080"
  - "10.0.0.1:9090"
  - "model-svc.default.svc"
  - "*.internal.svc"
```

### `StreamModeFormat: false`（合并日志）

一次 LLM 调用输出 **一条** 日志，同时包含请求与响应字段（见下表）。

- **无** `event.name`、**无** `event.id`
- 时间戳为**请求开始**时刻
- 同条内可有 `gen_ai.request.model`、`gen_ai.response.id`、`status_code`、`gen_ai.response.duration`、`gen_ai.response.finish_reasons`（JSON 数组）等

`Http` / `Https` 只影响**是否采集**对应流量，不改变上述合并/拆分形态。

### `StreamModeFormat: true`（默认，流式拆分）

一次 LLM 调用在同一日志组内输出 **两条** 日志，通过 `gen_ai.session.id`、`gen_ai.turn.id` 等关联：

| `event.name` | 时间戳 | 主要字段 |
| :--- | :--- | :--- |
| `gen_ai.model.request` | 请求开始（`timestamp_ns`） | `gen_ai.input.messages`（`AutoMessageTrim: false` 且去重允许时）、`gen_ai.input.messages.delta`、`gen_ai.input.messages_hash`、`gen_ai.system_instructions`、`gen_ai.tool.definitions`（后两者 `AutoMessageTrim: false` 时）、`gen_ai.request.model`、`server.*`、`gen_ai.request.timestamp` |
| `gen_ai.model.response` | 请求结束（开始 + `duration_ns`） | `gen_ai.output.messages`（始终）、`gen_ai.response.id`、`gen_ai.response.model`（非空时）、`gen_ai.response.finish_reasons`、`gen_ai.usage.*`（token，无 cost）、`status_code`、`is_sse`、`gen_ai.response.duration`、`gen_ai.provider.name` |

两条日志均可能包含：`event.id`（仅流式拆分）、`gen_ai.session.id`、`gen_ai.turn.id`、`pid`、`comm`、`gen_ai.agent.type`、`gen_ai.provider.name`。

### 字段表（合并模式 / 拆分模式中的并集）

| 字段 | 类型 | 说明 |
| :--- | :--- | :--- |
| `event.id` | string | 本条日志的唯一标识（UUID，大写带连字符）；**仅** `StreamModeFormat: true` 时输出，request/response 各 1 个 |
| `event.name` | string | **`gen_ai.model.request`** 或 **`gen_ai.model.response`**；**仅** `StreamModeFormat: true` 时输出 |
| `gen_ai.session.id` | string | 用户的会话 id |
| `gen_ai.turn.id` | string | 同一会话中其中一次对话的 id |
| `gen_ai.response.id` | string | 一次对话中其中一次对大模型请求的回复 id |
| `pid` | string | 进程号（十进制字符串） |
| `comm` | string | 进程名称 |
| `gen_ai.agent.type` | string | Agent **类型**（如 `openclaw`、`claude-code`），来自 cmdline 白名单 |
| `gen_ai.request.timestamp` | string | 一次对大模型请求开始的时间，毫秒时间戳（十进制字符串） |
| `gen_ai.response.duration` | string | 一次对大模型请求到大模型回复的耗时，毫秒（十进制字符串） |
| `server.address` | string | 从请求 URL 解析出的服务端主机名（有请求 URL 时输出） |
| `server.port` | string | 从请求 URL 解析出的端口（URL 中含显式端口时输出） |
| `gen_ai.provider.name` | string | 大模型厂商名称 |
| `gen_ai.request.model` | string | 请求侧模型名；合并日志与 request 条输出 |
| `gen_ai.response.model` | string | 响应侧模型名（非空时）；**仅** `StreamModeFormat: true` 的 response 条；合并日志**不**单独输出此字段（用 `gen_ai.request.model`） |
| `status_code` | string | 一次请求的状态码，同 HTTP 状态码（十进制字符串，如 `200`）；合并条或 response 条 |
| `is_sse` | string | 是否为 SSE（Server-Sent Events）连接；日志中取值为 `1`（是）或 `0`（否） |
| `gen_ai.response.finish_reasons` | string | 停止原因 **JSON 数组字符串**（如 `["stop"]`、`["tool_calls","stop"]`）；从 `output.messages`（含 `parts` 内）收集，无则将 FFI 单值包装为单元素数组 |
| `is_usage_from_api` | string | 数据来源标识，true 表示来自 LLM API response usage 字段（精确值），false 表示由插件本地估算（近似值） |
| `gen_ai.usage.input_tokens` | string | 发送给模型的 token 数量（十进制字符串） |
| `gen_ai.usage.output_tokens` | string | 模型实际生成的回复内容长度（十进制字符串） |
| `gen_ai.usage.total_tokens` | string | 一次请求消耗的 Token 总量（十进制字符串） |
| `gen_ai.usage.cache_creation.input_tokens` | string | 本次请求中，被系统新写入缓存的那部分输入 Token 数量（十进制字符串） |
| `gen_ai.usage.cache_read.input_tokens` | string | 本次请求中，直接从已有缓存中命中并读取的输入 Token 数量（十进制字符串） |
| `gen_ai.input.messages` | string | 完整 messages JSON 数组（**仅** `AutoMessageTrim: false` 且同 session 去重判定需上传时，非空则输出） |
| `gen_ai.input.messages.delta` | string | 当次 LLM 请求对应的 input messages 片段（JSON 数组字符串，非空时输出） |
| `gen_ai.input.messages_hash` | string | 当次全量 `gen_ai.input.messages` 的 SHA-256 摘要（十六进制字符串，非空时输出） |
| `gen_ai.system_instructions` | string | 系统指令（system 角色）消息（JSON 字符串，**仅** `AutoMessageTrim: false`，非空时输出） |
| `gen_ai.tool.definitions` | string | 请求 tools 定义 JSON 数组（**仅** `AutoMessageTrim: false`，非空时每次输出） |
| `gen_ai.output.messages` | string | 大模型回复 message 的序列化 json（**不受** `AutoMessageTrim` 控制，非空时输出） |

本表字段均由插件 `SetContent` 写入日志内容，**键值类型均为字符串**。其中带数值语义的字段以十进制文本（或 `is_sse` 的 `1`/`0`）落盘，与实现一致；并非日志 schema 中的强类型整型/浮点列。

### `AutoMessageTrim` 与 `gen_ai.input.messages_hash`

- `AutoMessageTrim: true`（默认）：不输出 system、tools、全量 input；**始终**输出 `gen_ai.input.messages.delta`（若有）、`hash`（若有）、`gen_ai.output.messages`（若有）。
- `AutoMessageTrim: false`：`gen_ai.system_instructions`、`gen_ai.tool.definitions` 非空时**每次**输出。全量 `gen_ai.input.messages` 在同一 `gen_ai.session.id`（无 session 时用 `gen_ai.turn.id`）下，若相对上次**仅尾部追加**且前缀 hash 与上次一致则**省略**（仍输出 `delta` 与 `hash`）。
- `gen_ai.input.messages_hash`：按 session 维护全量数组 hash（非空时输出）；与去重逻辑配合：下游可用 hash 判断何时需拉取缺失的全量 `gen_ai.input.messages`。

### `StreamModeFormat: false` 且 `AutoMessageTrim: true`（最瘦合并日志）

- 每次 LLM 调用 **一条** 日志，**无** `event.name`、**无** `event.id`；时间戳为请求开始时刻。
- **有**：关联字段、HTTP/`usage` 元数据、`gen_ai.input.messages.delta` / `gen_ai.input.messages_hash`（非空时）、`gen_ai.output.messages`。
- **无**：全量 `gen_ai.input.messages`、`gen_ai.system_instructions`、`gen_ai.tool.definitions`、拆分后的 `gen_ai.model.request` / `gen_ai.model.response`。

## 样例

### 采集 agent 与 llm 交互数据

- 输入

打开 agent 进行交流

- 采集配置

```yaml
enable: true
inputs:
  - Type: input_agentsight
    ProbeConfig:
      Verbose: 1
      LogPath: ""
      CmdlineWhitelist:
        - AgentType: openclaw
          Args: ["node*", "*openclaw*"]
      CmdlineBlacklist:
        - ["node*", "*webpack*"]
      Https:
        - "api.openai.com"
      Http:
        - ":8080"
        - "10.0.0.1:9090"
      StreamModeFormat: false
      AutoMessageTrim: false
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```

- 输出（`StreamModeFormat: false`，单条合并日志）

{
  "gen_ai.agent.type": "openclaw",
  "gen_ai.turn.id": "c47ac487c54c2da859ba2a0e873eeeae",
  "gen_ai.input.messages": [
    {
      "role": "system",
      "parts": [
        {
          "type": "text",
          "content": "You are a personal assistant running inside OpenClaw.\n## Tooling\n..."
        }
      ]
    },
    {
      "role": "user",
      "parts": [
        {
          "type": "text",
          "content": "今天晚饭吃什么？"
        }
      ]
    }
  ],
  "gen_ai.input.messages.delta": [
    {
      "role": "user",
      "parts": [
        {
          "type": "text",
          "content": "今天晚饭吃什么？"
        }
      ]
    }
  ],
  "gen_ai.input.messages_hash": "7f3b2c1a9e8d4f5061728394a5b6c7d8e9f0a1b2c3d4e5f6789012345678abcdef",
  "gen_ai.output.messages": [
    {
      "role": "assistant",
      "parts": [
        {
          "type": "reasoning",
          "content": "说不吃米饭\n"
        },
        {
          "type": "text",
          "content": "不吃米饭啊！"
        }
      ],
      "finish_reason": "stop"
    }
  ],
  "gen_ai.tool.definitions": [
    {
      "type": "function",
      "function": {
        "name": "read",
        "description": "Read file contents",
        "parameters": {
          "type": "object",
          "properties": {
            "path": {"type": "string"}
          },
          "required": ["path"]
        }
      }
    }
  ],
  "gen_ai.provider.name": "openai",
  "gen_ai.request.model": "qwen3.5-plus",
  "gen_ai.request.timestamp": "1749123456789",
  "gen_ai.response.duration": "3548",
  "gen_ai.response.finish_reasons": "[\"stop\"]",
  "gen_ai.response.id": "chatcmpl-3cd5d2d2-d2f5-91e9-a5e4-7fb740bb47f6",
  "gen_ai.usage.cache_creation.input_tokens": "0",
  "gen_ai.usage.cache_read.input_tokens": "0",
  "gen_ai.usage.input_tokens": "27466",
  "gen_ai.usage.output_tokens": "195",
  "gen_ai.usage.total_tokens": "27661",
  "is_sse": "1",
  "is_usage_from_api": "true",
  "pid": "705127",
  "comm": "openclaw-gatewa",
  "server.address": "dashscope.aliyuncs.com",
  "server.port": "80",
  "gen_ai.session.id": "dea5eed6-4a08-436c-b117-5ea14c9de39a",
  "status_code": "200"
}

- 采集配置（`StreamModeFormat: true`，其余同上，仅改此项）

```yaml
      StreamModeFormat: true
      AutoMessageTrim: false
```

- 输出（`StreamModeFormat: true`，同一 `gen_ai.turn.id` 下两条日志）

**`event.name` = `gen_ai.model.request`**

```json
{
  "event.id": "A1B2C3D4-E5F6-7890-ABCD-EF1234567890",
  "event.name": "gen_ai.model.request",
  "gen_ai.agent.type": "openclaw",
  "gen_ai.session.id": "dea5eed6-4a08-436c-b117-5ea14c9de39a",
  "gen_ai.turn.id": "c47ac487c54c2da859ba2a0e873eeeae",
  "gen_ai.input.messages": [
    {
      "role": "system",
      "parts": [{"type": "text", "content": "You are a personal assistant running inside OpenClaw.\n## Tooling\n..."}]
    },
    {
      "role": "user",
      "parts": [{"type": "text", "content": "今天晚饭吃什么？"}]
    }
  ],
  "gen_ai.input.messages.delta": [
    {
      "role": "user",
      "parts": [{"type": "text", "content": "今天晚饭吃什么？"}]
    }
  ],
  "gen_ai.input.messages_hash": "7f3b2c1a9e8d4f5061728394a5b6c7d8e9f0a1b2c3d4e5f6789012345678abcdef",
  "gen_ai.system_instructions": "{\"role\":\"system\",\"parts\":[{\"type\":\"text\",\"content\":\"You are a personal assistant...\"}]}",
  "gen_ai.tool.definitions": "[{\"type\":\"function\",\"function\":{\"name\":\"read\",\"description\":\"Read file contents\"}}]",
  "gen_ai.provider.name": "openai",
  "gen_ai.request.model": "qwen3.5-plus",
  "gen_ai.request.timestamp": "1749123456789",
  "pid": "705127",
  "comm": "openclaw-gatewa",
  "server.address": "dashscope.aliyuncs.com",
  "server.port": "80"
}
```

**`event.name` = `gen_ai.model.response`**

```json
{
  "event.id": "F0E1D2C3-B4A5-9678-9012-3456789ABCDE",
  "event.name": "gen_ai.model.response",
  "gen_ai.agent.type": "openclaw",
  "gen_ai.session.id": "dea5eed6-4a08-436c-b117-5ea14c9de39a",
  "gen_ai.turn.id": "c47ac487c54c2da859ba2a0e873eeeae",
  "gen_ai.output.messages": [
    {
      "role": "assistant",
      "parts": [
        {"type": "reasoning", "content": "说不吃米饭\n"},
        {"type": "text", "content": "不吃米饭啊！"}
      ],
      "finish_reason": "stop"
    }
  ],
  "gen_ai.response.id": "chatcmpl-3cd5d2d2-d2f5-91e9-a5e4-7fb740bb47f6",
  "gen_ai.response.model": "qwen3.5-plus",
  "gen_ai.response.finish_reasons": "[\"stop\"]",
  "gen_ai.response.duration": "3548",
  "gen_ai.provider.name": "openai",
  "gen_ai.usage.input_tokens": "27466",
  "gen_ai.usage.output_tokens": "195",
  "gen_ai.usage.total_tokens": "27661",
  "gen_ai.usage.cache_creation.input_tokens": "0",
  "gen_ai.usage.cache_read.input_tokens": "0",
  "is_sse": "1",
  "is_usage_from_api": "true",
  "status_code": "200",
  "pid": "705127",
  "comm": "openclaw-gatewa"
}
```
