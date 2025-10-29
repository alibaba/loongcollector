@flusher
Feature: flusher doris
  Test flusher doris

  @e2e @docker-compose
  Scenario: TestFlusherDoris
    Given {docker-compose} environment
    Given subcribe data from {doris} with config
    """
    address: http://doris:9030
    username: root
    password: ""
    database: test_db
    table: test_table
    """
    Given {flusher-doris-case} local config as below
    """
    enable: true
    inputs:
      - Type: metric_mock
        IntervalMs: 100
        Fields:
          content: "hello doris"
          value: "log contents"
    flushers:
      - Type: flusher_doris
        Addresses: ["http://doris:8030"]
        Database: test_db
        Table: test_table
        Authentication:
          PlainText:
            Username: root
            Password: ""
        Convert:
          Protocol: "custom_single_flatten"
          Encoding: "json"
    """
    Given loongcollector depends on containers {["doris", "init-test-env"]}
    When start docker-compose {flusher_doris}
    Then there is at least {10} logs
    Then the log fields match kv
    """
    content: "hello doris"
    value: "log contents"
    """
  

