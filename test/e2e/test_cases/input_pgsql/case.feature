@input
Feature: input pgsql
  Test input pgsql

  @e2e @docker-compose
  Scenario: TestInputPgsql
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-pgsql-case} local config as below
    """
    enable: true
    inputs:
      - Type: service_pgsql
        Address: pgsql
        CheckPoint: true
        CheckPointColumn: id
        CheckPointColumnType: int
        CheckPointSavePerPage: true
        CheckPointStart: "0"
        Database: postgres
        IntervalMs: 1000
        Limit: true
        MaxSyncSize: 100
        PageSize: 100
        User: postgres
        Password: postgres
        StateMent: "select * from specialalarmtest where id > $1"
    """
    Given iLogtail depends on containers {["pgsql"]}
    When start docker-compose {input_pgsql}
    Then there is at least {10} logs
    Then the log fields match
    """
    - id
    - time 
    - alarmtype
    - ip
    - count
    """