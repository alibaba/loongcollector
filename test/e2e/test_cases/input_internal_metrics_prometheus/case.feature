@flusher
Feature: input internal metrics to prometheus
  Verify native input_internal_metrics reaches flusher_prometheus via ProcessPipelineEventGroup bridge.

  @e2e @docker-compose
  Scenario: TestInternalMetricsPrometheusRemoteWrite
    Given {docker-compose} environment
    Given subcribe data from {prometheus} with config
    """
    prometheus_url: http://prometheus:9090
    query: agent_cpu
    """
    Given {input-internal-metrics-prometheus-case} local config as below
    """
    enable: true
    global:
      StructureType: v2
    inputs:
      - Type: input_internal_metrics
        Agent:
          Enable: true
          Interval: 1
        Pipeline:
          Enable: true
          Interval: 1
        Plugin:
          Enable: true
          Interval: 1
        Component:
          Enable: true
          Interval: 1
    flushers:
      - Type: flusher_prometheus
        Endpoint: "http://prometheus:9090/api/v1/write"
        SeriesLimit: 1000
        Compression: "snappy"
        Concurrency: 1
        QueueCapacity: 4096
        DropEventWhenQueueFull: false
        Timeout: "10s"
        Retry:
          Enable: true
          MaxRetryTimes: 3
          InitialDelay: "1s"
          MaxDelay: "30s"
    """
    Given loongcollector depends on containers {["prometheus"]}
    When start docker-compose {input_internal_metrics_prometheus}
    Then wait {90} seconds
    Then there is at least {1} logs
    Then the log fields match kv
    """
    __name__: agent_cpu
    """
