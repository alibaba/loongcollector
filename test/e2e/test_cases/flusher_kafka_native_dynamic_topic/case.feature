@flusher
Feature: flusher kafka native dynamic topic
  Test flusher kafka native with dynamic Topic formatting using input_file + processors on Kafka 2.x.x

  @e2e @docker-compose
  Scenario: TestFlusherKafkaNative_DynamicTopic
    Given {docker-compose} environment
    Given subcribe data from {kafka} with config
    """
    brokers:
      - "localhost:9092"
    topic: "app-serviceA"
    """
    Given {flusher-kafka-native-dynamic-topic-case} local config as below
    """
    enable: true
    inputs:
      - Type: input_file
        FilePaths:
          - "/root/test/**/dynamic_input.log"
        MaxDirSearchDepth: 10
        TailingAllMatchedFiles: true
    processors:
      - Type: processor_parse_json_native
        SourceKey: content
        KeepingSourceWhenParseSucceed: true
    flushers:
      - Type: flusher_kafka_native
        Brokers: ["kafka:29092"]
        Topic: "app-%{content.service}"
        Version: "2.8.0"
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
    Given loongcollector container mount {./flusher_dynamic.log} to {/root/test/1/2/3/dynamic_input.log}
    Given loongcollector depends on containers {["kafka", "zookeeper"]}
    When start docker-compose {flusher_kafka_native_dynamic_topic}
    Then there is at least {10} logs
    Then the log fields match kv
    """
    topic: "app-serviceA"
    content: ".*"
    """

  @e2e @docker-compose
  Scenario: TestFlusherKafkaNative_DynamicTopic_Tag
    Given {docker-compose} environment
    Given subcribe data from {kafka} with config
    """
    brokers:
      - "localhost:9092"
    topic: "app-loongcollector"
    """
    Given {flusher-kafka-native-dynamic-topic-case} local config as below
    """
    enable: true
    inputs:
      - Type: input_file
        FilePaths:
          - "/root/test/**/dynamic_input.log"
        MaxDirSearchDepth: 10
        TailingAllMatchedFiles: true
    processors:
      - Type: processor_parse_json_native
        SourceKey: content
        KeepingSourceWhenParseSucceed: true
    flushers:
      - Type: flusher_kafka_native
        Brokers: ["kafka:29092"]
        Topic: "app-%{tag.__hostname__}"
        Version: "2.8.0"
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
    Given loongcollector container mount {./flusher_dynamic.log} to {/root/test/1/2/3/dynamic_input.log}
    Given loongcollector depends on containers {["kafka", "zookeeper"]}
    When start docker-compose {flusher_kafka_native_dynamic_topic}
    Then there is at least {10} logs
    Then the log fields match kv
    """
    topic: "app-loongcollector"
    content: ".*"
    """

  @e2e @docker-compose
  Scenario: TestFlusherKafkaNative_DynamicTopic_EnvVar
    Given {docker-compose} environment
    Given subcribe data from {kafka} with config
    """
    brokers:
      - "localhost:9092"
    topic: "app-prod"
    """
    Given {flusher-kafka-native-dynamic-topic-case} local config as below
    """
    enable: true
    inputs:
      - Type: input_file
        FilePaths:
          - "/root/test/**/dynamic_input.log"
        MaxDirSearchDepth: 10
        TailingAllMatchedFiles: true
    processors:
      - Type: processor_parse_json_native
        SourceKey: content
        KeepingSourceWhenParseSucceed: true
    flushers:
      - Type: flusher_kafka_native
        Brokers: ["kafka:29092"]
        Topic: "app-${MY_ENV}"
        Version: "2.8.0"
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
    Given loongcollector container mount {./flusher_dynamic.log} to {/root/test/1/2/3/dynamic_input.log}
    Given loongcollector depends on containers {["kafka", "zookeeper"]}
    When start docker-compose {flusher_kafka_native_dynamic_topic}
    Then there is at least {10} logs
    Then the log fields match kv
    """
    topic: "app-prod"
    content: ".*"
    """

  @e2e @docker-compose
  Scenario: TestFlusherKafkaNative_HashPartition_Basic
    Given {docker-compose} environment
    Given subcribe data from {kafka} with config
    """
    brokers:
      - "localhost:9092"
    topic: "app-hash"
    """
    Given {flusher-kafka-native-dynamic-topic-case} local config as below
    """
    enable: true
    inputs:
      - Type: input_file
        FilePaths:
          - "/root/test/**/dynamic_input.log"
        MaxDirSearchDepth: 10
        TailingAllMatchedFiles: true
    processors:
      - Type: processor_parse_json_native
        SourceKey: content
        KeepingSourceWhenParseSucceed: true
    flushers:
      - Type: flusher_kafka_native
        Brokers: ["kafka:29092"]
        Topic: "app-hash"
        Version: "2.8.0"
        PartitionerType: "hash"
        HashKeys: ["content.msg"]
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
    Given loongcollector container mount {./flusher_dynamic.log} to {/root/test/1/2/3/dynamic_input.log}
    Given loongcollector depends on containers {["kafka", "zookeeper"]}
    When start docker-compose {flusher_kafka_native_dynamic_topic}
    Then there is at least {10} logs
    Then the log fields match kv
    """
    topic: "app-hash"
    content: ".*"
    partition: "[0-9]+"
    """
    Then the kafka partitions at least {2}
    Then the kafka partitions consistent by field {msg}
    Then print kafka partition mapping by field {msg}
