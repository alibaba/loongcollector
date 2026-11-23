# How to develop Flusher plugin

A Flusher sends event groups to an external system.

## Flusher interface definition

- `Init` initializes the plugin and opens connections or other resources.
- `Description` returns a description of the plugin.
- `IsReady` checks whether the destination can accept more data. The plugin system calls it before `Export` and waits before retrying if it returns `false`.
- `Export` receives a batch of `models.PipelineGroupEvents` and a `PipelineContext`. Encode and send the events according to the destination protocol.
- `SetUrgent` signals that the plugin will stop soon so it can adjust its readiness and delivery behavior.
- `Stop` sends any buffered data and releases resources.

```go
package pipeline

import "github.com/alibaba/ilogtail/pkg/models"

type Flusher interface {
	Init(Context) error
	Description() string
	IsReady(projectName string, logstoreName string, logstoreKey int64) bool
	SetUrgent(flag bool)
	Stop() error
	Export([]*models.PipelineGroupEvents, PipelineContext) error
}
```

## Flusher Development

To develop a Flusher plugin, follow these steps.

1. Create an Issue to describe the function of the plugin development. There will be community committers participating
   in the discussion. If the community review passes, please refer to step 2 to continue.
2. Implement `pipeline.Flusher`. See [`flusher_stdout`](../../../plugins/flusher/stdout/flusher_stdout.go) for an example of exporting event groups.
3. Call [`pipeline.AddFlusherCreator`](../../../pkg/pipeline/plugin.go) in `init`. The registered plugin type must start with `flusher_`.
4. Add the plugin package to [`plugins.yml`](../../../plugins.yml). Use the `common`, `linux`, or `windows` section as appropriate. Run `make all` to regenerate the imports under `plugins/all`.
5. For unit test or E2E test, please refer to [How to write single test](./How-to-write-unit-test.md)
   and [How to write E2E test](../../cn/developer-guide/test/e2e-test.md) .
6. Use *make lint* to check the code specification.
7. Submit a Pull Request.
