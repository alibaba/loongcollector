# ilogtail守护进程

当ilogtail因为各种原因异常退出，需要有一个守护来保证ilogtail进程的可用性。

## 安装ilogtail

参考快速开始，下载并安装ilogtail：
[快速开始](quick-start.md)

## linux环境

1. 执行生成systemd文件

其中ExecStart中路径根据ilogtail安装路径自行修改。

```
cat>/etc/systemd/system/ilogtaild.service<<EOF
[Unit]
Description=ilogtail

[Service]
Type=simple
User=root
Restart=always
RestartSec=60
ExecStart=/usr/local/ilogtail/ilogtail

[Install]
WantedBy=multi-user.target
EOF
```

2. 注册服务
```
systemctl daemon-reload
```

3. 启动服务
```
systemctl restart ilogtaild
```

4. 查看服务状态
```
systemctl status ilogtaild
```

在服务启动后，可以在ilogtail-<version>目录下查看ilogtail.LOG。

5. 停止服务
```
systemctl stop ilogtaild
```