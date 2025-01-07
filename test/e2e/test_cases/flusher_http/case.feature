@flusher
Feature: flusher http
  Test flusher http

  @e2e @docker-compose
  Scenario: TestFlusherHTTP
    Given {docker-compose} environment
    Given subcribe data from {influxdb} with config
    """
    db_host: http://influxdb:8086
    db_name: mydb
    measurement: weather
    """
    Given {flusher-http-case} local config as below
    """
    enable: true
    inputs:
      - Type: metric_mock
        IntervalMs: 100
        Fields:
          __name__: weather
          __value__: "32"
          __labels__: "city#$#hz"
          __tag__:db: mydb
    aggregators:
      - Type: aggregator_content_value_group
        GroupKeys:
          - '__tag__:db'
    flushers:
      - Type: flusher_http
        RemoteURL: http://influxdb:8086/write
        Query:
          db: "%{tag.db}"
        Convert:
          Protocol: influxdb
          Encoding: custom
        RequestInterceptors:
          - Type: ext_request_breaker
            FailureRatio: 0.1
    """
    Given loongcollector depends on containers {["influxdb"]}
    When start docker-compose {flusher_http}
    Then there is at least {10} logs
    Then the log fields match kv
    """
    __name__: "weather"
    __value__: "32"
    __labels__: "city#\\$#hz"
    __type__: "float"
    __time_nano__: "\\d+"
    """
  
