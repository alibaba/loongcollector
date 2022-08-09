# E2E测试

iLogtail提供了一个完整的E2E测试引擎，方便您快速开展各类插件的集成测试，从而进一步保证代码质量。在大部分情况下，您只需要编写一个Yaml配置文件来定义测试行为，即可轻松完成测试。如果测试流程涉及外部环境依赖，您只需额外提供该环境的镜像（或构建镜像所需文件），从而省却了人工配置测试环境（包括网络等）的麻烦。

## 测试流程

### 环境准备

在准备开始进行集成测试前，您首先需要在开发环境中准备以下内容：

- docker engine
- docker-compose v2：请确保在命令行中运行`docker-compose --version`能够正常输出结果，且测试期间非测试相关容器中不存在容器名中包含ilogtailC和goc的容器
- goc

### 目录组织

对于每一种测试场景，您都需要在./test/case/behavior目录下创建一个新的文件夹，并在文件夹中放置测试所需的文件，其中必须包含名为ilogtail-e2e.yaml的测试引擎配置文件。目前，我们已经提供了部分测试用例，您只需在此基础上进行添加即可，目录组织格式如下：

```plain
./test/case/
├── behavior
│   ├── <your_test_case_name>
│       ├── ilogtail-e2e.yaml
│       ├── docker-compose.yaml (可选，涉及外部环境依赖时需要)
│       ├── Dockerfile (可选，涉及外部环境依赖且该依赖无现成镜像时需要)
│       └── 其它测试所需文件 (如生成镜像所需的代码或脚本等)
│   └── 其它测试用例
├── core
└── performance
```

### 配置文件

Yaml配置文件的基本框架如下所示，其中`boot`模块规定了测试环境的创建方式，现阶段只支持Docker compose：

```yaml
boot:
  # 【必选】测试环境配置
  category: docker-compose # 测试环境默认由docker-compose创建
testing_interval: <string> # 【必选】单次验证的持续时间，超时将自动停止
ilogtail:
  # 【必选】iLogtail配置
trigger:
  # 【可选】触发器配置
subscriber:
  # 【可选】订阅器配置
verify:
  # 【必选】验证器配置
retry:
  # 【可选】验证重试配置
```

下面将首先介绍必选及部分可选模块的配置，其余可选配置将在下文具体涉及的场景中进行介绍。

#### iLogtail配置

iLogtail相关的配置项如下所示：

```yaml
ilogtail:
  config:
    - name: <string>        # 配置名称
      detail:               # 配置内容
        map<string, object>
  close_wait: <string>      # 测试结束后等待数据传送的时间
  env:                      # ilogtail容器的环境变量
    map<string, string>
  depends_on:               # iLogtail容器的依赖容器，格式同docker-compose文件
    map<string, object>
  mounts:                   # iLogtail容器的目录挂载情况
    array<string>
```

#### 验证器配置

验证器相关的配置项如下所示：

```yaml
verify:
  log_rules:
    # 业务日志验证配置
    - name: <string>         # 验证项名称
      validator: <string>    # 验证器名称
      spec:                  # 验证器具体配置
        map<string, object>
    - ...
  system_rules:
    # 系统日志验证配置
    - name: <string>         # 验证项名称
      validator: <string>    # 验证器名称
      spec:                  # 验证器具体配置
        map<string, object>
    - ...
retry:
  times: <int>               # 验证失败后的最大重试次数
  interval: <string>         # 重试间隔时间
```

有关可选择验证器的具体信息，可以参见[测试引擎插件概览](https://github.com/alibaba/ilogtail/blob/main/test/docs/plugin-list.md)。

### 运行测试

在所有测试内容准备完毕后，您可以直接在iLogtail的根目录下运行`TEST_SCOPE=<your_test_case_name> make e2e`。如果需要引擎输出测试的详细过程，可以在前述命令中额外增加`TEST_DEBUG=true`。

测试结束后，会在根目录下新建一个名为behavior_test的目录，其中涵盖了与测试过程有关的文件，目录组织结构如下：

```plain
./behavior_test/
├── report
│   ├── <your_test_case_name>_log
│       ├── ilogtail.LOG
│       └── logtail_plugin.LOG 
│   ├── <your_test_case_name>_coverage.out  (代码覆盖率结果)
│   ├── <your_test_case_name>_engine.log    (测试引擎日志)
│   └── <your_test_case_name>_report.json   (测试报告)
├── ilogtail-test-tool
└── plugin_logger.xml
```

## 测试模式

根据待测试插件类型的不同，测试引擎提供了不同的组件方便您生成所需的日志或验证日志内容，下面将对各类型插件的典型测试方法进行介绍。

### 输入插件测试

对于输入插件而言，绝大多数情况都存在外部依赖，因此必须进行集成测试。为此，您需要提供日志生成单元（即输入源）的镜像（如mysql和nginx等），并将其定义在docker-compose.yaml中。如果该依赖没有现成的镜像，您需要额外提供构建该镜像的Dockerfile（及用于构建镜像的其他文件）。

如果您所需的日志是需要对日志生成单元进行触发后才能得到（如nginx的运行日志），E2E测试引擎为您提供了触发器组件，该组件会定时向目的地之发送http请求，您可以在引擎配置文件中配置该组件的相关内容。

在该场景下，iLogtail默认使用grpc协议将从输入接收到的数据直接发送至日志订阅器，然后订阅器再将数据发送至验证器进行验证，整体测试架构如下图所示：
<div align="center">
    <img src="https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/test-framework/inputs-test-framework.jpg" width = "450"/>
</div>

其中，iLogtail容器的内部结构如下图所示：
<div align="center">
    <img src="https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/test-framework/inputs-ilogtail.jpg" width = "400"/>
</div>

输入插件测试的典型配置示例如下：

```yaml
boot:
  category: docker-compose
testing_interval: <string>
ilogtail:
  config:
    - name: <string>
      detail:
        - inputs:
            - Type: <string>    # 待测试的输入插件
              # 输入插件的具体配置
trigger:
  url: <string>                 # http请求目标地址
  method: <string>              # http请求方法
  interval: <string>            # http请求间隔
  times: <int>                  # http请求次数
verify:
  log_rules:
    - name: <string>
      validator: <string>
      spec: 
        map<string, object>
    - ...
  system_rules:
    - name: <string>
      validator: <string>
      spec: 
        map<string, object>
    - ...
retry:
  times: <int>
  interval: <string>
```

目前，我们已经提供了大量输入插件的测试用例，详细信息可参见[行为测试](https://github.com/alibaba/ilogtail/tree/main/test/case/behavior)文件夹中以input_开头的样例。

### 处理插件测试

由于不需要外部依赖，处理插件的集成测试并不是必须的，但也推荐您完成该项测试。为了完成测试，iLogtail为您提供了数据模拟输入插件`metric_mock`和`service_mock`，利用该插件可以直接在ilogtail中生成你所需的数据而无需外部输入。

同输入插件测试场景一样，在该场景下，iLogtail默认使用grpc协议将从输入接收到的数据直接发送至日志订阅器，然后订阅器再将数据发送至验证器进行验证，整体测试架构如下图所示：
<div align="center">
    <img src="https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/test-framework/processors-test-framework.jpg" width = "200"/>
</div>

其中，iLogtail容器的内部结构如下图所示：
<div align="center">
    <img src="https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/test-framework/processors-ilogtail.jpg" width = "500"/>
</div>

处理插件测试的典型配置示例如下（以`metric_mock`输入插件为例)：

```yaml
boot:
  category: docker-compose
testing_interval: <string>
ilogtail:
  config:
    - name: <string>
      detail:
        - inputs:
            - Type: metric_mock
              IntervalMs: <int>
              Tags:
                map<string, string>
              Fields:
                map<string, string>
          processors:
            - Type: <string>  # 待测试的处理插件
              # 处理插件的具体配置
verify:
  log_rules:
    - name: <string>
      validator: <string>
      spec: 
        map<string, object>
    - ...
  system_rules:
    - name: <string>
      validator: <string>
      spec: 
        map<string, object>
    - ...
retry:
  times: <int>
  interval: <string>
```

### 输出插件测试

对于输出插件而言，绝大多数情况都存在外部依赖，因此必须进行集成测试。为此，您需要提供日志存储单元（即iLogtail写出的目的地）的镜像（如kafka等），并将其定义在docker-compose.yaml中。如果该依赖没有现成的镜像，您需要额外提供构建该镜像的Dockerfile（及用于构建镜像的其他文件）。

为了完成测试，iLogtail为您提供了数据模拟输入插件`metric_mock`和`service_mock`，利用该插件可以直接在ilogtail中生成你所需的数据而无需外部输入。
对于输出部分，由于iLogtail将数据直接写入了目标存储单元，为了验证数据的正确性，必须在测试引擎中拉取已写入的这些数据。因此，您必须为您的输出插件编写一个对应的订阅器Subscriber，用于拉取日志存储单元中iLogtail写入的数据，从而能够进行下一步的验证。有关编写自定义订阅器的详细方法，参见[如何编写自定义订阅器](./How-to-add-subscriber.md)。

输出插件的测试架构如下图所示：
<div align="center">
    <img src="https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/test-framework/flushers-test-framework.jpg" width = "450"/>
</div>

其中，iLogtail容器的内部结构如下图所示：
<div align="center">
    <img src="https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/test-framework/flushers-ilogtail.jpg" width = "400"/>
</div>

输出插件测试的典型配置示例如下：

```yaml
boot:
  category: docker-compose
testing_interval: <string>
ilogtail:
  config:
    - name: <string>
      detail:
        - inputs:
            - Type: metric_mock
              IntervalMs: <int>
              Tags:
                map<string, string>
              Fields:
                map<string, string>
          outputs:
            - Type: <string>   # 待测试的输出插件
              # 输出插件的具体配置
subscriber:
  name: <string>         # 自定义订阅器的名称
  config: <object>       # 自定义订阅器的具体配置
verify:
  log_rules:
    - name: <string>
      validator: <string>
      spec: 
        map<string, object>
    - ...
  system_rules:
    - name: <string>
      validator: <string>
      spec: 
        map<string, object>
    - ...
retry:
  times: <int>
  interval: <string>
```