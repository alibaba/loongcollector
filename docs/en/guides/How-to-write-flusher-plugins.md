# How to develop Flusher plugin

The Flusher plug-in interacts with external systems and sends data to the outside. The following will guide how to
develop the Flusher plugin.

## Flusher interface definition

- Init: Plugin initialization interface, the main function of Flusher is to create links to external resources.
- Description: plugin description
- IsReady: The plugin system will call this interface to check whether the current flusher can still process more data
  before calling Flush. If the answer is no, it will wait for a while and try again.
- Flush: It is the entry point for the plugin system to submit data to the flusher plugin instance, which is used to
  output the data to the external system. In order to map to the concept of log service, we added three string
  parameters, which represent the project/logstore/config to which this flusher instance belongs. More details please
  see [Data Structure](../concept&designs/Datastructure.md)
  And [Basic structure](../concept&designs/Overview.md).

- SetUrgent: Identifies that iLogtail is about to exit, and passes the system status to the specific flusher plugin, so
  that the flusher plugin can automatically adapt to the system state, such as speeding up the output rate. (SetUrgent is called before Stop method of plugins of other kinds, however, there is no meaningful implementaion yet.)
- Stop: Stop the flusher plugin, such as disconnecting the link to interact with external systems.

```go
type Flusher interface {
   // Init called for init some system resources, like socket, mutex...
   Init(Context) error

   // Description returns a one-sentence description on the Input.
   Description() string

   // IsReady checks if flusher is ready to accept more data.
   // @projectName, @logstoreName, @logstoreKey: meta of the corresponding data.
   // Note: If SetUrgent is called, please make some adjustment so that IsReady
   //   can return true to accept more data in time and config instance can be
   //   stopped gracefully.
   IsReady(projectName string, logstoreName string, logstoreKey int64) bool

   // Flush flushes data to destination, such as SLS, console, file, etc.
   // It is expected to return no error at most time because IsReady will be called
   // before it to make sure there is space for next data.
   Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error

   // SetUrgent indicates the flusher that it will be destroyed soon.
   // @flag indicates if main program (Logtail mostly) will exit after calling this.
   //
   // Note: there might be more data to flush after SetUrgent is called, and if flag
   //   is true, these data will be passed to flusher through IsReady/Flush before
   //   program exits.
   //
   // Recommendation: set some state flags in this method to guide the behavior
   //   of other methods.
   SetUrgent(flag bool)

   // Stop stops flusher and release resources.
   // It is time for flusher to do cleaning jobs, includes:
   // 1. Flush cached but not flushed data. For flushers that contain some kinds of
   //   aggregation or buffering, it is important to flush cached out now, otherwise
   //   data will lost.
   // 2. Release opened resources: goroutines, file handles, connections, etc.
   // 3. Maybe more, it depends.
   // In a word, flusher should only have things that can be recycled by GC after this.
   Stop() error
}
```

## Flusher Development

The development of ServiceInput is divided into the following steps:

1. Create an Issue to describe the function of the plugin development. There will be community committers participating
   in the discussion. If the community review passes, please refer to step 2 to continue.
2. Implement the Flusher interface. We use [flusher/stdout](../../../plugins/flusher/stdout/flusher_stdout.go)
   as an example to introduce how to do. Also, You can
   use [example.json](../../../plugins/processor/addfields/example.json) to experiment with this plugin function.
3. Register your plugin to [Flushers](../../../plugin.go) in init function. The registered name (a.k.a. plugin_type in json configuration) of a Flusher plugin must start with "flusher_".  We
   use [flusher/stdout](../../../plugins/flusher/stdout/flusher_stdout.go)
   as an example to introduce how to do.
4. Add the plugin to the [Global Plugin Definition Center](../../../plugins/all/all.go). If it only runs on the
   specified system, please add it to the [Linux Plugin Definition Center](../../../plugins/all/all_linux.go)
   Or [Windows Plugin Definition Center](../../../plugins/all/all_windows.go).
5. For unit test or E2E test, please refer to [How to write single test](./How-to-write-unit-test.md)
   and [How to write E2E test](../../../test/README.md) .
6. Use *make lint* to check the code specification.
7. Submit a Pull Request.
