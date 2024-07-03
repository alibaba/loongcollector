@input
Feature: input docker event
  Test input docker event

  @e2e @docker-compose
  Scenario: TestInputDockerEvent
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-container-stdio-case} local config as below
    """
    enable: true
    inputs:
      - Type: service_docker_event
        IntervalMs: 1000
    """
    When start docker-compose {input_docker_event}
    Then there is at least {2} logs
    Then the log fields match kv
    """
    _time_nano_: "^[0-9]*$"
    _action_: "die|disconnect|exec_create|exec_start|health_status: healthy"
    _type_: "container|network"
    _id_: "^[a-z0-9]*$"
    """
