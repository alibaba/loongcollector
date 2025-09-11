@flusher
Feature: flusher kafka cpp 2.x.x
  Test flusher kafka cpp with native input_file on Kafka 2.x.x

  @e2e @docker-compose
  Scenario: TestFlusherKafkaCpp_2.x.x
    Given {docker-compose} environment
    Given subcribe data from {kafka} with config
    """
    brokers:
      - "localhost:9092"
    topic: "test-topic-2x"
    """
    Given {flusher-kafka-cpp-2.x.x-case} local config as below
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
        Topic: "test-topic-2x"
        Version: "2.8.0"
        MaxMessageBytes: 5242880
    """
    Given loongcollector container mount {./flusher_test_2.x.x.log} to {/root/test/1/2/3/flusher_testxxxx.log}
    Given loongcollector depends on containers {["kafka", "zookeeper"]}
    When start docker-compose {flusher_kafka_cpp_2.x.x}
    Then there is at least {1000} logs
    Then the log fields match kv
    """
    topic: "test-topic-2x"
    content: "^\\d+===="
    """
