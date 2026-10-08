@input
Feature: reader deleted
  Test reader deleted

  @e2e @docker-compose
  Scenario: TestReaderDeleted
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {reader-deleted-case} local config as below
    """
    enable: true
    inputs:
      - Type: input_file
        FilePaths: 
          - /root/test/simple.log
        FlushTimeoutSecs: 3
    """
    Given loongcollector container mount {./volume} to {/root/test}
    When start docker-compose {reader_deleted}
    Then there is at least {1} logs
    # The file is removed a few seconds after it is written, and the reader is
    # destroyed before the 60s self-monitor flush, so plugin_source is not exported.
    # Host-file cases that keep the file assert that container keys are absent.