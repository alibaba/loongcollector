@flusher
Feature: flusher kafka native 3.x.x
  Test flusher kafka native on the latest Kafka (3.x.x) broker. Only the newest
  broker version is kept from the old version matrix; the plaintext feature cases
  (basic delivery, gzip compression, static headers) are consolidated here as
  separate scenarios sharing one broker and one generated log. TLS / SASL / Kerberos
  keep their own directories because they require distinct broker auth topologies.

  @e2e @docker-compose
  Scenario: TestFlusherKafkaNative_3.x.x
    Given {docker-compose} environment
    Given subcribe data from {kafka} with config
    """
    brokers:
      - "localhost:9092"
    topic: "test-topic-3x"
    """
    Given {flusher-kafka-native-3.x.x-case} local config as below
    """
    enable: true
    global:
      UsingOldContentTag: true
      DefaultLogQueueSize: 10
    inputs:
      - Type: input_file
        FilePaths:
          - "/root/test/**/flusher_test*.log"
        MaxDirSearchDepth: 10
        TailingAllMatchedFiles: true
    flushers:
      - Type: flusher_kafka_native
        Brokers: ["kafka:29092"]
        Topic: "test-topic-3x"
        Version: "3.6.0"
        MaxMessageBytes: 5242880
    """
    Given loongcollector container mount {./flusher_test_3.x.x.log} to {/root/test/1/2/3/flusher_testxxxx.log}
    Given loongcollector depends on containers {["kafka", "zookeeper"]}
    When start docker-compose {flusher_kafka_native_3.x.x}
    Then there is at least {1000} logs
    Then the log fields match kv
    """
    topic: "test-topic-3x"
    content: "^\\d+===="
    """

  @e2e @docker-compose
  Scenario: TestFlusherKafkaNative_Compression_Gzip
    Given {docker-compose} environment
    Given subcribe data from {kafka} with config
    """
    brokers:
      - "localhost:9092"
    topic: "test-topic-compress"
    """
    Given {flusher-kafka-native-compression-case} local config as below
    """
    enable: true
    global:
      UsingOldContentTag: true
      DefaultLogQueueSize: 10
    inputs:
      - Type: input_file
        FilePaths:
          - "/root/test/**/flusher_test*.log"
        MaxDirSearchDepth: 10
        TailingAllMatchedFiles: true
    flushers:
      - Type: flusher_kafka_native
        Brokers: ["kafka:29092"]
        Topic: "test-topic-compress"
        Version: "3.6.0"
        MaxMessageBytes: 5242880
        Compression: gzip
        CompressionLevel: -1
        BulkFlushFrequency: 200
        BulkMaxSize: 1000
    """
    Given loongcollector container mount {./flusher_test_3.x.x.log} to {/root/test/1/2/3/flusher_testxxxx.log}
    Given loongcollector depends on containers {["kafka", "zookeeper"]}
    When start docker-compose {flusher_kafka_native_3.x.x}
    Then there is at least {1000} logs
    Then the log fields match kv
    """
    topic: "test-topic-compress"
    content: "^\\d+===="
    """

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
          - "/root/test/**/flusher_test*.log"
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
    Given loongcollector container mount {./flusher_test_3.x.x.log} to {/root/test/1/2/3/flusher_testxxxx.log}
    Given loongcollector depends on containers {["kafka", "zookeeper"]}
    When start docker-compose {flusher_kafka_native_3.x.x}
    Then there is at least {10} logs
    Then the log fields match kv
    """
    topic: "headers-topic"
    header.h1: "v1"
    header.h2: "v2"
    """
