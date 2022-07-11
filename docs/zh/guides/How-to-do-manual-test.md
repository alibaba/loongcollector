# 如何进行手工测试

iLogtail作为[Logtail AlibabaCloud](https://help.aliyun.com/document_detail/28979.html)
的插件部分，可以独立运行，也可以与C程序协同工作，对于大多数情况的验证独立模式便足够了，如果你熟悉 [Logtail AlibabaCloud](https://help.aliyun.com/document_detail/28979.html)
，你也可以使用cgo 模式在Alibaba Cloud 上进行验证。

## 独立模式运行

独立模式运行步骤仅仅以下3个步骤：

1. 运行*make plugin_main* 进行编译，编译后的程序将位于*output/ilogtail*。
2. 准备测试 demo.json 文件。
3. 运行 *./output/ilogtail --plugin=./demo.json* 启动程序，

以下内容将详细介绍 input 与 processor 插件如何进行本地调试。

### input 插件

对于input 插件来说，大多情况可能存在外部依赖，所以我们更推荐使用 [E2E](../../../test/README.md)
进行运行，比如 [E2E nginx](../../../test/case/behavior/input_nginx)，当然如果你本地准备好了相关依赖环境，也本地进行调试。 以下的例子展示了采集nginx
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

```

## CGO 模式运行

对于 iLogtail作为-GO(本代码库) 来说，既可以独立运行，也可以与 iLogtail作为-C
程序协同工作，共同组成[Logtail AlibabaCloud](https://help.aliyun.com/document_detail/28979.html)
，如果你已经在使用[Logtail AlibabaCloud](https://help.aliyun.com/document_detail/28979.html) ，你也可以将本代码库编译成so
library，直接使用 [SLS observable platform](https://www.aliyun.com/product/sls)
控制台进行配置设置，使用控制台查看采集数据。以下将分别介绍2种情况下如何使用AlibabaCloud 运行你自己编写的插件程序。

### 基于ECS 主机运行 [Logtail AlibabaCloud](https://help.aliyun.com/document_detail/28979.html)
1. 运行 `make plugin` 进行so library 编译，编译后程序为 `output/libPluginBase.so`。
2. 登录ECS主机。
3. 执行 `cd /usr/local/ilogtail/`，参考 `README` 停止 [Logtail AlibabaCloud] 程序。
4. 将编译后的`libPluginBase.so` 程序替换原有`libPluginBase.so`程序。
5. 参考`README` 重新启动[Logtail AlibabaCloud] 程序。

### 基于容器运行 [Logtail AlibabaCloud](https://help.aliyun.com/document_detail/28979.html)
1. 运行 `make docker` 进行 [Logtail AlibabaCloud](https://help.aliyun.com/document_detail/28979.html)docker 模式编译，编译后的镜像为 `aliyun/ilogtail:latest`。
2. 将`aliyun/ilogtail:latest` 重命名为镜像仓库镜像并push到远程仓库，如 `registry.cn-beijing.aliyuncs.com/aliyun/ilogtail:0.0.1`。
3. 将 [ACK](https://www.aliyun.com/product/list/alibabacloudnative)) logtail-ds 组件镜像进行替换并重启，或将自建平台镜像进行替换并重启。
