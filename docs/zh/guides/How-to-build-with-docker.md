# 如何编译镜像?
当前仓库只含有 iLogtail 的Go 部分代码，C++ 部分代码将在2022 年开源。但是我们目前仍然提供了很多方式来编译不同类型的 iLogtail 可执行文件，例如 纯Go 部分的镜像，完成镜像（包含GO 与C++），已经CGO 模式的动态链接库等。

## 编译CGO 模式的动态链接库
如果你在进行与C++ 部分一起的本地测试并，可以使用以下命令来编译CGO 模式的动态链接库。产物为 `bin/libPluginBase.so`。

```shell
make solib
```

## 编译完整 iLogtail 镜像
`注意：当前方式只会替换[已有完整镜像](../../../docker/Dockerfile_whole)的libPluginBase.so 文件，如果当前C++ 文件同样有修改，请尝试其他方式。`
### 编译镜像
目前有三个选项来控制编译行为，分别是DOCKER_REPOSITORY，VERSION 以及 DOCKER_PUSH。
- DOCKER_REPOSITORY 默认值为 `aliyun/ilogtail`，控制镜像名称。
- VERSION 默认值为 `1.1.0`， 控制镜像版本。
- DOCKER_PUSH 默认值为 `false`， 控制编译后是否进行远程仓库push 操作。

```shell
    DOCKER_PUSH={DOCKER_PUSH} DOCKER_REPOSITORY={DOCKER_REPOSITORY} VERSION={VERSION} make wholedocker
 ```
所以当我们执行`make wholedocker` 命令时，会产生一个名为`aliyun/ilogtail:1.1.0` 的镜像存在于本地仓库。

## 编译纯GO iLogtail 镜像
如果你使用的功能仅仅存在于GO 部分，例如标准输出流日志采集以及Metrics input 插件等，你可以选择仅仅使用纯GO 镜像。
### 编译镜像
目前有三个选项来控制编译行为，分别是DOCKER_REPOSITORY，VERSION 以及 DOCKER_PUSH。
- DOCKER_REPOSITORY 默认值为 `aliyun/ilogtail`，控制镜像名称。
- VERSION 默认值为 `1.1.0`， 控制镜像版本。
- DOCKER_PUSH 默认值为 `false`， 控制编译后是否进行远程仓库push 操作。

```shell
        DOCKER_PUSH={DOCKER_PUSH} DOCKER_REPOSITORY={DOCKER_REPOSITORY} VERSION={VERSION} make docker
```
所以当我们执行`make docker` 命令时，会产生一个名为`aliyun/ilogtail:1.1.0` 的纯GO版本镜像存在于本地仓库。
