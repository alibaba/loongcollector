---
name: project-knowledge
description: LoongCollector project knowledge: architecture, terminology, codebase map, and coding standards (C++/Go).
---
# LoongCollector Project Knowledge

## Architecture Overview

The LoongCollector architecture is based on a plugin system with the following key components:

1. **Core Application**: Main entry point in `core/logtail.cpp`, initializes `Application` class in `core/application/Application.cpp`. Follows singleton pattern, manages overall lifecycle.

2. **Plugin System**: Supports plugins for data collection, processing, and flushing:
   - **Inputs**: Collect data from various sources (files, network, system metrics, etc.)
   - **Processors**: Transform and process collected data
   - **Flushers**: Send processed data to various backends

3. **Pipeline Management**: Collection pipelines managed by `CollectionPipelineManager` handle data flow from inputs through processors to flushers.

4. **Configuration**: Supports both local and remote configuration management with watchers that monitor for configuration changes.

5. **Queuing System**: Implements various queue types including bounded queues, circular queues, and exactly-once delivery queues for reliable data transmission.

6. **Monitoring**: Built-in monitoring and metrics collection for tracking the collector's own performance and health.

## Project Structure

```
core/                    # Core C++ code
  plugin/                # Plugin system
    input/               # Data collection input plugins
    processor/           # Data processing plugins
    flusher/             # Data output plugins (SLS, file, etc.)
  collection_pipeline/   # Main pipeline flow (queue, batch, serialization)
  config/                # Configuration (loading, providers, feedback)
    provider/            # Config providers (Enterprise, Legacy)
  common/                # Common utilities, data structures, network, string, crypto
  monitor/               # Monitoring, metrics collection, alerting
  logger/                # Logging system
  checkpoint/            # Checkpoint, state management
  app_config/            # Global configuration
  models/                # Core data structures (events, logs, metrics)
  parser/                # Log parsers
  task_pipeline/         # Task scheduling
  go_pipeline/           # Go plugin integration
  ebpf/                  # eBPF collection and plugins
  host_monitor/          # Host-level monitoring
  shennong/              # Shennong metrics
  prometheus/            # Prometheus collection
  file_server/           # File collection and management
  container_manager/     # Container environment management
  application/           # Main application entry
  protobuf/              # Protobuf protocol definitions
  metadata/              # K8s and other metadata collection
  constants/             # Constants
  tools/                 # Internal utility scripts
  unittest/              # Unit tests
  legacy_test/           # Historical test cases

pkg/                     # Go packages
  helper/                # Go helper functions
    containercenter/     # Go container-related functions

plugin_main/             # Plugin main entry
pluginmanager/           # Go plugin manager (lifecycle, registration)
plugins/                 # Go plugin packages
  input/                 # Go input plugins (docker, etc.)
  processor/             # Go processor plugins
  flusher/               # Go flusher plugins
  aggregator/            # Go aggregator plugins
  extension/             # Go extension plugins
  all/                   # Plugin registration and init
  test/                  # Go plugin tests

test/                    # Integration tests
e2e/                     # E2E test cases (open source Go plugins)
e2e_enterprise/          # E2E enterprise test cases (host + K8s)
docs/                    # Project documentation
scripts/                 # Build, deploy, test scripts
docker/                  # Docker-related files
rpm/                     # RPM packaging
external/                # External dependencies
```

## Key Dependencies

### Header-Only Libraries
- `spdlog` - Logging
- `rapidjson` - JSON parsing

### Compiled Libraries
- **Testing**: `gtest`, `gmock`
- **Serialization**: `protobuf`
- **Regex**: `re2`
- **Hash**: `cityhash`
- **Config**: `jsoncpp`, `yamlcpp`
- **Compression**: `lz4`, `zlib`, `zstd`
- **Network**: `curl`, `ssl`, `crypto`
- **System**: `boost`, `gflags`, `leveldb`, `uuid`
- **Memory**: `tcmalloc` (optional)

### Tech Stack
- C++ (main implementation, C++17/20)
- Protobuf (data serialization)
- eBPF (kernel-level data collection)
- Prometheus (metrics collection)
- Go (plugin adaptation)
- Shell/Python (build and test scripts)

## Terminology Glossary

| Term | Description |
|------|-------------|
| LoongCollector | The observability data collection agent (formerly iLogtail) |
| Pipeline | A data processing chain: Input -> Processor(s) -> Flusher |
| Plugin | A modular component that performs specific data operations |
| Input Plugin | Collects data from a source (file, network, metric, etc.) |
| Processor Plugin | Transforms data (parse, filter, enrich, etc.) |
| Flusher Plugin | Sends data to a destination (SLS, stdout, Prometheus, etc.) |
| Config | Collection configuration defining pipeline behavior |
| Checkpoint | Persistent state tracking for exactly-once delivery |
| Runner | Execution wrapper for a specific plugin instance |
| Queue | Data buffer between pipeline stages |
| Batch | Group of events processed/sent together |
| SLS | Alibaba Cloud Simple Log Service |
| eBPF | Extended Berkeley Packet Filter (kernel tracing) |
| SPL | Structured Processing Language |

## Codebase Map

### Key Entry Points and Core Flows

| Path | Purpose |
|------|---------|
| `core/logtail.cpp` | Main entry point |
| `core/application/Application.cpp` | Application singleton, lifecycle management |
| `core/collection_pipeline/CollectionPipelineManager.cpp` | Pipeline lifecycle |
| `core/collection_pipeline/CollectionPipeline.cpp` | Pipeline execution |
| `core/runner/ProcessorRunner.cpp` | Processor execution |
| `core/runner/FlusherRunner.cpp` | Flusher execution |
| `core/config/watcher/PipelineConfigWatcher.cpp` | Config change detection |
| `core/file_server/FileServer.cpp` | File collection management |
| `core/file_server/checkpoint/CheckpointManagerV2.cpp` | Exactly-once checkpoint |

### Invariant Rules

- **Lifecycle**: All plugins follow Init -> Start -> Stop -> Close lifecycle
- **Resource Release**: Every thread/future/queue must be properly cleaned up on stop
- **Config**: Environment variables are case-insensitive with default fallbacks
- **Queue**: Bounded queue with backpressure; pop on disabled queue should not hang
- **Hot Reload**: After config change, system must return to consistent "collect+process+send" state

### Common Patterns

- RAII for resource management
- Smart pointers over raw pointers
- Singleton pattern for managers (Application, AlarmManager, WriteMetrics)
- Thread-safe queues with condition variables
- Plugin registration via static initialization

## C++ Coding Standards

### Naming
- **PascalCase** for class names, global functions, public methods
- **camelCase** for variable names and private methods
- **SCREAMING_SNAKE_CASE** for macros and constants
- **m** prefix for member variables (e.g., `mUserId`)
- **k** prefix for constants (e.g., `kMaxSendBufferSize`)

### Modern C++
- Prefer C++17/20 features (auto, range-based loops, smart pointers)
- Use `std::unique_ptr` / `std::shared_ptr` for memory management
- Prefer `std::optional`, `std::variant`, `std::any` for type-safe alternatives
- Use `constexpr` and `const` for compile-time computations
- Use `std::string_view` for read-only string operations

### Error Handling
- Use exceptions for error handling (`std::runtime_error`, `std::invalid_argument`)
- RAII for resource management to avoid memory leaks
- Validate inputs at function boundaries
- Log errors using spdlog

### Performance
- Avoid unnecessary heap allocations; prefer stack-based objects
- Use `std::move` for move semantics
- Optimize loops with `<algorithm>` (e.g., `std::sort`, `std::for_each`)
- Use `std::array` or `std::vector` over raw arrays

### Security
- Avoid C-style casts; use `static_cast`, `dynamic_cast`, `reinterpret_cast`
- Enforce const-correctness
- Avoid global variables; use singletons sparingly
- Use `enum class` for strongly typed enumerations

### Testing
- Unit tests using Google Test (GTest) / Google Mock
- Integration tests for system components

## Go Coding Standards

### Naming
- **PascalCase** for exported types and functions
- **camelCase** for unexported types and functions
- **snake_case** for variables and constants
- Package names use lowercase

### Error Handling
- Return errors explicitly, do not panic
- Use `fmt.Errorf` with `%w` for error wrapping
- Check errors at every call site

### Concurrency
- Use goroutines for concurrent operations
- Use channels or sync primitives for communication
- Avoid goroutine leaks; always provide exit paths

### Testing
- Use standard `testing` package
- Table-driven tests for function coverage
- Integration tests via E2E framework
