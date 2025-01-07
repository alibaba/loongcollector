@input
Feature: input canal
  Test input canal

  @e2e @docker-compose @input-canal
  Scenario: TestInputCanal
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-canal-case} local config as below
    """
    enable: true
    inputs:
      - Type: service_canal
        User: root
        Host: mysql
        ServerId: 9966
        Password: root
        IncludeTables:
          - mysql\\.specialalarmtest
        TextToString: true
        EnableDDL: true
    """
    Given loongcollector depends on containers {["mysql"]}
    When start docker-compose {input_canal}
    When generate {10} http logs, with interval {10}ms, url: {http://client:10999/add/data}, method: {GET}, body:
    """
    """
    Then there is at least {10} logs
    Then the log fields match as below
    """
    - _db_
    - _gtid_
    - _event_
    - _filename_
    - _host_
    - _offset_
    - _event_
    """
  
