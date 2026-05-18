# Container Stdio UT Spec

## 1. 背景

C++ `InputContainerStdio` 插件负责采集容器标准输出日志，支持两种容器运行时格式：

- **CRI containerd text**: `<timestamp> <stream> <P|F> <content>`
- **Docker json-file**: `{"log":"<content>","stream":"<stream>","time":"<timestamp>"}`

数据流经过以下处理链：

```
FileServer (读文件块)
  → LogFileReader::RemoveLastIncompleteLog (截断末尾不完整行)
  → LogFileReader::GetLastLine (LineParsers 链)
  → ProcessorSplitLogStringNative (按 \n 切行)
  → ProcessorParseContainerLogNative (解析 CRI / Docker JSON)
  → ProcessorMergeMultilineLogNative BY_FLAG (P/F 分片合并)
  → [可选] 用户多行: JSON(\0 切割) 或 CUSTOM(正则合并)
```

两层多行合并：

1. **运行时物理分片 (BY_FLAG)**：容器运行时将超 16KB 的行拆成多条。CRI 用 `P`/`F` 标记，Docker JSON 用 log 字段末尾是否有 `\n` 区分。
2. **语义多行 (BY_REGEX / JSON)**：用户日志本身跨多行（如 Java stack trace、多行 JSON）。

### 已知缺陷


| #   | 缺陷                                                                                  | 根因                                                                   | 影响                                                      |
| --- | ----------------------------------------------------------------------------------- | -------------------------------------------------------------------- | ------------------------------------------------------- |
| 1   | `ParseDockerJsonLogLine` 不检测 `\n` 缺失，不设 `PartLogFlag` / `HAS_PART_LOG`              | 只去掉末尾 `\n`，没有利用"有无 `\n`"这一信息                                         | Docker JSON 的 partial log 无法被 MergeByFlag 合并            |
| 2   | Container stdio + JSON 多行模式设 `SetRequiringJsonReaderFlag(true)`                     | 导致创建 `JsonLogFileReader`，`RemoveLastIncompleteLog` 在容器格式剥离前做 `{}` 配对 | JSON 多行模式完全不可用                                          |
| 3   | `DockerJsonFileParser::parseLine` 无条件 `fullLine=true`，`checkContainerType` buffer=0 | GetLastLine 永远把每行当 Full，无法合并 P 行                                     | 多行正则模式下 Docker JSON 的 `RemoveLastIncompleteLog` 返回截断的半行 |


---

## 2. 现有 UT 覆盖清单

### 2.1 ProcessorParseContainerLogNativeUnittest

> 文件: `core/unittest/processor/ProcessorParseContainerLogNativeUnittest.cpp`


| #   | 测试方法                                   | 格式     | 覆盖场景                                                                             | partial 覆盖     |
| --- | -------------------------------------- | ------ | -------------------------------------------------------------------------------- | -------------- |
| 1   | `TestInit`                             | -      | 参数非法时 Init 仍通过                                                                   | -              |
| 2   | `TestContainerdLog`                    | CRI    | P/F 标记解析、边界情况（空内容、无第三空格、无第二空格等）                                                  | CRI P ✅        |
| 3   | `TestIgnoringStdoutStderr`             | CRI    | IgnoringStdout/IgnoringStderr 过滤（true/true, true/false, false/true, false/false） | CRI P ✅        |
| 4   | `TestContainerdLogWithSplit`           | CRI    | 全链路 Split→Parse→MergeByFlag→MergeByRegex，P+P+F 合并后再按正则合并                         | CRI P ✅        |
| 5   | `TestDockerJsonLogLineParserWithSplit` | Docker | 全链路 Split→Parse→MergeByFlag→MergeByRegex，**但所有 log 字段都带 `\n`（全是 Full）**          | **Docker P ❌** |
| 6   | `TestDockerJsonLogLineParser`          | Docker | 单独 Parse: log/stream/time 缺失、为空、类型错误等边界                                          | **Docker P ❌** |
| 7   | `TestKeepingSourceWhenParseFail`       | 两者     | KeepingSourceWhenParseFail=false 时解析失败的事件被丢弃                                     | -              |
| 8   | `TestParseDockerLog`                   | Docker | 底层 `ParseDockerLog()`: 合法/非法 JSON、转义字符、Unicode、不完整 JSON                          | -              |


### 2.2 ProcessorMergeMultilineLogNativeUnittest

> 文件: `core/unittest/processor/ProcessorMergeMultilineLogNativeUnittest.cpp`


| #   | 测试类                                                 | 测试方法                           | 覆盖场景                                             |
| --- | --------------------------------------------------- | ------------------------------ | ------------------------------------------------ |
| 1   | `ProcessorMergeMultilineLogNativeUnittest`          | `TestInit`                     | MergeType flag/regex, 正则合法/非法                    |
| 2   |                                                     | `TestProcess`                  | 按正则合并基本流程                                        |
| 3   | `ProcessEventsWithPartLogUnittest`                  | `TestProcessEventsWithPartLog` | BY_FLAG: 手动构造带 PartLogFlag 的事件，验证合并              |
| 4   |                                                     | `TestProcess`                  | BY_FLAG 更多场景                                     |
| 5   | `ProcessorMergeMultilineLogDisacardUnmatchUnittest` | 5 个正则组合                        | BY_REGEX + UnmatchedContentTreatment=discard     |
| 6   | `ProcessorMergeMultilineLogKeepUnmatchUnittest`     | 5 个正则组合                        | BY_REGEX + UnmatchedContentTreatment=single_line |


> 注：#3 的 BY_FLAG 测试输入是手工构造的事件（直接设 PartLogFlag），不涉及 Docker JSON 格式。

### 2.3 GetLastLineDataUnittest

> 文件: `core/unittest/reader/GetLastLineDataUnittest.cpp`


| #   | 测试类                                               | 测试方法                                   | 覆盖场景                                                                 | partial 覆盖     |
| --- | ------------------------------------------------- | -------------------------------------- | -------------------------------------------------------------------- | -------------- |
| 1   | `LastMatchedContainerdTextLineUnittest`           | `TestLastContainerdTextLineSingleLine` | CRI GetLastLine needSingleLine=true: P/F 组合（F+P+P, P+P+F, P+P, 异常行…） | CRI P ✅        |
| 2   |                                                   | `TestLastContainerdTextLineMerge`      | CRI GetLastLine needSingleLine=false: P 行被 merge 成完整行                | CRI P ✅        |
| 3   | `LastMatchedDockerJsonFileUnittest`               | `TestLastDockerJsonFile`               | Docker JSON GetLastLine: **只测了单行 Full（合法/非法字段）**                     | **Docker P ❌** |
| 4   | `LastMatchedContainerdTextWithDockerJsonUnittest` | `TestContainerdTextWithDockerJson`     | 嵌套：外层 Docker JSON 包裹 CRI 格式，P+F 合并                                   | 嵌套 ✅           |
| 5   |                                                   | `TestDockerJsonWithContainerdText`     | 嵌套：外层 CRI 包裹 Docker JSON，P+F 合并                                      | 嵌套 ✅           |


### 2.4 RemoveLastIncompleteLogUnittest

> 文件: `core/unittest/reader/RemoveLastIncompleteLogUnittest.cpp`


| #   | 测试类                                                      | 测试方法                                       | 覆盖场景                                                                      |
| --- | -------------------------------------------------------- | ------------------------------------------ | ------------------------------------------------------------------------- |
| 1   | `RemoveLastIncompleteLogUnittest`                        | `TestSingleline`                           | 纯文本单行                                                                     |
| 2   |                                                          | `TestMultiline`                            | 纯文本多行正则                                                                   |
| 3   | `RemoveLastIncompleteLogMultilineUnittest`               | 5 个正则组合                                    | 纯文本多行正则各种 start/continue/end                                              |
| 4   | `GetLastLineUnittest`                                    | `TestGetLastLine` / `TestGetLastLineEmpty` | RawTextParser 的 GetLastLine                                               |
| 5   | `ContainerdTextRemoveLastIncompleteLogMultilineUnittest` | 3 个正则组合                                    | CRI + 多行正则：RemoveLastIncompleteLog 正确处理 P/F                               |
| 6   | `DockerJsonRemoveLastIncompleteLogMultilineUnittest`     | 3 个正则组合                                    | Docker JSON + 多行正则：**当前 fullLine 始终 true，测试通过但行为不正确**（测试用例未触发 partial 场景） |


> 注：#6 中 `TestRemoveLastIncompleteLogWithBeginEnd` 已声明但**未注册** UNIT_TEST_CASE。

### 2.5 JsonLogFileReaderUnittest

> 文件: `core/unittest/reader/JsonLogFileReaderUnittest.cpp`


| #   | 测试方法                                              | 覆盖场景                   |
| --- | ------------------------------------------------- | ---------------------- |
| 1   | `TestReadGBK` / `TestReadUTF8`                    | JsonLogFileReader 文件读取 |
| 2   | `TestRemoveLastIncompleteLogSingleLine`           | JSON 单行完整              |
| 3   | `TestRemoveLastIncompleteLogSingleLineIncomplete` | JSON 单行不完整             |
| 4   | `TestRemoveLastIncompleteLogMultiline`            | 多行 JSON                |
| 5   | `TestRemoveLastIncompleteLogNotValidJson`         | 非法 JSON                |
| 6   | 各 `NoRollback` 变体                                 | 无法回退时的行为               |


> 注：这些测试针对非容器场景的 JsonLogFileReader。缺陷 2 的修复是让容器 stdio 不再使用 JsonLogFileReader。

### 2.6 InputContainerStdioUnittest

> 文件: `core/unittest/input/InputContainerStdioUnittest.cpp`


| #   | 测试方法                         | 覆盖场景        |
| --- | ---------------------------- | ----------- |
| 1   | `OnSuccessfulInit`           | Init 成功     |
| 2   | `OnEnableContainerDiscovery` | 容器发现启用      |
| 3   | `OnPipelineUpdate`           | Pipeline 更新 |
| 4   | `TestTryGetRealPath`         | 符号链接路径解析    |


---

## 3. 覆盖缺口分析

### 3.1 Docker JSON Partial Log（缺陷 1）

**现状**：所有 Docker JSON 测试中 log 字段都以 `\n` 结尾（即 Full），无 partial log（无 `\n`）测试。

**缺口**：


| 缺口                                     | 说明                                                     |
| -------------------------------------- | ------------------------------------------------------ |
| ParseDockerJsonLogLine 不设 PartLogFlag  | 当 log 字段无 `\n` 时，应对事件设置 `PartLogFlag("P")` content key |
| ParseDockerJsonLogLine 不设 HAS_PART_LOG | 当存在 partial log 时，应对 eventGroup 设置 `HAS_PART_LOG` 元数据  |
| Docker JSON partial + full 全链路合并       | P+P+F 经过 Split→Parse→MergeByFlag 后应合并为一条               |


### 3.2 DockerJsonFileParser P 行支持（缺陷 3）

**现状**：`TestLastDockerJsonFile` 只测了单行 Full，没有 P/F 组合。CRI 侧的对等测试 `TestLastContainerdTextLineSingleLine` 和 `TestLastContainerdTextLineMerge` 覆盖了完整的 P/F 组合。

**缺口**：


| 缺口                                       | 说明                                                                  |
| ---------------------------------------- | ------------------------------------------------------------------- |
| `parseLine` fullLine 判断                  | log 末尾无 `\n` → fullLine=false（Partial），有 `\n` → fullLine=true（Full） |
| `GetLastLine` needSingleLine=true P 行跳过  | F+P+P → 返回 F 行内容；P+P → fullLine=false                               |
| `GetLastLine` needSingleLine=false P 行合并 | P+P+F → 返回合并后的完整内容                                                  |
| `checkContainerType` buffer 分配           | DockerJsonFileParser 需要与 ContainerdTextParser 相同大小的 buffer 用于 P 行合并 |


### 3.3 Container Stdio JSON 多行模式（缺陷 2）

**现状**：无相关测试。

**缺口**：


| 缺口                                                         | 说明                                                             |
| ---------------------------------------------------------- | -------------------------------------------------------------- |
| `CreateInnerProcessors` JSON 多行不应设 RequiringJsonReaderFlag | 容器 stdio 场景应始终使用普通 `LogFileReader`，JSON `{}` 配对在 processor 层完成 |


---

## 4. 需新增的测试用例

### 4.1 ProcessorParseContainerLogNativeUnittest — 新增

#### TC-DOCKER-PARTIAL-01: TestDockerJsonPartialLogBasic

**目的**：验证 `ProcessorParseContainerLogNative` 对 Docker JSON partial log 正确设置 PartLogFlag 和 HAS_PART_LOG。

**前置条件**：

- `ProcessorParseContainerLogNative` Init，IgnoringStdout=false, IgnoringStderr=false
- eventGroup metadata: `LOG_FORMAT = "docker_json-file"`

**输入**：4 个事件，每个事件的 content 是一条 Docker JSON 行：


| 事件  | content                                                           | log 字段末尾 | 预期标记          |
| --- | ----------------------------------------------------------------- | -------- | ------------- |
| E1  | `{"log":"Exception in thread ","stream":"stdout","time":"..."}`   | 无 `\n`   | PartLogFlag=P |
| E2  | `{"log":"'main' java.lang.","stream":"stdout","time":"..."}`      | 无 `\n`   | PartLogFlag=P |
| E3  | `{"log":"NullPointerException\n","stream":"stdout","time":"..."}` | 有 `\n`   | 无 PartLogFlag |
| E4  | `{"log":" at com.example\n","stream":"stdout","time":"..."}`      | 有 `\n`   | 无 PartLogFlag |


**预期输出**：

- 4 个事件保留
- E1, E2: content key `"P"` 存在（值为空 StringView）
- E3, E4: content key `"P"` 不存在
- E1 content = `"Exception in thread "`（原样保留，无 `\n` 可去）
- E3 content = `"NullPointerException"`（去掉末尾 `\n`）
- eventGroup metadata: `has.part.log = "P"`

#### TC-DOCKER-PARTIAL-02: TestDockerJsonPartialLogWithSplit

**目的**：验证 Docker JSON partial log 经过完整 pipeline（Split → Parse → MergeByFlag）后正确合并。

**前置条件**：

- 三个 processor 按顺序：`ProcessorSplitLogStringNative`(\n) → `ProcessorParseContainerLogNative` → `ProcessorMergeMultilineLogNative`(flag)
- eventGroup metadata: `LOG_FORMAT = "docker_json-file"`

**输入**：1 个事件，content 为多行 Docker JSON（用 `\n` 分隔）：

```
{"log":"Exception in thread ","stream":"stdout","time":"2024-02-19T03:49:37.793533014Z"}
{"log":"'main' java.lang.","stream":"stdout","time":"2024-02-19T03:49:37.793533014Z"}
{"log":"NullPointerException\n","stream":"stdout","time":"2024-02-19T03:49:37.793533014Z"}
{"log":"    at com.example\n","stream":"stdout","time":"2024-02-19T03:49:37.793559367Z"}
```

**预期输出**：

- 2 个事件（前 3 条合并为 1 条 + 第 4 条独立）
- E1 content = `"Exception in thread 'main' java.lang.NullPointerException"`
- E2 content = `"    at com.example"`
- eventGroup metadata: 无 `has.part.log`（合并完成后应被清除或不再存在）

#### TC-DOCKER-PARTIAL-03: TestDockerJsonPartialLogWithSplitAndRegex

**目的**：验证 Docker JSON partial log 经过完整四步 pipeline（Split → Parse → MergeByFlag → MergeByRegex）后，先物理合并再语义合并。

**前置条件**：

- 四个 processor：Split(\n) → Parse → MergeByFlag → MergeByRegex(StartPattern=`Exception.`*)
- eventGroup metadata: `LOG_FORMAT = "docker_json-file"`

**输入**：与 TC-DOCKER-PARTIAL-02 相同。

**预期输出**：

- 1 个事件（全部合并为 Java stack trace）
- content = `"Exception in thread 'main' java.lang.NullPointerException\n    at com.example"`

### 4.2 GetLastLineDataUnittest — 新增

#### TC-GETLASTLINE-DOCKER-PARTIAL-01: TestLastDockerJsonFileSingleLine

**目的**：验证 `DockerJsonFileParser::GetLastLine` 在 needSingleLine=true 时正确处理 Docker JSON P/F 行。

**前置条件**：

- `DockerJsonFileParser` buffer > 0（修复后）
- needSingleLine = true

**测试场景**（对标 CRI `TestLastContainerdTextLineSingleLine`）：


| 场景      | 输入                                                         | 预期 data | 预期 fullLine |
| ------- | ---------------------------------------------------------- | ------- | ----------- |
| F+P+P   | `Full("789")\nPartial("123")\nPartial("456")`              | "789"   | true        |
| F+P+P+F | `Full("789")\nPartial("123")\nPartial("456")\nFull("789")` | "789"   | true        |
| P+P+F   | `Partial("123")\nPartial("456")\nFull("789")`              | "789"   | true        |
| P+P     | `Partial("123")\nPartial("456")`                           | ""      | false       |


> 其中 `Full("789")` = `{"log":"789\n","stream":"stdout","time":"..."}`
> `Partial("123")` = `{"log":"123","stream":"stdout","time":"..."}`

#### TC-GETLASTLINE-DOCKER-PARTIAL-02: TestLastDockerJsonFileMerge

**目的**：验证 `DockerJsonFileParser::GetLastLine` 在 needSingleLine=false 时正确合并 P 行。

**前置条件**：

- `DockerJsonFileParser` buffer > 0
- needSingleLine = false

**测试场景**（对标 CRI `TestLastContainerdTextLineMerge`）：


| 场景      | 输入            | 预期 data            | 预期 fullLine | 预期 rollbackLineFeedCount |
| ------- | ------------- | ------------------ | ----------- | ------------------------ |
| F+P+P+F | Full+P+P+Full | "123456789"（P 行合并） | true        | 3                        |
| P+P+F   | P+P+Full      | "123456789"        | true        | 3                        |
| P+P     | P+P           | ""                 | false       | 0                        |


### 4.3 InputContainerStdioUnittest — 新增

#### TC-STDIO-JSON-MULTILINE-01: TestCreateInnerProcessorsJsonMultilineNoJsonReader

**目的**：验证 `InputContainerStdio::CreateInnerProcessors` 在 JSON 多行模式下不设置 `RequiringJsonReaderFlag`。

**前置条件**：

- `InputContainerStdio` Init，Multiline.Mode = JSON

**验证**：

- `mContext->RequiringJsonReader()` 返回 `false`
- inner processors 链中 step 4 为 `ProcessorMergeMultilineLogNative`(MergeType=json)（Phase 4 将 `SplitChar='\0'` 替换为 JSON 块合并）
- `mContext->RequiringJsonReader()` 返回 `false`

### 4.4 ProcessorMergeMultilineLogNativeUnittest — Phase 4 新增

新增测试类 `ProcessorMergeMultilineLogJsonUnittest`，验证 `MergeType::BY_JSON` 的 JSON 块合并功能。

#### TC-JSON-01: TestMergeJsonInit

**目的**：验证 `ProcessorMergeMultilineLogNative` 以 `MergeType="json"` Init 成功。

**验证**：
- Init 返回 true
- `mMergeType == MergeType::BY_JSON`

#### TC-JSON-02: TestMergeJsonSingleLineBlock

**目的**：验证单行完整 JSON 不触发合并。

**输入**：3 个事件，每个 content 是完整 JSON：
- `{"a":1}`
- `{"b":2}`
- `{"c":3}`

**预期**：3 个事件不变（braceDepth 在每行内 0→1→0，无需合并）。

#### TC-JSON-03: TestMergeJsonMultiLineBlock

**目的**：验证多行 JSON 块正确合并。

**输入**：6 个事件（模拟多行 JSON）：
```
{
  "key": "value",
  "nested": {
    "a": 1
  }
}
```

**预期**：1 个事件，content 为上述 6 行用 `\n` 拼接。

#### TC-JSON-04: TestMergeJsonMultipleBlocks

**目的**：验证连续多个 JSON 块各自独立合并。

**输入**：4 个事件：
- `{`
- `"a":1`
- `}`
- `{"b":2}`

**预期**：2 个事件（前 3 行合并为 `{\n"a":1\n}`，第 4 行独立）。

#### TC-JSON-05: TestMergeJsonBraceInString

**目的**：验证引号内的大括号不影响 braceDepth 计数。

**输入**：1 个事件 `{"key": "value with { and } braces"}`

**预期**：1 个事件不变（braceDepth 正确归零）。

#### TC-JSON-06: TestMergeJsonEscapedQuote

**目的**：验证转义引号 `\"` 不影响 inQuote 状态。

**输入**：1 个事件 `{"key": "he said \"hello\""}`

**预期**：1 个事件不变。

#### TC-JSON-07: TestMergeJsonOversized

**目的**：验证 JSON 块超过 `MaxJsonBlockSize` 时强制输出。

**前置条件**：Init config 设置 `MaxJsonBlockSize` 为较小值（如 50 字节）。

**输入**：多行 JSON 块，累积大小超过限制。

**预期**：
- 强制拆分输出
- 后续行继续正常处理

#### TC-JSON-08: TestMergeJsonIncompleteAtEnd

**目的**：验证批次末尾未闭合 JSON 块不丢弃。

**输入**：3 个事件 `{`, `"a":1`, `"b":2`（无 `}`）

**预期**：合并为 1 个事件输出（内容为 `{\n"a":1\n"b":2`）。

### 4.5 InputContainerStdioUnittest — Phase 4 更新

更新 TC-STDIO-JSON-MULTILINE-01 验证：
- step 4 创建的是 `ProcessorMergeMultilineLogNative`(MergeType=json)，而非 `ProcessorSplitLogStringNative`
- `mContext->RequiringJsonReader()` 返回 `false`

---

## 5. 现有测试 vs 新增测试对照

```
ProcessorParseContainerLogNativeUnittest
  ├── TestInit                                    [已有]
  ├── TestContainerdLog                           [已有] CRI P/F 解析
  ├── TestIgnoringStdoutStderr                    [已有] CRI stdout/stderr 过滤
  ├── TestContainerdLogWithSplit                  [已有] CRI 全链路 P/F + 正则
  ├── TestDockerJsonLogLineParserWithSplit         [已有] Docker 全链路，仅 Full
  ├── TestDockerJsonLogLineParser                 [已有] Docker 字段边界
  ├── TestKeepingSourceWhenParseFail              [已有] 解析失败保留/丢弃
  ├── TestParseDockerLog                          [已有] ParseDockerLog 底层函数
+ ├── TestDockerJsonPartialLogBasic               [新增] Docker partial: PartLogFlag 设置
+ ├── TestDockerJsonPartialLogWithSplit            [新增] Docker partial: P+P+F 全链路合并
+ └── TestDockerJsonPartialLogWithSplitAndRegex   [新增] Docker partial: 四步全链路

GetLastLineDataUnittest
  ├── LastMatchedContainerdTextLineUnittest
  │   ├── TestLastContainerdTextLineSingleLine     [已有] CRI P/F needSingleLine=true
  │   └── TestLastContainerdTextLineMerge          [已有] CRI P/F needSingleLine=false
  ├── LastMatchedDockerJsonFileUnittest
  │   ├── TestLastDockerJsonFile                   [已有] Docker 仅 Full，无 P 行
+ │   ├── TestLastDockerJsonFileSingleLine         [新增] Docker P/F needSingleLine=true
+ │   └── TestLastDockerJsonFileMerge              [新增] Docker P/F needSingleLine=false
  └── LastMatchedContainerdTextWithDockerJsonUnittest
      ├── TestContainerdTextWithDockerJson          [已有]
      └── TestDockerJsonWithContainerdText          [已有]

ProcessorMergeMultilineLogNativeUnittest (Phase 4 新增)
+ └── ProcessorMergeMultilineLogJsonUnittest
+     ├── TestMergeJsonInit                          [新增] MergeType=json Init
+     ├── TestMergeJsonSingleLineBlock               [新增] 单行 JSON 不合并
+     ├── TestMergeJsonMultiLineBlock                [新增] 多行 JSON 块合并
+     ├── TestMergeJsonMultipleBlocks                [新增] 连续多块各自合并
+     ├── TestMergeJsonBraceInString                 [新增] 引号内大括号忽略
+     ├── TestMergeJsonEscapedQuote                  [新增] 转义引号处理
+     ├── TestMergeJsonOversized                     [新增] 超限强制输出
+     └── TestMergeJsonIncompleteAtEnd               [新增] 末尾未闭合

InputContainerStdioUnittest
  ├── OnSuccessfulInit                             [已有]
  ├── OnEnableContainerDiscovery                   [已有]
  ├── OnPipelineUpdate                             [已有]
  ├── TestTryGetRealPath                           [已有]
+ └── TestCreateInnerProcessorsJsonMultilineNoJsonReader  [新增/更新] Phase 3 + Phase 4
```

新增 `+` 标记的共 **14 个测试方法**（Phase 1-3: 6 个 + Phase 4: 8 个），覆盖 3 个已知缺陷 + JSON 多行块合并功能。