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
    # metric_mock emits single- and multi-value Metric events. The flusher
    # writes one row per value with the canonical metric fields.
    Given {flusher-doris-case} local config as below
    """
    enable: true
    global:
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
