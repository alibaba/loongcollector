# How to auto generate plugin doc?

## Implement the Doc interface

``` go
// Doc generates markdown doc with the exported field.
// If some field want to add comment, please use comment tag to describe it.
type Doc interface {
	Description() string
}
```

## Add configuration

The upper case (exported) field names would be auto extracted. When some field tags with `json`, `yaml`
or `mapstructure`, the original name would be override by the previous sequence.

## Register to the doc center



``` go

import (
	"github.com/alibaba/ilogtail/pkg/doc"
)

func init(){
    doc.Register("subscriber", gRPCName, new(GrpcSubscriber))
}
```


## Demo

``` go
type TestDoc struct {
	Field1      int    `json:"field_1" comment:"field one"`
	Field2      string `json:"field_2" comment:"field two"`
	Field3      int64  `json:"field_3" mapstructure:"field_4" comment:"field three"`
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
|  field   |   type   |   description   |
| ---- | ---- | ---- |
|field_1|int|field one|
|field_2|string|field two|
|field_4|int64|field three|
```

