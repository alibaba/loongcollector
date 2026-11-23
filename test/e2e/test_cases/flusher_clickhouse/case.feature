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
    # metric_mock emits single- and multi-value Metric events. The flusher
    # writes one row per value with the canonical metric fields.
    Given {flusher-clickhouse-case} local config as below
    """
    enable: true
    global:
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
    # Verify the exact key-value rows produced by the v2 input, not a loose regex.
    # metric_mock emits two Metric identities per cycle; the converter turns them into
    # canonical metric-log rows keyed by __name__/__value__:
    #   - single_metrics_mock            (single-value counter; __value__ is a monotonic
    #                                      counter, so only its identity is pinned)
    #   - multi_values_metrics_mock__name  -> "hello"        (typed string field _name)
    #   - multi_values_metrics_mock__value -> "log contents" (typed string field _value)
    # Asserting the concrete (__name__, __value__) pairs proves the Metric fields reached
    # the target intact rather than merely that some column exists.
    Then the log fields have exact kv
    """
    - __name__: single_metrics_mock
    - __name__: multi_values_metrics_mock__name
      __value__: hello
    - __name__: multi_values_metrics_mock__value
      __value__: log contents
    """
