@flusher
Feature: flusher kafka cpp static
  Test flusher kafka cpp with native input_file

  @e2e @docker-compose
  Scenario: TestFlusherKafkaCppStatic
    Given {docker-compose} environment
    Given subcribe data from {kafka} with config
    """
    brokers:
      - "localhost:9092"
    topic: "test-topic"
    """
    Given {flusher-kafka-cpp-static-case} local config as below
    """
    enable: true
    global:
      UsingOldContentTag: true
      DefaultLogQueueSize: 10
    inputs:
      - Type: input_file
        FilePaths:
          - "/root/test/**/a*.log"
        MaxDirSearchDepth: 10
        TailingAllMatchedFiles: true
    flushers:
      - Type: flusher_kafka_cpp
        Brokers: ["kafka:29092"]
        Topic: "test-topic"
        KafkaVersion: "2.8.0"
        Producer:
          MaxMessageBytes: 5242880
    """
    Given loongcollector container mount {./a.log} to {/root/test/1/2/3/axxxx.log}
    Given loongcollector depends on containers {["kafka", "zookeeper"]}
    When start docker-compose {flusher_kafka_cpp_static}
    Then there is at least {1000} logs
    Then the log fields match kv
    """
    topic: "test-topic"
    content: "^\\d+===="
    """
