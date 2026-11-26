# How to generate plugin doc?

## Usage

The upper case with `comment` tag (exported) field names would be auto extracted. When some field tags with `json`, `yaml`
or `mapstructure`, the original name would be override by the previous sequence. Also，the default value set in the init function would be extracted as the default value column.

``` go
type TestDoc struct {
 Field1 int               `json:"field_1" comment:"field one"`
 Field2 string            `json:"field_2" comment:"field two"`
 Field3 int64             `json:"field_3" mapstructure:"field_33" comment:"field three"`
 Field4 []string          `json:"field_4" comment:"field four"`
 Field5 map[string]string `json:"field_5" comment:"field five"`
 ignoreField string
}

func (t TestDoc) Description() string {
 return "this is a test doc demo"
}
```

``` markdown
# test-plugin
## Description
this is a test doc demo
## Config
|  field   |   type   |   description   | default value   |
| ---- | ---- | ---- | ---- |
|field_1|int|field one|1|
|field_2|string|field two|"filed 2"|
|field_33|int64|field three|3|
|field_4|[]string|field four|["field","4"]|
|field_5|map[string]string|field five|{"k":"v"}|
```

## Contribute documentation

Document the plugin's configuration fields and defaults, then add the new plugin to `plugin-list.md`.
