@flusher
Feature: flusher kafka native headers
  Verify flusher_kafka_native sends static headers with each message

  @e2e @docker-compose
  Scenario: TestFlusherKafkaNative_Headers
    Given {docker-compose} environment
    Given subcribe data from {kafka} with config
    """
    brokers:
      - "localhost:9092"
    topic: "headers-topic"
    """
    Given {flusher-kafka-native-headers-case} local config as below
    """
    enable: true
    global:
      UsingOldContentTag: true
      DefaultLogQueueSize: 10
    inputs:
      - Type: input_file
        FilePaths:
          - "/root/test/**/flusher_test_headers*.log"
        MaxDirSearchDepth: 10
        TailingAllMatchedFiles: true
    flushers:
      - Type: flusher_kafka_native
        Brokers: ["kafka:29092"]
        Topic: "headers-topic"
        Version: "3.6.0"
        Headers:
          - key: "h1"
            value: "v1"
          - key: "h2"
            value: "v2"
    """
    Given loongcollector container mount {./flusher_headers.log} to {/root/test/1/2/3/flusher_test_headers.log}
    Given loongcollector depends on containers {["kafka", "zookeeper"]}
    When start docker-compose {flusher_kafka_native_headers}
    Then there is at least {10} logs
    Then the log fields match kv
    """
    topic: "headers-topic"
    header.h1: "v1"
    header.h2: "v2"
    """
