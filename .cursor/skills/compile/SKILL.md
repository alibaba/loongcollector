---
name: compile
description: Building
---
## 如何编译本项目

本项目分为 C++ 部分和 Go 部分。当你修改不同类型文件代码时，请使用不同的编译方法。

### C++ 部分编译方法

**重要：所有 CMake 和 make 命令必须在 `build/` 目录内执行。**

**前置条件** — 首次编译前需初始化 Git 子模块：
```bash
git submodule update --init --recursive
```
两个子模块位于 `core/_thirdparty/`：
- `DCGM` — NVIDIA DCGM 头文件
- `coolbpf` — eBPF 框架

如果子模块目录为空，编译会报 `No such file or directory` 错误。

#### 编译步骤

1. 判断是否进行增量编译。如果已有 `build` 目录，并且其中有内容，并且你的修改没有涉及到 CMake 相关文件，那么跳转到第 4 步进行增量编译。

2. 创建并进入编译目录

```bash
mkdir -p build && cd build
```

3. 构建 CMake 命令

```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DLOGTAIL_VERSION=0.0.1 \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
      -DCMAKE_CXX_FLAGS="-I/opt/rh/devtoolset-9/root/usr/lib/gcc/x86_64-redhat-linux/9/include -I/opt/logtail -I/opt/logtail_spl" \
      -DBUILD_LOGTAIL=ON -DBUILD_LOGTAIL_UT=ON -DWITHOUTGDB=ON -DENABLE_STATIC_LINK_CRT=ON -DWITHSPL=OFF ../core
```

关键 CMake 开关：

| 开关 | 用途 |
|------|------|
| `BUILD_LOGTAIL` | 编译 LoongCollector 二进制。必选。 |
| `BUILD_LOGTAIL_UT` | 编译单元测试。修改了测试代码时打开。 |
| `WITHSPL` | SPL 支持。除非修改了 SPL 相关文件，否则设为 `OFF`。 |

4. 编译

```bash
make -sj$(nproc)
```

#### C++ 单元测试

每个 `core/unittest/*/` 下的测试目录会生成独立的可执行文件。

**编译指定测试**（在 `build/` 目录内）：
```bash
make yaml_util_unittest app_config_unittest safe_queue_unittest -j$(nproc)
```

**运行测试**（在 `build/` 目录内）：
```bash
./unittest/common/yaml_util_unittest
./unittest/app_config/app_config_unittest
```

测试必须在 `build/` 目录内运行，因为部分测试依赖相对路径加载配置文件。

### Go 部分编译方法

```bash
make plugin_local
```

### Docker 构建

```bash
make image
```

### 交叉编译

ARM64 架构：
```bash
make image ARCH=arm64
```

### 常见问题

- 如果 CMake 报缺少依赖，通过 `apt` 或 `yum` 安装
- 如果链接失败，尝试 `make clean` 后重新构建
- 需要 SPL 相关功能时，将 `WITHSPL=OFF` 改为 `WITHSPL=ON`
