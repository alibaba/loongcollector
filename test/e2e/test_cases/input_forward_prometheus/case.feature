@input
Feature: native forward and Prometheus inputs in one pipeline
  Both native inputs send metrics through the same Go Prometheus flusher.

  @e2e @docker-compose
  Scenario: TestForwardAndPrometheusRemoteWrite
    Given {docker-compose} environment
    Given subcribe data from {prometheus} with config
    """
    prometheus_url: http://prometheus:9090
    query: '{__name__=~"e2e_forward_metric|e2e_scraped_metric"}'
    """
    Given {forward-prometheus} local config as below
    """
    enable: true
    inputs:
      - Type: input_forward
        Protocol: LoongSuite
        Endpoint: "0.0.0.0:7899"
      - Type: input_prometheus
        ScrapeConfig:
          job_name: e2e-multi-input
          host_only_mode: true
          scrape_interval: 2s
          scrape_timeout: 1s
          static_configs:
            - targets: ["metrics-source:18080"]
    flushers:
      - Type: flusher_prometheus
        Endpoint: http://prometheus:9090/api/v1/write
    """
    Given loongcollector depends on containers {["prometheus"]}
    When start docker-compose {input_forward_prometheus}
    Then there is at least {2} logs
    Then the log fields have exact kv
    """
    - __name__: e2e_forward_metric
      source: forward
      __value__: "42"
    - __name__: e2e_scraped_metric
      source: scrape
      __value__: "17"
    """
