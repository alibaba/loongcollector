# How to write a unit test

## Test Framework
If you need some test instructions, such as `assert`, `require`, `mock` and `suite`, you can use this package to assist you in testing: `github.com/stretchr/testify`. More usage methods can be found in [Instructions](https://github.com/stretchr/testify).

## test tools
From the plugin development and [Log Printing](How-to-use-logger.md), you can see that `the ilogtail.Context` interface contains the meta configuration information of iLogtail, so Mock Context and Mock Collector are provided for unit testing.

```go
import (
	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"

	"testing"
)

func TestInputSystem_CollectOpenFD(t *testing.T) {
	cxt := mock.NewEmptyContext("project", "store", "config")
	p := pipeline.MetricInputs["metric_system_v2"]().(*InputSystem)
	_, err := p.Init(cxt)
	assert.NoError(t, err, "cannot init the mock process plugin: %v", err)
	c := &test.MockMetricCollector{}
	p.CollectOpenFD(c)
	assert.Equal(t, 2, len(c.Logs))
	m := make(map[string]string)
	for _, log := range c.Logs {
		for _, content := range log.Contents {
			if content.Key == "__name__" {
				m[content.Value] = "exist"
			}
		}
	}
	assert.NotEqual(t, "", m["fd_allocated"])
	assert.NotEqual(t, "", m["fd_max"])
}
```
## Test plugin behavior
If you need logs to verify the specific behavior of the plugin, you can refer to the part named "[Read log content[Advanced]](How-to-use-logger.md)".