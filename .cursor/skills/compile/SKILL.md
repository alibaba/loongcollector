---
name: compile
description: Building
---
## 如何编译本项目

本项目分为 C++ 部分和 Go 部分。当你修改不同类型文件代码时，请使用不同的编译方法。

### C++ 部分编译方法

1. 判断是否进行增量编译。如果已有 `build` 目录，并且其中有内容，并且你的修改没有涉及到 CMake 相关文件，那么跳转到第5步进行增量编译。

2. 创建编译目录

``` bash
mkdir -p build
```

3. 进入编译目录

``` bash
cd build
```

4. 构建 CMake 命令

``` bash
cmake -DCMAKE_BUILD_TYPE=Debug -DLOGTAIL_VERSION=0.0.1 \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
      -DCMAKE_CXX_FLAGS="-I/opt/rh/devtoolset-9/root/usr/lib/gcc/x86_64-redhat-linux/9/include -I/opt/logtail -I/opt/logtail_spl" \
      -DBUILD_LOGTAIL=ON -DBUILD_LOGTAIL_UT=ON -DWITHOUTGDB=ON -DENABLE_STATIC_LINK_CRT=ON -DWITHSPL=ON ../core
```

注意其中的几个开关：
    - BUILD_LOGTAIL：表示编译 LoongCollector 二进制。必选
    - BUILD_LOGTAIL_UT：表示编译 LoongCollector 单测。仅当你修改了 LoongCollector 单测时才打开。
    - WITHSPL：表示编译 LoongCollector SPL 相关内容。仅当你修改了 LoongCollector SPL 相关文件时才打开。

5. 编译

``` bash
make -sj$(nproc)
```

### Go 部分编译方法

执行

``` bash
make plugin_local
```
