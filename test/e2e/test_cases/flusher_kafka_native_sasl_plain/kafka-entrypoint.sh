#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[kafka-entrypoint] $*"
}

log "waiting for zookeeper at zookeeper:2181 ..."
for i in {1..60}; do
  if nc -z zookeeper 2181 >/dev/null 2>&1; then
    log "zookeeper is up"
    break
  fi
  sleep 2
  if [[ $i -eq 60 ]]; then
    log "zookeeper not ready after timeout" >&2
    exit 1
  fi
done

cat >/tmp/server.properties <<'CONFIG'
broker.id=1
zookeeper.connect=zookeeper:2181

listeners=INTERNAL://0.0.0.0:29092,EXTERNAL_PLAIN://0.0.0.0:9092,SASL_PLAINTEXT://0.0.0.0:29093
advertised.listeners=INTERNAL://kafka:29092,EXTERNAL_PLAIN://localhost:9092,SASL_PLAINTEXT://kafka:29093
listener.security.protocol.map=INTERNAL:PLAINTEXT,EXTERNAL_PLAIN:PLAINTEXT,SASL_PLAINTEXT:SASL_PLAINTEXT
inter.broker.listener.name=INTERNAL

# SASL/PLAIN authentication config
sasl.enabled.mechanisms=PLAIN
listener.name.sasl_plaintext.plain.sasl.jaas.config=org.apache.kafka.common.security.plain.PlainLoginModule required \
  username="user" \
  password="pass" \
  user_user="pass";

# Broker basics for tests
message.max.bytes=5242880
replica.fetch.max.bytes=6291456
offsets.topic.replication.factor=1
transaction.state.log.replication.factor=1
transaction.state.log.min.isr=1
auto.create.topics.enable=true
default.replication.factor=1
num.partitions=1
CONFIG

log "starting kafka-server-start with SASL/PLAIN ..."
exec kafka-server-start /tmp/server.properties
