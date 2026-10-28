---
name: compile
description: 编译 LoongCollector C++ 与 Go 组件。涉及构建、单元测试或镜像时使用。
---
# LoongCollector 编译指南

与 **CI（`make core` / `scripts/run_core_ut.sh`）** 和 **E2E 镜像（`make e2edocker`）** 对齐；仅引用仓库内已有脚本与 Makefile target。

## 快速选择

| 目标 | 命令 |
|------|------|
| C++ 主程序 + UT（CI 同款） | `make core PATH_IN_DOCKER=$(pwd)` |
| 跑 C++ UT | `./scripts/run_core_ut.sh` |
| Go 插件（本机） | `make plugin_local` |
| Go 插件（CI 风格） | `make plugin_main` |
| E2E 运行时镜像 | `make e2edocker` → `aliyun/loongcollector:0.0.1` |
| 发布镜像 | `make image` |
| ARM64 交叉编译 | `make image ARCH=arm64` |
| 本机 CMake 增量编译 | 见下文「本机 CMake（build/）」 |

## 前置条件

```bash
git submodule update --init --recursive
```

两个子模块位于 `core/_thirdparty/`：

- `DCGM` — NVIDIA DCGM 头文件
- `coolbpf` — eBPF 框架

子模块目录为空会导致 `No such file or directory` 编译错误。CI 还会单独初始化 coolbpf 子模块。

## C++：CI 方式（推荐用于验证 PR）

在仓库根目录执行（会 `docker build` 构建镜像并 `docker cp` 产物到 `core/build`）：

```bash
export MAKE_JOBS=16
export BUILD_LOGTAIL=OFF BUILD_LOGTAIL_UT=ON
export WITHSPL=ON BUILD_TYPE=Debug ENABLE_AGENTSIGHT=ON
make core PATH_IN_DOCKER=$(pwd)
```

跑单元测试：

```bash
./scripts/run_core_ut.sh
# 单个用例示例：
./scripts/run_core_ut.sh --gtest_filter=CollectionConfigUnittest.* unittest/config/collection_config_unittest
```

**说明**：每次 `make core` 会重建构建镜像，较慢；适合与 CI 对齐的最终验证，不适合高频迭代。

## C++：本机 CMake（build/）

**重要：所有 CMake 和 make 命令必须在 `build/` 目录内执行。**

1. 若已有 `build/` 目录且修改未涉及 CMake 文件，可直接增量编译（跳到第 4 步）。
2. 创建并进入编译目录：

```bash
mkdir -p build && cd build
```

3. 构建 CMake 命令：

```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DLOGTAIL_VERSION=0.0.1 \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
      -DCMAKE_CXX_FLAGS="-I/opt/rh/devtoolset-9/root/usr/lib/gcc/x86_64-redhat-linux/9/include -I/opt/logtail -I/opt/logtail_spl" \
      -DBUILD_LOGTAIL=ON -DBUILD_LOGTAIL_UT=ON -DWITHOUTGDB=ON \
      -DENABLE_STATIC_LINK_CRT=ON -DWITHSPL=OFF ../core
```

| CMake 选项 | 用途 |
|------------|------|
| `BUILD_LOGTAIL` | 编译 loongcollector 主程序 |
| `BUILD_LOGTAIL_UT` | 编译单元测试 |
| `WITHSPL` | SPL 支持；非 SPL 改动设为 `OFF` 可加快编译 |

4. 编译：

```bash
make -sj$(nproc)
```

**编译指定测试**（在 `build/` 目录内）：

```bash
make yaml_util_unittest app_config_unittest safe_queue_unittest -j$(nproc)
```

**运行测试**（在 `build/` 目录内）：

```bash
./unittest/common/yaml_util_unittest
./unittest/app_config/app_config_unittest
```

测试必须在 `build/` 目录内运行，部分测试依赖相对路径加载配置文件。

## C++：本机 CMake（core/build/，可选）

需本机已安装与 `docker/Dockerfile_build` 兼容的工具链（如 devtoolset-9）。**在 `core/build` 目录内**执行：

```bash
mkdir -p core/build && cd core/build
cmake -DCMAKE_BUILD_TYPE=Debug -DLOGTAIL_VERSION=0.0.1 \
      -DBUILD_LOGTAIL=ON -DBUILD_LOGTAIL_UT=ON -DWITHOUTGDB=ON \
      -DENABLE_STATIC_LINK_CRT=ON -DWITHSPL=ON ..
make -sj$(nproc)
```

产物示例：`core/build/loongcollector`、`core/build/unittest/.../*_unittest`。

## Go 插件

```bash
make plugin_local    # 输出到 output/libGoPluginBase.so，本机 c-shared
make plugin_main     # CI / 发布风格
```

修改 Go 插件后跑 E2E：需同步进 `aliyun/loongcollector:0.0.1` 镜像（见 e2e skill §4.1 增量更新）。

## E2E 镜像

```bash
make e2edocker
```

生成 `aliyun/loongcollector:0.0.1`，与 `.github/workflows/e2e.yaml` 中 `GenerateE2ETestImage` 用途一致（CI 使用 `docker/Dockerfile_edge_linux` 构建）。

## Native → Go 桥接（PPEG）

批量数据仅走 **`ProcessPipelineEventGroup`**（`PipelineEventGroup` PB）。已移除 `ProcessLogGroup` / `ProcessLog` export；v1 插件 runner 在 Go 侧将 Log 转为 `LogGroup` 后 `ReceiveLogGroup`。

## 常见问题

- **CMake 选项变更**：删除 `build/` 或 `core/build/` 后重新 cmake。
- **缺少依赖**：通过 `apt` 或 `yum` 安装。
- **UT 链接失败**：在 `build/` 或 `core/build` 下编译；用 `run_core_ut.sh` 设置 `LD_LIBRARY_PATH`。
- **需要 SPL 功能**：将 `WITHSPL=OFF` 改为 `WITHSPL=ON`。
- **E2E 镜像过旧**：C++/Go 变更后执行 `make e2edocker` 或手动 `docker commit` 更新镜像内二进制与 `libGoPluginBase.so`。
