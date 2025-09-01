@flusher
Feature: flusher kafka cpp
  Test flusher kafka cpp plugin

  @e2e @docker-compose
  Scenario: TestFlusherKafkaCpp
    Given {docker-compose} environment
    Given subcribe data from {kafka} with config
    """
    brokers:
      - "localhost:9092"
    topic: "test-topic"
    """
    Given {flusher-kafka-cpp-case} local config as below
    """
    enable: true
    inputs:
      - Type: metric_mock
        IntervalMs: 100
        Fields:
          content: "hello"
          topic: "test-topic"
    flushers:
      - Type: flusher_kafka_cpp
        Brokers: ["kafka:29092"]
        Topic: "test-topic"
        KafkaVersion: "2.8.0"
        Producer:
          MaxMessageBytes: 10485760
        Delivery:
          Acks: "1"
          RequestTimeoutMs: 30000
    """
    Given loongcollector depends on containers {["kafka", "zookeeper"]}
    When start docker-compose {flusher_kafka_cpp}
    Then there is at least {10} logs
    Then the log fields match kv
    """
    content: "hello"
    topic: "test-topic"
    """