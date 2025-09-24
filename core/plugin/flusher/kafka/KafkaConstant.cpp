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

#include "KafkaConstant.h"

#include <string>

namespace logtail {

const std::string KAFKA_CONFIG_BOOTSTRAP_SERVERS = "bootstrap.servers";
const std::string KAFKA_CONFIG_PARTITIONER = "partitioner";

const std::string KAFKA_CONFIG_BATCH_NUM_MESSAGES = "batch.num.messages";
const std::string KAFKA_CONFIG_LINGER_MS = "linger.ms";
const std::string KAFKA_CONFIG_QUEUE_BUFFERING_MAX_KBYTES = "queue.buffering.max.kbytes";
const std::string KAFKA_CONFIG_QUEUE_BUFFERING_MAX_MESSAGES = "queue.buffering.max.messages";
const std::string KAFKA_CONFIG_MESSAGE_MAX_BYTES = "message.max.bytes";

const std::string KAFKA_CONFIG_ACKS = "acks";
const std::string KAFKA_CONFIG_REQUEST_TIMEOUT_MS = "request.timeout.ms";
const std::string KAFKA_CONFIG_MESSAGE_TIMEOUT_MS = "message.timeout.ms";
const std::string KAFKA_CONFIG_MESSAGE_SEND_MAX_RETRIES = "message.send.max.retries";
const std::string KAFKA_CONFIG_RETRY_BACKOFF_MS = "retry.backoff.ms";

const std::string KAFKA_CONFIG_API_VERSION_REQUEST = "api.version.request";
const std::string KAFKA_CONFIG_BROKER_VERSION_FALLBACK = "broker.version.fallback";
const std::string KAFKA_CONFIG_API_VERSION_FALLBACK_MS = "api.version.fallback.ms";

const int KAFKA_POLL_INTERVAL_MS = 100;
const int KAFKA_FLUSH_TIMEOUT_MS = 5000;

const std::string PARTITIONER_RANDOM = "random";
const std::string PARTITIONER_HASH = "hash";
const std::string PARTITIONER_PREFIX = "content.";

const std::string LIBRDKAFKA_PARTITIONER_RANDOM = "random";
const std::string LIBRDKAFKA_PARTITIONER_MURMUR2_RANDOM = "murmur2_random";

} // namespace logtail
