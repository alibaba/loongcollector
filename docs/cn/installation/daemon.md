# LoongCollector 守护进程

当 LoongCollector 因为各种原因异常退出，需要有一个守护来保证 LoongCollector 进程的可用性。

## 安装 LoongCollector

参考快速开始，下载并安装 LoongCollector：
[快速开始](quick-start.md)

## linux环境

1. 执行生成systemd文件

    其中`ExecStart`中路径根据 LoongCollector 安装路径自行修改。

    ```text
    cat>/etc/systemd/system/loongcollectord.service<<EOF
    [Unit]
    Description=loongcollector

    [Service]
    Type=simple
    User=root
    Restart=always
    RestartSec=60
    ExecStart=/usr/local/loongcollector/loongcollector

    [Install]
    WantedBy=multi-user.target
    EOF
    ```

2. 注册服务

    ```bash
    systemctl daemon-reload
    ```

3. 启动服务

    ```bash
    systemctl restart loongcollectord
    ```

4. 查看服务状态

    ```bash
    systemctl status loongcollectord
    ```

    在服务启动后，可以在`loongcollector/log`目录下查看loongcollector.LOG。

5. 停止服务

    ```bash
    systemctl stop loongcollectord
    ```
