@flusher
Feature: flusher kafka native compression
  Test flusher kafka native with compression enabled on Kafka 2.x.x

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
        Version: "2.8.0"
        MaxMessageBytes: 5242880
        Compression: gzip
        CompressionLevel: -1
        BulkFlushFrequency: 200
        BulkMaxSize: 1000
    """
    Given loongcollector container mount {./flusher_test_compression.log} to {/root/test/1/2/3/flusher_testxxxx.log}
    Given loongcollector depends on containers {["kafka", "zookeeper"]}
    When start docker-compose {flusher_kafka_native_compression}
    Then there is at least {1000} logs
    Then the log fields match kv
    """
    topic: "test-topic-compress"
    content: "^\\d+===="
    """

