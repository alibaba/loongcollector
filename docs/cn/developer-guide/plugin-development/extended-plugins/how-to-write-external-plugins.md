# 如何开发外部私有插件

## 场景

某些情况下，您可能想要开发自己的非公开插件，但又希望能够及时更新使用到社区 LoongCollector 不断迭代的更新功能（而不是在社区版本上分叉），LoongCollector 的外部插件开发机制可以满足您这样的需求。

## 步骤

假设您将要在仓库中 `my-repo.com/my_space/my_plugins` 中创建一个名为 `service_example_plugin` 的 input 插件。

### 1. 初始化 go 工程

```shell
go mod init my-repo.com/my_space/my_plugins
```

### 2. 引入插件API package

在仓库根目录执行如下目录，引入

```shell
go get github.com/alibaba/loongcollector/pkg
```

### 3. 创建插件目录

```shell
mkdir example_plugin
```

### 4. 编写插件代码

我们的插件实现 `ServiceInputV1`、`ServiceInputV2` 这两个接口

```shell
cd example_plugin
touch example_plugin.go
```

```go
package example_plugin

import (
 "time"

 "github.com/alibaba/loongcollector/pkg/models"
 "github.com/alibaba/loongcollector/pkg/pipeline"
 "github.com/alibaba/loongcollector/pkg/protocol"
)

type MyServiceInput struct {
 Interval time.Duration `json:"interval" mapstructure:"interval"`

 collector   pipeline.Collector
 collectorV2 pipeline.PipelineCollector
 stopCh      chan struct{}
}

func (m *MyServiceInput) StartService(context pipeline.PipelineContext) error {
 m.collectorV2 = context.Collector()
 return nil
}

func (m *MyServiceInput) Init(context pipeline.Context) (int, error) {
 return 0, nil
}

func (m *MyServiceInput) Description() string {
 return "custom service input"
}

func (m *MyServiceInput) Stop() error {
 return nil
}

func (m *MyServiceInput) Start(collector pipeline.Collector) error {
 m.collector = collector
 go m.loop()
 return nil
}

func (m *MyServiceInput) loop() {
 for {
  select {
  case <-m.stopCh:
   return
  case <-time.After(m.Interval):
   if m.collector != nil {
    m.collector.AddRawLog(&protocol.Log{
     Time: uint32(time.Now().Unix()),
     Contents: []*protocol.Log_Content{
      {Key: "host", Value: "1.1.1.1"},
     },
    })
   }
   if m.collectorV2 == nil {
    continue
   }
   m.collectorV2.Collect(&models.GroupInfo{},
    models.NewSingleValueMetric("test_metric",
     models.MetricTypeCounter,
     models.NewTagsWithMap(map[string]string{"hello": "world"}),
     time.Now().UnixNano(),
     1),
   )
  }
 }
}
```

### 5. 添加 init 函数，注册插件

`example_plugin.go` 文件中追加如下函数

```go
func init() {
 pipeline.AddServiceCreator("service_example_plugin", func() pipeline.ServiceInput {
  return &MyServiceInput{
   Interval: 5 * time.Second,
   stopCh:   make(chan struct{}, 1),
  }
 })
}
```

可以看到，插件的注册依赖 init 函数机制

### 6. 仓库根目录创建一个引用全部插件的go文件(可选)

当仓库中包含多个插件，并且你想提供一种一次性导入所有插件的便捷方式时，可以在根目录定义一个go文件，import 仓库中的所有其他插件。
创建 `all.go` 文件，并写入如下内容：

```go
package my_plugins

import (
 _ "my-repo.com/my_space/my_plugins/example_plugin"
)

```

至此，我们的插件仓库目录结构如下：

```plain
.
├── all.go
├── example_plugin
│ └── example_plugin.go
├── go.mod
└── go.sum

```

### 7. 编写插件引用配置文件

**以下内容在 LoongCollector 主仓库执行**。

在 LoongCollector 仓库根目录创建名为 `external_plugins.yml` 的配置文件，写入如下内容：

```yaml
plugins:
  common:
    - gomod: my-repo.com/my_space/my_plugins latest
      import: my-repo.com/my_space/my_plugins/example_plugin

```

若希望导入私有仓库中的所有插件（即引用到all.go文件中的插件），可以写入如下内容：

```yaml
plugins:
  common:
    - gomod: my-repo.com/my_space/my_plugins latest
      import: my-repo.com/my_space/my_plugins  // 相比上述文件，少了example_plugin部分

```

之后可以执行构建：

```shell
make all
```

关于如何自定义插件配置的更多内容，可以参阅 [如何自定义构建产物中默认包含的插件](how-to-custom-builtin-plugins.md)。
