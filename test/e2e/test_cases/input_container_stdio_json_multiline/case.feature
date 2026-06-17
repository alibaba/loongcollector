@input
Feature: input container stdio json multiline
  Test input container stdio with JSON multiline merge mode.
  Multi-line JSON blocks printed to stdout should be merged into single log events.

  @e2e @docker-compose
  Scenario: TestInputContainerStdioJsonMultiline
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-container-stdio-json-multiline-case} local config as below
    """
    enable: true
    inputs:
      - Type: input_container_stdio
        ContainerFilters:
          IncludeEnv:
            STDOUT_SWITCH: "true"
        IgnoringStderr: true
        IgnoringStdout: false
        Multiline:
          Mode: JSON
    """
    When start docker-compose {input_container_stdio_json_multiline}
    Then there is at least {2} logs
    Then the log tags match kv
    """
    _image_name_: ".*[-_]container:latest$"
    _container_name_: ".*[-_]container[-_]1$"
    """
    Then the log fields match kv
    """
    content: ".*name.*test_multiline.*"
    _source_: "^stdout$"
    """
