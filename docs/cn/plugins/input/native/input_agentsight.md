# input-agentsight 插件

## 简介

`input_agentsight` 插件实现对当前openclaw，hermes等agent工具等采集，支持的大模型供应商包括OpenAI，Anthropic，以及国内的厂商协议

## 版本

dev

## 版本说明

* 推荐版本：LoongCollector v3.1.4 及以上

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为 `input_agentsight`  |
|  ProbeConfig  |  object  |  否  |  /  |  插件配置参数列表  |
|  ProbeConfig.Verbose  |  uint  |  否  |  /  |  是否打印ebpf的详细日志，1代表开启，0代表关闭  |
|  ProbeConfig.LogPath  |  string  |  否  |  ""  | ebpf日志的输出位置  |
|  ProbeConfig.CmdlineWhitelist  |  array  |  否  |  /  |  进程命令行 **白名单**：数组的每一项为 **字符串数组**，与进程 `argv` 按位置一一 glob 匹配；由 AgentSight 进程匹配引擎消费。见下文「优先级与默认值」。  |
|  ProbeConfig.CmdlineBlacklist  |  array  |  否  |  /  |  进程命令行 **黑名单**，格式同 `CmdlineWhitelist`；命中则不 attach。**优先级高于白名单**（见下文）。  |
|  ProbeConfig.DomainWhitelist  |  array  |  否  |  /  |  域名 glob **白名单**（字符串数组），用于 AgentSight **DNS/SNI 阶段**的域名过滤；与 Prometheus 等插件中的 `array` 类似，每一项为字符串。未配置时不注入域名规则（见下文）。  |

为兼容旧配置，仍支持 **`ProbeConfig.CmdlineRules`**（内含 `whitelist` / `blacklist`）与 **`ProbeConfig.DomainRules`**：仅当 **未** 出现 **`CmdlineWhitelist` 与 `CmdlineBlacklist` 任一键**（YAML 中均未写字段）时才读取 `CmdlineRules`；域名在 **未** 出现 **`DomainWhitelist` 键**时才回退读取 **`DomainRules`**。若新旧键同时存在，**以拍平键为准**（嵌套/旧键中与之重叠的部分不再生效）。

### 优先级与默认值

**Cmdline（进程命令行）**

1. **黑名单优先于白名单**：同一进程若同时命中黑名单与白名单中的规则，**黑名单生效**（不 attach），语义与 AgentSight / coolbpf 一致。
2. **多条白名单规则之间**：为 **OR** 关系，任一 allow 规则匹配即可参与后续阶段（具体 attach 逻辑以 AgentSight 为准）。
3. **默认白名单注入**：当 **`CmdlineWhitelist` 与 `CmdlineBlacklist` 在解析结果中均为空**（未写拍平键且未使用兼容的 `CmdlineRules`，或写了但数组解析后仍无任何有效行）时，LoongCollector 会注入与内置文件 `core/_thirdparty/coolbpf/src/agentsight/agentsight.json` 中 `cmdline.allow` 一致的 **9 条**默认进程白名单（Hermes×3、Cosh×4、OpenClaw×2）。  
   一旦配置了 **任意一条** 用户 cmdline 白名单或黑名单（含「仅黑名单」），则 **不再** 注入上述内置白名单，完全以用户配置为准。

**Domain（域名）**

1. **多条域名规则之间**：为 **OR**，任一 glob 命中即视为命中该阶段白名单（语义以 AgentSight 为准）。
2. **默认值**：**不注入** 内置域名列表；未配置 `DomainWhitelist`（且未回退到 `DomainRules`）时，不向 AgentSight 注册域名规则，行为由 AgentSight 默认策略决定。
3. **与 cmdline 的关系**：进程/cmdline 与域名分阶段参与决策；若配置了 `DomainWhitelist`，仅命中域名列表的连接才会在域名阶段满足白名单（整体 attach 逻辑以 AgentSight 文档为准）。

### Cmdline 规则怎么写（含示例）

配置里**每一项**是一条规则，对应进程启动时的 **`argv` 分段**（与 shell 解析后的参数一致）。请先在本机用下面命令看真实命令行，再对照写数组：

```bash
tr '\0' ' ' < /proc/<PID>/cmdline; echo
```

**示例 A：Node 起脚本**

若输出为：

`node /opt/app/openclaw/cli.js gateway`

则常见写法是**两段**规则：第一段对 `argv[0]`，第二段对 `argv[1]`（一般是脚本路径）：

```yaml
CmdlineWhitelist:
  - ["node", "*openclaw*"]   # argv0≈node，argv1 路径里含 openclaw（见下通配符）
```

**示例 B：单段可执行文件**

若输出只有：

`openclaw-gateway`

则第一段就要能描述整个 argv0，例如：

```yaml
CmdlineWhitelist:
  - ["openclaw-gateway"]
  - ["*openclaw-gatewa*"]    # 若希望兼容带路径的前缀，可用 glob
```

**第一个参数写 `node`，`/usr/bin/node` 能匹配吗？**

不一定。`argv[0]` 往往是**实际传入的第一段字符串**：可能是 `node`，也可能是 `/usr/bin/node`。规则里每一段都是 **glob**，是否命中取决于 AgentSight 的 glob 语义（见 coolbpf AgentSight 文档：**按位置对 `cmdline[i]` 做 glob 匹配**）。

- 若实际是 **`/usr/bin/node`**，只写 **`node`** 常常**对不上**整段路径。
- 更稳妥的写法是让第一段也带通配，例如 **`node*`**（内置规则里有 `node*` 形态）或 **`*node*`**，以你在目标进程上看到的 `cmdline` 为准。

**`*` 是什么？还支持别的吗？**

- 通配符来自 **glob** 语义，不是正则表达式。
- 文档中明确强调：不关心的参数位可写 **`"*"`** 表示「该段任意」。
- **域名规则**侧 coolbpf 文档写明支持 **`*`**（任意字符序列）与 **`?`**（单个字符）。
- **cmdline 规则**侧文档对 `*` 说明最充分；**`?` 等是否可用于每一段以当前 AgentSight / coolbpf 版本为准**（不确定时先用 `*` 或对照 `agentsight.json` 里已有写法）。

**大小写**

cmdline 与域名的 glob 匹配在 AgentSight 侧均为 **不区分大小写**（见 coolbpf `agentsight-c-ffi-api` 说明）。配置里写 `Node` 与 `node` 效果相同。

### Domain 规则怎么写（含示例）

`DomainWhitelist` 里每一项是一条 **域名 glob**，用于 **SNI / HTTP Host** 等阶段（以 AgentSight 为准）。多条规则为 **OR**：命中任意一条即可。

**示例**

```yaml
DomainWhitelist:
  - "api.openai.com"           # 精确主机名（仍走 glob 匹配，大小写不敏感）
  - "*.anthropic.com"          # 子域：如 claude.anthropic.com
  - "*openai.com"              # 更宽的后缀匹配（按 glob 语义）
```

从请求里取到的主机名会去掉端口再匹配，例如 `Host: Api.OpenAI.Com:443` 与 `api.openai.com` 视为同类（**不区分大小写**）。

通配符：**`*`**、**`?`**（单字符）；**不是正则**。

## 输出格式

| 字段 | 类型 | 说明 |
| :--- | :--- | :--- |
| `gen_ai.session.id` | string | 用户的会话 id |
| `gen_ai.turn.id` | string | 同一会话中其中一次对话的 id |
| `gen_ai.response.id` | string | 一次对话中其中一次对大模型请求的回复 id |
| `pid` | string | 进程号（十进制字符串） |
| `comm` | string | 进程名称 |
| `gen_ai.agent.name` | string | agent 名称 |
| `gen_ai.request.timestamp` | string | 一次对大模型请求开始的时间，毫秒时间戳（十进制字符串） |
| `gen_ai.response.duration` | string | 一次对大模型请求到大模型回复的耗时，毫秒（十进制字符串） |
| `server.address` | string | 从请求 URL 解析出的服务端主机名（有请求 URL 时输出） |
| `server.port` | string | 从请求 URL 解析出的端口（URL 中含显式端口时输出） |
| `gen_ai.provider.name` | string | 大模型厂商名称 |
| `gen_ai.request.model` | string | 大模型厂商使用的模型名称 |
| `status_code` | string | 一次请求的状态码，同 HTTP 状态码（十进制字符串，如 `200`） |
| `is_sse` | string | 是否为 SSE（Server-Sent Events）连接；日志中取值为 `1`（是）或 `0`（否） |
| `gen_ai.response.finish_reasons` | string | 大模型停止产生 token 的原因 |
| `is_usage_from_api` | string | 数据来源标识，true 表示来自 LLM API response usage 字段（精确值），false 表示由 AgentSight 本地 tokenizer 计算（近似值） |
| `gen_ai.usage.input_tokens` | string | 发送给模型的 token 数量（十进制字符串） |
| `gen_ai.usage.output_tokens` | string | 模型实际生成的回复内容长度（十进制字符串） |
| `gen_ai.usage.total_tokens` | string | 一次请求消耗的 Token 总量（十进制字符串） |
| `gen_ai.usage.cache_creation.input_tokens` | string | 本次请求中，被系统新写入缓存的那部分输入 Token 数量（十进制字符串） |
| `gen_ai.usage.cache_read.input_tokens` | string | 本次请求中，直接从已有缓存中命中并读取的输入 Token 数量（十进制字符串） |
| `gen_ai.input.messages` | string | 大模型请求 message 的序列化 json |
| `gen_ai.output.messages` | string | 大模型回复 message 的序列化 json |

本表字段均由插件 `SetContent` 写入日志内容，**键值类型均为字符串**。其中带数值语义的字段以十进制文本（或 `is_sse` 的 `1`/`0`）落盘，与实现一致；并非日志 schema 中的强类型整型/浮点列。

## 样例

### 采集agent与llm交互数据

- 输入

打开agent进行交流

- 采集配置

```yaml
enable: true
inputs:
  - Type: input_agentsight
    ProbeConfig:
      Verbose: 1
      LogPath: ""
      CmdlineWhitelist:
        - ["node", "*claude*"]
        - ["node", "*hermes*"]
      CmdlineBlacklist:
        - ["node", "*webpack*"]
      DomainWhitelist:
        - "api.openai.com"
        - "*.anthropic.com"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```

- 输出

{
  "gen_ai.agent.name": "OpenClaw",
  "gen_ai.turn.id": "c47ac487c54c2da859ba2a0e873eeeae",
  "gen_ai.input.messages": [
    {
      "role": "system",
      "parts": [
        {
          "type": "text",
          "content": "You are a personal assistant running inside OpenClaw.\n## Tooling\nTool availability (filtered by policy):\nTool names are case-sensitive. Call tools exactly as listed.\n- read: Read file contents\n- write: Create or overwrite files\n- edit: Make precise edits to files\n- exec: Run shell commands (pty available for TTY-required CLIs)\n- process: Manage background exec sessions\n- web_search: Search the web (Brave API)\n- web_fetch: Fetch and extract readable content from a URL\n- cron: Manage cron jobs and wake events (use for reminders; when scheduling a reminder, write the systemEvent text as something that will read like a reminder when it fires, and mention that it is a reminder depending on the time gap between setting and firing; include recent context in reminder text if appropriate)\n- sessions_list: List other sessions (incl. sub-agents) with filters/last\n- sessions_history: Fetch history for another session/sub-agent\n- se..."
        }
      ]
    }
  ],
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
  "gen_ai.provider.name": "openai",
  "gen_ai.request.model": "qwen3.5-plus",
  "gen_ai.request.timestamp": "1749123456789",
  "gen_ai.response.duration": "3548",
  "gen_ai.response.finish_reasons": "stop",
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
