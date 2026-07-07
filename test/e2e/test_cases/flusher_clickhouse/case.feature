@flusher
Feature: flusher clickhouse
  Test flusher clickhouse

  @e2e @docker-compose
  Scenario: TestFlusherClickhouse
    Given {docker-compose} environment
    Given subcribe data from {clickhouse} with config
    """
    address: http://clickhouse:9000
    database: default
    table: demo
    """
    # Pipeline version switch: global.StructureType=v2 routes metric_mock through
    # FlusherV2.Export instead of the v1 Flush path. Structures at the flusher boundary:
    #   v1 pipeline (StructureType absent/v1):
    #     - input  to flusher: []*protocol.LogGroup. metric_mock.Collect() emits Log
    #       events via AddData, contents = {Index, _name, _value}.
    #     - output of flusher: converter.ToByteStream(LogGroup) -> 1 clickhouse row per
    #       Log, contents = {Index, _name, _value}.
    #   v2 pipeline (StructureType=v2, this case):
    #     - input  to flusher: []*models.PipelineGroupEvents. metric_mock.Read() emits
    #       2 Metric events/cycle: single_metrics_mock (counter, single value) and
    #       multi_values_metrics_mock (untyped; values{Index} + typedValues{_name,_value}).
    #     - output of flusher: converter.ToByteStreamV2 converts each Metric structurally
    #       into canonical metric-log rows {__name__, __labels__, __value__, __time_nano__}
    #       (one row per value / typed-value), never an opaque pass-through blob and never
    #       silently dropped. single_metrics_mock -> __name__=single_metrics_mock;
    #       multi_values_metrics_mock -> __name__=multi_values_metrics_mock_{Index,_name,_value}.
    Given {flusher-clickhouse-case} local config as below
    """
    enable: true
    global:
      StructureType: v2
      InputIntervalMs: 100
    inputs:
      - Type: metric_mock
        IntervalMs: 100
        Fields:
          _name: "hello"
          _value: "log contents"
    flushers:
      - Type: flusher_clickhouse
        Addresses: ["clickhouse:9000"]
        Authentication:
          PlainText:
            Database: default
            Username:
            Password:
        Table: demo
        BufferMinTime: 10000000
        BufferMaxTime: 100000000
        BufferMinRows: 10000000
        BufferMaxRows: 100000000
        BufferMinBytes: 10000000
        BufferMaxBytes: 100000000
    """
    Given loongcollector depends on containers {["clickhouse"]}
    When start docker-compose {flusher_clickhouse}
    Then there is at least {10} logs
    # Verify keys AND values on the structured metric-log rows produced by the v2 input:
    # every flushed row is a canonical metric log whose __name__ equals one of the two
    # metric identities metric_mock emits and whose __value__ is non-empty. This asserts
    # the Metric events reached the target as first-class metric fields (key + value),
    # not merely that a column exists.
    Then the log fields match kv
    """
    __name__: '^(single_metrics_mock|multi_values_metrics_mock)'
    __value__: '.+'
    """
