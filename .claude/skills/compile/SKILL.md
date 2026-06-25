---
name: compile
description: Building LoongCollector C++ and Go components. Use when compiling any part of the project.
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
| 本机 CMake（有 devtoolset 时） | 见下文「本机 CMake」 |

## 前置条件

```bash
git submodule update --init core/_thirdparty/coolbpf
```

`core/_thirdparty/coolbpf` 为空会导致编译失败。CI 还会单独初始化 coolbpf 子模块。

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

## C++：本机 CMake（可选）

需本机已安装与 `docker/Dockerfile_build` 兼容的工具链（如 devtoolset-9）。**在 `core/build` 目录内**执行 cmake/make：

```bash
mkdir -p core/build && cd core/build
cmake -DCMAKE_BUILD_TYPE=Debug -DLOGTAIL_VERSION=0.0.1 \
      -DBUILD_LOGTAIL=ON -DBUILD_LOGTAIL_UT=ON -DWITHOUTGDB=ON \
      -DENABLE_STATIC_LINK_CRT=ON -DWITHSPL=ON ..
make -sj$(nproc)
```

| CMake 选项 | 用途 |
|------------|------|
| `BUILD_LOGTAIL` | 编译 loongcollector 主程序 |
| `BUILD_LOGTAIL_UT` | 编译单元测试 |
| `WITHSPL` | SPL 支持；非 SPL 改动可 `OFF` 加快编译 |

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

- **CMake 选项变更**：删除 `core/build` 后重新 cmake。
- **UT 链接失败**：在 `core/build` 下编译；用 `run_core_ut.sh` 设置 `LD_LIBRARY_PATH`。
- **E2E 镜像过旧**：C++/Go 变更后执行 `make e2edocker` 或手动 `docker commit` 更新镜像内二进制与 `libGoPluginBase.so`。
