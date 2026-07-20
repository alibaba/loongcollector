@flusher
Feature: flusher http
  Test flusher http FlusherV2.Export

  @e2e @docker-compose
  Scenario: TestFlusherHTTPExportV2
    Given {docker-compose} environment
    Given subcribe data from {influxdb} with config
    """
    db_host: http://influxdb:8086
    db_name: mydb
    measurement: multi_values_metrics_mock
    """
    # Pure v2 pipeline: global.StructureType=v2 routes metric_mock through
    # FlusherV2.Export instead of the v1 Flush path. metric_mock.Read() emits Metric
    # events; flusher_http converts them via ToByteStreamWithSelectedFieldsV2 into
    # influxdb line protocol and writes them with the db query resolved from the group
    # tag (Query db="%{tag.db}"). In v2 the query templating reads group tags via
    # findTargetFieldsInGroup(group.GetTags()), so db must be a GroupTag, not a
    # per-metric tag; otherwise %{tag.db} stays empty, the write misses database "mydb"
    # and the influxdb subscriber (which queries mydb) finds nothing. Asserting the
    # record therefore proves the v2 export + group-tag query routing path.
    Given {flusher-http-export-v2-case} local config as below
    """
    enable: true
    global:
      StructureType: v2
      InputIntervalMs: 100
    inputs:
      - Type: metric_mock
        IntervalMs: 100
        GroupTags:
          db: mydb
        Tags:
          city: hz
        Fields:
          value: 32
    flushers:
      - Type: flusher_http
        RemoteURL: http://influxdb:8086/write
        Query:
          db: "%{tag.db}"
        Convert:
          Protocol: influxdb
          Encoding: custom
    """
    Given loongcollector depends on containers {["influxdb"]}
    When start docker-compose {flusher_http}
    Then there is at least {10} logs
    # multi_values_metrics_mock carries the configured field "value" (=32). The influxdb
    # subscriber maps the "value" field back to __name__=<measurement> and __value__=32,
    # proving the Metric field reached the target intact through the v2 export path.
    Then the log fields have exact kv
    """
    - __name__: multi_values_metrics_mock
      __value__: "32"
    """
