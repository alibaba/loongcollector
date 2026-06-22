# 插件文档规范

这是一份 LoongCollector 插件的中文文档的模版及格式说明。

## 格式说明

### 正文

#### 文件命名

文档以插件英文名命名文件，`_`改为`-`，例如`metric_mock`插件的文档名为`metric-mock.md`，保存在`docs/cn/plugins/input/extended/metric-mock.md`的相应文件夹下。

#### 标题部分

标题为插件的中文名。

#### 简介部分

简介处需要写上插件的英文名并附上介绍。简介的最后需要附上源代码链接。

#### 版本说明

- **新插件**（尚未随任一稳定 Release 宣发）：「推荐版本」可填 **【待发布】**；首次进入 Release 说明后，改为 **LoongCollector v{与 [GitHub Releases](https://github.com/alibaba/loongcollector/releases) 一致} 及以上**（或历史插件仍用 **iLogtail v…**，见项目插件文档惯例）。
- **已定版插件**：按插件**源码最后一次变更**时间，对照 Release 记录，取**已包含该变更的最早**主线版本写入「推荐版本」（本地可用 `git log -1 -- <源码路径>`；无 tag 时以 `docs/cn/installation/release-notes/release-notes.md` 等为准）。

#### 配置参数部分

需要填写参数列表，整体样式见下方的模版。有几点需要说明：

1. 所有的类型如下：
   * Integer
   * Long
   * Boolean
   * String
   * Map（需注明key和value类型）
   * Array（需注明value类型）
2. 类型与默认值间以中文逗号分隔，若无默认值则填写`无默认值（必填）`，若有默认值，在默认值外加上` `` `。
3. 特殊值：
   * 空字符串：`""`
   * 空array：`[]`
   * 空map：`{}`

#### 样例部分

样例主要包括输入、采集配置和输出三部分。

1. **采集配置（collection pipeline）定义**（`enable` / `inputs` / `processors` / `flushers` 等段）须使用 **YAML**。**输入**为 NDJSON 时，\`\`\`json 块内**每行一条紧凑 JSON**（单行对象）。**输出**示意：**每条数据**为**一个独立 JSON 对象**；**多条数据**写在同一 \`\`\`json 代码块内、**对象之间空行分隔**，**不要**额外包一层 JSON 数组 `[]`。原始日志行、行协议也可用 \`\`\`text。
2. 代码块标签常用 `bash`、`yaml`、`json`；根据插件差异可有多组样例，每组未必同时包含输入与输出。

#### 参考

可用于参考的`service_journal`插件文档 [service-journal.md](https://github.com/alibaba/loongcollector/blob/main/docs/cn/plugins/input/extended/service-journal.md) 。

### 汇总页

文档完成后，需要修改 `docs/cn/plugins/overview.md` 和 `docs/cn/SUMMARY.md`。

1. 两处以插件 **Type（英文名）字典序升序** 排列；新增插件时插入到正确位置。
2. `SUMMARY.md` 中插件小节顺序与 `overview.md` 保持一致。

## 文档模版

文档模版如下。

```text

# (插件的中文名)

## 简介

`（插件名）` （描述）（源代码链接）

## 版本说明

* 推荐版本：【待发布】

## 配置参数

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| Type | String，无默认值（必填） | 插件类型，固定为`（插件名）`。 |
|  | - | （若有参数则填写） |

## 样例

* 输入

```bash
```

* 采集配置

```yaml
```

* 输出

```json
```
