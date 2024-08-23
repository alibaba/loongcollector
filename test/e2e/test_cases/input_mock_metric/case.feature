@input
Feature: input mock metric
  Test input mock metric

  @e2e @docker-compose
  Scenario: TestInputMockMetric
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-mock-metric-case} local config as below
    """
    enable: true
    inputs: 
      - Type: metric_mock
        IntervalMs: 1000
        OpenPrometheusPattern: true
        Tags:
          tag1: aaaa
          tag2: bbb
        Fields:
          content: xxxxxx
          time: 2017.09.12 20:55:36
    """
    When start docker-compose {input_mock_metric}
    Then there is at least {15} logs
    Then the log fields match as below
    """
    - __labels__
    - __time_nano__
    - __value__
    - __name__
    """
    Then the log labels match as below
    """
    - content
    - tag1
    - tag2
    - time
    """