# E2E测试

## 独立模式运行

独立模式运行步骤仅仅以下3个步骤：

1. 运行*make plugin_main* 进行编译，编译后的程序将位于*output/ilogtail*。
2. 准备测试 demo.json 文件。
3. 运行 *./output/ilogtail --plugin=./demo.json* 启动程序，

以下内容将详细介绍 input 与 processor 插件如何进行本地调试。

### input 插件

对于input 插件来说，大多情况可能存在外部依赖，所以我们更推荐使用 [E2E](https://github.com/alibaba/ilogtail/blob/main/test/README.md)
进行运行，比如 [E2E nginx](https://github.com/alibaba/ilogtail/blob/main/test/case/behavior/input_nginx)，当然如果你本地准备好了相关依赖环境，也本地进行调试。 以下的例子展示了采集nginx
状态的输入型插件配置文件，IntervalMs表示采集的频率为1s一次。你可以直接在项目根路径运行 *TEST_SCOPE=input_nginx TEST_DEBUG=true make e2e*
命令使用E2E测试插件运行效果。对于本地启动后，采集的输出保存于*logtail_plugin.LOG*文件，当然你也可以使用
*./output/ilogtail --plugin=./demo.json --logger-console=true --logger-retain=false* 启动，采集信息将直接输出与控制台。

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

### processor 插件

对于 processor 插件来说，可以使用 metric_mock或service_mock 模拟采集输入，以下例子为验证测试 processor_add_fields 插件功能， metric_mock 模拟采集输入数据
Key/Value 为1111：2222 的数据，通过processor_add_fields 插件，最终输出含有3个字段的输出。对于本地启动后，采集的输出保存于*logtail_plugin.LOG*文件，当然你也可以使用
*./output/ilogtail --plugin=./demo.json --logger-console=true --logger-retain=false* 启动，采集信息将直接输出与控制台。

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
