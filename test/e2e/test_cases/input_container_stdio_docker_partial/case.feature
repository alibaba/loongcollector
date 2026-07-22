@input
Feature: input container stdio docker partial
  Test that Docker json-file 16KB partial log fragments are correctly merged.
  When a single log line exceeds 16KB, Docker splits it into multiple partial entries.
  The fix ensures ParseDockerJsonLogLine detects partial lines (no trailing \n)
  and MergeByFlag reassembles them into the original complete line.

  @e2e @docker-compose
  Scenario: TestInputContainerStdioDockerPartial
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-container-stdio-docker-partial-case} local config as below
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
    When start docker-compose {input_container_stdio_docker_partial}
    Then there is at least {1} logs
    Then the log tags match kv
    """
    _image_name_: ".*[-_]container:latest$"
    _container_name_: ".*[-_]container[-_]1$"
    """
    Then the log fields match kv
    """
    content: "^LONG_LINE_START_A+_LONG_LINE_END$"
    _source_: "^stdout$"
    """
