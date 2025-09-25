@flusher @sasl
@e2e @docker-compose
Feature: flusher kafka native SASL PLAIN
  SASL/PLAIN e2e against SASL-enabled Kafka listener.

  Scenario: TestFlusherKafkaNative_SASL_PLAIN
    Given {docker-compose} environment
    Given subcribe data from {kafka} with config
    """
    brokers:
      - "localhost:9092"
    topic: "sasl-topic"
    """
    Given {flusher-kafka-native-sasl-plain-case} local config as below
    """
    enable: true
    inputs:
      - Type: input_file
        FilePaths:
          - "/root/test/**/sasl_input.log"
        MaxDirSearchDepth: 10
        TailingAllMatchedFiles: true
    processors:
      - Type: processor_parse_json_native
        SourceKey: content
        KeepingSourceWhenParseSucceed: true
    flushers:
      - Type: flusher_kafka_native
        Brokers: ["kafka:29093"]
        Topic: "sasl-topic"
        Version: "3.6.0"
        Authentication:
          SASL:
            Mechanism: "PLAIN"
            Username: "user"
            Password: "pass"
    """
    Given loongcollector container mount {./flusher_sasl.log} to {/root/test/1/2/3/sasl_input.log}
    Given loongcollector depends on containers {["kafka", "zookeeper"]}
    When start docker-compose {flusher_kafka_native_sasl_plain}
    Then there is at least {10} logs
    Then the log fields match kv
    """
    topic: "sasl-topic"
    content: ".*"
    """
