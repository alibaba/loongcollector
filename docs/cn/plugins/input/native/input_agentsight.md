# input-agentsight 插件

## 简介

`input_agentsight` 插件实现对当前 openclaw、hermes 等 agent 工具等采集，支持的大模型供应商包括 OpenAI、Anthropic，以及国内的厂商协议。

## 事件能力

列含义见 [概览 · 事件能力列说明](../../overview.md#事件能力列说明)。

| Log | Metric | Span |
| --- | --- | --- |
| ✓ | — | — |

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
|  ProbeConfig.Https  |  array  |  否  |  内置 7 条  |  HTTPS 加密流量的域名白名单（字符串数组，glob 通配符 `*`，不区分大小写）。访问白名单内域名的进程可被识别为采集目标。未配置时注入默认列表，见下文。**注意本项只决定「哪些进程被纳入采集」，不限制已纳入进程的上报范围** —— 详见下文「采集范围」。  |
|  ProbeConfig.Http  |  array  |  否  |  `[]`（关闭）  |  HTTP 明文流量的目标列表（字符串数组）。每项可为 `:端口`、`IP`、`IP:端口` 或域名（如 `model-svc.default.svc`、`*.internal.svc`）。**留空时不采集明文 HTTP 流量**。  |
|  ProbeConfig.EventStreamFormat  |  bool  |  否  |  `true`  |  为 `true` 时，每次 LLM 调用在同一 `PipelineEventGroup` 内输出两条日志（各有 `event.id`）：`event.name=gen_ai.model.request`（请求开始时间戳）与 `gen_ai.model.response`（请求结束时间戳）。为 `false` 时输出单条合并日志，**无** `event.name` / `event.id`。  |
|  ProbeConfig.MessageDeltaOnly  |  bool  |  否  |  `true`  |  为 `true` 时**不**输出全量 `gen_ai.input.messages`；仍输出 `gen_ai.input.messages_delta`、`gen_ai.system_instructions_hash` / `gen_ai.tool.definitions_hash`（非空时），以及 hash 相对上一轮变化时的 `gen_ai.system_instructions` / `gen_ai.tool.definitions`。为 `false` 时**每次**输出非空的全量 `gen_ai.input.messages`。**不影响** `gen_ai.output.messages`；`messages_delta` 及 session 状态维护**不受**本开关影响。  |
|  ProbeConfig.RawHttpsFallback  |  bool  |  否  |  `false`  |  为 `true` 时，**无法解析为 LLM 语义**的流量（未知 API path、非标准 body）不再被丢弃，而是以原始 HTTP 形式上报（`event.name=http.request` / `http.response`，见下文）。需要 `libagentsight >= 0.9.0`；旧版动态库上该开关无效（日志告警后自动降级为关闭）。**默认关闭**：原始 body 未做任何脱敏。  |

### 容器元信息标签（自动附加）

当采集器运行在**容器化部署**（K8s DaemonSet、或挂载了 `/:/logtail_host` 的 docker 容器；判定依据为 `/logtail_host` 路径存在）时，本插件会自动把事件所属容器的元信息以**组级 Tag** 形式附加到每条事件组上——与 `input_file` / `input_container_stdio` 的容器元信息透传**同一渠道、同一套键名**，下游可用相同的键跨插件聚合。无需任何配置开关。

| Tag 键 | 说明 |
| --- | --- |
| `_container_name_` | 容器名 |
| `_image_name_` | 镜像名 |
| `_container_ip_` | 容器 IP |
| `_pod_name_` | Pod 名（K8s 场景） |
| `_namespace_` | 命名空间（K8s 场景） |
| `_pod_uid_` | Pod UID（K8s 场景） |

容器自定义元信息（如环境配置产生的附加标签）按原键名透传。在 SLS 中这些键呈现为 `__tag__:` 前缀；日志内容里的 `container.id` 字段（见字段表）保持不变。

**自动降级**（事件照常输出，仅缺少容器 Tag）：

- 采集器为宿主机直装等非容器化部署；
- 进程不在容器内（`container.id` 为空）；
- 容器刚退出、已移出容器快照，或事件早于启动后首轮容器快照完成（秒级窗口）；
- `container.id` 非完整 64 位容器 ID 的变体形态。

### `RawHttpsFallback: true` 输出的原始 HTTP 日志

仅在 AgentSight 无法把流量解析成 GenAI 语义时产生，与 `gen_ai.*` 日志**互斥**（一次流量最多产生一种）。

一次完整交换在同一个 `PipelineEventGroup` 内输出**两条**日志，共享同一个 `http.exchange.id` 供下游配对：`event.name=http.request`（请求开始时间戳）与 `http.response`（请求结束时间戳）。两条日志各有**自己的** `event.id`，先后顺序由 `event.sequence`（request=1、response=2）确定 —— 与 `gen_ai.*` 日志的 `event.id` / `gen_ai.step.id` / `gen_ai.event.sequence` 三者职责划分一致。

**响应侧确实为空**（无响应体且无响应头）的记录只输出请求那一条。注意判据不是 `status_code == 0`：HTTP/2 在 `:status` 伪头 HPACK 解码失败时也会上报 `status_code = 0`，但此时响应头和响应体是完整的，这类记录仍输出两条日志，`http.response.status_code` 为 `0`（下游据此区分「响应存在但状态码不可解」与「完全没有响应」，后者根本没有 `http.response` 那条日志）。

耗时由两条日志的时间戳之差表示，不单独输出 duration 字段。不受 `EventStreamFormat` 影响。

**公共字段**

| 字段 | 说明 |
| --- | --- |
| `event.name` | `http.request` 或 `http.response`，用于与 `gen_ai.*` 数据流区分 |
| `event.id` | **本条日志**的唯一标识（UUID），request / response 各 1 个，互不相同 —— 下游可安全用作去重、join、幂等写入的唯一键 |
| `http.exchange.id` | 配对键（UUID）：同一次交换的两条日志取相同值。对应 `gen_ai.*` 数据流的 `gen_ai.step.id`；raw 事件不写 `gen_ai.` 前缀，跨两条数据流查询时需 `COALESCE` 两个键名 |
| `event.sequence` | 交换内排序：request 为 `1`、response 为 `2`。`duration_ns == 0` 时两条日志时间戳相同，只能靠该字段定序 |
| `pid` / `comm` | 进程 ID 与进程名 |
| `cmdline` | 进程完整命令行（argv 以空格连接，截断到 127 字节）。进程已退出时为空，此时该字段不输出 |
| `agent.type` | 命中 `CmdlineWhitelist` 的 agent 类型（小写）。未命中任何规则时回退为进程名 —— 与 `gen_ai.*` 日志同一套解析口径，**但键名不带 `gen_ai.` 前缀**，见下方说明 |
| `container.id` | 从 `/proc/<pid>/cgroup` 解析的容器 ID；非容器进程为空，此时该字段不输出。容器化部署下容器元信息另以组级 Tag 附加，见上文「容器元信息标签（自动附加）」 |
| `url.scheme` | 固定 `https` |
| `time_unix_nano` / `observed_time_unix_nano` | 同 `gen_ai.*` 日志 |

> **raw 日志不输出任何 `gen_ai.*` 字段。** 这类事件存在的前提就是「流量没能解析成 GenAI 语义」，往 `gen_ai.*` 命名空间里写东西会误导下游。因此 agent 类型用裸键名 `agent.type`，与本路径已有的 `pid` / `comm` / `cmdline` / `container.id` 保持一致。
>
> `gen_ai.*` 日志仍用 `gen_ai.agent.type`，**取值完全相同、仅键名不同**。所以跨两条数据流做聚合时，需要把 `agent.type` 和 `gen_ai.agent.type` 归并处理（例如 `agent.type` 为空则取 `gen_ai.agent.type`）。
>
> 也**没有** `gen_ai.session.id` / `gen_ai.turn.id` —— raw 事件本身不存在 session 归属，只能靠 `(pid, comm)` 与同进程的 LLM 事件关联。

**`http.request` 独有**

| 字段 | 说明 |
| --- | --- |
| `http.request.method` / `url.path` | 请求方法与路径。**无完整 URL** |
| `server.address` / `server.port` | 目标主机与端口，从请求头的 `host`（HTTP/2 为 `:authority`）提取而来 |
| `user_agent.original` | 客户端 UA，从请求头 `user-agent` 提取（OTel Stable 字段名，**不**输出 `http.request.header.user-agent`）|
| `http.request.header.<小写头名>` | 白名单请求头，仅 `content-type`、`content-length`、`traceparent` 三个，见下方说明 |
| `http.request.body.content` / `.size` | 见下方「body 的保真度」 |

**`http.response` 独有**

| 字段 | 说明 |
| --- | --- |
| `http.response.status_code` / `is_sse` | 状态码、是否 SSE |
| `http.response.header.<小写头名>` | **逐头输出**（OTel semconv 模板名），如 `http.response.header.content-type`。头名统一小写。**敏感头、头名不合规的头均不输出，且每条日志上限 64 个**，见下方说明 |
| `http.response.body.content` / `.size` | 见下方「body 的保真度」 |

#### body 的保真度（重要）

**一句话原因**：AgentSight 的 analyzer 把 body 存成 Rust `String`（`analyzer/result.rs`），经 `String::from_utf8_lossy` 转换 —— 每个非法字节变成 U+FFFD（占 3 字节），原始字节在交给本插件之前就已丢失，不可恢复。

| 载荷类型 | `.content` | `.size` | 实测证据 |
| --- | --- | --- | --- |
| **JSON / 纯文本** | ✅ 线上原文 | ✅ 准确 | DashScope 原生 API 200 响应：`.size = 440`，与响应头 `content-length: 440` 完全一致 |
| **文本 + gzip 压缩** | ✅ 解压后原文（**响应侧**；HTTP/1 请求侧 ❌ 不解压） | ✅ 准确 | 响应头带 `content-encoding: gzip`，采到 112 字节明文，与不压缩时逐字节相同 |
| **文本 + chunked** | ✅ 去框后原文（**仅响应侧**；HTTP/1 请求侧 ❌ 不去框） | ✅ 准确 | 上述两例均为响应且带 `transfer-encoding: chunked`，采到的 body 无 `c3b\r\n` 残框 |
| **二进制**（protobuf / 二进制流 / 图片） | ❌ **U+FFFD 污染，静默失真** | ❌ **虚高** | 发送 256 字节 `0x00..0xFF`，采到 `.size = 512`（128 ASCII + 128 × 3 字节替换字符） |

使用者必须知道的两点：

1. **没有任何字段标记失真。** 二进制载荷的 `.content` 看起来"有内容"，但已不是原始数据，无法从日志本身判断是否可信。
2. **`.size` 不能当线上长度用。** 文本场景下它等于 `content-length`，可用于交叉校验；二进制场景下是转换后的长度，每个非法字节膨胀 3 倍。

本插件不做 base64 等编码兜底 —— 拿到的字节已经不是原始数据，编码也无从恢复。根治需在上游（anolisa）把 body 改成 `Vec<u8>` 贯穿 analyzer 与 FFI；`AgentsightHttpsData` 的 ABI 本来就是 `(ptr, len)` 形式，届时本插件侧无需改动。

#### 去框与解压：请求侧和响应侧不对称

上面保真度表格的证据**全部取自响应**。上游对两侧用的是不同的访问器，请求侧既不去框也不解压：

| 场景 | chunked 去框 | gzip 解压 | 上游实现 |
| --- | --- | --- | --- |
| HTTP/1 响应（非 SSE） | ✅ | ✅ | `analyzer/unified.rs` 走 `body_str_decompressed()` → `dechunked_body()` |
| HTTP/1 响应（SSE） | 不适用（按 SSE 事件解析后重组） | — | `analyzer/unified.rs` 走 `resp.json_body()` |
| **HTTP/1 请求** | ❌ | ❌ | 走 `req.body()` 原始字节；`ParsedRequest` 只有 `body` / `body_str` / `json_body`，**没有**去框或解压访问器 |
| HTTP/2 双向 | 不适用（h2 分块在帧层，无 `transfer-encoding: chunked`） | ✅ | `aggregator/http2.rs` 的 `request_body_str()` / `response_body_str()` |

因此 **chunked 残框只出现在两种情况**，不要按「一律可能有框」来处理：

1. **HTTP/1 请求体：恒定带框。** `http.request.body.content` 会是 `c3b\r\n...\r\n0\r\n\r\n` 形态的原文。接入方要解析 chunked 请求体**必须自己 dechunk**。
2. **HTTP/1 响应体的降级情形。** 去框以 `transfer-encoding` 头存在为前提（`is_chunked()`），该头缺失或被改写时不去框；另外 `dechunk_body()` 对非空 body 解出空结果时会回退到原始 body（`parser/http/response.rs`）。

请求侧的根治需要在 AgentSight（Rust）侧给 `ParsedRequest` 补上与响应侧对等的访问器。

> **请求头按白名单过滤。** `Authorization` / `x-api-key` / `cookie` 这类头携带有效凭据，而本路径原样上报、不做任何脱敏，输出即等于把凭据写入磁盘（实测确认过）。因此请求头采用**白名单**：只输出 `content-type`、`content-length`、`traceparent` 三个（键名 `http.request.header.<小写头名>`），另外 `host` 与 `user-agent` 分别提取为 `server.address` / `server.port` 与 `user_agent.original`。**白名单之外的头一律丢弃**，包括未知厂商的自定义头 —— 这一侧宁可少报也不能漏一个凭据。
>
> 这三个头之所以值得救回：`content-type` 直接解释「为什么没能解析成 GenAI 语义」；`content-length` 是判定 body 是否被上游 `from_utf8_lossy` 污染/截断的唯一交叉校验基准；`traceparent` 是把 raw 日志与应用侧 trace 关联起来的唯一途径。新增白名单头请修改 `AgentsightManager.cpp` 的 `kAllowedRequestHeaders` 常量。
>
> 白名单模型天然 fail-safe：HTTP/2 上头名可能无法解码（见下方响应头说明），解不出的头名匹配不上白名单，自动被丢弃。代价是 `traceparent` 不在 HPACK 静态表内，h2 场景下**可能采不到** —— 丢一个诊断字段，而不是漏一个凭据。
>
> **响应头则相反，按黑名单过滤后逐头输出。** 两侧风险画像正好镜像：请求头敏感面大、价值集中在少数几个已知头名，所以白名单更安全；响应头是诊断价值的主要来源、厂商自定义头多，白名单会挡掉大量有用信息，所以用黑名单：以下头名（小写全等匹配）永不落盘 —— `set-cookie`、`set-cookie2`、`www-authenticate`、`proxy-authenticate`、`authorization`、`proxy-authorization`、`x-api-key`、`api-key`、`x-auth-token`、`x-amz-security-token`。`set-cookie` 是可直接重放的会话凭据，`*-authenticate` 带 challenge/nonce。
>
> **头名不合规的整项丢弃（连值一起）。** 黑名单是按「解码后的头名」匹配的，而 HTTP/2 的头名来自**无状态** HPACK 解码（`decode_headers_stateless`，没有动态表状态）：当头名以动态表索引形式给出时，上游无法还原，会替换成字面量 `<unknown:N>`，**而值是完整真实值**。黑名单 10 项里只有 `set-cookie`、`www-authenticate`、`proxy-authenticate`、`authorization`、`proxy-authorization` 在 HPACK 静态表内；`set-cookie2`、`x-api-key`、`api-key`、`x-auth-token`、`x-amz-security-token` 不在 —— 而把自定义头放进动态表复用正是 HPACK 的用途，所以长连接上第二次出现的 `x-api-key` 会以 `<unknown:62>` 出现。因此凡头名不符合 RFC 7230 token 字符集的项一律整项丢弃：头名不可信时无法判断是否敏感，宁可少报一个诊断字段，不能漏一个凭据。该规则同时挡掉 `<dynamic:N>`、Huffman 解码失败产生的乱码头名，以及 `:status` 这类伪头（`:` 不是 token 字符；状态码本来就有 `http.response.status_code`，不需要重复输出）。
>
> **每条日志最多输出 64 个响应头**，与 HTTP/1 侧 `httparse` 的 `MAX_HEADERS = 64` 对齐。HTTP/2 侧上游没有条数上限，而逐头输出意味着每个头名都是一个字段名，不设上限会让字段名基数无限膨胀 —— 对按字段名建动态列的存储侧是运维风险。超限和头名不合规的丢弃条数会打 DEBUG 日志，不静默。
>
> **残余风险：黑名单是开放集合。** 未知厂商在自定义响应头（如某些网关的 `x-*`）里回吐 token、且头名能正常解码的情况会默认放行。发现新的敏感响应头请补充到 `AgentsightManager.cpp` 的 `kSensitiveResponseHeaders` 常量。
>
> 但请注意 **body 仍是原文**：部分厂商在请求体内传凭据，这类内容依然会落盘 —— 这是原始采集的固有性质。
>
> body 的去框 / 解压情况见上方「去框与解压：请求侧和响应侧不对称」—— 与脱敏无关，但同样影响接入方要不要自己写解析逻辑。

#### 采集范围：`Https` 域名列表**不限制**上报范围

容易误解的一点：`Https` 只决定**给哪些进程挂 SSL 探针**，不过滤上报内容。一旦某进程被纳入采集（无论是命中 `CmdlineWhitelist`、还是命中 `Https` 域名规则），它的**全部**无法解析为 LLM 语义的 HTTPS 流量都会产生 raw 事件，**与 `Https` 列表无关**。

这是上游刻意的设计（`alibaba/anolisa#1665` 为此移除了按域名过滤上报的 `HttpReportFilter`）。实际后果：一个被纳入采集的进程，其遥测上报、健康检查、对象存储下载等全部非 LLM 流量都会被原文写出。

因此收窄 `Https` 只减少「被 attach 的进程数」，**不减少已 attach 进程的上报范围**。真正能控制范围的是：

- **`CmdlineBlacklist`** —— 排除不希望被采集的进程（优先级高于白名单）
- **不开 `RawHttpsFallback`** —— LLM 调用本身走 `gen_ai.*`，raw 主要捞的是非 LLM 噪音

在多用户共享的机器上尤其要注意：`Https` 里的域名一旦被别人的进程访问，那个进程也会被 attach，其全部流量随之落盘。

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
| `codex` | `Codex` |

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

**默认 `Https`（6 条）**

| 域名 | 说明 |
| --- | --- |
| `api.openai.com` | OpenAI |
| `api.anthropic.com` | Anthropic |
| `dashscope.aliyuncs.com` | DashScope/百炼 按量付费·华北2（北京） |
| `dashscope-intl.aliyuncs.com` | DashScope/百炼 按量付费·新加坡 |
| `dashscope-us.aliyuncs.com` | DashScope/百炼 按量付费·美国（弗吉尼亚） |
| `coding.dashscope.aliyuncs.com` | DashScope/百炼 Coding Plan |
| `*.maas.aliyuncs.com` | DashScope/百炼 业务空间专属 / 试用 / Token Plan 域名 |

> DashScope/百炼 的业务空间专属、试用与 Token Plan 均使用 `[workspace-id｜trial｜token-plan].[region].maas.aliyuncs.com` 形式的域名——`workspace-id` 与地域（`cn-beijing`、`ap-southeast-1`、`ap-northeast-1`、`eu-central-1` 等）为动态前缀，无法穷举，因此用通配 `*.maas.aliyuncs.com` 统一覆盖（glob `*` 可跨 `.`）。
>
> 通配域名仅用于 **SNI 层的 SSL/TLS 探针挂载**（HTTPS 加密流量采集主路径）；基于 TCP 连接目的 IP 的进程发现会跳过通配域名（无法 DNS 解析），对加密流量采集本身无影响。

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

### Codex 采集支持说明

官方发布的 Codex CLI 是 strip 后的静态链接二进制，缺少符号表，**不支持** HTTPS 流量采集。如需采集，请使用**自行编译的未 strip 版本**（保留符号表即可自动定位 SSL 收发函数）。

### `EventStreamFormat: false`（合并日志）

一次 LLM 调用输出 **一条** 日志，同时包含请求与响应字段（见下表）。

- **无** `event.name`、**无** `event.id`
- 时间戳为**请求开始**时刻
- 同条内可有 `gen_ai.request.model`、`gen_ai.response.id`、`status_code`、`gen_ai.response.duration`、`gen_ai.response.finish_reasons`（JSON 数组）等

`Http` / `Https` 只影响**是否采集**对应流量，不改变上述合并/拆分形态。

### `EventStreamFormat: true`（默认，流式拆分）

一次 LLM 调用在同一日志组内输出 **两条** 日志，通过 `gen_ai.session.id`、`gen_ai.turn.id` 等关联：

| `event.name` | 时间戳 | 主要字段 |
| :--- | :--- | :--- |
| `gen_ai.model.request` | 请求开始时刻 | `gen_ai.input.messages`（`MessageDeltaOnly: false` 且非空时**每次**）、`gen_ai.input.messages_delta`、`gen_ai.system_instructions_hash` / `gen_ai.tool.definitions_hash`（非空时）、`gen_ai.system_instructions` / `gen_ai.tool.definitions`（hash 相对 session 上一轮变化时）、`gen_ai.request.model`、`server.*`、`time_unix_nano`、`observed_time_unix_nano` |
| `gen_ai.model.response` | 请求结束时刻（开始 + 耗时） | `gen_ai.output.messages`（始终）、`gen_ai.response.id`、`gen_ai.response.model`（非空时）、`gen_ai.response.finish_reasons`、`gen_ai.usage.*`（token，无 cost）、`status_code`、`is_sse`、`gen_ai.response.duration`、`gen_ai.provider.name` |

两条日志均可能包含：`event.id`（仅流式拆分）、`gen_ai.session.id`、`gen_ai.turn.id`、`pid`、`comm`、`cmdline`、`container.id`、`gen_ai.agent.type`、`gen_ai.provider.name`。

### 字段表（合并模式 / 拆分模式中的并集）

| 字段 | 类型 | 说明 |
| :--- | :--- | :--- |
| `event.id` | string | 本条日志的唯一标识（UUID，大写带连字符）；**仅** `EventStreamFormat: true` 时输出，request/response 各 1 个 |
| `event.name` | string | **`gen_ai.model.request`** 或 **`gen_ai.model.response`**；**仅** `EventStreamFormat: true` 时输出 |
| `gen_ai.session.id` | string | 用户的会话 id |
| `gen_ai.turn.id` | string | 同一会话中其中一次对话的 id |
| `gen_ai.response.id` | string | 一次对话中其中一次对大模型请求的回复 id |
| `pid` | string | 进程号（十进制字符串） |
| `comm` | string | 进程名称 |
| `cmdline` | string | 进程完整命令行（`argv` 空格拼接，截断到 127 字节），来自 `/proc/<PID>/cmdline`；进程已退出时为空则不输出 |
| `container.id` | string | 进程所属容器 id，由 agentsight 侧按 pid 解析；非容器进程或解析失败时为空则不输出。容器化部署下容器元信息另以组级 Tag 附加，见「容器元信息标签（自动附加）」 |
| `gen_ai.agent.type` | string | Agent **类型**（如 `openclaw`、`claude-code`），来自 cmdline 白名单 |
| `time_unix_nano` | string | 本条日志事件时刻，Unix 纪元纳秒（十进制字符串）；与 `SetLogTimestampFromNs` 所用时间戳一致 |
| `observed_time_unix_nano` | string | 观测时刻，与 `time_unix_nano` **相同**（十进制字符串） |
| `gen_ai.response.duration` | string | 一次对大模型请求到大模型回复的耗时，毫秒（十进制字符串） |
| `server.address` | string | 从请求 URL 解析出的服务端主机名（有请求 URL 时输出） |
| `server.port` | string | 从请求 URL 解析出的端口（URL 中含显式端口时输出） |
| `gen_ai.provider.name` | string | 大模型厂商名称 |
| `gen_ai.request.model` | string | 请求侧模型名；合并日志与 request 条输出 |
| `gen_ai.step.id` | string | 同一 `gen_ai.turn.id` 内每次 LLM hop 递增（`{turn.id}:s1`、`{turn.id}:s2`…）；**仅** `EventStreamFormat: true` 时输出，request 与 response **均**携带且同 hop 值相同；合并条无此字段 |
| `gen_ai.event.sequence` | string | **仅** `EventStreamFormat: true`：同一 `gen_ai.turn.id` 内按**实际落盘日志行**单调递增（request=1、response=2、request=3…）；合并条无此字段 |
| `gen_ai.response.model` | string | 响应侧模型名（非空时）；**仅** `EventStreamFormat: true` 的 response 条；合并日志**不**单独输出此字段（用 `gen_ai.request.model`） |
| `status_code` | string | 一次请求的状态码，同 HTTP 状态码（十进制字符串，如 `200`）；合并条或 response 条 |
| `is_sse` | string | 是否为 SSE（Server-Sent Events）连接；日志中取值为 `1`（是）或 `0`（否） |
| `gen_ai.response.finish_reasons` | string | 停止原因 **JSON 数组字符串**（如 `["stop"]`、`["tool_calls","stop"]`）；从 `output.messages`（含 `parts` 内）收集；仅有一个停止原因时也输出单元素数组 |
| `is_usage_from_api` | string | 数据来源标识，true 表示来自 LLM API response usage 字段（精确值），false 表示由插件本地估算（近似值） |
| `gen_ai.usage.input_tokens` | string | 发送给模型的 token 数量（十进制字符串） |
| `gen_ai.usage.output_tokens` | string | 模型实际生成的回复内容长度（十进制字符串） |
| `gen_ai.usage.total_tokens` | string | 一次请求消耗的 Token 总量（十进制字符串） |
| `gen_ai.usage.cache_creation.input_tokens` | string | 本次请求中，被系统新写入缓存的那部分输入 Token 数量（十进制字符串） |
| `gen_ai.usage.cache_read.input_tokens` | string | 本次请求中，直接从已有缓存中命中并读取的输入 Token 数量（十进制字符串） |
| `gen_ai.input.messages` | string | 当次 LLM 请求的完整 messages JSON 数组（**仅** `MessageDeltaOnly: false`，非空则**每次**输出） |
| `gen_ai.input.messages_delta` | string | 当次请求 input 相对同 session 上一轮的增量片段（JSON 数组字符串）；**不含** `role=system` 消息；非空时输出 |
| `gen_ai.system_instructions_hash` | string | 从 request messages 提取的 system 消息 JSON 数组的 SHA-256 摘要（十六进制；非空时输出；用于漂移检测） |
| `gen_ai.system_instructions` | string | 系统指令（system 角色）消息 JSON 数组字符串；**仅当** `gen_ai.system_instructions_hash` 相对同 session 上一轮变化时输出 |
| `gen_ai.tool.definitions_hash` | string | 请求 tools 定义 JSON 的 SHA-256 摘要（十六进制；非空时输出；用于漂移检测） |
| `gen_ai.tool.definitions` | string | 请求 tools 定义 JSON 数组；**仅当** `gen_ai.tool.definitions_hash` 相对同 session 上一轮变化时输出 |
| `gen_ai.output.messages` | string | 大模型回复 message 的序列化 json（**不受** `MessageDeltaOnly` 控制，非空时输出） |

本表字段在日志内容中**键值类型均为字符串**。其中带数值语义的字段以十进制文本（或 `is_sse` 的 `1`/`0`）输出。

### `MessageDeltaOnly` 与 input 字段

- `MessageDeltaOnly: true`（默认）：不输出全量 `gen_ai.input.messages`；**始终**输出 `gen_ai.input.messages_delta`（若有）、`gen_ai.system_instructions_hash` / `gen_ai.tool.definitions_hash`（非空时），以及 hash 变化时的 system/tools 全文；`gen_ai.output.messages`（若有）**不受**本开关影响。
- `MessageDeltaOnly: false`：全量 `gen_ai.input.messages` 非空时**每次**输出；system/tools 仍按 hash 变化决定是否输出全文；**不因 turn 切换而重置** session 状态（同 session 内 input 通常连续累积）。

#### 会话、`gen_ai.step.id` 与 `gen_ai.event.sequence`

- 会话级 input 关联按 **`gen_ai.session.id`** 索引（无 session 时用 `gen_ai.turn.id`）；同一会话内 **不因 turn 切换而清空**。最多保留 **4096** 个 session，超出时淘汰最久未使用的条目。
- **`gen_ai.turn.id`**：独立字段，来自 lib conversation id。
- **`gen_ai.step.id`**：格式 `{turn.id}:s{N}`（冒号分隔，如 `278a5a71…:s2`）。在同一 `gen_ai.turn.id` 内，每次 LLM hop 递增（从 `s1` 起）；**仅** `EventStreamFormat: true` 时输出，request 与 response 行均携带，同一 hop 的值相同。
- **`gen_ai.event.sequence`**：**仅** `EventStreamFormat: true`。在同一 `gen_ai.turn.id` 内，按实际输出的日志行单调递增（request=1、response=2、request=3、response=4…）；新 turn 时从 1 重新计数。合并模式（`EventStreamFormat: false`）不输出。

#### 全量 `gen_ai.input.messages`

- `MessageDeltaOnly: false`：每次 LLM 调用在 request 侧日志中输出当次完整 input 数组（非空时）。
- `MessageDeltaOnly: true`：**不**输出全量 input；仍输出 `gen_ai.input.messages_delta`（非空时）。插件内部按 session 维护上一轮条数与 H_in（role+parts 归一化）以计算 delta；H_out 仅保留 **role**（便于 response 与下一轮 replay 在 `parts`/tool_call id 等字段不一致时仍能匹配）。

#### system / tools 与 hash 去重

- `gen_ai.system_instructions_hash`：对 `ExtractSystemInstructionsJson` 提取结果的 SHA-256；**每轮**非空时输出。
- `gen_ai.tool.definitions_hash`：对 tools JSON 原文的 SHA-256；**每轮**非空时输出。
- 全文 `gen_ai.system_instructions` / `gen_ai.tool.definitions`：**仅当**对应 hash 相对同 session 上一轮变化时输出（首轮视为变化）。

字段含义见上文字段表中的 `gen_ai.input.messages`、`gen_ai.input.messages_delta`。

### `EventStreamFormat: false` 且 `MessageDeltaOnly: true`（最瘦合并日志）

- 每次 LLM 调用 **一条** 日志，**无** `event.name`、**无** `event.id`；时间戳为请求开始时刻。
- **有**：关联字段、HTTP/`usage` 元数据、`gen_ai.input.messages_delta`（非空时）、system/tools hash（非空时）、hash 变化时的 system/tools 全文、`gen_ai.output.messages`。
- **无**：全量 `gen_ai.input.messages`、拆分后的 `gen_ai.model.request` / `gen_ai.model.response`。

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
      EventStreamFormat: false
      MessageDeltaOnly: false
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```

- 输出（`EventStreamFormat: false`，单条合并日志）

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
  "gen_ai.input.messages_delta": [
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
  "gen_ai.system_instructions_hash": "a1b2c3d4e5f6789012345678901234567890abcdef1234567890abcdef12345678",
  "gen_ai.tool.definitions_hash": "b2c3d4e5f6789012345678901234567890abcdef1234567890abcdef1234567890",
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
  "time_unix_nano": "1749123456789000000",
  "observed_time_unix_nano": "1749123456789000000",
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

- 采集配置（`EventStreamFormat: true`，其余同上，仅改此项）

```yaml
      EventStreamFormat: true
      MessageDeltaOnly: false
```

- 输出（`EventStreamFormat: true`，同一 `gen_ai.turn.id` 下两条日志）

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
  "gen_ai.input.messages_delta": [
    {
      "role": "user",
      "parts": [{"type": "text", "content": "今天晚饭吃什么？"}]
    }
  ],
  "gen_ai.system_instructions_hash": "a1b2c3d4e5f6789012345678901234567890abcdef1234567890abcdef12345678",
  "gen_ai.system_instructions": "{\"role\":\"system\",\"parts\":[{\"type\":\"text\",\"content\":\"You are a personal assistant...\"}]}",
  "gen_ai.tool.definitions_hash": "b2c3d4e5f6789012345678901234567890abcdef1234567890abcdef1234567890",
  "gen_ai.tool.definitions": "[{\"type\":\"function\",\"function\":{\"name\":\"read\",\"description\":\"Read file contents\"}}]",
  "gen_ai.provider.name": "openai",
  "gen_ai.request.model": "qwen3.5-plus",
  "gen_ai.step.id": "c47ac487c54c2da859ba2a0e873eeeae:s1",
  "gen_ai.event.sequence": "1",
  "time_unix_nano": "1749123456789000000",
  "observed_time_unix_nano": "1749123456789000000",
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
  "gen_ai.step.id": "c47ac487c54c2da859ba2a0e873eeeae:s1",
  "gen_ai.event.sequence": "2",
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
  "time_unix_nano": "1749123460337000000",
  "observed_time_unix_nano": "1749123460337000000",
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
