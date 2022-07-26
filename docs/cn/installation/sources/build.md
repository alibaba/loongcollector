# 编译

## 编译前准备

[下载源代码](download.md)

[安装docker](https://docs.docker.com/engine/install/)

## 快速编译

以下命令可以快速编译出iLogtail的可执行程序和插件。产出在output目录。

1\. 进入源代码顶层目录。

2\. 执行命令`make`。

## 增量编译

虽然快速编译可以实现iLogtail的快速编译，但却不适合开发场景。因为快速编译每次都是从头开始编译因而耗时较长。对于开发场景下面将介绍增量编译。

### C++核心编译

iLogtail依赖了诸多第三方库（详见附录），为了简化编译流程ilogtail提供了预编译依赖的镜像辅助编译。

1\. 进入源代码顶层目录

2\. 创建编译容器，并挂载代码目录。

```bash
docker run --name ilogtail-build -d \
  -v `pwd`:/src -w /src \
  sls-registry.cn-beijing.cr.aliyuncs.com/sls-microservices/ilogtail-build-linux-amd64:latest \
  bash -c "sleep infinity"
```

3\. 进入容器

```bash
docker exec -it ilogtail-build bash
```

4\. 在容器内编译

```bash
mkdir -p /src/core/build
cd /src/core/build
cmake ..
make -sj$(nproc)
```

需要编译UT的话，可以打开开关。替换上述第2行为

```bash
cmake -DBUILD_LOGTAIL_UT=ON ..
```

5\. 编译产出

编译产出在容器的`/src/core/build`目录下。

`/src/core/build`目录的结构如下

```
/src/core/build
├── ilogtail (主程序）
├── plugin (主程序）
│   ├── libPluginAdapter.so（插件接口）
│   └── ...
├── unittest (单元测试程序）
└── ...
```

### Golang插件编译

可以在镜像内继续编译Golang插件（即从上面的3开始）。

1\. 在容器内编译

```bash
cd /src
./scripts/plugin_build.sh vendor c-shared output
```

2\. 编译产出

编译产出在容器的`/src/output`目录下。

`/src/output`目录的结构如下

```text
/src/output
├── libPluginBase.h
└── libPluginBase.so (插件lib）
```

### 组装构建结果

当C++核心和Golang插件都编译完成后，可以将C++核心的构建结果拷贝到`/src/output`目录组装出完整的构建结果。

```bash
cp -a /src/core/build/ilogtail /src/output
cp -a /src/core/build/plugin/libPluginAdapter.so /src/output
```

`/src/output`目录的结构如下

```text
/src/output
├── ilogtail (主程序）
├── libPluginAdapter.so（插件接口）
├── libPluginBase.h
└── libPluginBase.so (插件lib）
```

由于容器挂载了主机的源代码目录，因此源代码目录下的`output`目录与之完全相同。
