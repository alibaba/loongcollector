---
name: compile
description: Building LoongCollector C++ and Go components. Use when compiling any part of the project.
---
# Compile Skill

## How to Compile This Project

This project has both C++ and Go components. Use the appropriate build method based on what you modified.

### C++ Build

**IMPORTANT: All CMake and make commands must run from inside the `build/` directory.** Running from repo root will reconfigure incorrectly.

**Prerequisites** — Git submodules must be populated before first build:
```bash
git submodule update --init --recursive
```
Two submodules live under `core/_thirdparty/`:
- `DCGM` — NVIDIA DCGM headers (`dcgm_agent.h` etc.)
- `coolbpf` — eBPF framework

If either is empty, compilation fails with `No such file or directory`.

#### Build Steps

```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DLOGTAIL_VERSION=0.0.1 \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
      -DCMAKE_CXX_FLAGS="-I/opt/rh/devtoolset-9/root/usr/lib/gcc/x86_64-redhat-linux/9/include -I/opt/logtail -I/opt/logtail_spl" \
      -DBUILD_LOGTAIL=ON -DBUILD_LOGTAIL_UT=ON -DWITHOUTGDB=ON -DENABLE_STATIC_LINK_CRT=ON -DWITHSPL=OFF ../core
make -sj$(nproc)
```

**Key CMake flags:**
| Flag | Purpose |
|------|---------|
| `BUILD_LOGTAIL` | Build LoongCollector binary. Required. |
| `BUILD_LOGTAIL_UT` | Build unit tests. Enable when modifying tests. |
| `WITHSPL` | SPL support. Set `OFF` unless working on SPL files. |

#### C++ Unit Tests

Each test directory under `core/unittest/*/` produces its own executable.

**Build tests** (from inside `build/`):
```bash
make yaml_util_unittest app_config_unittest safe_queue_unittest -j$(nproc)
```

**Run tests** (from inside `build/`):
```bash
./unittest/common/yaml_util_unittest
./unittest/app_config/app_config_unittest
```

Tests must run from `build/` because some rely on relative paths for config files and temporary output.

### Go Plugin Build

```bash
make plugin_local
```

### Docker Build

```bash
make image
```

### Cross-Compilation

For ARM64:
```bash
make image ARCH=arm64
```

### Common Issues

- If CMake complains about missing dependencies, install them via `apt` or `yum`
- If linking fails, try `make clean` then rebuild
- For SPL-related builds, change `WITHSPL=OFF` to `WITHSPL=ON` in the cmake command
