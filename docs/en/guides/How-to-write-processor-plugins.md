# How to develop Processor plugin ?

The Processor plugin performs secondary processing on the input data. The following will guide how to develop the
Processor plugin.

## Processor interface definition

The Processor plugin interface method is very easy to understand. In addition to the two standard methods
Init/Description, there is only one ProcessLogs method. Its input and output are both []*Log, which accepts a Log
list, and returns a Log list after processing. Naturally, the processor plugin instance triggers a serial processing chain
through this method.

```go
// Processor also can be a filter
type Processor interface {
// Init called for init some system resources, like socket, mutex...
Init(Context) error

// Description returns a one-sentence description on the Input
Description() string

// Apply the filter to the given metric
ProcessLogs(logArray []*protocol.Log) []*protocol.Log
}
```

## Processor Development

The development of MetricInput is divided into the following steps:

1. Create an Issue to describe the function of the plugin development. There will be community committers participating
   in the discussion. If the community review passes, please refer to step 2 to continue.
2. Implement the Processor interface. We
   use [processor/addfields](../../../plugins/processor/addfields/processor_add_fields.go)
   as an example mode to introduce how to do. Also, You can
   use [example.json](../../../plugins/processor/addfields/example.json) to experiment with this plugin function.
3. Add the plugin to the [Global Plugin Definition Center](../../../plugins/all/all.go). If it only runs on the
   specified system, please add it to the [Linux Plugin Definition Center](../../../plugins/all/all_linux.go)
   Or [Windows Plugin Definition Center](../../../plugins/all/all_windows.go).
4. For unit test or E2E test, please refer to [How to write single test](./How-to-write-unit-test.md)
   and [How to write E2E test](../../../test/README.md) .
5. Use *make lint* to check the code specification.
6. Submit a Pull Request.