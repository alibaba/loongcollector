# Native Input Event Type Matrix

This file documents the expected `PipelineEvent` semantics for C++ native inputs in phase A2.
The source of truth in code is `core/plugin/input/InputEventTypeRegistry.cpp`.

## Input -> Event Types

| Input Type | Event Types | Main Producer Path |
| --- | --- | --- |
| `input_file` | `Log` | `core/file_server/reader/LogFileReader.cpp` |
| `input_static_file_onetime` | `Log` | `core/file_server/reader/LogFileReader.cpp` |
| `input_container_stdio` | `Log` | `core/file_server/reader/LogFileReader.cpp` |
| `input_prometheus` | `Metric` | `core/prometheus/labels/TextParser.cpp` |
| `input_host_monitor` | `Metric` | `core/host_monitor/collector/*Collector.cpp` |
| `input_host_meta` | `Log` | `core/host_monitor/collector/ProcessEntityCollector.cpp` |
| `input_internal_metrics` | `Metric` | `core/monitor/SelfMonitorServer.cpp` |
| `input_internal_alarms` | `Log` | `core/monitor/AlarmManager.cpp` |
| `input_internal_config_container_info` | `Log` | `core/container_manager/ContainerManager.cpp` |
| `input_file_security` | `Log` | `core/ebpf/plugin/file_security/FileSecurityManager.cpp` |
| `input_network_security` | `Log` | `core/ebpf/plugin/network_security/NetworkSecurityManager.cpp` |
| `input_process_security` | `Log` | `core/ebpf/plugin/process_security/ProcessSecurityManager.cpp` |
| `input_agentsight` | `Log` | `core/ebpf/plugin/agentsight/AgentsightManager.cpp` |
| `input_cpu_profiling` | `Log` | `core/ebpf/plugin/cpu_profiling/CpuProfilingManager.cpp` |
| `input_network_observer` | `Log`, `Metric`, `Span` | `core/ebpf/plugin/network_observer/NetworkObserverManager.cpp` |
| `input_forward` | `Log`, `Metric`, `Span` | `core/plugin/processor/inner/ProcessorParseFromPBNative.cpp` |

## Notes

- `input_forward` receives `RawEvent` payload first, then decodes to `PipelineEventGroup` through `processor_parse_from_pb_native`; the output semantics above refer to decoded events entering the normal pipeline.
- This phase does **not** change default `StructureType`.
