@input
Feature: input mock log
  Test input mock log

  @e2e @docker-compose
  Scenario: TestInputMockLog
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-mock-log-case} local config as below
    """
    enable: true
    inputs: 
      - Type: metric_mock
        IntervalMs: 1000
        Tags:
          tag1: aaaa
          tag2: bbb
        Fields:
          content: xxxxxx
          time: 2017.09.12 20:55:36
    """
    When start docker-compose {input_mock_log}
    Then there is at least {15} logs
    Then the log fields match
    """
    - tag1
    - tag2
    - content
    - time
    """