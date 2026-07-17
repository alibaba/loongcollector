@flusher
Feature: flusher doris
  Test flusher doris

  @e2e @docker-compose @disabled
  Scenario: TestFlusherDoris
    Given {docker-compose} environment
    Given subcribe data from {doris} with config
    """
    address: http://doris:9030
    username: root
    password: ""
    database: test_db
    table: test_table
    """
    # Pipeline version switch: global.StructureType=v2 routes metric_mock through
    # FlusherV2.Export instead of the v1 Flush path. Structures at the flusher boundary:
    #   v1 pipeline (StructureType absent/v1):
    #     - input  to flusher: []*protocol.LogGroup. metric_mock.Collect() emits Log
    #       events via AddData, contents = {Index, content, value}.
    #     - output of flusher: converter.ToByteStream(LogGroup) -> 1 doris row per Log,
    #       flattened columns = {Index, content, value}.
    #   v2 pipeline (StructureType=v2, this case):
    #     - input  to flusher: []*models.PipelineGroupEvents. metric_mock.Read() emits
    #       2 Metric events/cycle: single_metrics_mock (counter, single value) and
    #       multi_values_metrics_mock (untyped; values{Index} + typedValues{content,value}).
    #     - output of flusher: converter.ToByteStreamV2 (custom_single_flatten) converts each
    #       Metric structurally into flattened canonical metric-log columns
    #       {__name__, __labels__, __value__, __time_nano__} (one row per value / typed-value),
    #       never an opaque pass-through blob and never silently dropped. single_metrics_mock
    #       -> __name__=single_metrics_mock; multi_values_metrics_mock ->
    #       __name__=multi_values_metrics_mock_{Index,content,value}.
    Given {flusher-doris-case} local config as below
    """
    enable: true
    global:
      StructureType: v2
      InputIntervalMs: 100
    inputs:
      - Type: metric_mock
        IntervalMs: 100
        Fields:
          content: "hello doris"
          value: "log contents"
    flushers:
      - Type: flusher_doris
        Addresses: ["http://doris:8040"]
        Database: test_db
        Table: test_table
        Authentication:
          PlainText:
            Username: root
            Password: ""
        Convert:
          Protocol: "custom_single_flatten"
          Encoding: "json"
    """
    Given loongcollector depends on containers {["doris", "init-test-env"]}
    When start docker-compose {flusher_doris}
    Then there is at least {10} logs
    # Verify the exact key-value rows produced by the v2 input, not a loose regex.
    # metric_mock emits two Metric identities per cycle; the converter flattens them into
    # canonical metric-log columns keyed by __name__/__value__:
    #   - single_metrics_mock              (single-value counter; __value__ is a monotonic
    #                                        counter, so only its identity is pinned)
    #   - multi_values_metrics_mock_content -> "hello doris"  (typed string field content)
    #   - multi_values_metrics_mock_value   -> "log contents" (typed string field value)
    # Asserting the concrete (__name__, __value__) pairs proves the Metric fields reached
    # the target intact rather than merely that some column exists.
    Then the log fields have exact kv
    """
    - __name__: single_metrics_mock
    - __name__: multi_values_metrics_mock_content
      __value__: hello doris
    - __name__: multi_values_metrics_mock_value
      __value__: log contents
    """
