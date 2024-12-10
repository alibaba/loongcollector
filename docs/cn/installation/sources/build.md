# 编译

## Linux版本编译

### 编译前准备

[下载源代码](download.md)

[安装docker](https://docs.docker.com/engine/install/)

### 编译目标 <a name="veSpV"></a>

Makefile描述了整个项目的所有编译目标，主要的包括：

| **目标** | **描述** |
| --- | --- |
| core | 仅编译C++核心 |
| plugin | 仅编译Go插件 |
| all | 编译完整iLogtail |
| dist | 打包发行 |
| docker | 制作iLogtail镜像 |
| plugin_local | 本地编译Go插件 |

使用`make <target>`命令编译所选目标，如果需要指定生成的版本号则在编译命令前加上VERSION环境变量，如：

```shell
VERSION=1.8.3 make dist
```

如果发生编译错误，如

``` shell
/src/core/common/CompressTools.cpp:18:23: fatal error: zstd/zstd.h: No such file or directory
 #include <zstd/zstd.h>
                       ^
compilation terminated.
```

请确保本地编译镜像为最新版本。可使用以下命令更新：

``` shell
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail-build-linux
```

### 使用镜像编译完整iLogtail

编译完整iLogtail的命令是`make all`，由于all是默认的编译目标，因此也可以直接`make`。该命令首先清理`output`目录，然后调用`./scripts/gen_build_scripts.sh`脚本生成编译用的脚本和镜像描述保存到`./gen`目录，调用 docker 制作镜像，制作的过程即镜像内的编译过程，最后将镜像内的编译结果复制到`output`目录。

以下命令可以快速编译出iLogtail的可执行程序和插件。

1\. 进入源代码顶层目录。

2\. 执行命令`make`。

3\. 查看`output`目录结果。

```text
./output
├── ilogtail （主程序）
├── libPluginAdapter.so（插件接口）
├── libPluginBase.h
└── libPluginBase.so （插件lib）
```

开发环境增量编译的方法请参考[开发环境](../../developer-guide/development-environment.md)。

### Go插件本地编译

Go插件可以在主机上进行直接编译，编译前，需要安装基础的 Go 1.16+
语言开发环境，如何安装可以参见[官方文档](https://golang.org/doc/install)。
在安装完成后，为了方便后续地开发，请遵照[此文档](https://golang.org/doc/code#Organization)正确地设置你的开发目录以及 GOPATH 等环境变量。。

```bash
go mod tidy       # 若需要更新插件依赖库
make plugin_local # 每次更新插件代码后从这里开始
```

如果未对只是对插件依赖库进行修改，则只需要执行最后一行命令即可。

## Windows版本编译

### 编译前准备

- 装备一台windows机器
- 安装Visual Studio Community Edition，推荐2017版本
- 安装golang
- 安装MinGW(根据windows机器位数选择对应版本)
- 下载相应位数的ilogtail-deps编译依赖，解压
  - [64位依赖](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/prebuilt-dependencies/2.0/ilogtail-deps.windows-x64-2.0-version.zip)

### 修改编译脚本

将ilogtail/scripts/windows64_build.bat(windows32_build.bat)脚本中的的ILOGTAIL_DEPS_PATH、CMAKE_BIN、DEVENV_BIN、MINGW_PATH四个环境变量值替换成编译机器上实际的路径

``` bat
REM Change to where ilogtail-deps.windows-x64 locates
set ILOGTAIL_DEPS_PATH=D:\workspace\ilogtail-deps.windows-x64-2.0-version
REM Change to where cmake locates
set CMAKE_BIN="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake"
REM Change to where devenv locates
set DEVENV_BIN="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\IDE\devenv.com"
REM Change to where mingw locates
set MINGW_PATH=C:\local\mingw64\bin
```

### 执行编译脚本

cd 到ilogtail/scripts目录下，执行windows64_build.bat(windows32_build.bat)脚本，等待约6分钟，完成编译。
编译产物列表：
ilogtail/output目录下:
- ilogtail.exe (主程序)
- PluginAdapter.dll (插件接口)
- PluginBase.dll (插件lib)
- PluginBase.h
