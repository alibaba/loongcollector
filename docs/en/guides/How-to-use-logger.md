# How to use the log function

## Print iLogtail configuration meta information

For iLogtail, it can support multiple collection configurations to work at the same time. iLogtail supports printing the
meta-information of the collection configuration to the log, which is convenient for troubleshooting.

```go
import (
"github.com/alibaba/ilogtail/pkg/logger"
)
```

The following code block is necessary for each plugin. When printing logs, we only need to pass
context.GetRuntimeContext() to the first parameter of the logger. The printing effect is as follows, and the collection
configuration and logstore name will be automatically appended.

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

## Test how to use Logger

### General usage

For most cases, you only need to import the `_ "github.com/alibaba/ilogtail/pkg/logger/test"` package.

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

### Usage-Custom Logger[Advanced]

You can use [logger.ConfigOption](../../../pkg/logger) to set the behavior of Logger, such as output, log level,
asynchronous printing, etc.

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

### Read log content[Advanced]

For some unit tests, developers may need to read logs to verify the branch coverage behavior of the code. In this case,
the `logger.OptionOpenMemoryReceiver` parameter can be used.

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

## How to control log behavior at startup?

When iLogtail is started, the default log behavior is Info level logger with asynchronous file output. If you need to
dynamically adjust, you can refer to the following settings:

### Adjust the log level

If there is no plugin_logger.xml file in the relative path of the startup program during startup, you can use the
following command to set it:

```shell
./ilogtail --logger-level=debug
```

If there is a plugin_logger.xml file, you can modify the file, or use the following command to force the log configuration file to be regenerated:

```shell
./ilogtail --logger-level=info --logger-retain=false
```

### Whether to enable console printing

The default generation environment turns off console printing. If the local debugging environment wants to turn on the console log and there is no plugin_logger.xml file in the relative path, you can use the following command:

```shell
./ilogtail --logger-console=true
```

If there is a plugin_logger.xml file, you can modify the file, or use the following command to force the log configuration file to be regenerated:

```shell
./ilogtail --logger-console=true --logger-retain=false
```