# 单元测试

## C++单元测试

C++部分单测基于 gtest 实现，具体编写方法可以参考已有测试用例。

### 单测覆盖率统计

在开发镜像中，内置了C++单测覆盖率统计工具 gcovr，可以通过以下命令在本地查看覆盖率。

1. 进入开发容器中，参考[开发环境](../development-environment.md)。
2. CMake 设置参数 BUILD_LOGTAIL_UT=ON，编译单测
```
cmake -DBUILD_LOGTAIL_UT=ON <其他编译参数> ..
```
3. 运行脚本 `./scripts/run_core_ut.sh`，运行单测
4. 生成覆盖率报告

```shell
mkdir -p coverage-report
# 生成详细的报告
gcovr -r ./core --txt coverage-report/index.txt --html-details --html coverage-report/index.html -e ".*\.pb\.cc" -e ".*\.pb\.h" -e ".*unittest.*" -e ".*config_server.*" -e ".*go_pipeline.*" -e ".*sdk.*"
# 生成本次commit diff的报告
python3 tools/coverage-diff/main.py coverage-report/index.txt
```

## Go插件单元测试

### 测试框架

如何需要一些测试指令，比如 `assert`, `require`, `mock` 以及 `suite`，可以使用这个包协助你进行测试： `github.com/stretchr/testify`。更多使用方法可以查看 [使用说明](https://github.com/stretchr/testify)。

### 测试工具

从插件开发以及 [日志打印](How-to-use-logger.md) 篇幅可以看到，ilogtail.Context 接口包含了iLogtail 的元配置信息，因此提供了Mock Context 以及Mock Collector 实现进行单元测试。

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

### 测试插件行为

如果需要日志的方式进行验证插件具体行为，可以参加[日志功能高级用法](../plugin-development/logger-api.md)
