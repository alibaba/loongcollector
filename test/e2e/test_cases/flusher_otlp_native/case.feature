@flusher
Feature: flusher OTLP native gRPC
  Test flusher_otlp_native sending OTLP data via gRPC to an OTel Collector

  @e2e @docker-compose
  Scenario: TestFlusherOTLPNativeLogs
    Given {docker-compose} environment
    Given {otlp-grpc-input-forward-case} local config as below
    """
    enable: true
    global:
      ExcutionTimeout: 600
    inputs:
      - Type: input_forward
        Protocol: OTLP
        Endpoint: "0.0.0.0:4320"
    flushers:
      - Type: flusher_otlp_native
        Endpoint: "otel-collector:4317"
        TimeoutMs: 5000
        EnableTLS: false
    """
    When start docker-compose {flusher_otlp_native}
    When generate {1} OTLP {logs} via otelgen to endpoint {loongcollectorC:4320}, protocol {grpc}
    Then wait {5} seconds
    Then otlp collector received at least {1} logs from file {/tmp/otel-export/logs.json}

  @e2e @docker-compose
  Scenario: TestFlusherOTLPNativeMetrics
    Given {docker-compose} environment
    Given {otlp-grpc-metric-forward-case} local config as below
    """
    enable: true
    global:
      ExcutionTimeout: 600
    inputs:
      - Type: input_forward
        Protocol: OTLP
        Endpoint: "0.0.0.0:4321"
    flushers:
      - Type: flusher_otlp_native
        Endpoint: "otel-collector:4317"
        TimeoutMs: 5000
        EnableTLS: false
    """
    When start docker-compose {flusher_otlp_native}
    When generate {1} OTLP {metrics} via otelgen to endpoint {loongcollectorC:4321}, protocol {grpc}
    Then wait {5} seconds
    Then otlp collector received at least {1} metrics from file {/tmp/otel-export/metrics.json}

  @e2e @docker-compose
  Scenario: TestFlusherOTLPNativeTraces
    Given {docker-compose} environment
    Given {otlp-grpc-trace-forward-case} local config as below
    """
    enable: true
    global:
      ExcutionTimeout: 600
    inputs:
      - Type: input_forward
        Protocol: OTLP
        Endpoint: "0.0.0.0:4322"
    flushers:
      - Type: flusher_otlp_native
        Endpoint: "otel-collector:4317"
        TimeoutMs: 5000
        EnableTLS: false
    """
    When start docker-compose {flusher_otlp_native}
    When generate {1} OTLP {traces} via otelgen to endpoint {loongcollectorC:4322}, protocol {grpc}
    Then wait {5} seconds
    Then otlp collector received at least {1} traces from file {/tmp/otel-export/traces.json}
