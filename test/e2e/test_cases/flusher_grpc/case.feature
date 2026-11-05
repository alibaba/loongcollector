@flusher
Feature: flusher grpc
  Test default event pipeline to gRPC

  @e2e @docker-compose
  Scenario: TestFlusherGrpcDefaultEventExport
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    # The default event runner emits typed Metric events. Only the final gRPC
    # encoder builds protocol.LogGroup to preserve LogReportService.Collect.
    Given {flusher-grpc-event-case} local config as below
    """
    enable: true
    global:
      InputIntervalMs: 1000
    inputs:
      - Type: metric_mock
        IntervalMs: 1000
        Tags:
          tag1: aaaa
          tag2: bbb
        Fields:
          content: xxxxxx
    """
    When start docker-compose {flusher_grpc}
    Then there is at least {10} logs
    # Verify the exact key-value entries produced by the event input, not a loose regex.
    # metric_mock emits two Metric identities per cycle; Export turns them into canonical
    # metric-log entries keyed by __name__/__value__, each carrying the configured tags as
    # a sorted __labels__ string "tag1#$#aaaa|tag2#$#bbb":
    #   - single_metrics_mock               (single-value counter; __value__ is a monotonic
    #                                         counter, so only its identity + labels are pinned)
    #   - multi_values_metrics_mock_content -> "xxxxxx" (typed string field content)
    # Asserting the concrete (__name__, __value__, __labels__) triples proves the Metric
    # fields and their tag dimensions reached the target intact.
    Then the log fields have exact kv
    """
    - __name__: single_metrics_mock
      __labels__: tag1#$#aaaa|tag2#$#bbb
    - __name__: multi_values_metrics_mock_content
      __value__: xxxxxx
      __labels__: tag1#$#aaaa|tag2#$#bbb
    """
