# Codebase Map for AI Review (LoongCollector)

## 1) Core Runtime Map（最关键链路）

- Entry: `core/logtail.cpp` -> `core/application/Application.cpp`
- Config scan/update:
  - `core/config/watcher/PipelineConfigWatcher.cpp`
  - `core/collection_pipeline/CollectionPipelineManager.cpp::UpdatePipelines`
- Pipeline runtime:
  - `core/collection_pipeline/CollectionPipeline.cpp`
  - `core/runner/ProcessorRunner.cpp`
  - `core/runner/FlusherRunner.cpp`
- File collecting lifecycle:
  - `core/file_server/FileServer.cpp`
  - `core/file_server/ConfigManager.cpp`
- Checkpoint:
  - v1 file checkpoint: `core/file_server/checkpoint/CheckPointManager.cpp`
  - v2 exactly-once checkpoint: `core/file_server/checkpoint/CheckpointManagerV2.cpp`
  - onetime config checkpoint: `core/config/OnetimeConfigInfoManager.cpp`

## 2) Public Capability Entrances（优先复用，避免重复造轮子）

- String and parse utils:
  - `core/common/StringTools.h`
    - 常用能力：`StringViewSplitter`（零拷贝切分）、`Trim`（统一裁剪语义）、`StringTo`（返回值判错的数值转换）
- Dynamic library loading:
  - `core/common/DynamicLibHelper.cpp`
- Queue and pipeline routing:
  - `core/collection_pipeline/queue/*`
- App-level config and dynamic params:
  - `core/app_config/AppConfig.*`
- Alarm and metrics:
  - `core/monitor/*`
  - `core/monitor/metric_constants/*`

## 3) Lifecycle & Resource Invariants（评审时必须核对）

### 3.1 Pipeline start/stop invariants

- Start order (pipeline):
  - flusher start -> go(without input) start -> enable pop -> go(with input) start -> input start
- Stop order (pipeline):
  - input stop -> go(with input) stop -> disable pop -> wait in-process done -> flush batch -> go(without input) stop -> flusher stop

### 3.2 Config hot reload invariants

- Modified-but-invalid config must NOT kill current valid pipeline/task.
- Input type set changes may trigger complete stop path; review resource cleanup and state rebuild.
- For file inputs (`input_file`, `input_container_stdio`), update should go through `FileServer::Pause/Resume` path.

### 3.3 State recovery invariants

- `Pause(isConfigUpdate=true)` should persist handler meta + checkpoint before cache cleanup.
- Exit path should not leave live threads/queues/plugins/checkpoint inconsistencies.
- Exactly-once checkpoint primary/range keys must be consistent (no orphan range keys, no wrong GC delete).

## 4) Config/Env Conventions（高频踩坑）

- Env var compatibility:
  - keep lower-case key compatibility where existing behavior expects it
  - upper-case aliases should follow project naming convention (`LOONG_{GFLAG_UPPER}`)
- Deprecated env/config flags:
  - keep compatibility mapping and explicit message
  - avoid message key mismatch with actual parsed key
- Boolean/default naming:
  - default false preferred unless there is strong backward-compat reason
  - naming should express behavior directly (avoid ambiguous negation)

## 5) Historical Review Hotspots (Sampled from >=30 recent merged PR comments)

### A. Concurrency/lifecycle

- Lock scope inconsistency and potential deadlock/cost from lock placement differences.
- Resource release path completeness questioned frequently (especially around container/file pipeline updates).

### B. State and checkpoint semantics

- Checkpoint normalization/cleanup location confusion (where to normalize, where to delete).
- Restore/delete behavior mismatch under corner paths (map empty, deleted file/container states).

### C. Config semantics and compatibility

- Timeout and force-release semantics frequently misunderstood or changed unintentionally.
- Env key name/read-key/log-message mismatch appears repeatedly.

### D. Test robustness

- Avoid non-deterministic tests:
  - avoid `rand()` in concurrent tests
  - avoid fixed sleeps for synchronization
- Prefer reusable invariant check helpers over duplicated complex assertions.

### E. Comment/doc consistency

- Comment says one behavior but code does another.
- Parameter behavior change without synchronized docs update.

## 6) AI Review Priority Checklist（每次评审先扫）

1. Are lifecycle paths complete for all outcomes (success/failure/update/remove/exit)?
2. Are resources always released (thread/future/queue/plugin/checkpoint)?
3. Can state recover correctly after hot reload or restart?
4. Are config/env compatibility and deprecation mappings preserved?
5. Are tests deterministic and aligned with invariants?
6. Are docs/comments updated with behavior changes?

## 7) Maintenance

- This map is a "semi-static" review accelerator.
- Update triggers:
  - major architecture/lifecycle refactor
  - repeated review comments on the same class of issues
  - new compatibility contract (config/env/checkpoint)
