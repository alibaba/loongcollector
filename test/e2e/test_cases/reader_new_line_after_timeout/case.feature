@input
Feature: reader new line after timeout
  Test reader new line after timeout

  @e2e @docker-compose
  Scenario: TestReaderNewLineAfterTimeout
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {reader-new-line-after-timeout-case} local config as below
    """
    enable: true
    inputs:
      - Type: input_file
        FilePaths: 
          - /root/test/a.log
        FlushTimeoutSecs: 1
    """
    Given iLogtail container mount {./a.log} to {/root/test/a.log}
    When start docker-compose {reader_new_line_after_timeout}
    Then there is at least {6} logs