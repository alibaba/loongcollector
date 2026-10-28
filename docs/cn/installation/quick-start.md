# 快速开始

## 采集主机日志

1. 下载预编译的 LoongCollector 包，解压后进入目录，该目录下文均称为部署目录。

    **版本号请始终以 [GitHub Releases — alibaba/loongcollector](https://github.com/alibaba/loongcollector/releases/latest) 为准**（含 `linux-amd64` / `linux-arm64` / `windows-amd64` 资产及 checksum）。下例中的 `VERSION` 仅为演示；你也可使用 Release 正文中给出的 OSS 镜像 URL（与 GitHub 资产一致）。

    ```bash
    VERSION=3.3.2
    curl -fL -o "loongcollector-${VERSION}.linux-amd64.tar.gz" \
      "https://github.com/alibaba/loongcollector/releases/download/v${VERSION}/loongcollector-${VERSION}.linux-amd64.tar.gz"
    tar -xzvf "loongcollector-${VERSION}.linux-amd64.tar.gz"
    cd "loongcollector-${VERSION}"
    ```

    若解压后的顶层目录名与上述 `cd` 不一致，请以包内实际目录名为准。

2. 对 LoongCollector 进行配置

    部署目录中`conf/instance_config/local/loongcollector_config.json`是 LoongCollector 的系统参数配置文件，`conf/continuous_pipeline_config/local`是 LoongCollector 的本地采集配置目录。 这里我们在采集配置目录中创建`file_simple.yaml`文件，配置采集当前目录`simple.log`文件并输出到标准输出：

    ```yaml
    enable: true
    inputs:
      - Type: input_file     # 文件输入类型
        FilePaths:
        - ./simple.log
    flushers:
      - Type: flusher_stdout # 标准输出流输出类型
        OnlyStdout: true
    ```

    您也可以直接从下面的地址下载示例配置。

    ```bash
    cd conf/continuous_pipeline_config/local
    wget https://raw.githubusercontent.com/alibaba/loongcollector/main/example_config/quick_start/config/file_simple.yaml
    cd -
    ```

3. 后台启动 LoongCollector

    ```bash
    nohup ./loongcollector > stdout.log 2> stderr.log &
    ```

    以上命令将标准输出重定向到 `stdout.log`以便观察。

4. 构造示例日志

    ```bash
    echo 'Hello, LoongCollector!' >> simple.log
    ```

5. 查看采集到的文件日志

    ```bash
    cat stdout.log
    ```

    结果为

    ```json
    2024-12-05 15:50:29 {"__tag__:__path__":"./simple.log","content":"Hello, LoongCollector!","__time__":"1733385029"}
    ```

## 更多采集配置

了解采集配置结构：[采集配置](../configuration/collection-config.md)

参考更多可组合的日志采集和处理配置样例：<https://github.com/alibaba/loongcollector/blob/main/example_config>
