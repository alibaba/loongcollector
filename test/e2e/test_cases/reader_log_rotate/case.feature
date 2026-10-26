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
    Given loongcollector container mount {./volume} to {/root/test}
    When start docker-compose {reader_log_rotate}
    Then there is at least {6} logs
    Then the self-monitor plugin_source labels match kv
    """
    file_name: ".*/root/test/simple.log$"
    file_dev: "^[0-9]+$"
    file_inode: "^[0-9]+$"
    """
    Then the self-monitor plugin_source labels do not have container keys