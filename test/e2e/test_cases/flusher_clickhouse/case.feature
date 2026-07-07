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
    #     - output of flusher: converter.ToByteStreamV2 -> 1 clickhouse row per Metric,
    #       each serialized into the __pipeline_passthrough__ blob
    #       (eventType/name/tags/value(s)/typedValues), never silently dropped.
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
    # Verify keys AND values: every flushed row carries a __pipeline_passthrough__
    # blob whose value contains the real metric identity emitted by metric_mock
    # (eventType + one of the two metric names), proving the Metric events reached
    # the target intact rather than only that a column exists.
    Then the log fields match kv
    """
    __pipeline_passthrough__: '"eventType":"metric".*"name":"(single_metrics_mock|multi_values_metrics_mock)"'
    """

