@input
Feature: input observer dns
  Test input observer dns

  @e2e @docker-compose @ebpf
  Scenario: TestInputObserverDNS
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-observer-dns-case} local config as below
    """
    enable: true
    inputs:
      - Type: input_observer_network
        Common:
          FlushOutL7Interval: 1
          FlushOutL4Interval: 100
          IncludeCmdRegex: "^.*simulatorclient.*$"
          IncludeProtocols: [ "DNS" ]
        EBPF:
          Enabled: true
    processors:
      - Type: processor_default
    """
    When start docker-compose {input_observer_dns}
    Then wait
    Then there is at least {100} logs with filter key {req_resource} value {sls.test.ebpf}