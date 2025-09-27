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

} // namespace logtail
