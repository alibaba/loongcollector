@flusher
Feature: flusher loki
  Test flusher loki

  @e2e @docker-compose
  Scenario: TestFlusherLoki
    Given {docker-compose} environment
    Given subcribe data from {loki} with config
    """
    address: http://loki:3100
    tenant_id: loongcollector
    target_labels:
      loki_name: hello
      source: loongcollector
    """
    # Pipeline version switch: global.StructureType=v2 routes metric_mock through
    # FlusherV2.Export instead of the v1 Flush path. Structures at the flusher boundary:
    #   v1 pipeline (StructureType absent/v1):
    #     - input  to flusher: []*protocol.LogGroup. metric_mock.Collect() emits Log
    #       events via AddData, contents = {Index, value}, tags = {name}.
    #     - output of flusher: converter.ToByteStreamWithSelectedFields(LogGroup) -> loki
    #       push stream; log line contents = {Index, value}, labels from DynamicLabels.
    #   v2 pipeline (StructureType=v2, this case):
    #     - input  to flusher: []*models.PipelineGroupEvents. metric_mock.Read() emits
    #       2 Metric events/cycle: single_metrics_mock (counter, single value) and
    #       multi_values_metrics_mock (untyped; values{Index} + typedValues{value}),
    #       both tagged {name=hello}.
    #     - output of flusher: converter.ToByteStreamWithSelectedFieldsV2 converts each
    #       Metric structurally into canonical metric-log lines
    #       {__name__, __labels__, __value__, __time_nano__} (one line per value /
    #       typed-value), never an opaque pass-through blob and never silently dropped. The
    #       metric tag becomes __labels__=name#$#hello on every line. single_metrics_mock
    #       -> __name__=single_metrics_mock; multi_values_metrics_mock ->
    #       __name__=multi_values_metrics_mock_{Index,value}.
    Given {flusher-loki-case} local config as below
    """
    enable: true
    global:
      StructureType: v2
      InputIntervalMs: 100
    inputs:
      - Type: metric_mock
        IntervalMs: 100
        Tags:
          name: "hello"
        Fields:
          value: "log contents"
    flushers:
      - Type: flusher_loki
        Convert:
          TagFieldsRename:
            loki_name: name
        URL: http://loki:3100/loki/api/v1/push
        TenantID: loongcollector
        MaxMessageWait: 100000000
        MaxMessageBytes: 1024
        Timeout: 1000000000000
        MinBackoff: 100000000000
        MaxBackoff: 1000000000000
        MaxRetries: 10
        DynamicLabels:
          - tag.loki_name
        StaticLabels:
          source: loongcollector
    """
    Given loongcollector depends on containers {["loki"]}
    When start docker-compose {flusher_loki}
    Then there is at least {10} logs
    # Verify keys AND values on the structured metric-log lines produced by the v2 input:
    # every flushed line is a canonical metric log whose __name__ equals one of the two
    # metric identities metric_mock emits, whose __value__ is non-empty, and whose
    # __labels__ carries the configured tag (name=hello) that rides on both Metric events.
    # This asserts the Metric events reached the target as first-class metric fields with
    # their key/value dimensions intact.
    Then the log fields match kv
    """
    __name__: '^(single_metrics_mock|multi_values_metrics_mock)'
    __value__: '.+'
    __labels__: 'name#\$#hello'
    """
