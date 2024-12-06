# 插件文档规范

这是一份 ilogtail 插件的中文文档的模版及格式说明。

## 格式说明

### 正文

#### 文件命名

文档以插件英文名命名文件，`_`改为`-`，例如`metric_mock`插件的文档名为`metric-mock.md`，保存在`docs/cn/plugins/input/extended/metric-mock.md`的相应文件夹下。

#### 标题部分

标题为插件的中文名。

#### 简介部分

简介处需要写上插件的英文名并附上介绍。简介的最后需要附上源代码链接。

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

1. 在代码块附上`bash`、`json`、`yaml`等标签可以更加美观。
2. 根据具体插件差异，可以有多组样例，每组样例也并不一定要有输入。

#### 参考

可用于参考的`service_journal`插件文档 [service-journal.md](https://github.com/alibaba/loongcollector/blob/main/docs/cn/plugins/input/extended/service-journal.md) 。

### 汇总页

文档完成后，需要修改`docs/cn/plugins/overview.md`和`docs/cnSUMMARY.md`。

1. `overview.md`里所有的插件按英文名字典序升序排列，添加的时候注意插入的位置。
2. `SUMMARY.md`中的插件顺序与`overview.md`保持一致，并附上链接。

## 文档模版

文档模版如下。

```text

# (插件的中文名)

## 简介

`（插件名）` （描述）（源代码链接）

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
