@input
Feature: input mysql
  Test input mysql

  @e2e @docker-compose
  Scenario: TestInputMysql
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-mysql-case} local config as below
    """
    enable: true
    inputs:
      - Type: service_mysql
        Address: mysql:3306
        User: root
        Password: root
        Database: mysql
        Limit: true
        PageSize: 100
        MaxSyncSize: 100
        StateMent: "select * from help_keyword where help_keyword_id > ?"
        CheckPoint: true
        CheckPointColumn: help_keyword_id
        CheckPointColumnType: int
        CheckPointSavePerPage: true
        CheckPointStart: "0"
        IntervalMs: 1000
    """
    Given loongcollector depends on containers {["mysql"]}
    When start docker-compose {input_mysql}
    Then there is at least {500} logs
    Then the log fields match as below
    """
    - help_keyword_id
    - name
    """
    Then the log fields match kv
    """
    help_keyword_id: "\\d+"
    name: "[A-Z\\(\\-\\<\\>]+"
    """