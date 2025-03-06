# 字段打包

## 简介

`processor_packjson`插件可添加指定的字段（支持多个）以 JSON 格式打包成单个字段。

## 版本

[Stable](../../stability-level.md)

## 版本说明

* 推荐版本：iLogtail v1.0.27 及以上

## 配置参数

| 参数              | 类型   | 是否必选 | 参数说明                                    |
| ----------------- | ------ | -------- | ------------------------------------------- |
| Type              | String | 是       | 插件类型，固定为`processor_packjson`。      |
| SourceKeys        | array  | 是       | 字符串数组，需要打包的 key。                |
| DestKey           | string | 是       | 目标 key。                                  |
| KeepSource        | bool   | 否       | 是否保留源字段，默认为 true。               |
| AlarmIfIncomplete | bool   | 否       | 是否在不存在任何源字段时告警，默认为 true。 |

## 样例

将指定的 `a`、`b` 两个字段打包成 JSON 字段 `d_key`，配置详情及处理结果如下：

* 输入

```text
"a":"1"
"b":"2"
```

* 配置详情

```yaml
processors:
  - Type: processor_packjson
    SourceKeys: 
      - "a"
      - "b"
    DestKey: d_key
    KeepSource: true
    AlarmIfEmpty: true
```

* 配置后结果

```text
"a":"1"
"b":"2"
"d_key":"{\"a\":\"1\",\"b\":\"2\"}"
```
