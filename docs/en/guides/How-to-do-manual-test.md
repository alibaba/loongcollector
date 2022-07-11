# How to do manual testing

iLogtail as the plugin part of [Logtail AlibabaCloud](https://help.aliyun.com/document_detail/28979.html) can run
independently or work with the iLogtail-C. For most cases, the standalone mode is sufficient to verify. If you are
familiar with [Logtail AlibabaCloud](https://help.aliyun.com/document_detail/28979.html)
, you can also use cgo mode to verify on Alibaba Cloud.

## Standalone mode

You can verify your case with the following 3 steps on standalone mode:

1. Run *make plugin_main* to compile, and the binary program will be located in *output/ilogtail*.
2. Prepare to test the demo.json file.
3. Run *./output/ilogtail --plugin=./demo.json* to start the program,

The following parts will introduce how the input and processor plugins perform local manual testing with standalone
mode.

### input plugin

For input plugins, there may be external dependencies in most cases, so we recommend
using [E2E](../../../test/README.md)
to run it, such as [E2E nginx](../../../test/case/behavior/input_nginx). Of course, if you have prepared the dependent
environment locally, you can also run it locally. The following example shows the collection of nginx status, and
IntervalMs indicates that the acquisition frequency is 1s once. You cloud test the running state with **
TEST_SCOPE=input_nginx TEST_DEBUG=true make e2e** command in the project root path. For the local startup, the collected
output is saved in the *logtail_plugin.LOG* file, that is also cloud be printed to the stdout with *./output/ilogtail
--plugin=./demo.json --logger-console=true --logger-retain=false* command.

```json
{
  "inputs": [
    {
      "type": "metric_nginx_status",
      "detail": {
        "IntervalMs": 1000,
        "Urls": [
          "http://nginx/nginx_status"
        ]
      }
    }
  ],
  "flushers": [
    {
      "type": "flusher_stdout",
      "detail": {
      }
    }
  ]
}
```

### processor plugin

For the processor plugin, you can use metric_mock or service_mock to mock the collection input. The following example is
to verify and test the processor_add_fields plugin function, and metric_mock simulates the collection of input Key/Value
data is 1111:2222 pair, the final output contains 3 fields after processor_add_fields processing. For the local startup,
the collected output is saved in the *logtail_plugin.LOG* file, that is also cloud be printed to the stdout with *
./output/ilogtail --plugin=./demo.json --logger-console=true --logger-retain=false* command.

```json
{
  "inputs": [
    {
      "type": "metric_mock",
      "detail": {
        "Fields": {
          "1111": "2222"
        }
      }
    }
  ],
  "processors": [
    {
      "type": "processor_add_fields",
      "detail": {
        "Fields": {
          "aaa2": "value2",
          "aaa3": "value3"
        }
      }
    }
  ],
  "flushers": [
    {
      "type": "flusher_stdout",
      "detail": {
        "OnlyStdout": true
      }
    }
  ]
}

```

## CGO mode operation

For ilogtail-GO (this code repository), it can run standalone or work together with ilogtail-C to
make [Logtail AlibabaCloud](https://help.aliyun.com/document_detail/28979.html). If you are
using [Logtail AlibabaCloud](https://help.aliyun.com/document_detail/28979.html), the repository can be compiled as an
so library to work on the AlibabaCloud. And you cloud use
the [SLS observable platform](https://www.aliyun.com/product/sls) to
config the settings or view the collect data. The following will introduce how to use AlibabaCloud to run the plugin
program written by yourself.

### Run [Logtail AlibabaCloud](https://help.aliyun.com/document_detail/28979.html) on ECS

1. Run `make plugin` to compile so library, and the compiled program is `output/libPluginBase.so`.
2. Login the ECS host.
3. Execute `cd /usr/local/ilogtail/`, and stop the [Logtail AlibabaCloud] program with `README` guides.
4. Replace the compiled `libPluginBase.so` program with the compiled `libPluginBase.so` program.
5. Restart the [Logtail AlibabaCloud] program with `README` guides.

### Run [Logtail AlibabaCloud](https://help.aliyun.com/document_detail/28979.html) on container

1. Run `make docker` to compile [Logtail AlibabaCloud](https://help.aliyun.com/document_detail/28979.html) docker
   images named `aliyun/ilogtail:1.1.0`.
2. Rename `aliyun/ilogtail:1.1.0` to a custom name and push to the remotes, such
   as `registry.cn-beijing.aliyuncs.com/aliyun/ilogtail:1.1.0`.
3. Replace the mirror of `logtail-ds` of [ACK](https://www.aliyun.com/product/list/alibabacloudnative) or your self
   platform and restart.
