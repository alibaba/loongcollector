# 如何生成插件文档

## 用法

使用`comment` tag 标记的大写字符开头的字段，这些字段将会被自动提取到文档中。如果字段还有`json`, `yaml`
或 `mapstructure` tag 标记，字段的名称会被上述tag中的名字所替换, 此外init 注册函数设置的默认值同样会被提取到默认值字段。

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

## 提交文档

根据插件的配置字段和默认值编写文档，并将新插件添加到对应的插件列表中。
