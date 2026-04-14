@flusher
Feature: flusher OTLP HTTP native
  Test flusher_otlp_http_native sending OTLP data via HTTP to an OTel Collector

  @e2e @docker-compose
  Scenario: TestFlusherOTLPHttpNativeLogs
    Given {docker-compose} environment
    Given {otlp-http-input-forward-case} local config as below
    """
    enable: true
    global:
      ExcutionTimeout: 600
    inputs:
      - Type: input_forward
        Protocol: OTLP
        Endpoint: "0.0.0.0:4320"
    flushers:
      - Type: flusher_otlp_http_native
        Url: "http://otel-collector:4318/v1/logs"
        Format: "protobuf"
    """
    When start docker-compose {flusher_otlp_http_native}
    When generate {1} OTLP {logs} via otelgen to endpoint {loongcollectorC:4320}, protocol {grpc}
    Then wait {5} seconds
    Then otlp collector received at least {1} logs from file {/tmp/otel-export/logs.json}

  @e2e @docker-compose
  Scenario: TestFlusherOTLPHttpNativeMetrics
    Given {docker-compose} environment
    Given {otlp-http-metric-forward-case} local config as below
    """
    enable: true
    global:
      ExcutionTimeout: 600
    inputs:
      - Type: input_forward
        Protocol: OTLP
        Endpoint: "0.0.0.0:4321"
    flushers:
      - Type: flusher_otlp_http_native
        Url: "http://otel-collector:4318/v1/metrics"
        Format: "protobuf"
    """
    When start docker-compose {flusher_otlp_http_native}
    When generate {1} OTLP {metrics} via otelgen to endpoint {loongcollectorC:4321}, protocol {grpc}
    Then wait {5} seconds
    Then otlp collector received at least {1} metrics from file {/tmp/otel-export/metrics.json}

  @e2e @docker-compose
  Scenario: TestFlusherOTLPHttpNativeTraces
    Given {docker-compose} environment
    Given {otlp-http-trace-forward-case} local config as below
    """
    enable: true
    global:
      ExcutionTimeout: 600
    inputs:
      - Type: input_forward
        Protocol: OTLP
        Endpoint: "0.0.0.0:4322"
    flushers:
      - Type: flusher_otlp_http_native
        Url: "http://otel-collector:4318/v1/traces"
        Format: "protobuf"
    """
    When start docker-compose {flusher_otlp_http_native}
    When generate {1} OTLP {traces} via otelgen to endpoint {loongcollectorC:4322}, protocol {grpc}
    Then wait {5} seconds
    Then otlp collector received at least {1} traces from file {/tmp/otel-export/traces.json}
