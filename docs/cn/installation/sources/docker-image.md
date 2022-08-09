# Docker镜像

## 制作前准备

[下载源代码](download.md)

[安装docker](https://docs.docker.com/engine/install/)

## 镜像制作

制作镜像的命令是`make docker`。制作的过程是将iLogtail发行版的tar包安装到base镜像中，因此依赖本地的ilogtail-<VERSION>.tar.gz文件，已发行的版本可以从[iLogtail发布记录](../release-notes.md)下载，本地最新代码则可以通过`make dist`生成。

制作的镜像默认名称为aliyun/ilogtail，可以DOCKER_REPOSITORY环境变量覆盖，Tag则必须与iLogtail包的版本相同，可以通过环境变量VERSION指定，例如：

```shell
$ VERSION=1.1.1 make dist
$ VERSION=1.1.1 make docker
docker image list
REPOSITORY        TAG     IMAGE ID       CREATED         SIZE
aliyun/ilogtail   1.1.1   c7977eb7dcc1   2 minutes ago   764MB
```
