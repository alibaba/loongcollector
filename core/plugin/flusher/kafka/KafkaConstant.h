/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <string>

namespace logtail {

extern const std::string KAFKA_CONFIG_BOOTSTRAP_SERVERS;
extern const std::string KAFKA_CONFIG_PARTITIONER;

extern const std::string KAFKA_CONFIG_BATCH_NUM_MESSAGES;
extern const std::string KAFKA_CONFIG_LINGER_MS;
extern const std::string KAFKA_CONFIG_QUEUE_BUFFERING_MAX_KBYTES;
extern const std::string KAFKA_CONFIG_QUEUE_BUFFERING_MAX_MESSAGES;
extern const std::string KAFKA_CONFIG_MESSAGE_MAX_BYTES;

extern const std::string KAFKA_CONFIG_ACKS;
extern const std::string KAFKA_CONFIG_REQUEST_TIMEOUT_MS;
extern const std::string KAFKA_CONFIG_MESSAGE_TIMEOUT_MS;
extern const std::string KAFKA_CONFIG_MESSAGE_SEND_MAX_RETRIES;
extern const std::string KAFKA_CONFIG_RETRY_BACKOFF_MS;

extern const std::string KAFKA_CONFIG_API_VERSION_REQUEST;
extern const std::string KAFKA_CONFIG_BROKER_VERSION_FALLBACK;
extern const std::string KAFKA_CONFIG_API_VERSION_FALLBACK_MS;

extern const int KAFKA_POLL_INTERVAL_MS;
extern const int KAFKA_FLUSH_TIMEOUT_MS;

extern const std::string PARTITIONER_RANDOM;
extern const std::string PARTITIONER_HASH;
extern const std::string PARTITIONER_PREFIX;

extern const std::string LIBRDKAFKA_PARTITIONER_RANDOM;
extern const std::string LIBRDKAFKA_PARTITIONER_MURMUR2_RANDOM;

extern const std::string KAFKA_CONFIG_SECURITY_PROTOCOL;
extern const std::string KAFKA_CONFIG_SSL_CA_LOCATION;
extern const std::string KAFKA_CONFIG_SSL_CERTIFICATE_LOCATION;
extern const std::string KAFKA_CONFIG_SSL_KEY_LOCATION;
extern const std::string KAFKA_CONFIG_SSL_KEY_PASSWORD;
extern const std::string KAFKA_SECURITY_PROTOCOL_SSL;

extern const std::string KAFKA_CONFIG_SASL_MECHANISMS;
extern const std::string KAFKA_CONFIG_SASL_KERBEROS_SERVICE_NAME;
extern const std::string KAFKA_CONFIG_SASL_KERBEROS_PRINCIPAL;
extern const std::string KAFKA_CONFIG_SASL_KERBEROS_KEYTAB;
extern const std::string KAFKA_CONFIG_SASL_KERBEROS_KINIT_CMD;
extern const std::string KAFKA_CONFIG_SASL_USERNAME;
extern const std::string KAFKA_CONFIG_SASL_PASSWORD;

extern const std::string KAFKA_CONFIG_COMPRESSION_CODEC;
extern const std::string KAFKA_CONFIG_COMPRESSION_LEVEL;

} // namespace logtail
