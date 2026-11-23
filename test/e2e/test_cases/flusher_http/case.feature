@flusher
Feature: flusher http
  Test flusher HTTP event export

  @e2e @docker-compose
  Scenario: TestFlusherHTTPExport
    Given {docker-compose} environment
    Given subcribe data from {influxdb} with config
    """
    db_host: http://influxdb:8086
    db_name: mydb
    measurement: multi_values_metrics_mock
    """
    # HTTP writes metric events in InfluxDB line protocol. The group tag
    # db resolves the database in the request query.
    Given {flusher-http-export-case} local config as below
    """
    enable: true
    global:
      InputIntervalMs: 100
    inputs:
      - Type: metric_mock
        IntervalMs: 100
        GroupTags:
          db: mydb
        Tags:
          city: hz
        Fields:
          value: 32
    flushers:
      - Type: flusher_http
        RemoteURL: http://influxdb:8086/write
        Query:
          db: "%{tag.db}"
        Convert:
          Protocol: influxdb
          Encoding: custom
    """
    Given loongcollector depends on containers {["influxdb"]}
    When start docker-compose {flusher_http}
    Then there is at least {10} logs
    # multi_values_metrics_mock carries the configured field "value" (=32). The influxdb
    # subscriber maps the "value" field back to __name__=<measurement> and __value__=32,
    # proving the Metric field reached the target intact through the v2 export path.
    Then the log fields have exact kv
    """
    - __name__: multi_values_metrics_mock
      __value__: "32"
    """
