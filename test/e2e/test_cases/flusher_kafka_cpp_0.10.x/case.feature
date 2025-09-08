@flusher
Feature: flusher kafka cpp 0.10.x
  Test flusher kafka cpp with native input_file on Kafka 0.10.x

  @e2e @docker-compose
  Scenario: TestFlusherKafkaCpp_0.10.x
    Given {docker-compose} environment
    Given subcribe data from {kafka} with config
    """
    brokers:
      - "localhost:9092"
    topic: "test-topic-010x"
    version: "0.10.2.0"
    """
    Given {flusher-kafka-cpp-0.10.x-case} local config as below
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
      - Type: flusher_kafka_cpp
        Brokers: ["kafka:29092"]
        Topic: "test-topic-010x"
        Version: "0.10.2.1"
        BulkFlushFrequency: 0
        BulkMaxSize: 2048
        MaxMessageBytes: 5242880
        QueueBufferingMaxKbytes: 1048576
        QueueBufferingMaxMessages: 100000
        RequiredAcks: 1
        Timeout: 30000
        MessageTimeoutMs: 300000
        MaxRetries: 3
        RetryBackoffMs: 100
    """
    Given loongcollector container mount {./flusher_test_0.10.x.log} to {/root/test/1/2/3/flusher_testxxxx.log}
    Given loongcollector depends on containers {["kafka", "zookeeper"]}
    When start docker-compose {flusher_kafka_cpp_0.10.x}
    Then there is at least {1000} logs
    Then the log fields match kv
    """
    topic: "test-topic-010x"
    content: "^\\d+===="
    """
