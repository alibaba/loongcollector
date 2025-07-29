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
extern const std::string KAFKA_CONFIG_CLIENT_ID;
extern const std::string KAFKA_CONFIG_RETRIES;
extern const std::string KAFKA_CONFIG_BATCH_NUM_MESSAGES;
extern const std::string KAFKA_CONFIG_LINGER_MS;
extern const std::string KAFKA_CONFIG_ACKS;
extern const std::string KAFKA_CONFIG_COMPRESSION_TYPE;
extern const std::string KAFKA_CONFIG_QUEUE_BUFFERING_MAX_KBYTES;
extern const std::string KAFKA_CONFIG_QUEUE_BUFFERING_MAX_MESSAGES;
extern const std::string KAFKA_CONFIG_MESSAGE_SEND_MAX_RETRIES;
extern const std::string KAFKA_CONFIG_RETRY_BACKOFF_MS;
extern const std::string KAFKA_CONFIG_REQUEST_TIMEOUT_MS;

extern const std::string KAFKA_ERROR_UNKNOWN;
extern const std::string KAFKA_ERROR_BROKER_NOT_AVAILABLE;
extern const std::string KAFKA_ERROR_LEADER_NOT_AVAILABLE;
extern const std::string KAFKA_ERROR_NOT_LEADER_FOR_PARTITION;
extern const std::string KAFKA_ERROR_REQUEST_TIMED_OUT;
extern const std::string KAFKA_ERROR_NETWORK_EXCEPTION;
extern const std::string KAFKA_ERROR_OFFSET_OUT_OF_RANGE;
extern const std::string KAFKA_ERROR_CORRUPT_MESSAGE;
extern const std::string KAFKA_ERROR_UNKNOWN_TOPIC_OR_PARTITION;
extern const std::string KAFKA_ERROR_INVALID_FETCH_SIZE;
extern const std::string KAFKA_ERROR_MESSAGE_TOO_LARGE;
extern const std::string KAFKA_ERROR_GROUP_LOAD_IN_PROGRESS;
extern const std::string KAFKA_ERROR_GROUP_COORDINATOR_NOT_AVAILABLE;
extern const std::string KAFKA_ERROR_NOT_COORDINATOR_FOR_GROUP;
extern const std::string KAFKA_ERROR_INVALID_TOPIC;
extern const std::string KAFKA_ERROR_RECORD_LIST_TOO_LARGE;
extern const std::string KAFKA_ERROR_NOT_ENOUGH_REPLICAS;
extern const std::string KAFKA_ERROR_NOT_ENOUGH_REPLICAS_AFTER_APPEND;


extern const int KAFKA_DEFAULT_RETRIES;
extern const int KAFKA_DEFAULT_BATCH_NUM_MESSAGES;
extern const int KAFKA_DEFAULT_LINGER_MS;
extern const int KAFKA_DEFAULT_TIMEOUT_MS;
extern const std::string KAFKA_DEFAULT_CLIENT_ID;
extern const std::string KAFKA_DEFAULT_ACKS;
extern const std::string KAFKA_DEFAULT_COMPRESSION_TYPE;

extern const int KAFKA_POLL_INTERVAL_MS;
extern const int KAFKA_FLUSH_TIMEOUT_MS;
extern const int KAFKA_RETRY_BACKOFF_BASE_MS;

extern const std::string METRIC_KAFKA_MESSAGES_SENT;
extern const std::string METRIC_KAFKA_MESSAGES_SUCCESS;
extern const std::string METRIC_KAFKA_MESSAGES_FAILED;
extern const std::string METRIC_KAFKA_BYTES_SENT;
extern const std::string METRIC_KAFKA_RETRIES;
extern const std::string METRIC_KAFKA_QUEUE_SIZE;
extern const std::string METRIC_KAFKA_BATCH_SIZE;

} // namespace logtail
