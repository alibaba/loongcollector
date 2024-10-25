# Benchmark测试

## 工作原理
与e2e测试工作原理相同，详情可见：
- [e2e-test.md](./e2e-test.md)

## 测试流程

### 环境准备

目前仅支持通过docker-compose搭建测试环境，在准备开始进行benchmark测试前，您首先需要准备以下内容：

- 测试环境：Docker-Compose环境（需在本地安装docker-compose）

### 配置文件

对于每一个新的benchmark，您需要在`./test/benchmark/test_cases/<your_scenario>`目录下创建一个新的feature配置文件。每个配置文件中可以包含多个测试场景，每个测试场景由一个或多个步骤组成。

配置文件的基本框架如下：

```plain
@input
Feature: performance file to blackhole vector
  Performance file to blackhole vector

  @e2e-performance @docker-compose
  Scenario: PerformanceFileToBlackholeVector
    Given {docker-compose} environment
    Given docker-compose type {benchmark}
    When start docker-compose {performance_file_to_blackhole_vector}
    When start monitor {vector}
    When generate random json logs to file, speed {10}MB/s, total {1}min, to file {./a.log}
    When wait monitor until log processing finished
```

- `@e2e-performance @docker-compose`: 表示测试场景为e2e-performance，测试场景由本地docker-compose运行
- `Given {docker-compose} environment`: 配置启动测试环境，以docker-compose环境启动测试
- `Given docker-compose boot type {benchmark}`: 配置docker-compose启动模式，以benchmark模式启动docker-compose，`{}`中参数有两种选项，`e2e`/`benchmark`。以`e2e`模式启动会默认启动ilogtail、goc-server容器，用作e2e测试；以`benchmark`模式启动会默认启动cadvisor容器，用于监控容器运行过程中的资源占用；若在配置文件中不配置该参数，则默认以上一个scenario的启动模式启动。
- `When start docker-compose {directory}`: `{}`中参数为当前scenario的文件夹名，该行动作会读取`directory`文件夹下的docker-compose.yaml文件，通过docker-compose命令启动所有容器
- `When start monitor {vector}`: `{}`中参数为待监控的容器，该参数需要与docker-compose中的service name相同
- `When generate random logs to file`: 向文件中按照速率生成json格式测试数据，其他生成测试数据的方法请参考[e2e-test-step.md](./e2e-test-step.md)
- `When wait monitor util log processing finished`: 等待采集agent日志收集完成

### 运行测试

在所有测试内容准备完毕后，您可以直接在test目录下以go test的方式运行E2E测试。根据您的需求，可以选择运行所有测试或者指定测试。

```shell
# 运行所有benchmark测试
go test -v -timeout 30m -run ^TestE2EOnDockerComposePerformance$ github.com/alibaba/ilogtail/test/benchmark

# 以正则表达式匹配Scenario运行测试，以下为运行Scenario以Vector为结尾的test_case
go test -v -timeout 30m -run ^TestE2EOnDockerComposePerformance$/^.*Vector$ github.com/alibaba/ilogtail/test/benchmark
```

### 测试结果

- 所有统计结果将以json格式记录在`test/benchmark/report/<your_scenario>_statistic.json`中，目前记录了测试过程中CPU最大使用率、CPU平均使用率、内存最大使用率、内存平均使用率参数；所有实时结果序列将以json格式记录在`test/benchmark/report/<your_scenario>_records.json`中，目前记录了测试运行过程中的CPU使用率、内存使用率时间序列。
- 运行`scripts/benchmark_collect_result.sh`会将数据以github benchmark action所需格式汇总，会将`test/benchmark/report/*ilogtail_statistic.json`下所有结果收集并生成汇总结果到`test/benchmark/report/ilogtail_statistic_all.json`中，并将`test/benchmark/report/*records.json`汇总到`test/benchmark/report/records_all.json`