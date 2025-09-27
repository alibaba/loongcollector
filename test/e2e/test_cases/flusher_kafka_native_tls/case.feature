@flusher @tls
@e2e @docker-compose
Feature: flusher kafka native TLS
  TLS e2e using SSL-enabled Kafka listener (no client cert auth).

  Scenario: TestFlusherKafkaNative_TLS
    Given {docker-compose} environment
    Given subcribe data from {kafka} with config
    """
    brokers:
      - "localhost:9092"
    topic: "tls-topic"
    """
    Given {flusher-kafka-native-tls-case} local config as below
    """
    enable: true
    inputs:
      - Type: input_file
        FilePaths:
          - "/root/test/**/tls_input.log"
        MaxDirSearchDepth: 10
        TailingAllMatchedFiles: true
    processors:
      - Type: processor_parse_json_native
        SourceKey: content
        KeepingSourceWhenParseSucceed: true
    flushers:
      - Type: flusher_kafka_native
        Brokers: ["kafka:29093"]
        Topic: "tls-topic"
        Version: "2.8.0"
        Authentication:
          TLS:
            Enabled: true
            CAFile: "/etc/kafka/ssl/ca.crt"
    """
    Given loongcollector container mount {./flusher_tls.log} to {/root/test/1/2/3/tls_input.log}
    Given loongcollector container mount {kafka_certs} to {/etc/kafka/ssl}
    Given loongcollector depends on containers {["kafka", "zookeeper", "cert-generator"]}
    When start docker-compose {flusher_kafka_native_tls}
    Then there is at least {10} logs
    Then the log fields match kv
    """
    topic: "tls-topic"
    content: ".*"
    """
