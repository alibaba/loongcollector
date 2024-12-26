# GroupInfoFilter FlushInterceptor 扩展

## 简介

`ext_groupinfo_filter` 扩展插件，实现了 [extensions.FlushInterceptor](https://github.com/alibaba/loongcollector/blob/main/pkg/pipeline/extensions/flush_interceptor.go) 接口，可以在 http_flusher 插件中引用，提供在向远端最终提交前筛选数据的能力。

## 版本

[Alpha](../stability-level.md)

## 配置参数

| 参数    | 类型                    | 是否必选 | 说明                                 |
|-------|-----------------------|------|------------------------------------|
| Tags  | Map<String,Condition> | 否    | 需要过滤的GroupInfo.Tags的 key,value     |
| Metas | Map<String,Condition> | 否    | 需要过滤的GroupInfo.Metadata的 key,value |

其中，**Condition** 的字段结构如下：

| 字段      | 类型      | 是否必填 | 说明                        |
|---------|---------|------|---------------------------|
| Pattern | String  | 是    | 匹配正则表达式                   |
| Reverse | Boolean | 否    | 是否反向选择，即选择不匹配 Pattern 的数据 |

## 样例

使用 `metric_mock` input 插件生成数据，并将采集结果以 `custom_single` 协议、`json`格式提交到 `http://localhost:8086/write`。

且在flusher处理时，仅选择GroupInfo的Tags中包含 `tag1=tag1` 的数据发送

```yaml
enable: true
version: v2
inputs:
  - Type: metric_mock
    GroupMeta:
      meta1: meta1
    GroupTags:
      tag1: tag1
  - Type: metric_mock
    GroupMeta:
      meta2: meta2
    GroupTags:
      tag2: tag2
flushers:
  - Type: flusher_http
    RemoteURL: "http://localhost:8086/write"
    Convert:
      Protocol: custom_single
      Encoding: json
    FlushInterceptor: 
      Type: ext_groupinfo_filter
extensions:
  - Type: ext_groupinfo_filter
    Tags:
      tag1: 
        Pattern: tag1
```

## 使用命名扩展

当希望在同一个pipeline中使用多个扩展时，可一给同一类型的不同扩展实例附加一个命名，引用时可以通过full-name来引用特定的实例。

如下示例，通过将`Type: ext_groupinfo_filter` 改写为 `Type: ext_groupinfo_filter/tag1`，追加命名`tag1`，在 http_flusher插件中通过 `ext_groupinfo_filter/tag1`来引用。

```yaml
enable: true
version: v2
inputs:
  - Type: metric_mock
    GroupMeta:
      meta1: meta1
    GroupTags:
      tag1: tag1
  - Type: metric_mock
    GroupMeta:
      meta2: meta2
    GroupTags:
      tag2: tag2
flushers:
  - Type: flusher_http
    RemoteURL: "http://localhost:8086/write_select_tag1"
    Convert:
      Protocol: custom_single
      Encoding: json
    FlushInterceptor: 
      Type: ext_groupinfo_filter/tag1
  - Type: flusher_http
    RemoteURL: "http://localhost:8086/write_select_tag2"
    Convert:
      Protocol: custom_single
      Encoding: json
    FlushInterceptor: 
      Type: ext_groupinfo_filter/tag2
extensions:
  - Type: ext_groupinfo_filter/tag1
    Tags:
      tag1: 
        Pattern: tag1
  - Type: ext_groupinfo_filter/tag2
    Tags:
      tag2:
        Pattern: tag2
```
