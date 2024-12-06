# 纯插件模式启动

纯插件模式为iLogtail插件开发提供轻量级测试能力，以下我们将介绍如以纯插件模式启动iLogtail。

## 本地启动

在根目录下执行 `make plugin_main` 命令，会得到 `output/ilogtail` 可执行文件，使用以下命令可以快速启动iLogtail 程序，并将日志使用控制台输出。

```shell
# 默认插件启动行为是使用metric_mock 插件mock 数据，并将数据进行日志模式打印。
 ./output/ilogtail --logger-console=true --logger-retain=false
```

## 配置

iLogtail 目前提供以下4种模式进行配置设置：

* 指定配置文件模式启动。
* 文件至文件快速测试
* iLogtail 暴露Http 端口，可以进行配置变更。
* iLogtail-C程序通过程序API进行配置变更。

### 指定配置文件模式启动

在使用独立模式编译得到 ilogtail 这个可执行程序后，你可以通过为其指定一个配置文件（不指定的话默认为当前目录下 plugin.json）来启动它。

```json
{
    "inputs": [
        {
            "type": "plugin name"
            "detail": {
                "Parameter1": "value"
                "Parameter2": 10
            }
        },
        { ... }
    ],
    "processors": [ ... ],
    "aggregators": [ ... ],
    "flushers": [ ... ]
}
```

首先，如上所示，配置文件是一个标准的 JSON 文件。其中的四个顶层键分别对应于文章中所说的四类插件：

* inputs: 输入插件，获取数据。
* processors: 处理插件，对得到的数据进行处理。
* aggregators: 聚合插件，对数据进行聚合。
* flushers: 输出插件，将数据输出到指定 sink。

从它们的类型是 array 可知，在一个配置中，每类插件都可以同时指定多项，但需要注意同时指定的多个同类插件之间的关系，除了 processors 之间是串型关系以外，其他都是并行的关系，如下图所示：&#x20;

![图片](https://sls-opensource.oss-us-west-1.aliyuncs.com/ilogtail/logtail-libPluginBase.png?versionId=CAEQMxiBgIDM6YCk6BciIDBjYmVkZjQ2Yjg5NzQwY2NhZjI4MmFmZDA2M2MwZTU2)

对于 array 中的每一个 object，它们指定了一个个具体插件，type 字段指定插件的名字，detail 字段对应的 object 指定了对于该插件的配置。

如下是一个非常简单地示例配置文件（plugin.quickstart.json）：

* inputs: 使用 metric\_mock 插件来生成 mock 数据，通过设置 detail，两个插件生成的 mock 数据将有所不同。
* processors: 使用 processor\_default 插件，该插件不对数据任何操作，只是简单地返回，保持数据的原样。
* flushers: 使用 flusher\_stdout 插件将数据输出到本地文件，类似地，我们也通过 detail 来将数据同时输出到两个文件。

```json
{
   "inputs":[
      {
         "type":"metric_mock",
         "detail":{
            "Index":0,
            "Fields":{
               "Content":"quickstart_input_1"
            }
         }
      },
      {
         "type":"metric_mock",
         "detail":{
            "Index":100000000,
            "Fields":{
               "Content":"quickstart_input_2"
            }
         }
      }
   ],
   "processors":[
      {
         "type":"processor_default"
      }
   ],
   "flushers":[
      {
         "type":"flusher_stdout",
         "detail":{
            "FileName":"quickstart_1.stdout"
         }
      },
      {
         "type":"flusher_stdout",
         "detail":{
            "FileName":"quickstart_2.stdout"
         }
      }
   ]
}
```

执行 `./output/ilogtail --plugin=plugin.quickstart.json`，在一段时间后，使用 ctrl+c 中断运行。通过查看目录，会发现生成了 quickstart\_1.stdout 和 quickstart\_2.stdout 两个文件，并且它们的内容一致。查看内容可以发现，其中的每条数据都包含 Index 和 Content 两个键，并且由于有两个输入插件，Content 会有所不同。

### 文件至文件快速测试

可以使用如下指令，从文件输入数据并输出到文件，快速进行配置测试

```shell
./output/ilogtail --plugin=plugin.json --file-io=true
```

在测试前，需要创建上文所说的json格式的配置文件。与上文不同的是，这里的配置文件不需要配置inputs和flushers（如果配置，inputs会失效，flushers会保留）。当file-io开关被打开时，会自动指定为文件输入，并输出到文件。默认的输入文件是input.log，默认的输出文件是output.log，也可以设置input-file和output-file参数来修改输入和输出文件。

需要说明的是，读取的每行数据的fieldKey默认都是content，可以通过input-field参数来修改Field名称。输入文件并不是全部读入，而是默认从头读取1000行，可以通过修改input-line-limit参数读取需要的行数。

下面是一个简单的例子，创建一个采集配置plugin.file2filetest.json进行json解析，输入源文件是stdin.log，输出结果到stdout.log文件。

配置文件plugin.file2filetest.json如下：

```json
{
    "processors": [
        {
            "detail": {
                "SourceKey": "content",
                "KeepSource": false,
                "ExpandDepth": 1,
                "ExpandConnector": ""
            },
            "type": "processor_json"
        }
    ]
}
```

stdin.log内容如下：

```text
{"inputs":[{"type":"metric_mock","detail":{"Tags":{"tag1":"aaaa","tag2":"bbb"},"Fields":{"content":"xxxxx","time":"2017.09.12 20:55:36"}}}],"flushers":[{"type":"flusher_stdout"}]}
```

执行如下命令：

```shell
./output/ilogtail --plugin=plugin.file2filetest.json --file-io=true --input-file=stdin.log --output-file=stdout.log
```

可以发现生成了一个stdout.log文件，内容如下：

```text
2023-05-23 11:33:48 {"inputs":"[{\"type\":\"metric_mock\",\"detail\":{\"Tags\":{\"tag1\":\"aaaa\",\"tag2\":\"bbb\"},\"Fields\":{\"content\":\"xxxxx\",\"time\":\"2017.09.12 20:55:36\"}}}]","flushers":"[{\"type\":\"flusher_stdout\"}]","__time__":"1684841628"}
```

### HTTP API 配置变更

当iLogtail 以独立模式运行时，可以使用HTTP API 进行配置文件变更。

* 端口： iLogtail 独立运行时，默认启动18689 端口进行监听配置输入。
* 接口：/loadconfig

接下来我们将使用HTTP 模式重新进行动态加载**指定配置文件模式启动**篇幅中的静态配置案例。

1. 首先我们启动 iLogtail 程序： `./output/ilogtail`
2. 使用以下命令进行配置重新加载。

    ```shell
    curl 127.0.0.1:18689/loadconfig -X POST -d '[{"project":"e2e-test-project","logstore":"e2e-test-logstore","config_name":"test-case_0","logstore_key":1,"json_str":"{\"inputs\":[{\"type\":\"metric_mock\",\"detail\":{\"Index\":0,\"
    Fields\":{\"Content\":\"quickstart_input_1\"}}},{\"type\":\"metric_mock\",\"detail\":{\"Index\":100000000,\"Fields\":{\"Content\":\"quickstart_input_2\"}}}],\"processors\":[{\"type\":\"processor_default\"}],\"flushers\":[{\"type\":\"flusher_stdout\",\"detail\":{\"FileName\":\"quickstart_1.stdout\"}},{\"type\":\"flusher_stdout\",\"detail\":{\"FileName\":\"quickstart_2.stdout\"}}]}\n"}]'
    ```

3. 查看日志观察到配置变更。

    ```log
    2021-11-15 14:38:42 [INF] [logstore_config.go:113] [Start] [logtail_alarm,logtail_alarm]        config start:begin      
    2021-11-15 14:38:42 [INF] [logstore_config.go:151] [Start] [logtail_alarm,logtail_alarm]        config start:success    
    2021-11-15 14:38:42 [INF] [logstore_config.go:113] [Start] [test-case_0,e2e-test-logstore]      config start:begin      
    2021-11-15 14:38:42 [INF] [logstore_config.go:151] [Start] [test-case_0,e2e-test-logstore]      config start:success
    ```

4. 通过查看目录，会发现行为与上述静态配置方式一致，生成了 quickstart\_1.stdout 和 quickstart\_2.stdout 两个文件，并且它们的内容一致。

### C API 配置变更

以C-shared模式编译，与C程序结合使用，对外开放API参考 [plugin\_export.go](https://github.com/alibaba/loongcollector/blob/main/plugin\_main/plugin\_export.go)。
