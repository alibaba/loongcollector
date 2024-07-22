@input
Feature: input observer http
  Test input observer http

  @e2e @docker-compose @ebpf
  Scenario: TestInputObserverHTTP
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-observer-http-case} local config as below
    """
    enable: true
    inputs:
      - Type: input_observer_network
        Common:
          FlushOutL7Interval: 1
          FlushOutL4Interval: 100
          IncludeCmdRegex: "^.*simulatorclient.*$"
          IncludeProtocols: [ "HTTP" ]
        EBPF:
          Enabled: true
    processors:
      - Type: processor_default
    """
    When start docker-compose {input_observer_http}
    Then there is at least {100} logs