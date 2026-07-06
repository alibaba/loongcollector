@flusher
Feature: flusher kafka
  Test flusher kafka FlusherV2.Export

  @e2e @docker-compose
  Scenario: TestFlusherKafkaExportV2
    Given {docker-compose} environment
    Given subcribe data from {kafka} with config
    """
    brokers:
      - "localhost:9092"
    topic: "test-topic-flusher-kafka-v2"
    """
    Given {flusher-kafka-export-v2-case} local config as below
    """
    enable: true
    global:
      StructureType: v2
      InputIntervalMs: 1000
    inputs:
      - Type: metric_mock
        IntervalMs: 1000
        Tags:
          tag1: aaaa
          tag2: bbb
        Fields:
          content: xxxxxx
    flushers:
      - Type: flusher_kafka
        Brokers: ["kafka:29092"]
        Topic: "test-topic-flusher-kafka-v2"
        PartitionerType: "random"
    """
    Given loongcollector depends on containers {["kafka", "zookeeper"]}
    When start docker-compose {flusher_kafka}
    Then there is at least {10} logs
    Then the log fields match kv
    """
    topic: "test-topic-flusher-kafka-v2"
    """
