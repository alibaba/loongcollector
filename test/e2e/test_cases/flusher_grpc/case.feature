@flusher
Feature: flusher grpc
  Test flusher grpc FlusherV2.Export

  @e2e @docker-compose
  Scenario: TestFlusherGrpcExportV2
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    # Pipeline version switch: global.StructureType=v2 routes metric_mock through
    # FlusherV2.Export instead of the v1 Flush path. Structures at the flusher boundary:
    #   v1 pipeline (StructureType absent/v1):
    #     - input  to flusher: []*protocol.LogGroup. metric_mock.Collect() emits Log
    #       events via AddData, contents = {Index, content}, tags = {tag1, tag2}.
    #     - output of flusher: flusher_grpc streams the protocol.LogGroup verbatim over
    #       LogReportService.Collect; each Log keeps contents {Index, content}.
    #   v2 pipeline (StructureType=v2, this case):
    #     - input  to flusher: []*models.PipelineGroupEvents. metric_mock.Read() emits
    #       2 Metric events/cycle: single_metrics_mock (counter, single value) and
    #       multi_values_metrics_mock (untyped; values{Index} + typedValues{content}),
    #       both tagged {tag1, tag2}.
    #     - output of flusher: Export -> PipelineGroupEventsToLogGroup maps each Metric to
    #       one protocol.Log carrying the __pipeline_passthrough__ blob
    #       (eventType/name/tags/value(s)/typedValues), streamed over the same gRPC wire;
    #       never silently dropped.
    Given {flusher-grpc-export-v2-case} local config as below
    """
    enable: true
    global:
      StructureType: v2
      InputIntervalMs: 1000
    inputs:
      - Type: metric_mock
        IntervalMs: 1000
        Tags:
          tag1: aaaa
          tag2: bbb
        Fields:
          content: xxxxxx
    """
    When start docker-compose {flusher_grpc}
    Then there is at least {10} logs
    # Verify keys AND values: every flushed row carries a __pipeline_passthrough__
    # blob whose value contains the real metric identity (eventType + metric name)
    # AND the configured tags (tag1=aaaa, tag2=bbb) that ride on both Metric events,
    # proving the Metric events reached the target intact with their key/value data.
    Then the log fields match kv
    """
    __pipeline_passthrough__: '"eventType":"metric".*"name":"(single_metrics_mock|multi_values_metrics_mock)".*"tag1":"aaaa".*"tag2":"bbb"'
    """
