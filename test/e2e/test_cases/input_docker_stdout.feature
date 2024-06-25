@input
Feature: input docker stdout
  Test input docker stdout

  @e2e @docker-compose
  Scenario: TestInputDockerStdout
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-docker-stdout-case} config as below
    """
    enable: true
    inputs:
      - Type: service_docker_stdout
        IncludeEnv:
          STDOUT_SWITCH: "true"
    """
    When start docker-compose dependencies {input_docker_stdout}
    Then there is at least {1} logs
    Then the log fields match kv
    """
    _time_: ^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}(\.[0-9]+)?([zZ]|([\+-])([01]\d|2[0-3]):?([0-5]\d)?)?$
    content: "^hello$"
    _source_: "^stdout$"
    _image_name_: "^e2e_container:latest$"
    _container_name_: "^e2e-container-1$"
    _container_ip_: ^\b(?:(?:2(?:[0-4][0-9]|5[0-5])|[0-1]?[0-9]?[0-9])\.){3}(?:(?:2([0-4][0-9]|5[0-5])|[0-1]?[0-9]?[0-9]))\b$
    """
  
  @e2e @docker-compose
  Scenario: TestInputDockerStdoutMultiline
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-docker-stdout-multiline-case} config as below
    """
    enable: true
    inputs:
      - Type: service_docker_stdout
        IncludeEnv:
          STDOUT_SWITCH: "true"
        Stdout: true
        BeginLineRegex: "today"
    """
    When start docker-compose dependencies {input_docker_stdout_multiline}
    Then there is at least {1} logs
    Then the log fields match kv
    """
    _time_: ^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}(\.[0-9]+)?([zZ]|([\+-])([01]\d|2[0-3]):?([0-5]\d)?)?$
    content: "^today\nhello$"
    _source_: "^stdout$"
    _image_name_: "^e2e_container:latest$"
    _container_name_: "^e2e-container-1$"
    _container_ip_: ^\b(?:(?:2(?:[0-4][0-9]|5[0-5])|[0-1]?[0-9]?[0-9])\.){3}(?:(?:2([0-4][0-9]|5[0-5])|[0-1]?[0-9]?[0-9]))\b$
    """