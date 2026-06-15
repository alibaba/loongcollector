---
name: compile
description: Building LoongCollector C++ and Go components. Use when compiling any part of the project.
---
# Compile Skill

## How to Compile This Project

This project has both C++ and Go components. Use the appropriate build method based on what you modified.

### Where to Build (read this first)

| Rule | Detail |
|------|--------|
| **Environment** | Compile **inside** the LoongCollector devcontainer / build image. Do **not** run `cmake`/`make` on the bare host (missing `/opt/logtail`, coolbpf headers, etc.). |
| **Build directory** | **`core/build`** — all `cmake` and `make` commands run from here. |
| **Wrong directory** | Do **not** use repo-root `build/` with `../core`; that is not the project convention and will misconfigure or fail. |
| **Source of truth** | `docs/cn/developer-guide/development-environment.md` §4–§5 |

**Enter container** (pick one):

- VS Code: `Remote-Containers: Reopen in Container`
- Docker: `docker exec -it loongcollector-build bash` (see dev-environment doc §「直接使用镜像编译」)

### C++ Build

**Prerequisites** — Git submodules must be populated before first build (inside container):

```bash
git submodule update --init
```

Two submodules live under `core/_thirdparty/`:

- `DCGM` — NVIDIA DCGM headers (`dcgm_agent.h` etc.)
- `coolbpf` — eBPF framework (`coolbpf/net.h` etc.; empty submodule → compile fails)

#### Build Steps (inside container)

```bash
mkdir -p core/build
cd core/build
cmake ..                              # first time, or after CMakeLists / file add/remove
make -sj$(nproc)                      # incremental after core code changes
```

**Common cmake variants** (replace the `cmake ..` line):

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..     # debug symbols
cmake -DBUILD_LOGTAIL_UT=ON ..        # unit test binaries
cmake -DBUILD_LOGTAIL_UT=ON -DCMAKE_BUILD_TYPE=Debug ..
cmake -DENABLE_AGENTSIGHT=ON ..       # only if cache had OFF; Linux default is ON
```

Advanced / CI-style flags (only when needed):

```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DLOGTAIL_VERSION=0.0.1 \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
      -DBUILD_LOGTAIL=ON -DBUILD_LOGTAIL_UT=ON -DWITHOUTGDB=ON \
      -DENABLE_STATIC_LINK_CRT=ON -DWITHSPL=OFF ..
```

| Flag | Purpose |
|------|---------|
| `BUILD_LOGTAIL` | Build `loongcollector` binary (on by default in tree cmake) |
| `BUILD_LOGTAIL_UT` | Build `core/build/unittest/**` test executables |
| `ENABLE_AGENTSIGHT` | Build `libagentsight.so` + link AgentSight into loongcollector (**default ON on Linux**; plain `cmake ..` is enough unless cache has OFF) |
| `WITHSPL` | SPL support; keep `OFF` unless working on SPL |

#### Build Artifacts (under `core/build/`)

Paths are relative to repo root. With devcontainer bind-mount, the host sees the same files.

| Artifact | Path | Notes |
|----------|------|-------|
| Main binary | `core/build/loongcollector` | Agent / daemon executable |
| Go plugin adapter | `core/build/go_pipeline/libGoPluginAdapter.so` | Required at runtime |
| Go plugin base | `core/build/go_pipeline/libGoPluginBase.so` | Required at runtime |
| eBPF driver | `core/build/ebpf/driver/libeBPFDriver.so` | eBPF plugins |
| coolbpf | `core/build/_thirdparty/coolbpf/src/libcoolbpf.so.1.0.0` | eBPF runtime |
| AgentSight FFI | `core/build/_thirdparty/coolbpf/src/agentsight/release/libagentsight.so` | AgentSight input |
| Unit tests | `core/build/unittest/<area>/<name>_unittest` | e.g. `unittest/ebpf/agentsight_message_util_unittest` |
| compile_commands | `core/build/compile_commands.json` | IDE / clangd (if exported) |

**Assemble runnable `output/`** (from repo root, host or container):

```bash
./run_scripts/restart.sh
```

This copies the binaries above from `core/build/` into `./output/` and starts local loongcollector. Manual equivalent:

```bash
cp -a core/build/loongcollector output/
cp -a core/build/go_pipeline/libGoPluginAdapter.so output/
cp -a core/build/go_pipeline/libGoPluginBase.so output/
cp -a core/build/ebpf/driver/libeBPFDriver.so output/
cp -a core/build/_thirdparty/coolbpf/src/libcoolbpf.so.1.0.0 output/
cp -a core/build/_thirdparty/coolbpf/src/agentsight/release/libagentsight.so output/
```

#### C++ Unit Tests

Build (from `core/build/`):

```bash
make agentsight_message_util_unittest yaml_util_unittest -j$(nproc)
```

Run (must cwd = `core/build/` — tests use relative paths):

```bash
cd core/build
./unittest/ebpf/agentsight_message_util_unittest
./unittest/common/yaml_util_unittest
```

Run all core UT from repo root:

```bash
./scripts/run_core_ut.sh
```

(default discovers `core/build/unittest/**/*_unittest`)

### Go Plugin Build

Inside container, from repo root:

```bash
make plugin_local
```

### Docker / Release Image Build

On host (has Docker):

```bash
make image
make core PATH_IN_DOCKER=$(pwd)   # full core build in CI-style container
```

### Cross-Compilation

For ARM64:

```bash
make image ARCH=arm64
```

### Common Issues

- **`coolbpf/net.h: No such file or directory`** — run `git submodule update --init`, then `cmake ..` from `core/build` **inside** the build container.
- **CMake cache from wrong directory** — remove `core/build/CMakeCache.txt` and re-run `cmake ..` from `core/build`.
- **Link fails after partial edit** — `cd core/build && make -sj$(nproc)` (full incremental link), not manual `link.txt` hacks.
- **Permission on host** — container may build as root; on host run `sudo chown -R $USER .` if needed.
