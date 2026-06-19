@input
Feature: input internal metrics prometheus
  Test self-monitor metrics can be sent to Go Pipeline (flusher_prometheus)

  @e2e @docker-compose
  Scenario: TestInputInternalMetricsToGoPipeline
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-internal-metrics-prometheus-case} local config as below
    """
    enable: true
    inputs:
      - Type: input_internal_metrics
        Agent:
          Enable: true
          Interval: 1
        Runner:
          Enable: true
          Interval: 1
    flushers:
      - Type: flusher_grpc
        Endpoint: grpc:9000
        Retry:
          Enable: false
    """
    When start docker-compose {input_internal_metrics_prometheus}
    Then there is at least {5} logs
    Then the log fields match as below
    """
    - __name__
    - __labels__
    - __time_nano__
    - __value__
    """
