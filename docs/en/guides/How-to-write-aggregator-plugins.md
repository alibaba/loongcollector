# How to develop Aggregator plugin

The Aggregator plugin packs the input data. The following will guide how to develop the Aggregator plugin.

## Aggregator interface definition

The role of the Aggregator plugin is to aggregate individual Logs into a LogGroup according to some rules, and then
submit them to the flusher plugin at the next level for processing.

- Add interface for external log input.
- Flush interface for getting LogGroup by aggregation.
- The Reset interface is currently only used internally and can be ignored.
- The Init interface is similar to the Init interface of the input plugin. The first parameter of the return value
  represents the cycle value of the plugin system calling the Flush interface. When the value is 0, the global parameter
  is used, and the second parameter represents an initialization error. But different from the input plugin, the Init
  interface of the aggregator plugin adds a parameter of type LogGroupQueue, which is defined in the loggroup_queue.go
  file, as follows:
    ```go
    type LogGroupQueue interface {
        // Returns errAggAdd immediately if queue is full.
        Add(loggroup *LogGroup) error
        // Wait at most @duration if queue is full and returns errAggAdd if timeout.
        // Do not use this method if you are unsure.
        AddWithWait(loggroup *LogGroup, duration time.Duration) error
    }
    ```
  LogGroupQueue instance actually acts as a queue, and the aggregator plugin instance can immediately insert the
  aggregated LogGroup object into the queue through the AddXXX interface. This is an option. As you can see from the
  previous description, Add->Flush is a periodically called data link, and LogGroupQueue can provide a higher real-time
  link, after the aggregator gets the newly aggregated LogGroup and submit it directly without waiting for Flush to be
  called.

  But it should be noted that when the queue is full, Add will return an error, which generally means that the flusher
  is blocked, such as a network exception.

```go
// Aggregator is an interface for implementing an Aggregator plugin.
// the RunningAggregator wraps this interface and guarantees that
// Add, Push, and Reset can not be called concurrently, so locking is not
// required when implementing an Aggregator plugin.
type Aggregator interface {
// Init called for init some system resources, like socket, mutex...
// return flush interval(ms) and error flag, if interval is 0, use default interval
Init(Context, LogGroupQueue) (int, error)

// Description returns a one-sentence description on the Input.
Description() string

// Add the metric to the aggregator.
Add(log *protocol.Log) error

// Flush pushes the current aggregates to the accumulator.
Flush() []*protocol.LogGroup

// Reset resets the aggregators caches and aggregates.
Reset()
}
```

## Aggregator Development

The development of ServiceInput is divided into the following steps:

1. Create an Issue to describe the function of the plugin development. There will be community committers participating
   in the discussion. If the community review passes, please refer to step 2 to continue.
2. Implement the Aggregator interface. We
   use [aggregator/defaultone](../../../plugins/aggregator/defaultone/aggregator_default.go)
   as an example to introduce how to do.
3. Register your plugin to [Aggregators](../../../plugin.go) in init function. The registered name (a.k.a. plugin_type in json configuration) of a Aggregator plugin must start with "aggregator_".  We
   use [aggregator/defaultone](../../../plugins/aggregator/defaultone/aggregator_default.go)
   as an example to introduce how to do.
4. Add the plugin to the [Global Plugin Definition Center](../../../plugins/all/all.go). If it only runs on the
   specified system, please add it to the [Linux Plugin Definition Center](../../../plugins/all/all_linux.go)
   Or [Windows Plugin Definition Center](../../../plugins/all/all_windows.go).
5. For unit test or E2E test, please refer to [How to write single test](./How-to-write-unit-test.md)
   and [How to write E2E test](../../../test/README.md) .
6. Use *make lint* to check the code specification.
7. Submit a Pull Request.
