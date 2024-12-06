# Docker镜像

## 制作前准备

[下载源代码](download.md)

[安装docker](https://docs.docker.com/engine/install/)

## 镜像制作

制作镜像分为2步：

1. 代码库根目录准备 loongcollector-\<VERSION\>.\<os-arch\>.tar.gz
   若使用已发布版本，可以从[LoongCollector 发布记录](../release-notes/release-notes.md)下载压缩包。
   若要使用本地最新代码构建，则执行如下命令`make dist`。
2. 将 LoongCollector 发行版的tar包安装到base镜像中。
   执行命令`make docker`。

制作的镜像默认名称为aliyun/loongcollector ，可以DOCKER_REPOSITORY环境变量覆盖，Tag则必须与 LoongCollector 包的版本相同，可以通过环境变量VERSION指定，例如：

```shell
$ VERSION=0.0.2 make dist
$ VERSION=0.0.2 make docker
$ docker image list
REPOSITORY              TAG     IMAGE ID       CREATED         SIZE
aliyun/loongcollector   0.0.2   c7977eb7dcc1   2 minutes ago   764MB
```
