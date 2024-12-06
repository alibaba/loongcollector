# Docker使用

## 使用前提

已安装[docker](https://docs.docker.com/engine/install/)

## 采集Docker容器日志

1. 准备 LoongCollector 配置目录

    新建配置目录`config`目录，在目录中创建`file_simple.yaml`和`stdout_simple.yaml`。

    在`file_simple.yaml`中配置采集容器中的`simple.log`到标准输出。

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

    在`stdout_simple.yaml`中配置采集容器标准输出并输出到`simple.stdout`文件。

    ```yaml
    enable: true
    inputs:
      - Type: service_docker_stdout # 容器标准输出流输入类型
        Stderr: false               # 不采集标准错误流
        Stdout: true                # 采集标准输出流
    flushers:
      - Type: flusher_stdout        # 标准输出流输出类型
        FileName: simple.stdout     # 重定向文件名
    ```

    您也可以直接从下面的地址下载示例配置。

    ```bash
    mkdir config && cd config
    wget https://raw.githubusercontent.com/alibaba/loongcollector/main/example_config/start_with_docker/config/file_simple.yaml
    wget https://raw.githubusercontent.com/alibaba/loongcollector/main/example_config/start_with_docker/config/stdout_simple.yaml
    cd -
    ```

2. 启动 LoongCollector 容器，并挂载 LoongCollector 配置目录

    ```bash
    docker run -d --name docker_loongcollector \
      -v /:/logtail_host:ro \
      -v /var/run:/var/run \
      -v /var/lib/docker_loongcollector/checkpoint:/usr/local/loongcollector/data/checkpoint \
      -v `pwd`/config:/usr/local/loongcollector/conf/continuous_pipeline_config/local \
      sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/loongcollector-community-edition/loongcollector:latest
    ```

    第1行`-d`参数表示后台启动 LoongCollector 容器，`--name`指定容器名称以便引用。\
    第2行将主机`/`目录挂载到 LoongCollector 容器中，LoongCollector 依赖`logtail_host`路径采集容器日志。\
    第3行将主机`/var/run`目录挂载到 LoongCollector 容器中，LoongCollector 依赖`/var/run`目录与容器引擎通信。\
    第4行将主机目录挂载到容器中 LoongCollector 的`checkpoint`目录，使采集状态在容器重启时可恢复。\
    第5行将配置目录挂载到 LoongCollector 容器中。

3. 查看 docker_loongcollector 容器自身标准输出日志

    ```bash
    docker logs docker_loongcollector
    ```

    结果为

    ```text
    loongcollector started. pid: 11
    /usr/local/loongcollector/thirdparty dir is not existing, create done
    register fun v2 0x7ade80 0x7b4350 0x7b39f0 0x7b04c0
    load log config /usr/local/loongcollector/conf/plugin_logger.xml 
    recover stderr
    recover stdout
    ```

4. 进入iLogtail容器

    ```bash
    docker exec -it docker_loongcollector bash
    ```

5. 查看采集到的标准输出

    ```bash
    cat /usr/local/loongcollector/simple.stdout
    ```

    结果为

    ```text
    2024-12-05 08:26:39 {"content":"loongcollector started. pid: 11","_time_":"2024-12-05T08:26:30.642276065Z","_source_":"stdout","_image_name_":"sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/loongcollector-community-edition/loongcollector:0.2.0","_container_name_":"docker_loongcollector","_container_ip_":"172.17.0.7","__time__":"1733387196"}
    2024-12-05 08:26:39 {"content":"/usr/local/loongcollector/thirdparty dir is not existing, create done","_time_":"2024-12-05T08:26:30.666735624Z","_source_":"stdout","_image_name_":"sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/loongcollector-community-edition/loongcollector:0.2.0","_container_name_":"docker_loongcollector","_container_ip_":"172.17.0.7","__time__":"1733387196"}
    2024-12-05 08:26:39 {"content":"recover stdout","_time_":"2024-12-05T08:26:33.775046868Z","_source_":"stdout","_image_name_":"sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/loongcollector-community-edition/loongcollector:0.2.0","_container_name_":"docker_loongcollector","_container_ip_":"172.17.0.7","__time__":"1733387196"}
    ```

6. 构造示例日志

    ```bash
    echo 'Hello, LoongCollector!' >> ./simple.log
    ```

7. 查看采集到的容器文件日志

    跳出容器，在宿主机上执行

    ```bash
    docker logs docker_loongcollector
    ```

    结果相比第3步的结果，多了

    ```text
    2024-12-05 08:28:12 {"content":"Hello, LoongCollector!","__time__":"1733387291"}
    ```

## 采集模版

了解采集配置结构：[采集配置](../configuration/collection-config.md)

参考更多可组合的日志采集和处理配置样例：<https://github.com/alibaba/loongcollector/blob/main/example_config>
