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
    # metric_mock emits single- and multi-value Metric events. The flusher
    # writes one row per value with the canonical metric fields.
    Given {flusher-loki-case} local config as below
    """
    enable: true
    global:
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
    # Verify the exact key-value lines produced by the v2 input, not a loose regex.
    # metric_mock emits two Metric identities per cycle; the converter turns them into
    # canonical metric-log lines keyed by __name__/__value__, each carrying the configured
    # tag as __labels__ "name#$#hello":
    #   - single_metrics_mock             (single-value counter; __value__ is a monotonic
    #                                       counter, so only its identity + labels are pinned)
    #   - multi_values_metrics_mock_value -> "log contents" (typed string field value)
    # Asserting the concrete (__name__, __value__, __labels__) triples proves the Metric
    # fields and their tag dimension reached the target intact.
    Then the log fields have exact kv
    """
    - __name__: single_metrics_mock
      __labels__: name#$#hello
    - __name__: multi_values_metrics_mock_value
      __value__: log contents
      __labels__: name#$#hello
    """
