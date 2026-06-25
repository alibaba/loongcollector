# [1928-B2] Plugin Event Matrix (Processor/Flusher × Log/Metric/Span)

Generated at: `2026-06-25 07:50:54Z` (UTC)  
Repository root: `/apsara/workspace/wt-1928-b2`

## Scope & Method

- Read-only scan of `plugins/processor` and `plugins/flusher`; no plugin implementation changes.
- Interface detection:
  - Processor v1: `ProcessLogs([]*protocol.Log)`
  - Processor v2: `Process(*models.PipelineGroupEvents, pipeline.PipelineContext)`
  - Flusher v1: `Flush(..., []*protocol.LogGroup)`
  - Flusher v2: `Export([]*models.PipelineGroupEvents, pipeline.PipelineContext)`
- Event support values in the table:
  - `v1 only`, `dual`, `v2 only`, `passthrough unknown`
- `passthrough unknown` means the plugin has no explicit type-specific signal in code for that event type.

## Processor Summary

- Total registered processors: **36**
- v1-only: **32**
- dual-interface: **4**
- v2-only: **0**

- `v1-only` (32): `processor_add_fields`, `processor_anchor`, `processor_appender`, `processor_base64_decoding`, `processor_base64_encoding`, `processor_cloud_meta`, `processor_csv`, `processor_desensitize`, `processor_dict_map`, `processor_drop`, `processor_drop_last_key`, `processor_encrypt`, `processor_fields_with_condition`, `processor_filter_key_regex`, `processor_filter_regex`, `processor_geoip`, `processor_gotime`, `processor_grok`, `processor_log_to_sls_metric`, `processor_md5`, `processor_otel_metric`, `processor_otel_trace`, `processor_packjson`, `processor_pick_key`, `processor_rate_limit`, `processor_regex`, `processor_split_char`, `processor_split_key_value`, `processor_split_log_regex`, `processor_split_string`, `processor_string_replace`, `processor_strptime`
- `dual-interface` (4): `processor_default`, `processor_json`, `processor_rename`, `processor_split_log_string`
- `v2-only` (0): -
- `unknown` (0): -

## Processor Matrix

| Plugin | Interface class | Log | Metric | Span | Source directories |
|---|---|---|---|---|---|
| processor_add_fields | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/addfields` |
| processor_anchor | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/anchor` |
| processor_appender | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/appender` |
| processor_base64_decoding | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/base64/decoding` |
| processor_base64_encoding | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/base64/encoding` |
| processor_cloud_meta | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/cloudmeta` |
| processor_csv | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/csv` |
| processor_default | dual-interface | dual | passthrough unknown | passthrough unknown | `plugins/processor/defaultone` |
| processor_desensitize | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/desensitize` |
| processor_dict_map | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/dictmap` |
| processor_drop | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/drop` |
| processor_drop_last_key | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/droplastkey` |
| processor_encrypt | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/encrypt` |
| processor_fields_with_condition | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/fieldswithcondition` |
| processor_filter_key_regex | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/filter/keyregex` |
| processor_filter_regex | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/filter/regex` |
| processor_geoip | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/geoip` |
| processor_gotime | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/gotime` |
| processor_grok | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/grok` |
| processor_json | dual-interface | dual | passthrough unknown | passthrough unknown | `plugins/processor/json` |
| processor_log_to_sls_metric | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/logtoslsmetric` |
| processor_md5 | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/md5` |
| processor_otel_metric | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/otel` |
| processor_otel_trace | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/otel` |
| processor_packjson | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/packjson` |
| processor_pick_key | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/pickkey` |
| processor_rate_limit | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/ratelimit` |
| processor_regex | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/regex` |
| processor_rename | dual-interface | dual | passthrough unknown | passthrough unknown | `plugins/processor/rename` |
| processor_split_char | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/split/char` |
| processor_split_key_value | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/split/keyvalue` |
| processor_split_log_regex | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/split/logregex` |
| processor_split_log_string | dual-interface | dual | passthrough unknown | passthrough unknown | `plugins/processor/split/logstring` |
| processor_split_string | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/split/string` |
| processor_string_replace | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/stringreplace` |
| processor_strptime | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/processor/strptime` |

## Flusher Summary

- Total registered flushers: **15**
- v1-only: **11**
- dual-interface: **4**
- v2-only: **0**

- `v1-only` (11): `flusher_checker`, `flusher_clickhouse`, `flusher_doris`, `flusher_elasticsearch`, `flusher_grpc`, `flusher_kafka`, `flusher_loki`, `flusher_pulsar`, `flusher_sleep`, `flusher_sls`, `flusher_statistics`
- `dual-interface` (4): `flusher_http`, `flusher_kafka_v2`, `flusher_otlp`, `flusher_stdout`
- `v2-only` (0): -
- `unknown` (0): -

## Flusher Matrix

| Plugin | Interface class | Log | Metric | Span | Source directories |
|---|---|---|---|---|---|
| flusher_checker | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/flusher/checker` |
| flusher_clickhouse | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/flusher/clickhouse` |
| flusher_doris | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/flusher/doris` |
| flusher_elasticsearch | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/flusher/elasticsearch` |
| flusher_grpc | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/flusher/grpc` |
| flusher_http | dual-interface | dual | passthrough unknown | passthrough unknown | `plugins/flusher/http` |
| flusher_kafka | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/flusher/kafka` |
| flusher_kafka_v2 | dual-interface | dual | passthrough unknown | passthrough unknown | `plugins/flusher/kafkav2` |
| flusher_loki | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/flusher/loki` |
| flusher_otlp | dual-interface | dual | passthrough unknown | passthrough unknown | `plugins/flusher/opentelemetry` |
| flusher_pulsar | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/flusher/pulsar` |
| flusher_sleep | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/flusher/sleep` |
| flusher_sls | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/flusher/sls` |
| flusher_statistics | v1-only | v1 only | passthrough unknown | passthrough unknown | `plugins/flusher/statistics` |
| flusher_stdout | dual-interface | dual | v2 only | v2 only | `plugins/flusher/stdout` |
