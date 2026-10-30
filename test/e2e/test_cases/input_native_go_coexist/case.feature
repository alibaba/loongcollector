@input
Feature: native input with Go processor/flusher
  A3 removed the native-input whitelist/blacklist gate. This scenario
  proves a previously blacklisted native input can load and reach a Go
  processor plus the default Go flusher_grpc (injected by the subscriber).

  Same-pipeline multi input_file stays rejected (FileServer keying,
  tracked in #2644). Two independent input_file pipelines are existing
  behavior and are not covered here.

  @e2e @docker-compose
  Scenario: TestHostMetaWithGoProcessorFlusher
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-host-meta-go-case} local config as below
    """
    enable: true
    inputs:
      - Type: input_host_meta
        Interval: 5
    processors:
      - Type: processor_add_fields
        Fields:
          e2e_go_processor: "1"
    """
    When start docker-compose {input_native_go_coexist}
    Then there is at least {1} logs
    Then there is at least {1} logs with filter key {e2e_go_processor} value {1}
    Then there is at least {1} logs with filter key {__entity_type__} value {acs.ecs.process}
