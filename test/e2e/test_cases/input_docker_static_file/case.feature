@input
Feature: input docker static file
  Test input docker static file

  @e2e @docker-compose
  Scenario: TestInputDockerStaticFile
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-docker-static-file-case} local config as below
    """
    enable: true
    inputs:
      - Type: input_file
        FilePaths:
          - /root/test/**/a*.log
        MaxDirSearchDepth: 10
        EnableContainerDiscovery: true
        ContainerFilters:
          IncludeEnv:
            STDOUT_SWITCH: "true"
    """
    When start docker-compose {input_docker_static_file}
    Then there is at least {1000} logs
    Then the log tags match kv
    """
    "__path__": "^/root/test/a/b/c/d/axxxxxxx.log$"
    """
    Then the log fields match kv
    """
    content: "^\\d+===="
    """