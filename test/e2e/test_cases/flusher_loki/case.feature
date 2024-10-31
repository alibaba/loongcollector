@flusher
Feature: flusher loki
  Test flusher loki

  @e2e @docker-compose
  Scenario: TestFlusherLoki
    Given {docker-compose} environment
    Given subcribe data from {loki} with config
    """
    address: http://loki:3100
    tenant_id: loongcollector
    target_labels:
      loki_name: hello
      source: loongcollector
    """
    Given {flusher-loki-case} local config as below
    """
    enable: true
    inputs:
      - Type: metric_mock
        IntervalMs: 100
        Tags:
          __tag__:name: "hello"
        Fields:
          value: "log contents"
    flushers:
      - Type: flusher_loki
        Convert:
          TagFieldsRename:
            loki_name: name
        URL: http://loki:3100/loki/api/v1/push
        TenantID: loongcollector
        MaxMessageWait: 100000000
        MaxMessageBytes: 1024
        Timeout: 1000000000000
        MinBackoff: 100000000000
        MaxBackoff: 1000000000000
        MaxRetries: 10
        DynamicLabels:
          - tag.loki_name
        StaticLabels:
          source: loongcollector
    """
    Given loongcollector depends on containers {["loki"]}
    When start docker-compose {flusher_loki}
    Then there is at least {10} logs
    Then the log fields match kv
    """
    name: "hello"
    value: "log contents"
    """
  
