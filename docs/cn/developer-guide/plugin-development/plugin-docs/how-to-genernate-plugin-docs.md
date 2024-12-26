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

## 生成用法

1. 执行 `make docs` 生成插件文档. 注意: 如果你编写的插件只运行在具体的操作系统，如linux，请在具体的操作系统环境进行执行，否则不会生成。
2. 由于文档建设目前还不完善，每次生成后会覆盖`plugin-list.md`里的插件列表，请保证此插件列表只增加您的插件，并且git 提交只新增贡献插件的插件文档文件。
