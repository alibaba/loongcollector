@input
Feature: input container stdio
  Test input container stdio

  @e2e @docker-compose
  Scenario: TestInputContainerStdio
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-container-stdio-case} local config as below
    """
    enable: true
    inputs:
      - Type: input_container_stdio
        ContainerFilters:
          IncludeEnv:
            STDOUT_SWITCH: "true"
        IgnoringStderr: true
        IgnoringStdout: false
    """
    When start docker-compose {input_container_stdio}
    Then there is at least {1} logs
    Then the log tags match kv
    """
    _image_name_: ".*_container:latest$"
    _container_name_: ".*[-_]container[-_]1$"
    _container_ip_: ^\b(?:(?:2(?:[0-4][0-9]|5[0-5])|[0-1]?[0-9]?[0-9])\.){3}(?:(?:2([0-4][0-9]|5[0-5])|[0-1]?[0-9]?[0-9]))\b$
    """
    Then the log fields match kv
    """
    content: "^hello$"
    _time_: ^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}(\.[0-9]+)?([zZ]|([\+-])([01]\d|2[0-3]):?([0-5]\d)?)?$
    _source_: "^stdout$"
    """