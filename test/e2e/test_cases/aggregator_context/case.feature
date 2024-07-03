@aggregator
Feature: aggregator context
  Test aggregator context

  @e2e @docker-compose @WIP
  Scenario: TestAggregatorContext
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {aggregator-context-case} local config as below
    """
    global:
      DefaultLogQueueSize: 10
    inputs:
      - Type: input_file
        FilePaths: 
          - /root/test/example.log
        EnableContainerDiscovery: true
        ContainerFilters:
          IncludeEnv:
            STDOUT_SWITCH: "true"
    processors:
      - Type: processor_split_char
        SourceKey: content
        SplitSep: "|"
        SplitKeys: ["no", "content"]
    aggregators:
      - Type: aggregator_context
    """
    When start docker-compose {aggregator_context}
    Then there is at least {200} logs
    Then the context of log is valid
  
