@input
Feature: input docker event
  Test input docker event

  @e2e @docker-compose
  Scenario: TestInputDockerEvent
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-docker-event-case} local config as below
    """
    enable: true
    inputs:
      - Type: service_docker_event
        IntervalMs: 1000
    """
    When start docker-compose {input_docker_event}
    Then there is at least {2} logs
    Then there is at least {1} logs with filter key {_action_} value {die}
    Then the log fields match kv
    """
    _time_nano_: "^[0-9]+$"
    _id_: "^[a-f0-9]{64}$"
    """
    Then the log fields have exact kv
    """
    - _action_: "die"
      _type_: "container"
      name: "input-docker-event-target"
      image: "input-docker-event-target:latest"
      exitCode: "0"
    """
