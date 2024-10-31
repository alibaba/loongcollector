@flusher
Feature: flusher clickhouse
  Test flusher clickhouse

  @e2e @docker-compose
  Scenario: TestFlusherClickhouse
    Given {docker-compose} environment
    Given subcribe data from {clickhouse} with config
    """
    address: http://clickhouse:9000
    database: default
    table: demo
    """
    Given {flusher-clickhouse-case} local config as below
    """
    enable: true
    inputs:
      - Type: metric_mock
        IntervalMs: 100
        Fields:
          _name: "hello"
          _value: "log contents"
    flushers:
      - Type: flusher_clickhouse
        Addresses: ["clickhouse:9000"]
        Authentication:
          PlainText:
            Database: default
            Username:
            Password:
        Table: demo
        BufferMinTime: 10000000
        BufferMaxTime: 100000000
        BufferMinRows: 10000000
        BufferMaxRows: 100000000
        BufferMinBytes: 10000000
        BufferMaxBytes: 100000000
    """
    Given loongcollector depends on containers {["clickhouse"]}
    When start docker-compose {flusher_clickhouse}
    Then there is at least {10} logs
    Then the log fields match kv
    """
    _name: "hello"
    _value: "log contents"
    """
  
