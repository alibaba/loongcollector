# 开发环境

虽然[源代码编译](../installation/sources/build.md)已经提供了方便的 LoongCollector 编译方法，但却不适合开发场景。因为开发过程中需要不断进行编译调试，重复全量编译的速度太慢，因此需要构建支持增量编译开发环境。

## 进程结构

LoongCollector 为了支持插件系统，引入了 libPluginAdaptor 和 libPluginBase（以下简称 adaptor 和 base）这两个动态库，它们与 LoongCollector 之间的关系如下：<br />
LoongCollector 动态依赖于这两个动态库（即 binary 中不依赖），在初始化时，LoongCollector 会尝试使用动态库接口（如 dlopen）动态加载它们，获取所需的符号。<br />
Adaptor 充当一个中间层，LoongCollector 和 base 均依赖它，LoongCollector 向 adaptor 注册回调，adpator 将这些回调记录下来以接口的形式暴露给 base 使用。<br />
Base 是插件系统的主体，它包含插件系统所必须的采集、处理、聚合以及输出（向 LoongCollector 递交可以视为其中一种）等功能。<br />
因此，完整的 LoongCollector 包含loongcollector 、libPluginAdaptor.so 和 libPluginBase.so 3个二进制文件。

![image.png](https://sls-opensource.oss-us-west-1.aliyuncs.com/ilogtail/ilogtail-adapter-cgo.png)

## 目录结构 <a name="iKc61"></a>

LoongCollector 的大致目录结构如下：

```shell
.
├── core                  # C++核心代码
│   ├── CMakeLists.txt    # C++项目描述文件
│   └── ilogtail.cpp      # C++主函数
├── plugins               # Go插件代码
├── go.mod                # Go项目描述文件
├── docker                # 辅助编译的镜像描述目录
├── scripts               # 辅助编译的脚本目录
└── Makefile              # 编译描述文件
```

core目录包含了 LoongCollector C++核心代码，ilogtail.cpp是其主函数入口文件。C++项目使用CMake描述，CMakeLists.txt是总入口，各子目录中还有CMakeLists.txt描述子目录下的编译目标。

顶层目录.本身就是一个Go项目，该项目为 LoongCollector 插件，go.mod为其描述文件。插件代码主体在plugins目录。

docker目录和scripts目录分别为辅助编译的镜像描述目录和脚本目录。Makefile为整个 LoongCollector 的编译描述文件，对编译命令进行了封装。

## 开发镜像

LoongCollector 依赖了诸多第三方库（详见[编译依赖](../installation/sources/dependencies.md)

LoongCollector 提供了预编译依赖的镜像辅助编译。开发镜像的地址在`sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/loongcollector-community-edition/loongcollector-build-linux`，可通过下面命令获取。

```shell
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/loongcollector-community-edition/loongcollector-build-linux
```

开发镜像预先安装了gcc和go编译器，更新了git，为了代码风格统一安装了clang-format、go-tools，为了便于调试也安装了gdb、go-delve等工具。为方便国内开发者，预先设置了GOPROXY="[https://goproxy.cn,direct"](https://goproxy.cn,direct")。

```shell
$ gcc --version
gcc (GCC) 9.3.1 20200408 (Red Hat 9.3.1-2)
$ go version
go version go1.16.15 linux/amd64
$ git --version
git version 2.29.3
```

C++核心的编译依赖在/opt/logtail/deps/目录下，该路径是CMakeLists.txt的DEFAULT_DEPS_ROOT（可以查看./core/dependencies.cmake找到修改路径的变量）。

```shell
$ ls /opt/logtail/deps/
bin  include  lib  lib64  share  ssl
```

如果需要安装更多工具或编译依赖，可以在开发镜像上镜像叠加，制作自定义的开发镜像。

```dockerfile
from sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/loongcollector-community-edition/loongcollector-build-linux
yum -y install ...
go install ...
```

## 使用VS Code构建开发环境

[VS Code](https://code.visualstudio.com/)通过[Remote Development](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.vscode-remote-extensionpack)插件可以实现远程开发、在镜像中开发，甚至远程+镜像中开发，在镜像中开发的功能使得编译环境在不同部署间都能保持统一。由于VS Code免费而功能强大，因此我们选用VS Code来为 LoongCollector 创建一致的、可移植的开发环境。

### 1. 安装插件  <a name="zpMpx"></a>

在VS Code的Marketplace中搜索“Remote Development”安装插件。<br />![image.png](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/developer-guide/development-environment/vscode-remote-development-plugin.png)

### 2. 创建镜像开发环境配置  <a name="S3QyX"></a>

在 LoongCollector 代码库的顶层目录创建`.devcontainer`目录，并在里面创建`devcontainer.json`文件，文件的内容如下：

```json
{
  "image": "sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/loongcollector-community-edition/loongcollector-build-linux:2.0.5",
  "customizations": {
    "vscode": {
      "extensions": [
        "golang.Go",
        "ms-vscode.cpptools-extension-pack",
        "DavidAnson.vscode-markdownlint",
        "redhat.vscode-yaml"
      ]
    }
  }
}
```

其中，image指定了 LoongCollector 的开发镜像地址，customizations.vscode.extensions指定了开发环境的插件。部分插件介绍如下，开发者也可以按照自己的习惯进行修改，[欢迎讨论](https://github.com/alibaba/loongcollector/discussions/299)。

| **插件名** | **用途** |
| --- | --- |
| golang.Go | Go开发必备插件 |
| ms-vscode.cpptools-extension-pack | C++开发必备插件 |
| DavidAnson.vscode-markdownlint | Markdown代码风格检查 |
| redhat.vscode-yaml | YAML语言支持 |

### 3. 在容器中打开代码库  <a name="Rsqu1"></a>

使用Shift + Command + P（Mac）或Ctrl + Shift + P（Win）打开命令面板，输入`reopen`，选择`Remote-Containers: Reopen in Container`。<br />![image.png](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/developer-guide/development-environment/reopen-in-palette.png)<br />或者若出现如下图提示，则可以直接点击在容器中重新打开。<br />![image.png](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/developer-guide/development-environment/reopen-in-tip.png)<br />首次打开时会比较慢，因为要下载编译镜像并安装插件，后面再次打开时速度会很快。按照提示进行镜像Build。<br />完成上述步骤后，我们已经可以使用VS Code进行代码编辑，并在其中进行代码编译。<br />注：如果以前拉取过编译镜像，可能需要触发`Remote-Containers: Rebuild Container Without Cache`重新构建。

### 4. 在容器中进行编译  <a name="wEf4T"></a>

打开新Terminal（找不到的可以在命令面板中打Terminal，选择新开一个）<br />![image.png](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/developer-guide/development-environment/terminal.png)

- 编译Go插件

```bash
go mod tidy       # 若需要更新插件库
make plugin_local # 每次更新插件代码后从这里开始
```

![image.png](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/developer-guide/development-environment/go-compile-result.png)<br />如果只是对插件代码进行了修改，则只需要执行最后一行命令即可增量编译。

- 编译C++代码

```bash
mkdir -p core/build # 若之前没有建过
cd core/build
cmake ..            # 若增删文件，修改CMakeLists.txt后需要重新执行
make -sj$(nproc)    # 每次更新core代码后从这里开始
```

![image.png](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/developer-guide/development-environment/c%2B%2B-compiling.png)<br />![image.png](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/developer-guide/development-environment/c%2B%2B-compile-result.png)

如果只是对core代码进行了修改，则只需要执行最后一行命令即可增量编译。

默认的编译选项代码可能被优化，若需要Debug建议修改CMAKE_BUILD_TYPE开关。替换上述第2行为

```bash
cmake -D CMAKE_BUILD_TYPE=Debug ..
```

同理，若需要明确需要代码优化，则将上面的Debug改为Release。

默认的编译开关没有打开UT，如果需要编译UT，需要增加BUILD_LOGTAIL_UT开关。替换上述第2行为

```bash
cmake -DBUILD_LOGTAIL_UT=ON ..
```

- 制作镜像

目前不支持，没有安装docker，请直接在主机上进行编译。

### 5. 获取编译产出  <a name="X0fef"></a>

由于VS Code是直接将代码库目录挂载到镜像内的，因此主机上可以直接访问镜像内的编译产出。<br />![image.png](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/developer-guide/development-environment/artifacts-on-host.png)<br />目前，镜像使用的是root用户权限，因此在主机上可能需要执行`sudo chown -R $USER .`来修复一下权限。

可以将C++核心的构建结果拷贝到`./output`目录组装出完整的构建结果。

```bash
cp -a ./core/build/loongcollector ./output
cp -a ./core/build/go_pipeline/libPluginAdapter.so ./output
```

最终组装的`./output`目录的结构如下

```text
./output
├── loongcollector (主程序）
├── libPluginAdapter.so（插件接口）
├── libPluginBase.h
└── libPluginBase.so (插件lib）
```

## 直接使用镜像编译  <a name="VsYKL"></a>

如果不方便使用VS Code，也可以通过Docker手动挂载代码库目录的方式，启动编译镜像进入编译。

### 1. 进入源代码顶层目录

### 2. 创建编译容器，并挂载代码目录

```bash
docker run --name loongcollector-build -d \
  -v `pwd`:/src -w /src \
  sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/loongcollector-community-edition/loongcollector-build-linux:2.0.5 \
  bash -c "sleep infinity"
```

### 3. 进入容器

```bash
docker exec -it loongcollector-build bash
```

### 4. 在容器内编译

后续步骤均与VS Code的操作步骤相同。

## 使用编译产出

在主机上，编译的后的产出可以直接替换对应的文件使用。而对于容器场景，虽然在编译镜像内没有制作镜像能力，但通过下面的方法可以在不制作新镜像的情况下实现快速测试。

### 1. 修改官方镜像entrypoint

基于官方镜像包进行调试，首先用bash覆盖官方镜像的entrypoint，避免杀死 LoongCollector 后容器直接退出。

- docker：指定CMD

```bash
docker run -it --name docker_loongcollector -v /:/logtail_host:ro -v /var/run:/var/run aliyun/loongcollector:<VERSION> bash
```

- k8s：用command覆盖entrypoint

```yaml
   command:
        - sleep
        - 'infinity'
  # 删除livenessProbe，要不然会因为探测不到端口重启
```

### 2. 将自己编的二进制文件、so，替换到容器里

由于 LoongCollector 容器挂载了主机目录，因此将需要替换掉文件放到主机目录上容器内就能访问。

```bash
# 将开发机上编译的so scp到container所在node上
scp libPluginBase.so <user>@<node>:/home/<user>
```

主机的根路径在 LoongCollector 容器中位于/logtail_host，找到对应目录进行copy即可。

```bash
cp /logtail_host/home/<user>/libPluginBase.so /usr/local/loongcollector
```

## 改善vscode开发体验

### 1. 图形界面调试

我相信在有可能的情况下，没人愿意使用gdb命令界面进行调试，繁琐，不友好，因此第一步需要解决的就是使用现代图形化的调试界面调试代码。

1. 首先使用vscode打开ilogtail的项目目录；
2. 安装`devcontainer`插件（如果没有的话）；
3. 使用新的devcontainer配置文件：将项目目录下`.devcontainer`目录下的`devcontainer.json`里面的内容替换成`devcontainer.json.lldb`文件里面的内容。
4. 在开发容器里打开项目：打开`Command Palette`，输入`Reopen in container` 回车，如下图：

![1.png](https://s2.loli.net/2024/01/18/Y3WVivjBhnD1yfL.png)

![image-20240115003845492.png](https://s2.loli.net/2024/01/18/BGM17Etcyo6puKn.png)

5. `launch.json` 文件：进入`.vscode`目录下，修改`launch.json`文件输入内容如下：

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            // 这个配置的目的主要用于第一次全量编译c++ core代码
            "type": "lldb",
            "request": "launch",
            "name": "First run",
            "program": "${workspaceFolder}/output/ilogtail",
            "args": [
                "--version"
            ],
            "preLaunchTask": "c++ first build",
            "cwd": "${workspaceFolder}"
        },
        {
            // 这个配置用于后续修改相关c++ core代码之后增量编译以及调试
            "type": "lldb",
            "request": "launch",
            "name": "Debug c++",
            "program": "${workspaceFolder}/output/ilogtail",
            "args": [
                // "--ilogtail_local_config_dir=${workspaceFolder}/config"
            ],
            "preLaunchTask": "c++ build",
            "cwd": "${workspaceFolder}",
            "preRunCommands": [
                "pro hand -p true -n false -s false SIGURG"
            ],
            "env": {
                "ALIYUN_LOGTAIL_USER_DEFINED_ID": "test",
                "default_container_host_path": "/logtail_host"
            }
        },
        {
            // 这个配置用于单独调试go插件代码
            "name": "Debug go",
            "type": "go",
            "request": "launch",
            "mode": "debug",
            "preLaunchTask": "go build",
            "program": "${workspaceFolder}/plugin_main/",
            "args": [
                // "--file-io=true",
                "--plugin=${workspaceFolder}/config/json-kafka.json",
                "--input-file=${workspaceFolder}/config/input.log",
                "--output-file=${workspaceFolder}/config/output.log"
            ],
            "env": {
                "LD_LIBRARY_PATH": "${workspaceFolder}/output/"
            }
        },
    ]
}

```

6. `tasks.json`文件：同样在`.vscode`目录下，新建文件`tasks.json`，文件内容如下：

```
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "mkdir build",
            "type": "shell",
            "command": "mkdir",
            "args": [
                "-p",
                "${workspaceFolder}/core/build"
            ]
        },
        {
            "label": "mkdir output",
            "type": "shell",
            "command": "mkdir",
            "args": [
                "-p",
                "${workspaceFolder}/output"
            ]
        },
        {
            "label": "cmake",
            "type": "shell",
            "command": "cmake",
            "args": [
                "-D",
                "CMAKE_BUILD_TYPE=Debug",
                "-D",
                "CMAKE_EXPORT_COMPILE_COMMANDS=1",
                "../"
            ],
            "options": {
                "cwd": "${workspaceFolder}/core/build"
            }
        },
        {
            "label": "make",
            "type": "shell",
            "command": "make",
            "args": [
                "-j",
                "$(nproc)"
            ],
            "options": {
                "cwd": "${workspaceFolder}/core/build"
            }
        },
        {
            "label": "make plugin_local",
            "type": "shell",
            "command": "make",
            "args": [
                "plugin_local",
            ],
            "options": {
                "cwd": "${workspaceFolder}"
            }
        },
        {
            "label": "cp",
            "type": "shell",
            "command": "cp",
            "args": [
                "-a",
                "./ilogtail",
                "./go_pipeline/libPluginAdapter.so",
                "${workspaceFolder}/output"
            ],
            "options": {
                "cwd": "${workspaceFolder}/core/build"
            }
        },
        {
            "label": "cp plugin_main",
            "type": "shell",
            "command": "cp",
            "args": [
                "-a",
                "${workspaceFolder}/output/libPluginAdapter.so",
                "${workspaceFolder}/core/go_pipeline/LogtailPluginAdapter.h",
                "${workspaceFolder}/pkg/logtail/"
            ]
        },
        {
            "label": "c++ first build",
            "type": "shell",
            "dependsOrder": "sequence",
            "dependsOn": [
                "make plugin_local",
                "mkdir build",
                "cmake",
                "make",
                "cp",
                "cp plugin_main"
            ],
        },
        {
            "label": "c++ build",
            "type": "shell",
            "dependsOrder": "sequence",
            "dependsOn": [
                "make",
                "cp"
            ],
        },
        {
            "label": "go build",
            "type": "shell",
            "command": "make",
            "args": [
                "plugin_main"
            ],
            "options": {
                "cwd": "${workspaceFolder}"
            }
        }
    ]
}
```

7. 打开vscode的调试那一栏，可以看到以下内容：

![image-20240115000234066.png](https://s2.loli.net/2024/01/18/bKZNQadM2fTltH8.png)

运行第一个选项`First run`：该选项主要进行第一次C++代码编译，并复制相关文件当指定目录；

8. 运行第二个选项`Debug c++`：打开`logtail.cpp`文件并打上断点

   ![image-20240115000951727.png](https://s2.loli.net/2024/01/18/SqoIDVCnf8OrTHv.png)

接着运行`Debug c++`选项，可以看到程序将中断在断点处：

![image-20240115001306196.png](https://s2.loli.net/2024/01/18/sV9lcoH7bEtiLJm.png)

后续修改相关c++代码之后需要进行调试就只需要执行`Debug c++`选项就可以了。

> launch.json和tasks.json文件的结合会使调试过程变得非常灵活，上面我提供的launch.json和tasks.json文件只是我自己开发的时候用的，各位可以自行进行修改以满足需求。
>
> 各位也可以通过为调试相关选项配置快捷键，配合`launch.json和tasks.json`文件实现真正的一键调试，这将极大的改善开发体验。

9. `Debug go`选项就是用来调试go插件系统的，比较简单，就不多说了，可以结合[纯插件模式启动](https://ilogtail.gitbook.io/ilogtail-docs/developer-guide/plugin-development/pure-plugin-start)这篇文档和`launch.json`文件中的`Debug go`配置以及`plugin_main.go`源码文件自行领悟；我提供两份配置文件方便做测试：

```json
{
    "inputs": [
        {
            "detail": {
                "FieldName": "content",
                "InputFilePath": "/workspaces/ilogtail_src/config/input.log",
                "LineLimit": 1000
            },
            "type": "metric_debug_file"
        }
    ],
    "processors": [
        {
            "type": "processor_json",
            "detail": {
                "SourceKey": "content",
                "KeepSource": false,
                "ExpandDepth": 0,
                "IgnoreFirstConnector": true,
                "ExpandConnector": "_",
                "NestJsonKey": "message"
            }
        },
        {
            "type": "processor_add_fields",
            "detail": {
                "Fields": {
                    "config_file": "wo giao"
                }
            }
        }
    ],
    "flushers": [
        {
            "type": "flusher_kafka_v2_compatible",
            "detail": {
                "Brokers": [
                    "192.168.6.11:32245"
                ],
                "Topic": "test-topic"
            }
        }
    ]
}
```

```json
{
    "inputs": [
        {
            "detail": {
                "FieldName": "content",
                "InputFilePath": "/workspaces/ilogtail_src/config/input.log",
                "LineLimit": 1000
            },
            "type": "metric_debug_file"
        }
    ],
    "processors": [
        {
            "type": "processor_json",
            "detail": {
                "SourceKey": "content",
                "KeepSource": false,
                "ExpandDepth": 0,
                "IgnoreFirstConnector": true,
                "ExpandConnector": "",
                "NestJsonKey": "message"
            }
        },
        {
            "type": "processor_add_fields",
            "detail": {
                "Fields": {
                    "config_file": "wo giao"
                }
            }
        }
    ],
    "flushers": [
        {
            "type": "flusher_otlp",
            "detail": {
                "Logs": {
                    "Endpoint": "100.105.62.83:4317"
                }
            }
        }
    ]
}
```

### 2. 代码跳转

有了一键调试可以方便修改代码之后，下面就需要解决阅读代码的问题，毕竟为了看一个调用函数实现得靠全文搜索那效率也太低了。

#### 2.1 安装clangd

1. 当你第一次通过`Command Palette`执行`Reopen in container`完成之后，vscode应该会跳出以下提示：

![image-20240115002642430.png](https://s2.loli.net/2024/01/18/BhZapEeMVYG9ygw.png)

这时候点击`Install`安装`clangd`，该程序就是提供代码跳转的；

如果出于某些原因你没有点的话可以通过呼出`Command Palette`执行`Rstart language server`，如果没有安装`clangd`那么它会再次跳出上面的提示框，接着就可以再次点击安装了：

![image-20240115003845492.png](https://s2.loli.net/2024/01/18/BGM17Etcyo6puKn.png)

又或者这样：

![PixPin_2024-01-18_23-00-23.png](https://s2.loli.net/2024/01/18/irsLjwBogf6cauI.png)

2. 点开vscode的设置页面，定位到`clangd`插件：

![image-20240115003340641.png](https://s2.loli.net/2024/01/18/EH5AnYhKO3ZbQ9w.png)

为`clangd` 添加以上截图中的运行参数：

```json
  "clangd.arguments": [
    "--background-index",
    "-j=32",
    "--all-scopes-completion",
    "--completion-style=detailed",
    "--header-insertion=iwyu",
    "--pch-storage=memory",
    "--enable-config",
    "--pretty",
    "--clang-tidy",
    "--header-insertion-decorators",
    "--fallback-style=Webkit"
  ],
```

> 其中的的`-j`选项后面的数字是你cpu的核心数，可以根据自己的实际情况进行修改。

#### 2.2 编译指令信息文件

当你执行完前面调试选项`First run`之后，会在`ilogtail项目目录/core/build`目录下生成一个名为`compile_commands.json`的文件，该文件包含了项目中每个文件的编译指令，例如编译器参数、定义的宏、包含的目录等，这些信息是从实际的构建过程中提取的；该文件也是clangd程序提供代码跳转能力的关键。

当你有了`clangd`以及`compile_commands.json`文件之后就可以通过鼠标点击来自由的在各个函数及其实现之间跳转了，极大的提升阅读代码的体验。

实际上此时你就应该可以通过鼠标随意点击任意函数进行代码跳转了，如果不行的话请呼出`Command Palette`执行`Rstart language server`来重启`clangd`程序。当第一次启动`clangd`的时候会读取`compile_commands.json`文件进行索引：

![image-20240115005943930.png](https://s2.loli.net/2024/01/18/QbYCDSvVIAocap6.png)

索引完成之后是下面这个状态：

![image-20240115010037830.png](https://s2.loli.net/2024/01/18/4eFKldVtGRzscYM.png)

## 常见问题

### 1. 更新代码后编译错误

如果发生编译错误，如

``` shell
/src/core/common/CompressTools.cpp:18:23: fatal error: zstd/zstd.h: No such file or directory
 #include <zstd/zstd.h>
                       ^
compilation terminated.
```

请确保本地编译镜像为最新版本。可使用以下命令更新：

``` shell
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/loongcollector-community-edition/loongcollector-build-linux
```

若使用VS Code进行开发，请重新编译开发容器镜像。使用Shift + Command + P（Mac）或Ctrl + Shift + P（Win）打开命令面板，输入rebuild，选择`Dev-Containers: Rebuild Without Cache and Reopen in Container`。
