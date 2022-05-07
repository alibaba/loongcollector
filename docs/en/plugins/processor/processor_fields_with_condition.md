# processor_fields_with_condition
## Description
Processor to match multiple conditions, and if one of the conditions is met, the corresponding action is performed.
## Config
|  field   |   type   |   description   | default value   |
| ---- | ---- | ---- | ---- |
|DropIfNotMatchCondition|bool|Optional. When the case condition is not met, whether the log is discarded (true) or retained (false)|false|
|Switch|[]Condition|The switch-case conditions|null|

### Condition
|  field   |   type   |   description   | default value   |
| ---- | ---- | ---- | ---- |
|Case|ConditionCase|The condition that log data satisfies|null|
|Actions|[]ConditionAction|The action that executes when the case condition is met|null|

### ConditionCase
|  field   |   type   |   description   | default value   |
| ---- | ---- | ---- | ---- |
|LogicalOperator|string|Optional. The Logical operators between multiple conditional fields, alternate values are and/or|and|
|RelationOperator|string|Optional. The Relational operators for conditional fields, alternate values are equals/regexp/contains/startwith|equals|
|FieldConditions|map[string]string|The key-value pair of field names and expressions|null|

### ConditionAction
|  field   |   type   |   description   | default value   |
| ---- | ---- | ---- | ---- |
|type|string|action type. alternate values are processor_add_fields/processor_drop||
|IgnoreIfExist|bool|Optional. Whether to ignore when the same key exists|false|
|Fields|map[string]string|The appending fields|null|
|DropKeys|[]string|The dropping fields|null|