@flusher
Feature: flusher elasticsearch
  Test flusher elasticsearch

  @e2e @docker-compose
  Scenario: TestFlusherElasticSearch
    Given {docker-compose} environment
    Given subcribe data from {elasticsearch} with config
    """
    address: http://localhost:9200
    username: elastic
    password: BtpoRTeyjmC=ruTIUoNN
    index: default
    """
    # metric_mock emits single- and multi-value Metric events. The flusher
    # writes one row per value with the canonical metric fields.
    Given {flusher-elasticsearch-case} local config as below
    """
    enable: true
    global:
      InputIntervalMs: 100
    inputs:
      - Type: metric_mock
        IntervalMs: 100
        Fields:
          Index: "default"
          Content: "hello"
    flushers:
      - Type: flusher_elasticsearch
        Index: default
        Addresses: ["http://elasticsearch:9200"]
        Authentication:
          PlainText:
            Username: elastic
            Password: BtpoRTeyjmC=ruTIUoNN
    """
    Given loongcollector depends on containers {["elasticsearch"]}
    When start docker-compose {flusher_elasticsearch}
    Then there is at least {10} logs
    # Verify the exact key-value docs produced by the v2 input, not a loose regex.
    # metric_mock emits two Metric identities per cycle; the converter turns them into
    # canonical metric-log docs keyed by __name__/__value__:
    #   - single_metrics_mock              (single-value counter; __value__ is a monotonic
    #                                        counter, so only its identity is pinned)
    #   - multi_values_metrics_mock_Content -> "hello"   (typed string field Content)
    #   - multi_values_metrics_mock_Index   -> "default" (typed string field Index)
    # Asserting the concrete (__name__, __value__) pairs proves the Metric fields reached
    # the target intact rather than merely that some field exists.
    Then the log fields have exact kv
    """
    - __name__: single_metrics_mock
    - __name__: multi_values_metrics_mock_Content
      __value__: hello
    - __name__: multi_values_metrics_mock_Index
      __value__: default
    """
