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
const std::string KAFKA_CONFIG_CLIENT_ID = "client.id";
const std::string KAFKA_CONFIG_RETRIES = "retries";
const std::string KAFKA_CONFIG_BATCH_NUM_MESSAGES = "batch.num.messages";
const std::string KAFKA_CONFIG_LINGER_MS = "linger.ms";
const std::string KAFKA_CONFIG_ACKS = "acks";
const std::string KAFKA_CONFIG_COMPRESSION_TYPE = "compression.type";
const std::string KAFKA_CONFIG_QUEUE_BUFFERING_MAX_KBYTES = "queue.buffering.max.kbytes";
const std::string KAFKA_CONFIG_QUEUE_BUFFERING_MAX_MESSAGES = "queue.buffering.max.messages";
const std::string KAFKA_CONFIG_MESSAGE_SEND_MAX_RETRIES = "message.send.max.retries";
const std::string KAFKA_CONFIG_RETRY_BACKOFF_MS = "retry.backoff.ms";
const std::string KAFKA_CONFIG_REQUEST_TIMEOUT_MS = "request.timeout.ms";


const std::string KAFKA_ERROR_UNKNOWN = "UNKNOWN";
const std::string KAFKA_ERROR_BROKER_NOT_AVAILABLE = "BROKER_NOT_AVAILABLE";
const std::string KAFKA_ERROR_LEADER_NOT_AVAILABLE = "LEADER_NOT_AVAILABLE";
const std::string KAFKA_ERROR_NOT_LEADER_FOR_PARTITION = "NOT_LEADER_FOR_PARTITION";
const std::string KAFKA_ERROR_REQUEST_TIMED_OUT = "REQUEST_TIMED_OUT";
const std::string KAFKA_ERROR_NETWORK_EXCEPTION = "NETWORK_EXCEPTION";
const std::string KAFKA_ERROR_OFFSET_OUT_OF_RANGE = "OFFSET_OUT_OF_RANGE";
const std::string KAFKA_ERROR_CORRUPT_MESSAGE = "CORRUPT_MESSAGE";
const std::string KAFKA_ERROR_UNKNOWN_TOPIC_OR_PARTITION = "UNKNOWN_TOPIC_OR_PARTITION";
const std::string KAFKA_ERROR_INVALID_FETCH_SIZE = "INVALID_FETCH_SIZE";
const std::string KAFKA_ERROR_MESSAGE_TOO_LARGE = "MESSAGE_TOO_LARGE";
const std::string KAFKA_ERROR_GROUP_LOAD_IN_PROGRESS = "GROUP_LOAD_IN_PROGRESS";
const std::string KAFKA_ERROR_GROUP_COORDINATOR_NOT_AVAILABLE = "GROUP_COORDINATOR_NOT_AVAILABLE";
const std::string KAFKA_ERROR_NOT_COORDINATOR_FOR_GROUP = "NOT_COORDINATOR_FOR_GROUP";
const std::string KAFKA_ERROR_INVALID_TOPIC = "INVALID_TOPIC";
const std::string KAFKA_ERROR_RECORD_LIST_TOO_LARGE = "RECORD_LIST_TOO_LARGE";
const std::string KAFKA_ERROR_NOT_ENOUGH_REPLICAS = "NOT_ENOUGH_REPLICAS";
const std::string KAFKA_ERROR_NOT_ENOUGH_REPLICAS_AFTER_APPEND = "NOT_ENOUGH_REPLICAS_AFTER_APPEND";


const int KAFKA_DEFAULT_RETRIES = 3;
const int KAFKA_DEFAULT_BATCH_NUM_MESSAGES = 1000;
const int KAFKA_DEFAULT_LINGER_MS = 100;
const int KAFKA_DEFAULT_TIMEOUT_MS = 30000;
const std::string KAFKA_DEFAULT_CLIENT_ID = "loongcollector";
const std::string KAFKA_DEFAULT_ACKS = "1";
const std::string KAFKA_DEFAULT_COMPRESSION_TYPE = "none";

const int KAFKA_POLL_INTERVAL_MS = 100;
const int KAFKA_FLUSH_TIMEOUT_MS = 5000;
const int KAFKA_RETRY_BACKOFF_BASE_MS = 50;

const std::string METRIC_KAFKA_MESSAGES_SENT = "kafka_messages_sent";
const std::string METRIC_KAFKA_MESSAGES_SUCCESS = "kafka_messages_success";
const std::string METRIC_KAFKA_MESSAGES_FAILED = "kafka_messages_failed";
const std::string METRIC_KAFKA_BYTES_SENT = "kafka_bytes_sent";
const std::string METRIC_KAFKA_RETRIES = "kafka_retries";
const std::string METRIC_KAFKA_QUEUE_SIZE = "kafka_queue_size";
const std::string METRIC_KAFKA_BATCH_SIZE = "kafka_batch_size";

} // namespace logtail
