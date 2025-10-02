@flusher @kerberos
@e2e @docker-compose
Feature: flusher kafka native Kerberos
  Kerberos e2e using SASL_PLAINTEXT-enabled Kafka listener. Keep Kafka plaintext port for test consumer simplicity.

  Scenario: TestFlusherKafkaNative_Kerberos
    Given {docker-compose} environment
    Given subcribe data from {kafka} with config
    """
    brokers:
      - "localhost:9092"
    topic: "kerberos-topic"
    """
    Given {flusher-kafka-native-kerberos-case} local config as below
    """
    enable: true
    inputs:
      - Type: input_file
        FilePaths:
          - "/root/test/**/kerberos_input.log"
        MaxDirSearchDepth: 10
        TailingAllMatchedFiles: true
    processors:
      - Type: processor_parse_json_native
        SourceKey: content
        KeepingSourceWhenParseSucceed: true
    flushers:
      - Type: flusher_kafka_native
        Brokers: ["kafka:29093"]
        Topic: "kerberos-topic"
        Version: "2.6.0"
        MessageTimeoutMs: 60000
        Authentication:
          Kerberos:
            Enabled: true
            Mechanisms: "GSSAPI"
            ServiceName: "kafka"
            Principal: "client"
            Keytab: "/var/kerberos/client.keytab"
        Kafka:
          debug: "security,broker"
    """
    Given loongcollector container mount {./flusher_kerberos.log} to {/root/test/1/2/3/kerberos_input.log}
    Given loongcollector container mount {./krb5.conf} to {/etc/krb5.conf}
    Given loongcollector container mount {kerberos_data} to {/var/kerberos}
    Given loongcollector depends on containers {["kafka", "zookeeper", "kerberos-server"]}
    # Ensure host-side log file exists (avoid docker bind mount creating a directory)
    Given run command on datasource {cd test_cases/flusher_kafka_native_kerberos && rm -rf flusher_kerberos.log && touch flusher_kerberos.log}
    When start docker-compose {flusher_kafka_native_kerberos}
    Then there is at least {10} logs
    Then the log fields match kv
    """
    topic: "kerberos-topic"
    content: ".*"
    """
