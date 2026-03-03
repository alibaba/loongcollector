@input
Feature: input static file docker
  Test input_static_file_onetime collect static files from existing container

  @e2e @docker-compose
  Scenario: TestInputStaticFileOnetimeFromContainer
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given loongcollector depends on containers {["container"]}
    Given {input-static-file-docker-case} onetime pipeline local config as below
    """
    enable: true
    global:
      ExcutionTimeout: 600
      UsingOldContentTag: true
      DefaultLogQueueSize: 10
    inputs:
      - Type: input_static_file_onetime
        FilePaths:
          - /root/test/**/a*.log
        MaxDirSearchDepth: 10
        EnableContainerDiscovery: true
        ContainerFilters:
          IncludeEnv:
            STDOUT_SWITCH: "true"
    """
    When start docker-compose {input_static_file_docker}
    Then there is at least {1000} logs
    Then the log tags match kv
    """
    "__path__": "^/root/test/a/b/c/d/axxxxxxx.log$"
    """
    Then the log fields match kv
    """
    content: "^\\d+===="
    """
