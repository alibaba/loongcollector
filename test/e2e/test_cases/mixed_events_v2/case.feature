@flusher
Feature: mixed log metric span v2
  B6 regression gate (#2605 / Epic #2595 / Discussion #1928): after B3/B4,
  an explicit StructureType: v2 pipeline must carry mixed Log + Metric + Span
  without dropping or mis-typing events. This is the prerequisite for C1.

  Existing e2e cases are single-kind (processor_v2 = Log, flusher_http =
  Metric, input_internal_metrics_prometheus = Metric). This case is the
  first that asserts all three kinds on the v2 path.

  Contract:
    - processor_add_fields mutates Log and pass-through Metric (B4
      ProcessLogEventsOnly)
    - flusher_http serializes Log / Metric / Span via custom_single v2
    - flusher_prometheus keeps Metric identity when Log/Span also reach Export
  Span runs in a sibling v2 pipeline: processor_otel_trace replaces the
  source Log, so it cannot share the pipeline with logs we still assert.

  @e2e @docker-compose
  Scenario: TestMixedEventsFlusherHTTP
    Given {docker-compose} environment
    Given subcribe data from {elasticsearch} with config
    """
    address: http://localhost:9200
    username: elastic
    password: BtpoRTeyjmC=ruTIUoNN
    index: mixed
    """
    Given loongcollector container mount {./log_line.log} to {/root/test/log_line.log}
    Given loongcollector container mount {./otel_trace.json} to {/root/test/otel_trace.json}
    # Pipeline 1: Log + Metric in one v2 group path.
    #   - metric_debug_file.Read emits a Log (content="hello mixed log")
    #   - metric_mock.Read emits Metric events (single_metrics_mock +
    #     multi_values_metrics_mock_{Index,value})
    #   - processor_add_fields.Process appends event_kind/source on Log only
    #     and CollectGroupEvents the Metric unchanged (B4 pass-through)
    #   - flusher_http.Export -> ConvertToSingleProtocolStreamV2:
    #       Log -> {content, event_kind, source}
    #       Metric -> {__name__, __labels__, __value__, __time_nano__}
    #     posted as custom_single JSON to Elasticsearch _doc (same envelope
    #     the elasticsearch subscriber already knows how to unwrap).
    Given {mixed-log-metric-http} local config as below
    """
    enable: true
    global:
      StructureType: v2
      InputIntervalMs: 1000
    inputs:
      - Type: metric_debug_file
        InputFilePath: /root/test/log_line.log
      - Type: metric_mock
        IntervalMs: 1000
        Tags:
          city: hz
        Fields:
          value: mixed_metric
    processors:
      - Type: processor_add_fields
        Fields:
          event_kind: log
          source: e2e_mixed
    flushers:
      - Type: flusher_http
        RemoteURL: http://elasticsearch:9200/mixed/_doc
        Convert:
          Protocol: custom_single
          Encoding: json
    """
    # Pipeline 2: OTLP JSON Log -> processor_otel_trace -> native Span.
    # ConvertToSingleProtocolStreamV2 then flattens the Span into
    # {traceID, spanID, name, ...} so the same ES subscriber can assert it.
    Given {mixed-span-http} local config as below
    """
    enable: true
    global:
      StructureType: v2
      InputIntervalMs: 1000
    inputs:
      - Type: metric_debug_file
        InputFilePath: /root/test/otel_trace.json
    processors:
      - Type: processor_otel_trace
        SourceKey: content
        Format: json
    flushers:
      - Type: flusher_http
        RemoteURL: http://elasticsearch:9200/mixed/_doc
        Convert:
          Protocol: custom_single
          Encoding: json
    """
    Given loongcollector depends on containers {["elasticsearch"]}
    When start docker-compose {mixed_events_v2}
    Then there is at least {3} logs
    Then the log fields have exact kv
    """
    - content: hello mixed log
      event_kind: log
      source: e2e_mixed
    - __name__: multi_values_metrics_mock_value
      __value__: mixed_metric
    - name: e2e-mixed-span
      traceID: 5b8aa5a2d2c872e8321cf37308d69df2
      spanID: 051581bf3cbd71c1
    """

  @e2e @docker-compose
  Scenario: TestMixedEventsFlusherPrometheus
    Given {docker-compose} environment
    Given subcribe data from {prometheus} with config
    """
    prometheus_url: http://prometheus:9090
    query: single_metrics_mock
    """
    Given loongcollector container mount {./log_line.log} to {/root/test/log_line.log}
    Given loongcollector container mount {./otel_trace.json} to {/root/test/otel_trace.json}
    # Same mixed Log + Metric v2 pipeline, but Export goes to
    # flusher_prometheus. The prometheus encoder skips non-Metric events
    # (debug-log, no error) and remote-writes the Metric series. Querying
    # single_metrics_mock proves the Metric identity survived the mixed
    # group; Log assertions live in TestMixedEventsFlusherHTTP because
    # Prometheus Remote Write cannot carry Log/Span.
    Given {mixed-log-metric-prometheus} local config as below
    """
    enable: true
    global:
      StructureType: v2
      InputIntervalMs: 1000
    inputs:
      - Type: metric_debug_file
        InputFilePath: /root/test/log_line.log
      - Type: metric_mock
        IntervalMs: 1000
        Tags:
          city: hz
        Fields:
          value: mixed_metric
    processors:
      - Type: processor_add_fields
        Fields:
          event_kind: log
          source: e2e_mixed
    flushers:
      - Type: flusher_prometheus
        Endpoint: "http://prometheus:9090/api/v1/write"
        SeriesLimit: 1000
        Compression: "snappy"
        Concurrency: 1
        QueueCapacity: 4096
        DropEventWhenQueueFull: false
    """
    # Span sibling: native Span events reach flusher_prometheus.Export and
    # are skipped by the encoder. The pipeline must stay up so the Metric
    # assertion below is not a crash-and-restart artifact.
    Given {mixed-span-prometheus} local config as below
    """
    enable: true
    global:
      StructureType: v2
      InputIntervalMs: 1000
    inputs:
      - Type: metric_debug_file
        InputFilePath: /root/test/otel_trace.json
    processors:
      - Type: processor_otel_trace
        SourceKey: content
        Format: json
    flushers:
      - Type: flusher_prometheus
        Endpoint: "http://prometheus:9090/api/v1/write"
        SeriesLimit: 1000
        Compression: "snappy"
        Concurrency: 1
        QueueCapacity: 4096
        DropEventWhenQueueFull: false
    """
    Given loongcollector depends on containers {["prometheus"]}
    When start docker-compose {mixed_events_v2}
    Then there is at least {1} logs
    Then the log fields have exact kv
    """
    - __name__: single_metrics_mock
    """
