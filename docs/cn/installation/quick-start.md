# 快速开始

## 采集主机日志

1. 下载预编译的iLogtail包，解压后进入目录，该目录下文均称为部署目录。

    ```bash
    wget https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/latest/ilogtail-latest.linux-amd64.tar.gz
    tar -xzvf ilogtail-latest.linux-amd64.tar.gz
    cd ilogtail-<version>
    ```

2. 对iLogtail进行配置

    部署目录中`ilogtail_config.json`是iLogtail的系统参数配置文件，`config/local`是iLogtail的本地采集配置目录。

    这里我们在采集配置目录中创建`file_simple.yaml`文件，配置采集当前目录simple.log文件并输出到标准输出：

    ```yaml
    enable: true
    inputs:
      - Type: input_file          # 文件输入类型
        FilePaths: 
          - ./simple.log
    flushers:
      - Type: flusher_stdout    # 标准输出流输出类型
        OnlyStdout: true
    ```

    您也可以直接从下面的地址下载示例配置。

    ```bash
    cd ./config/local
    wget https://raw.githubusercontent.com/alibaba/ilogtail/main/example_config/quick_start/config/file_simple.yaml
    cd -
    ```

3. 后台启动iLogtail

    ```bash
    nohup ./ilogtail > stdout.log 2> stderr.log &
    ```

    以上命令将标准输出重定向到stdout.log以便观察。

4. 构造示例日志

    ```bash
    echo 'Hello, iLogtail!' >> simple.log
    ```

5. 查看采集到的文件日志

    ```bash
    cat stdout.log
    ```

    结果为

    ```json
    2022-07-15 00:20:29 {"__tag__:__path__":"./simple.log","content":"Hello, iLogtail!","__time__":"1657815627"}
    ```

## 更多采集配置

了解采集配置结构：[采集配置](../configuration/collection-config.md)

参考更多可组合的日志采集和处理配置样例：<https://github.com/alibaba/ilogtail/blob/main/example_config>
