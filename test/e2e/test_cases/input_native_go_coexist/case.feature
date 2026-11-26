@input
Feature: native input with Go processor/flusher
  Registered native inputs can coexist with Go plugins.
  These scenarios send native Logs and Metrics through a Go processor and the
  Go flusher_grpc injected by the subscriber on the default event pipeline.

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

  @e2e @docker-compose
  Scenario: TestNativeMetricWithGoProcessorFlusher
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-native-metric-go-case} local config as below
    """
    enable: true
    inputs:
      - Type: input_host_monitor
        Interval: 5
        EnableCPU: false
        EnableSystem: true
        EnableMemory: false
        EnableDisk: false
        EnableProcess: false
        EnableNet: false
    processors:
      - Type: processor_add_fields
        Fields:
          e2e_go_processor: "1"
    """
    When start docker-compose {input_native_go_coexist}
    Then there is at least {1} logs
    Then the log fields match kv
    """
    "__name__": ".+"
    "__value__": "[0-9].*"
    """
