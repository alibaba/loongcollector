# AgentSight 远端 E2E 验证（OpenClaw / Hermes + `new_delta`）

环境默认：`47.239.119.134`，远端安装路径 **`/usr/local/loongcollector`**（`send_remote.sh` / `remote_restart.sh` 默认 `REMOTE_PATH`），SLS `xiaotian-config` / `xiaotian-logstore`。

**字段矩阵与 full/delta/output 语义**：见 [`docs/cn/plugins/input/native/agentsight_field_report.md`](../../docs/cn/plugins/input/native/agentsight_field_report.md)。

## 1. 本地脚本关系

| 脚本 | 作用 |
|------|------|
| `run_scripts/restart.sh` | 把 `core/build/*` 同步到 `output/`，拷贝 `.vscode/configs` → `output/conf/continuous_pipeline_config/local/` |
| `run_scripts/send_remote.sh` | `output/` 整包 tar 到远端 `/usr/local/loongcollector/` |
| `run_scripts/remote_restart.sh` | 远端 kill + 后台启动 loongcollector |
| `run_scripts/agentsight_e2e/use_case.sh` | 切换四种 ProbeConfig 之一，并暂时禁用 wholefile 配置 |
| `run_scripts/agentsight_e2e/deploy_case.sh` | 一键：use_case + restart + send_remote + remote_restart |
| `run_scripts/agentsight_e2e/trigger_e2e.sh` | 按 `AGENT_TYPE` 分发到 openclaw / hermes trigger（默认 openclaw） |
| `run_scripts/agentsight_e2e/trigger_openclaw.sh` | 远端 OpenClaw 发一条对话；`VERIFY_BASELINE_DELTA=1` 时自动跑 delta 校验 |
| `run_scripts/agentsight_e2e/trigger_hermes.sh` | 远端 Hermes `chat -Q` 发一条对话；自动嵌入 `E2E_HERMES_*` marker |
| `run_scripts/agentsight_e2e/trigger_openclaw_tool_e2e.sh` | 工具调用两轮 E2E（read/edit，仅 openclaw） |
| `run_scripts/agentsight_e2e/verify_baseline_delta.sh` | SLS 校验 delta 公式；默认 `--format split`；支持 `AGENT_TYPE` |
| `run_scripts/agentsight_e2e/run_baseline_e2e.sh` | 一键 delta 公式 E2E：默认 deploy **split** + trigger + 校验；`AGENT_TYPE=hermes` |

**注意**：`restart.sh` 里本地 `nohup loongcollector` 已注释；E2E 以**远端**进程为准。

### 1.1 Agent 选择与 `AGENT_TYPE`

| `AGENT_TYPE` | Trigger 脚本 | SLS `gen_ai.agent.type` | 说明 |
|--------------|-------------|-------------------------|------|
| `openclaw`（默认） | `trigger_openclaw.sh` | `openclaw` | CLI `--session-id` 可能与 SLS `gen_ai.session.id` 不同 |
| `hermes` | `trigger_hermes.sh` | `hermes` | 用 `E2E_HERMES_*` marker 检索；`--resume` 需完整 `session_id` |

```bash
# OpenClaw（默认，行为与改前一致）
bash run_scripts/agentsight_e2e/trigger_openclaw.sh "Please reply: OK"

# Hermes bubble/chat
AGENT_TYPE=hermes bash run_scripts/agentsight_e2e/trigger_hermes.sh "Please reply: OK"

# 统一入口
AGENT_TYPE=hermes bash run_scripts/agentsight_e2e/trigger_e2e.sh "your message"
AGENT_TYPE=openclaw bash run_scripts/agentsight_e2e/trigger_e2e.sh

# 一键 delta 公式 E2E（默认 split case：EventStreamFormat=true + MessageDeltaOnly=false）
AGENT_TYPE=hermes bash run_scripts/agentsight_e2e/run_baseline_e2e.sh
AGENT_TYPE=openclaw bash run_scripts/agentsight_e2e/run_baseline_e2e.sh --deploy

#  legacy merged 单条校验
bash run_scripts/agentsight_e2e/run_baseline_e2e.sh --case merged --deploy
```

**Hermes 环境变量**（远端 SSH 内生效）：

| 变量 | 默认 | 说明 |
|------|------|------|
| `HERMES_HOME` | `/root/.hermes` | Hermes 配置目录 |
| `HERMES_BIN` | `/usr/local/bin/hermes` | Hermes CLI |
| `HERMES_SESSION_ID` | （空） | 非空时传 `--resume`（须为 Hermes 打印的完整 `session_id`） |
| `HERMES_TIMEOUT` | `120` | SSH 侧 timeout 秒数 |

**Session ID 说明**：OpenClaw CLI 的 `--session-id` 与 Hermes CLI 打印的 `session_id:` **不一定**等于 SLS 里的 `gen_ai.session.id`。推荐用 **marker 检索**（Hermes 自动嵌入 `E2E_HERMES_*`；OpenClaw baseline 消息含 `BASELINE_OK`）。`verify_delta_baseline.py` 的 `--marker` 仅用于解析真实 `gen_ai.session.id`（`--resolve-session`，默认开启）；主查询按 `session_id` + `agent_type` 拉取，避免 marker 过滤掉 response 行。

## 2. 四种用例

| Case | `EventStreamFormat` | `MessageDeltaOnly` | 预期（每次 LLM 调用） |
|------|-------------------|-------------------|------------------------|
| `baseline` | false | false | **1 条**合并日志；无 `event.name` / `event.id`；有 delta、hash、output；system/tools 每次（非空）；full input **首条或前缀变化时** |
| `split` | true | false | **2 条**：`gen_ai.model.request` + `gen_ai.model.response`（各有 `event.id`）；request 含 input 侧字段 |
| `no-detail` | false | true | **1 条**合并；**无**全量 input/system/tools；**有** delta、hash、**有** `gen_ai.output.messages` |
| `split-no-detail` | true | true | **2 条**；request 仅 delta/hash；response 含 output、usage、`status_code`、`is_sse` |

每轮只跑一个 case，避免配置混在一起。

## 3. 工具调用 E2E（推荐，比纯文本 baseline 更有辨识度）

```bash
bash run_scripts/agentsight_e2e/trigger_openclaw_tool_e2e.sh both
```

脚本会：

1. 在远端创建 `/root/.openclaw/etl-probe/e2e-agentsight-tool-probe.txt`
2. **Round1**：强制 `read` 该文件 → 回复 `TOOL_OK=...`
3. **Round2**（同 session）：`read` + `edit` 追加 `ROUND2` → 回复 `TOOL_ROUND2=done`

OpenClaw 侧已验证：`toolSummary.calls` 为 1 / 2，`tools` 为 `["read"]` / `["read","edit"]`。

SLS 校验（约 14:42–14:45）：

```sql
* | where gen_ai.agent.type = 'openclaw' and __time__ > datetime(2026-06-04 14:41:00)
| extend len_tools=len(gen_ai.tool.definitions), len_in=len(gen_ai.input.messages),
         len_out=len(gen_ai.output.messages)
| select __time__, gen_ai.response.id, len_tools, len_in, len_out
| where len_out > 0
| order by __time__ desc
```

再抽查 `gen_ai.output.messages` 含 `tool_call` / `read` / `edit`；两轮中**至少一轮** `len_in` > 0（去重时后续轮可能为 0）。

## 4. 操作步骤（单 case）

```bash
cd /home/zhaoxiaotian/project/beartyson/loongcollector

# 1) 选 case 并部署（示例：拆分 + 详细消息）
bash run_scripts/agentsight_e2e/deploy_case.sh split

# 2) 远端 OpenClaw 发一条对话（Web UI / CLI，需走 HTTPS 大模型 API）
#    建议附带唯一文案便于检索，例如：E2E_SPLIT_20260604_1

# 3) 等 30~90s 后在 SLS 查询（见下）

# 4) 换下一个 case 前再 deploy_case.sh <other>
```

恢复 wholefile 配置：

```bash
bash run_scripts/agentsight_e2e/restore_configs.sh
```

## 5. 远端 Agent 检查

### 5.1 OpenClaw

```bash
ssh root@47.239.119.134 'pgrep -af openclaw; pgrep -af /usr/local/loongcollector/loongcollector'
ssh root@47.239.119.134 'tail -50 /tmp/loongcollector.stderr'
ssh root@47.239.119.134 'tail -30 /tmp/agentsight.log'
ssh root@47.239.119.134 'ls -la /usr/local/loongcollector/loongcollector /usr/local/loongcollector/libagentsight.so'
```

确认 gateway 进程 cmdline 能命中内置 `openclaw` 规则（`gen_ai.agent.type=openclaw`）。

### 5.2 Hermes

```bash
ssh root@47.239.119.134 'export HERMES_HOME=/root/.hermes; which hermes; hermes --version'
ssh root@47.239.119.134 'pgrep -af hermes; pgrep -af /usr/local/loongcollector/loongcollector'
ssh root@47.239.119.134 'tail -30 /tmp/agentsight.log'
```

Hermes 单次非交互：`hermes chat -Q -q "message" --yolo --accept-hooks`；续聊：`--resume <full_session_id>`。

确认 `hermes` 进程 cmdline 命中内置 `hermes` 规则（`gen_ai.agent.type=hermes`）。

## 6. SLS 查询模板（`xiaotian-logstore`）

时间范围选对话后 10 分钟。将 `openclaw` 换成 `hermes` 即可查 Hermes agent。

### 6.1 通用：最近 agent LLM（openclaw）

```sql
* | where gen_ai.agent.type = 'openclaw'
| select __time__, event.name, gen_ai.session.id, gen_ai.turn.id,
         gen_ai.request.model, gen_ai.response.duration, status_code,
         length(gen_ai.input.messages) as len_full,
         length(gen_ai.input.messages.delta) as len_delta,
         length(gen_ai.output.messages) as len_out
| order by __time__ desc
| limit 20
```

### 6.1b 通用：最近 Hermes LLM

```sql
* | where gen_ai.agent.type = 'hermes'
| select __time__, gen_ai.session.id, gen_ai.turn.id,
         gen_ai.request.model, gen_ai.response.duration, status_code,
         length(gen_ai.input.messages) as len_full,
         length(gen_ai.input.messages.delta) as len_delta,
         length(gen_ai.output.messages) as len_out
| order by __time__ desc
| limit 20
```

按 E2E marker 精确定位（Hermes trigger 自动嵌入）：

```sql
* | where gen_ai.agent.type = 'hermes' and gen_ai.output.messages like '%E2E_HERMES_%'
| select __time__, gen_ai.session.id, gen_ai.response.id, gen_ai.output.messages
| order by __time__ desc
| limit 10
```

### 6.2 Case `baseline`（`EventStreamFormat: false`）

- 无 `event.name`、无 `event.id`（合并单条）
- 至少一条含非空 `gen_ai.output.messages`
- `gen_ai.response.finish_reasons` 为 JSON 数组（如 `["stop"]`），非裸字符串
- 同 session 多轮：**至少一条**有全量 `gen_ai.input.messages`；前缀未变时后续轮次可仅 delta+hash（`MessageDeltaOnly: false` + 去重）

```sql
* | where gen_ai.agent.type = 'openclaw' and not isnull(gen_ai.output.messages)
| extend has_event = isnull(event.name)
| select __time__, has_event, len(gen_ai.input.messages), len(gen_ai.input.messages.delta)
```

### 6.3 Case `split`（`EventStreamFormat: true`）

- 同一 `gen_ai.turn.id`（或相近时间）应有 **2 条**：`event.name` = `gen_ai.model.request` 与 `gen_ai.model.response`
- 两条均有 **`event.id`**，且互不相同
- request **无** `gen_ai.output.messages`、`gen_ai.response.finish_reasons`；response **有** output 与 `finish_reasons`（数组）

```sql
* | where gen_ai.agent.type = 'openclaw' and event.name in ('gen_ai.model.request', 'gen_ai.model.response')
| stats count() by event.name, gen_ai.turn.id
```

### 6.4 Case `no-detail` / `split-no-detail`

- **不应**出现 `gen_ai.input.messages`、`gen_ai.system_instructions`、`gen_ai.tool.definitions`（或长度恒为 0）
- **应**有 `gen_ai.input.messages.delta`（非空时）
- **应**有 `gen_ai.output.messages`

```sql
* | where gen_ai.agent.type = 'openclaw'
| extend full=len(gen_ai.input.messages), sys=len(gen_ai.system_instructions),
         delta=len(gen_ai.input.messages.delta), out=len(gen_ai.output.messages)
| select __time__, event.name, full, sys, delta, out
```

### 6.5 明确不应出现

- `gen_ai.input.messages.count`
- `gen_ai.usage.*_cost`
- `gen_ai.request.temperature` 等（FFI 未提供）

## 7. 通过标准（ checklist ）

- [ ] 远端 `loongcollector` + `libagentsight.so` 为新编译产物（`deploy_case` 后 mtime 更新）
- [ ] `baseline`：单条、无 `event.name` / `event.id`、有 output、`finish_reasons` 为数组
- [ ] **`split` delta 公式**（自动或手动 `verify_baseline_delta.sh`）：见 §7.1
- [ ] `split`：成对 request/response、两条 `event.id` 不同；`gen_ai.response.model` 仅在 response 条
- [ ] `no-detail`：无全量 input/system/tools，仍有 delta + output
- [ ] `split-no-detail`：request 瘦、response 全
- [ ] 各 case 切换后行为变化与上表一致（排除旧配置缓存：每次 `deploy_case` 会清远端 `data/`）

### 7.1 Delta 公式校验（split + 全量 input）

**公式校验默认使用 `split` case**（`EventStreamFormat=true` + `MessageDeltaOnly=false`），**不是** `split-no-detail`。

| 配置 | 能否校验公式 | 原因 |
|------|-------------|------|
| `split`（`EventStreamFormat=true`, `MessageDeltaOnly=false`） | **是（推荐）** | request 行含 `gen_ai.input.messages` + delta；response 行含 output；`prev_out` 取自配对的上一条 **response** |
| `baseline`（merged, `MessageDeltaOnly=false`） | 是（legacy） | 单条合并日志；`prev_out` 取自上一条合并行；`--format merged` |
| `split-no-detail` / `no-detail`（`MessageDeltaOnly=true`） | **否** | 无全量 `gen_ai.input.messages`，无法构造 `cur` |

若同时设 `EventStreamFormat=true` 与 `MessageDeltaOnly=true`，只能做字段形态检查，**不能**验证 delta 公式。脚本会对缺少 `gen_ai.input.messages` 的 request 行打印 `warn`。

同一 `gen_ai.session.id` 内，按 **request 行**时间序排列；对第 `i` 条 request（`i>0`）：

```text
canon(omit_system(prev_in) + prev_out + delta) == canon(omit_system(cur))
```

| 符号 | split 含义 | merged 含义 |
|------|-----------|-------------|
| `canon` | 每条 message 仅保留 `role` + `parts` 后 JSON 比较 | 同左 |
| `omit_system` | 从 messages 数组去掉 `role=system` | 同左 |
| `prev_in` | 上一条 **request** 的 `gen_ai.input.messages` | 上一条合并行的 input |
| `prev_out` | 与上一条 request **配对**的 response 的 `gen_ai.output.messages` | 上一条合并行的 output |
| `delta` | 当前 request 的 `gen_ai.input.messages.delta` | 当前合并行的 delta |
| `cur` | 当前 request 的 `gen_ai.input.messages` | 当前合并行的 input |

**首条 request**（session 内无 prior）：`delta` 应等于 `omit_system(cur)`。

**单条 HTTP（一次 chat）** 只能测 `first_in_session`。要测 `same_turn_step_2` / `cross_turn_step_1` 需同 session **至少 2 次 LLM 调用**：

- OpenClaw：`trigger_openclaw_tool_e2e.sh both`（工具两轮）
- Hermes：第一次 trigger 记下 `session_id:`，第二次 `HERMES_SESSION_ID=<id> bash trigger_hermes.sh "round 2 ..."`

**场景标签**（脚本输出 `[scenario]`）：

| 标签 | 含义 |
|------|------|
| `first_in_session` | session 内第一条 request |
| `same_turn_step_2` | 同一 `gen_ai.turn.id` 内第 2+ 次 request（agent 多步 tool loop） |
| `cross_turn_step_1` | 新 turn 的首条 request（跨轮用户消息） |

**SLS 去重**：同一 `gen_ai.response.id` / `event.id` 可能出现重复条（pipeline flush 镜像）；脚本按 id 去重，签名比较时忽略 `gen_ai.step.id`。

**其他 case 校验范围**（形态 only，无 delta 公式）：

| Case | 校验内容 |
|------|----------|
| `split-no-detail` | 成对 request/response；request **无** full input |
| `no-detail` | 无 full input/system/tools；有 delta + output |

#### 手动运行 delta 校验

```bash
cd /home/zhaoxiaotian/project/beartyson/loongcollector

# 部署 split case（公式校验前提）
bash run_scripts/agentsight_e2e/deploy_case.sh split

# 默认 split 公式校验（Hermes marker，推荐）
bash run_scripts/agentsight_e2e/verify_baseline_delta.sh \
  --agent-type hermes --marker E2E_HERMES_20260606_110133 --wait 60

# OpenClaw：marker 检索（工具 E2E，2 次 HTTP → same_turn / cross_turn）
bash run_scripts/agentsight_e2e/verify_baseline_delta.sh \
  --marker TOOL_OK --minutes 20 --format split

# legacy merged 单条
bash run_scripts/agentsight_e2e/verify_baseline_delta.sh \
  --session-id e2e-baseline-1717654321 --wait 60 --format merged

# 一键：deploy split + trigger + 校验
bash run_scripts/agentsight_e2e/run_baseline_e2e.sh --deploy

# Hermes 一键
AGENT_TYPE=hermes bash run_scripts/agentsight_e2e/run_baseline_e2e.sh --deploy

# Hermes 两轮（同 session，测第 2 条 request 公式）
AGENT_TYPE=hermes bash run_scripts/agentsight_e2e/run_baseline_e2e.sh --deploy
# 记下 session_id: ...
HERMES_SESSION_ID='<full_session_id>' AGENT_TYPE=hermes \
  bash run_scripts/agentsight_e2e/trigger_hermes.sh "Round 2: reply ROUND2_OK"
bash run_scripts/agentsight_e2e/verify_baseline_delta.sh \
  --agent-type hermes --marker ROUND2_OK --wait 60

# OpenClaw 工具两轮 + 公式（需 split deploy）
bash run_scripts/agentsight_e2e/run_baseline_e2e.sh --deploy --tool
```

凭证：默认读 `.vscode/loongcollector_config.json`，或通过 `LOONGCOLLECTOR_CONFIG` 覆盖。

**期望输出示例**（split，2 次 HTTP 全部通过）：

```text
AgentSight delta formula verify — format=split agent=hermes session=...
  Requires EventStreamFormat=true + MessageDeltaOnly=false (request rows carry gen_ai.input.messages).
Config: .../loongcollector_config.json | window: ... | fetched=4 checks=2
  PASS [first_in_session] turn=turn-abc step=step_1 response.id=resp-001
  PASS [cross_turn_step_1] turn=turn-def step=step_1 response.id=resp-002
Summary: 2/2 checks passed
RESULT: PASS
```

失败时会打印 `FAIL [scenario]`、`left`/`right` JSON 摘要，并以 exit code 1 退出。

## 8. 故障排查

| 现象 | 可能原因 |
|------|----------|
| 无 agentsight 日志 | OpenClaw 未走 HTTPS 白名单域名；或 ebpf 未启 |
| 仍是旧字段行为 | 未 `send_remote` / 未 `remote_restart` |
| 仍是两条 split 日志 | `EventStreamFormat` 仍为 `true`；或远端有**第二个** loongcollector / 多份 pipeline |
| 两条变一条 | 期望 split 时检查 active yaml 是否 `EventStreamFormat: true` |
| 仍有全量 input | 检查是否误设 `MessageDeltaOnly: false` |
| `finish_reasons` 为裸字符串 `stop` | 旧进程或重复 pipeline；新二进制应为 JSON 数组 `["..."]` |
| 合并条上出现 `event.id` | 应为 split 路径或重复写入；合并模式设计上无 `event.id` |
