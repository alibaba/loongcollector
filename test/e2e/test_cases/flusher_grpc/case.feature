@flusher
Feature: flusher grpc
  Test default event pipeline to gRPC

  @e2e @docker-compose
  Scenario: TestFlusherGrpcDefaultEventExport
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    # metric_mock sends numeric and typed fields through the event RPC.
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
    # The E2E subscriber exposes received metrics as rows for this assertion.
    Then the log fields have exact kv
    """
    - __name__: single_metrics_mock
      __labels__: tag1#$#aaaa|tag2#$#bbb
    - __name__: multi_values_metrics_mock_content
      __value__: xxxxxx
      __labels__: tag1#$#aaaa|tag2#$#bbb
    """
