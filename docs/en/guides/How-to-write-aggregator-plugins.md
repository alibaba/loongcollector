# How to develop Aggregator plugin

An Aggregator batches `models.PipelineGroupEvents` for downstream flushers.

## Aggregator interface definition

- `Init` initializes the plugin and returns the interval, in milliseconds, at which the runner calls `GetResult`. Return zero to use the pipeline default.
- `Description` returns a description of the plugin.
- `Record` accepts an event group. It can emit completed groups immediately through `ctx.Collector().Collect` or `CollectList`.
- `GetResult` emits pending groups on the timer and when the pipeline stops. `Record` and `GetResult` may run concurrently, so synchronize access to shared state.
- `Reset` clears pending state.

```go
package pipeline

import "github.com/alibaba/ilogtail/pkg/models"

type Aggregator interface {
	Init(Context) (int, error)
	Description() string
	Record(*models.PipelineGroupEvents, PipelineContext) error
	GetResult(PipelineContext) error
	Reset()
}
```

## Aggregator Development

To develop an Aggregator plugin, follow these steps.

1. Create an Issue to describe the function of the plugin development. There will be community committers participating
   in the discussion. If the community review passes, please refer to step 2 to continue.
2. Implement `pipeline.Aggregator`. See [`aggregator_base`](../../../plugins/aggregator/baseagg/aggregator_base.go) for an example of batching and emitting event groups.
3. Call [`pipeline.AddAggregatorCreator`](../../../pkg/pipeline/plugin.go) in `init`. The registered plugin type must start with `aggregator_`.
4. Add the plugin package to [`plugins.yml`](../../../plugins.yml). Use the `common`, `linux`, or `windows` section as appropriate. Run `make all` to regenerate the imports under `plugins/all`.
5. For unit test or E2E test, please refer to [How to write single test](./How-to-write-unit-test.md)
   and [How to write E2E test](../../cn/developer-guide/test/e2e-test.md) .
6. Use *make lint* to check the code specification.
7. Submit a Pull Request.
