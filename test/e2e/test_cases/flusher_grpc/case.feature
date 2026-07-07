@flusher
Feature: flusher grpc
  Test flusher grpc FlusherV2.Export

  @e2e @docker-compose
  Scenario: TestFlusherGrpcExportV2
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {flusher-grpc-export-v2-case} local config as below
    """
    enable: true
    global:
      StructureType: v2
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
    Then the log fields match as below
    """
    - __pipeline_passthrough__
    """
