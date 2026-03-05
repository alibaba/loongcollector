# input_cpu_profiling 插件

## 简介

`input_cpu_profiling`插件可以实现利用ebpf探针采集进程CPU性能剖析数据。

## 版本

[Beta](../../stability-level.md)

## 版本说明

* 推荐版本：LoongCollector v3.3.4 及以上

## 配置参数

* 表1：基本选项

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为input\_cpu\_profiling  |
|  CollectIntervalMs  |  int  |  否  |  1000  |  Profiling 数据采集间隔，间隔内的数据将会进行聚合，若存在多个配置，则取最小值作为采集间隔  |
|  CommandLines  |  []string  |  否  |  空  |  一组正则表达式，筛选命令行匹配的进程以进行性能剖析数据采集，若不配置，则视为采集所有进程  |
|  AppName  |  string  |  否  |  空  |  采集结果名称，只影响日志labels.\_\_name\_\_字段  |
|  Language  |  string  |  否  |  java  |  采集语言名称，只影响日志language字段  |

* 表2：容器过滤选项

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  K8sNamespaceRegex  |  string  |  否  |  空  |  对于部署于K8s环境的容器，指定待采集容器所在Pod所属的命名空间条件。如果未添加该参数，则表示采集所有容器。支持正则匹配。  |
|  K8sPodRegex  |  string  |  否  |  空  |  对于部署于K8s环境的容器，指定待采集容器所在Pod的名称条件。如果未添加该参数，则表示采集所有容器。支持正则匹配。  |
|  IncludeK8sLabel  |  map[string]string  |  否  |  空  |  对于部署于K8s环境的容器，指定待采集容器所在pod的标签条件。多个条件之间为“或”的关系，如果未添加该参数，则表示采集所有容器。支持正则匹配。 map中的key为Pod标签名，value为Pod标签的值，说明如下：<ul><li>如果map中的value为空，则pod标签中包含以key为键的pod都会被匹配；</li><li>如果map中的value不为空，则：<ul></li><li>若value以`^`开头并且以`$`结尾，则当pod标签中存在以key为标签名且对应标签值能正则匹配value的情况时，相应的pod会被匹配；</li><li>其他情况下，当pod标签中存在以key为标签名且以value为标签值的情况时，相应的pod会被匹配。</li></ul></ul>       |
|  ExcludeK8sLabel  |  map[string]string  |  否  |  空  |  对于部署于K8s环境的容器，指定需要排除采集容器所在pod的标签条件。多个条件之间为“或”的关系，如果未添加该参数，则表示采集所有容器。支持正则匹配。 map中的key为pod标签名，value为pod标签的值，说明如下：<ul><li>如果map中的value为空，则pod标签中包含以key为键的pod都会被匹配；</li><li>如果map中的value不为空，则：<ul></li><li>若value以`^`开头并且以`$`结尾，则当pod标签中存在以key为标签名且对应标签值能正则匹配value的情况时，相应的pod会被匹配；</li><li>其他情况下，当pod标签中存在以key为标签名且以value为标签值的情况时，相应的pod会被匹配。</li></ul></ul>       |
|  K8sContainerRegex  |  string  |  否  |  空  |  对于部署于K8s环境的容器，指定待采集容器的名称条件。如果未添加该参数，则表示采集所有容器。支持正则匹配。  |
|  IncludeEnv  |  map[string]string  |  否  |  空  |  指定待采集容器的环境变量条件。多个条件之间为“或”的关系，如果未添加该参数，则表示采集所有容器。支持正则匹配。 map中的key为环境变量名，value为环境变量的值，说明如下：<ul><li>如果map中的value为空，则容器环境变量中包含以key为键的容器都会被匹配；</li><li>如果map中的value不为空，则：<ul></li><li>若value以`^`开头并且以`$`结尾，则当容器环境变量中存在以key为环境变量名且对应环境变量值能正则匹配value的情况时，相应的容器会被匹配；</li><li>其他情况下，当容器环境变量中存在以key为环境变量名且以value为环境变量值的情况时，相应的容器会被匹配。</li></ul></ul>       |
|  ExcludeEnv  |  map[string]string  |  否  |  空  |  指定需要排除采集容器的环境变量条件。多个条件之间为“或”的关系，如果未添加该参数，则表示采集所有容器。支持正则匹配。 map中的key为环境变量名，value为环境变量的值，说明如下：<ul><li>如果map中的value为空，则容器环境变量中包含以key为键的容器都会被匹配；</li><li>如果map中的value不为空，则：<ul></li><li>若value以`^`开头并且以`$`结尾，则当容器环境变量中存在以key为环境变量名且对应环境变量值能正则匹配value的情况时，相应的容器会被匹配；</li><li>其他情况下，当容器环境变量中存在以key为环境变量名且以value为环境变量值的情况时，相应的容器会被匹配。</li></ul></ul>       |
|  IncludeContainerLabel  |  map[string]string  |  否  |  空  |  指定待采集容器的标签条件。多个条件之间为“或”的关系，如果未添加该参数，则默认为空，表示采集所有容器。支持正则匹配。 map中的key为容器标签名，value为容器标签的值，说明如下：<ul><li>如果map中的value为空，则容器标签中包含以key为键的容器都会被匹配；</li><li>如果map中的value不为空，则：<ul></li><li>若value以`^`开头并且以`$`结尾，则当容器标签中存在以key为标签名且对应标签值能正则匹配value的情况时，相应的容器会被匹配；</li><li>其他情况下，当容器标签中存在以key为标签名且以value为标签值的情况时，相应的容器会被匹配。</li></ul></ul>       |
|  ExcludeContainerLabel  |  map[string]string  |  否  |  空  |  指定需要排除采集容器的标签条件。多个条件之间为“或”的关系，如果未添加该参数，则默认为空，表示采集所有容器。支持正则匹配。 map中的key为容器标签名，value为容器标签的值，说明如下：<ul><li>如果map中的value为空，则容器标签中包含以key为键的容器都会被匹配；</li><li>如果map中的value不为空，则：<ul></li><li>若value以`^`开头并且以`$`结尾，则当容器标签中存在以key为标签名且对应标签值能正则匹配value的情况时，相应的容器会被匹配；</li><li>其他情况下，当容器标签中存在以key为标签名且以value为标签值的情况时，相应的容器会被匹配。</li></ul></ul>       |

## 样例

### 采集进程CPU性能剖析数据。

* 输入

```shell
cat > fibo.c << 'EOF'
#include <stdio.h>
int fibo(int n) {
    if (n <= 1)
        return n;
    return fibo(n - 1) + fibo(n - 2);
}
int main() {
    int n = 42;
    int result = fibo(n);
    printf("fibo(%d) = %d\n", n, result);
    return 0;
}
EOF
gcc -o fibo fibo.c
./fibo &
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_cpu_profiling
    CommandLines:
      - ".*fibo.*"
    AppName: "fibo_app"
    Language: "C"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```

* 输出

```json
{
    "__time__": 1772699244,
    "profileID": "22FC6238-186D-11F1-9929-FF1B98ADDDD9",
    "dataType": "CallStack",
    "language": "C",
    "name": "fibo",
    "stack": "fibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nfibo\nmain\n__libc_start_main\n_start",
    "stackID": "5939135DE07BB9AA",
    "type": "profile_cpu",
    "type_cn": "",
    "units": "nanoseconds",
    "val": "50000000",
    "valueTypes": "cpu",
    "valueTypes_cn": "",
    "labels.__name__": "fibo_app",
    "labels.thread": "fibo",
}
```