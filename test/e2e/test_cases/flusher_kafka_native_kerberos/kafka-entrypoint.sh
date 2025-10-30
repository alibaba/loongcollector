#!/usr/bin/env bash
set -euo pipefail

echo "[kafka-entrypoint] waiting for zookeeper at zookeeper:2181 ..."
for i in {1..60}; do
  if nc -z zookeeper 2181 >/dev/null 2>&1; then
    echo "[kafka-entrypoint] zookeeper is up"
    break
  fi
  sleep 2
  if [[ $i -eq 60 ]]; then
    echo "[kafka-entrypoint] zookeeper not ready after timeout" >&2
    exit 1
  fi
done

cat >/tmp/server.properties <<'EOF'
broker.id=1
zookeeper.connect=zookeeper:2181

listeners=INTERNAL://0.0.0.0:29092,EXTERNAL://0.0.0.0:9092,SASL_PLAINTEXT://0.0.0.0:29093
advertised.listeners=INTERNAL://kafka:29092,EXTERNAL://localhost:9092,SASL_PLAINTEXT://kafka:29093
listener.security.protocol.map=INTERNAL:PLAINTEXT,EXTERNAL:PLAINTEXT,SASL_PLAINTEXT:SASL_PLAINTEXT
inter.broker.listener.name=INTERNAL

# Kerberos / SASL
sasl.enabled.mechanisms=GSSAPI
sasl.kerberos.service.name=kafka

# Broker basics for tests
message.max.bytes=5242880
replica.fetch.max.bytes=6291456
offsets.topic.replication.factor=1
transaction.state.log.replication.factor=1
transaction.state.log.min.isr=1
auto.create.topics.enable=true
default.replication.factor=1
num.partitions=1
EOF

echo "[kafka-entrypoint] starting kafka-server-start with Kerberos (GSSAPI) ..."
exec kafka-server-start /tmp/server.properties

