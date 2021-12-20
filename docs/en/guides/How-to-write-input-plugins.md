# How to develop Input plugin

Before you understand how to develop the Input plugin, you need to understand the Input plugin interface. The current
Input plugins are divided into two categories, namely MetricInput and ServiceInput. MetricInput can be used to control
the collection frequency globally, which is suitable for the pull of Metric indicators. ServiceInput is more suitable to
receive external input, such as receiving SkyWalking's Trace data.

## MetricInput interface definition

- Init: The plugin initialization interface, mainly for the plugin to do some parameter checking, and provide the
  context object Context for it. The first parameter of the return value represents the call frequency (milliseconds),
  and the plugin system will use this value as the data fetch frequency. If it returns 0, the plugin system will use the
  global data fetch frequency. The second parameter of the return value indicates an error occurred during
  initialization. Once an error occurs, the plugin instance will be ignored.
- Description: plugin self-describing interface.
- Collect: The most critical interface of MetricInput, the plugin system fetch data from plugin instances through this
  interface. The plugin system will periodically call this interface to collect data into the incoming Collector.

```go
// MetricInput ...
type MetricInput interface {
// Init called for init some system resources, like socket, mutex...
// return call interval(ms) and error flag, if interval is 0, use default interval
Init(Context) (int, error)

// Description returns a one-sentence description on the Input
Description() string

// Collect takes in an accumulator and adds the metrics that the Input
// gathers. This is called every "interval"
Collect(Collector) error
}
```

## MetricInput Development

The development of MetricInput is divided into the following steps:

1. Create an Issue to describe the function of the plugin development. There will be community committers participating
   in the discussion. If the community review passes, please refer to step 2 to continue.
2. Implement the MetricInput interface. We use [input/example/metric](../../../plugins/input/example/metric_example.go)
   as an example to introduce how to do. Also, You can
   use [example.json](../../../plugins/input/example/metric_example_input.json) to experiment with this plugin function.
3. Register your plugin to [MetricInputs](../../../plugin.go) in init function. The registered name (a.k.a. plugin_type in json configuration) of a MetricInputs plugin must start with "metric_".  We
   use [input/example/metric](../../../plugins/input/example/metric_example.go)
   as an example to introduce how to do.
4. Add the plugin to the [Global Plugin Definition Center](../../../plugins/all/all.go). If it only runs on the
   specified system, please add it to the [Linux Plugin Definition Center](../../../plugins/all/all_linux.go)
   Or [Windows Plugin Definition Center](../../../plugins/all/all_windows.go).
5. For unit test or E2E test, please refer to [How to write single test](./How-to-write-unit-test.md)
   and [How to write E2E test](../../../test/README.md).
6. Use *make lint* to check the code specification.
7. Submit a Pull Request.

## ServiceInput interface definition

- Init/Description: Same as MetricInput interface, but a return value of Init has no meaning for ServiceInput because it
  will not be called periodically.
- Start: Since the positioning of ServiceInput is a continues input, the plugin system creates an independent goroutine
  for the plugin instance, and calls this interface in the goroutine to start data collection. Collector is still to act
  as a data pipeline between the plugin instance and the plugin system.
- Stop: Provides the ability to terminate the plugin. The plugin must implement this method correctly to prevent
  blocking of the plugin system.

```go

// ServiceInput ...
type ServiceInput interface {
// Init called for init some system resources, like socket, mutex...
// return interval(ms) and error flag, if interval is 0, use default interval
Init(Context) (int, error)

// Description returns a one-sentence description on the Input
Description() string

// Start starts the ServiceInput's service, whatever that may be
Start(Collector) error

// Stop stops the services and closes any necessary channels and connections
Stop() error
}
```

## ServiceInput Development

The development of ServiceInput is divided into the following steps:

1. Create an Issue to describe the function of the plugin development. There will be community committers participating
   in the discussion. If the community review passes, please refer to step 2 to continue.
2. Implement the ServiceInput interface. We
   use [input/example/service](../../../plugins/input/example/service_example.go)
   as an example to introduce how to do. Also, You can
   use [example.json](../../../plugins/input/example/service_example_input.json) to experiment with this plugin
   function.
3. Register your plugin to [ServiceInputs](../../../plugin.go) in init function. The registered name (a.k.a. plugin_type in json configuration) of a ServiceInputs plugin must start with "service_".  We
   use [input/example/service](../../../plugins/input/example/service_example.go)
   as an example to introduce how to do.
4. Add the plugin to the [Global Plugin Definition Center](../../../plugins/all/all.go). If it only runs on the
   specified system, please add it to the [Linux Plugin Definition Center](../../../plugins/all/all_linux.go)
   Or [Windows Plugin Definition Center](../../../plugins/all/all_windows.go).
5. For unit test or E2E test, please refer to [How to write single test](./How-to-write-unit-test.md)
   and [How to write E2E test](../../../test/README.md).
6. Use *make lint* to check the code specification.
7. Submit a Pull Request.
