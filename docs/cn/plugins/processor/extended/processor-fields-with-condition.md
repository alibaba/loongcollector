# 条件字段处理

## 简介

`processor_fields_with_condition`插件支持根据日志部分字段的取值，动态进行字段扩展或删除。[源代码](https://github.com/alibaba/loongcollector/blob/main/plugins/processor/fieldswithcondition/processor_fields_with_condition.go)

## 版本

[Stable](../../stability-level.md)

### 条件判断

* 支持多字段取值比较。
* 关系运算符：equals（默认）、regexp、contains、startwith。
* 逻辑运算符：and（默认）、or。

### 过滤能力

* 默认仅做字段动态处理。
* 可以开启过滤功能，未命中任意条件则丢弃。

### 处理能力

* 与现有插件processor_add_fields、processor_drop保持近似的使用习惯。
* 可以支持多组条件（Case）进行动态字段处理，但是按顺序匹配上一条后即退出。

## 配置参数

### `processor_fields_with_condition`配置

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| Type    | String，无默认值（必填） | 插件类型，固定为`processor_fields_with_condition`      |
| DropIfNotMatchCondition | Boolean，`false`| 当条均件不满足时，日志是被丢弃（true）还是被保留（false）。|
| Switch | Array，其中value为Condition，无默认值（必填） | 切换行动的条件。 |

### `Condition`类型说明

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| Case | ConditionCase，无默认值（必填） | 日志数据满足的条件。|
| Actions | Array，其中value为ConditionAction，无默认值（必填） | 满足条件时执行的动作。 |

### `ConditionCase`类型说明

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| LogicalOperator | String，`and` | 多个条件字段之间的逻辑运算符（and/or）。 |
| RelationOperator | String，`equals` | 条件字段的关系运算符（equals/regexp/contains/startwith）。 |
| FieldConditions | Map，其中fieldKey和fieldValue为String类型，无默认值（必填） | 字段名和表达式的键值对。 |

### `ConditionAction`类型说明

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| type | String，无默认值（必填） |  行动类型，可选值是`processor_add_fields`/`processor_drop`。|
| IgnoreIfExist | Boolean，`false` | 当相同的键存在时是否要忽略。 |
| Fields | Map，其中fieldKey和fieldValue为String类型，无默认值（必填） | 附加字段的键值对。 |
| DropKeys | Array，其中value为String，无默认值（必填） | 丢弃字段。 |

## 样例

* 采集配置(使用`metric_mock`插件模拟输入)

```yaml
enable: true
inputs:
  - Type: metric_mock
    Fields:
      content : "{\"t1\": \"2022-07-22 12:50:08.571218122\", \"t2\": \"2022-07-22 12:50:08\", \"a\":\"b\",\"c\":2,\"d\":10, \"seq\": 20}"
      t1 : "2022-07-22 12:50:08.571218122"
      t2 : "2022-07-22 12:50:08"
      a : b
      c : "2"
      d : "10"
      seq : "20"
processors:
    - Type: processor_fields_with_condition
      DropIfNotMatchCondition: true 
      Switch:
        - Case:
            FieldConditions:
              seq: "10"
              d: "10"
          Actions:
            - type: processor_add_fields
              IgnoreIfExist: false
              Fields:
                eventCode: event_00001
                name: error_oom
            - type: processor_drop
              DropKeys:
                - c
        - Case:
            FieldConditions:
              seq: "20"
              d: "10"
          Actions:
            - type: processor_add_fields
              IgnoreIfExist: false
              Fields:
                eventCode: event_00002
                name: error_oom2
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
  "Index":"1",
  "a":"b",
  "c":"2",
  "content":"{\"t1\": \"2022-07-22 12:50:08.571218122\", \"t2\": \"2022-07-22 12:50:08\", \"a\":\"b\",\"c\":2,\"d\":10, \"seq\": 20}",
  "d":"10",
  "seq":"20",
  "t1":"2022-07-22 12:50:08.571218122",
  "t2":"2022-07-22 12:50:08",
  "eventCode":"event_00002",
  "name":"error_oom2",
  "__time__":"1658490721"
}
```
