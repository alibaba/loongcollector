@input
Feature: input static file
  Test input static file

  @e2e @docker-compose
  Scenario: TestInputStaticFile
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-static-file-case} local config as below
    """
    enable: true
    global:
      UsingOldContentTag: true
      DefaultLogQueueSize: 10
    inputs:
      - Type: input_file
        FilePaths: 
          - "/root/test/**/a*.log"
        MaxDirSearchDepth: 10
    """
    Given loongcollector container mount {./a.log} to {/root/test/1/2/3/axxxx.log}
    When start docker-compose {input_static_file}
    Then there is at least {1000} logs
    Then the log fields match kv
    """
    "__tag__:__path__": "^/root/test/1/2/3/axxxx.log$"
    content: "^\\d+===="
    """