# input-agent-security 插件

## 简介

input_agent_security插件实现对当前openclaw，hermes等agent工具等采集，支持的大模型供应商包括openai，antronpic，以及国内的厂商协议

## 版本

dev

## 版本说明

* 推荐版本：LoongCollector v3.1.4 及以上

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为input\_agent\_security  |
|  ProbeConfig  |  object  |  是  |  /  |  插件配置参数列表  |
|  ProbeConfig.Verbose  |  uint  |  否  |  /  |  是否打印ebpf的详细日志，1代表开启，0代表关闭  |
|  ProbeConfig.LogPath  |  string  |  否  |  false  | ebpf日志的输出位置 |

## 输出格式

| 字段 | 类型 | 说明 |
| :--- | :--- | :--- |
| `session.id` | string | 用户的会话 id |
| `gen_ai.conversation.id` | string | 同一会话中其中一次对话的 id |
| `gen_ai.response.id` | string | 一次对话中其中一次对大模型请求的回复 id |
| `pid` | int32 | 进程号 |
| `process_name` | string | 进程名称 |
| `gen_ai.agent.name` | string | agent 名称 |
| `gen_ai.request.timestamp_ns` | uint64 | 一次对大模型请求开始的时间 |
| `gen_ai.response.duration_ns` | uint64 | 一次对大模型请求到大模型回复的时间 |
| `server.address` | string | 请求大模型厂商的 url |
| `gen_ai.provider.name` | string | 大模型厂商名称 |
| `gen_ai.request.model` | string | 大模型厂商使用的模型名称 |
| `status_code` | uint16 | 一次请求的状态码，同 http 状态码 |
| `is_sse` | uint8 | 是否为 SSE（Server-Sent Events）连接，1 表示是，0 表示否 |
| `gen_ai.response.finish_reasons` | string | 大模型停止产生 token 的原因 |
| `is_usage_from_api` | string | 数据来源标识，true 表示来自 LLM API response usage 字段（精确值），false 表示由 AgentSight 本地 tokenizer 计算（近似值） |
| `gen_ai.usage.input_tokens` | uint32 | 发送给模型的 token 数量 |
| `gen_ai.usage.output_tokens` | uint32 | 模型实际生成的回复内容长度 |
| `gen_ai.usage.total_tokens` | uint32 | 一次请求消耗的 Token 总量 |
| `gen_ai.usage.cache_creation.input_tokens` | uint32 | 本次请求中，被系统新写入缓存的那部分输入 Token 数量 |
| `gen_ai.usage.cache_read.input_tokens` | uint32 | 本次请求中，直接从已有缓存中命中并读取的输入 Token 数量 |
| `gen_ai.input.messages` | string | 大模型请求 message 的序列化 json |
| `gen_ai.output.messages` | string | 大模型回复 message 的序列化 json |


## 样例

### 采集agent与llm交互数据

- 输入

打开agent进行交流

- 采集配置

```yaml
enable: true
inputs:
  - Type: input_agent_security
    ProbeConfig:
      Verbose: 1
      LogPath: ""
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```

- 输出

{
  "agent.name": "OpenClaw",
  "gen_ai.conversation.id": "c47ac487c54c2da859ba2a0e873eeeae",
  "gen_ai.duration_ms": 3548,
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
  "gen_ai.request.is_stream": true,
  "gen_ai.request.model": "qwen3.5-plus",
  "gen_ai.response.finish_reason": "stop",
  "gen_ai.response.id": "chatcmpl-3cd5d2d2-d2f5-91e9-a5e4-7fb740bb47f6",
  "gen_ai.response.model": "qwen3.5-plus",
  "gen_ai.system": "openai",
  "gen_ai.usage.input_tokens": 27466,
  "gen_ai.usage.output_tokens": 195,
  "gen_ai.usage.total_tokens": 27661,
  "http.response.status_code": 200,
  "is_usage_from_api": true,
  "process.executable.name": "openclaw-gatewa",
  "process.pid": 705127,
  "server.address": "dashscope.aliyuncs.com",
  "session.id": "dea5eed6-4a08-436c-b117-5ea14c9de39a",
  "url.full": "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions"
}
