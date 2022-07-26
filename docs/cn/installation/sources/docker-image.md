# Docker镜像

## 制作前准备

[下载源代码](download.md)

[安装docker](https://docs.docker.com/engine/install/)

## 镜像制作

iLogtail镜像制作使用两阶段编译，第一阶段同编译，第二阶段将iLogtail的构建产出copy到干净的基础镜像中，以缩减镜像大小。

以下命令可以制作出iLogtail的生产镜像，默认的本地镜像名称遵循`aliyun/ilogtail-<release version>`的规则。

1\. 进入源代码顶层目录。

2\. 执行命令`make dist`。

3\. 执行命令`make docker`。
