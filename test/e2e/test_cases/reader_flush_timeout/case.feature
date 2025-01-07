@input
Feature: reader flush timeout
  Test reader flush timeout

  @e2e @docker-compose
  Scenario: TestReaderFlushTimeout
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {reader-flush-timeout-case} local config as below
    """
    enable: true
    inputs:
      - Type: input_file
        FilePaths: 
          - /root/test/simple.log
        FlushTimeoutSecs: 1
    """
    Given loongcollector container mount {./a.log} to {/root/test/simple.log}
    When start docker-compose {reader_flush_timeout}
    Then there is at least {5} logs