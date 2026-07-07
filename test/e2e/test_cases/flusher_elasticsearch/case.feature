@flusher
Feature: flusher elasticsearch
  Test flusher elasticsearch

  @e2e @docker-compose
  Scenario: TestFlusherElasticSearch
    Given {docker-compose} environment
    Given subcribe data from {elasticsearch} with config
    """
    address: http://localhost:9200
    username: elastic
    password: BtpoRTeyjmC=ruTIUoNN
    index: default
    """
    # Pipeline version switch: global.StructureType=v2 routes metric_mock through
    # FlusherV2.Export instead of the v1 Flush path. Structures at the flusher boundary:
    #   v1 pipeline (StructureType absent/v1):
    #     - input  to flusher: []*protocol.LogGroup. metric_mock.Collect() emits Log
    #       events via AddData, contents = {Index, Content}.
    #     - output of flusher: converter serialized doc per Log, fields = {Index, Content}.
    #   v2 pipeline (StructureType=v2, this case):
    #     - input  to flusher: []*models.PipelineGroupEvents. metric_mock.Read() emits
    #       2 Metric events/cycle: single_metrics_mock (counter, single value) and
    #       multi_values_metrics_mock (untyped; values{Index} + typedValues{Content}).
    #     - output of flusher: converter.ToByteStreamV2 -> 1 ES doc per Metric, each
    #       serialized into the __pipeline_passthrough__ blob
    #       (eventType/name/tags/value(s)/typedValues), never silently dropped.
    Given {flusher-elasticsearch-case} local config as below
    """
    enable: true
    global:
      StructureType: v2
      InputIntervalMs: 100
    inputs:
      - Type: metric_mock
        IntervalMs: 100
        Fields:
          Index: "default"
          Content: "hello"
    flushers:
      - Type: flusher_elasticsearch
        Index: default
        Addresses: ["http://elasticsearch:9200"]
        Authentication:
          PlainText:
            Username: elastic
            Password: BtpoRTeyjmC=ruTIUoNN
    """
    Given loongcollector depends on containers {["elasticsearch"]}
    When start docker-compose {flusher_elasticsearch}
    Then there is at least {10} logs
    # Verify keys AND values: every flushed row carries a __pipeline_passthrough__
    # blob whose value contains the real metric identity emitted by metric_mock
    # (eventType + one of the two metric names), proving the Metric events reached
    # the target intact rather than only that a column exists.
    Then the log fields match kv
    """
    __pipeline_passthrough__: '"eventType":"metric".*"name":"(single_metrics_mock|multi_values_metrics_mock)"'
    """

