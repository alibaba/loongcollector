@input
Feature: reader log rotate
  Test reader log rotate

  @e2e @docker-compose
  Scenario: TestReaderLogRotate
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {reader-log-rotate-case} local config as below
    """
    enable: true
    inputs:
      - Type: input_file
        FilePaths: 
          - /root/test/simple.log
        FlushTimeoutSecs: 2
    """
    Given iLogtail container mount {./volume} to {/root/test}
    When start docker-compose {reader_log_rotate}
    Then there is at least {6} logs