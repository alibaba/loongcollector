@input
Feature: input prometheus
  Test input prometheus

  @e2e @docker-compose
  Scenario: TestInputPrometheus
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-prometheus-case} local config as below
    """
    enable: true
    inputs:
      - Type: service_prometheus
        Yaml: |-
          global:
            scrape_interval: 15s 
            evaluation_interval: 15s 
          scrape_configs:
            - job_name: "prometheus"
              static_configs:
                - targets: ["exporter:18080"]
    """
    When start docker-compose {input_prometheus}
    Then there is at least {10} logs
    Then the log fields match as below
    """
    - __name__
    - __labels__
    - __time_nano__
    - __value__
    """