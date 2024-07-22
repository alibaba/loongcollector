@input
Feature: input mssql
  Test input mssql

  @e2e @docker-compose
  Scenario: TestInputMssql
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-mssql-case} local config as below
    """
    enable: true
    inputs:
      - Type: service_mssql
        Address: mssql
        CheckPoint: true
        CheckPointColumn: id
        CheckPointColumnType: int
        CheckPointSavePerPage: true
        CheckPointStart: "0"
        Database: LogtailTest
        IntervalMs: 1000
        Limit: true
        MaxSyncSize: 100
        PageSize: 100
        User: sa
        Password: MSsqlpa#1word
        StateMent: "select * from LogtailTestTable where id > ? ORDER BY id"
    """
    Given iLogtail depends on containers {["setup"]}
    When start docker-compose {input_mssql}
    Then there is at least {4} logs
    Then the log fields match
    """
    - id
    - name 
    - quantity 
    """