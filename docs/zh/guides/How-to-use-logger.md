# 如何使用日志功能


## 打印采集配置元信息
对于iLogtail，具有多租户的特点，可以支持多份采集配置同时工作，iLogtail 支持将采集配置的元信息打印到日志中，便于问题的排查与定位。
```go
import (
"github.com/alibaba/ilogtail/pkg/logger"
)
```

以下代码块是一个插件所必备的内容，我们打印日志时仅需要将context.GetRuntimeContext() 传入logger包的第一个参数，打印效果如下，会自动附加采集配置与logstore 名称。

```
type plugin struct {
	context ilogtail.Context
}

func (p *plugin) func1() {
	logger.Warning(p.context.GetRuntimeContext(), "a", "b")
}
```

```
WithMetaData:
2021-08-24 18:20:02 [DBG] [logger_test.go:174] [func1] [mock-configname,mock-logstore]	[a b]:
WithoutMetaData:
2021-08-24 18:20:02 [DBG] [logger_test.go:174] [func1] [a b]:
```

## 测试如何使用Logger
### 一般用法
对于大多数情况，只需要引入`_ "github.com/alibaba/ilogtail/pkg/logger/test"`包即可。
```go
import (
"context"
"testing"

"github.com/alibaba/ilogtail/pkg/logger"
_ "github.com/alibaba/ilogtail/pkg/logger/test"
)

func Test_plugin_func1(t *testing.T) {
logger.Info(context.Background(), "a", "b")
}
```

### 高级用法-自定义Logger
你可以使用[logger.ConfigOption](../../../pkg/logger)设置Logger 的行为，比如输出、日志级别、异步打印等。
```go
package test

import (
	"context"
	"testing"

	"github.com/alibaba/ilogtail/pkg/logger"
)

func init() {
	logger.InitTestLogger(logger.OptionDebugLevel)

}

func Test_plugin_func1(t *testing.T) {
	logger.Debug(context.Background(), "a", "b")
}
```

### 高级用法-读取日志内容
某些单测下，开发者可能需要读取日志来验证代码的分支覆盖行为。在这种情况下可以使用`logger.OptionOpenMemoryReceiver` 参数。
```go
import (
	"context"
	"strings"
	"testing"

	"github.com/alibaba/ilogtail/pkg/logger"
	
	"github.com/stretchr/testify/assert"
)

func init() {
	logger.InitTestLogger(logger.OptionOpenMemoryReceiver)
}

func Test_plugin_func1(t *testing.T) {
	logger.ClearMemoryLog()
	logger.Info(context.Background(), "a", "b")
	assert.Equal(t, 1, logger.GetMemoryLogCount())
	assert.Truef(t, strings.Contains(logger.ReadMemoryLog(1), "a:b"), "got %s", logger.ReadMemoryLog(1))
}
```


## 启动时如何控制日志行为？
启动iLogtail 时，默认的日志行为是异步文件Info级别输出，如果需要动态调整，可以参考以下内容进行设置：
### 调整日志级别
启动时如果启动程序相对路径下没有 plugin_logger.xml 文件，则可以使用以下命令设置：
```shell
./ilogtail --logger-level=debug
```
如果存在 plugin_logger.xml 文件，可以修改文件，或使用以下命令强制重新生成日志配置文件：
```shell
./ilogtail --logger-level=info --logger-retain=false
```

### 是否开启控制台打印
默认生成环境关闭控制台打印，如果本地调试环境想开启控制台日志，相对路径下没有 plugin_logger.xml 文件，则可以使用以下命令：
```shell
./ilogtail --logger-console=true
```
如果存在 plugin_logger.xml 文件，可以修改文件，或使用以下命令强制重新生成日志配置文件：
```shell
./ilogtail --logger-console=true --logger-retain=false
```