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

#include "plugin/flusher/kafka/KafkaUtil.h"

#include <librdkafka/rdkafka.h>

#include <sstream>

namespace logtail {

std::string KafkaUtil::BrokersToString(const std::vector<std::string>& brokers) {
    if (brokers.empty()) {
        return "";
    }

    std::stringstream ss;
    for (size_t i = 0; i < brokers.size(); ++i) {
        if (i > 0) {
            ss << ",";
        }
        ss << brokers[i];
    }
    return ss.str();
}

std::string KafkaUtil::GetErrorString(int err) {
    return std::string(rd_kafka_err2str(static_cast<rd_kafka_resp_err_t>(err)));
}

bool KafkaUtil::IsRetriableError(int err) {
    switch (err) {
        case RD_KAFKA_RESP_ERR_BROKER_NOT_AVAILABLE:
        case RD_KAFKA_RESP_ERR_LEADER_NOT_AVAILABLE:
        case RD_KAFKA_RESP_ERR_NOT_LEADER_FOR_PARTITION:
        case RD_KAFKA_RESP_ERR_REQUEST_TIMED_OUT:
        case RD_KAFKA_RESP_ERR_NETWORK_EXCEPTION:
        case RD_KAFKA_RESP_ERR_GROUP_COORDINATOR_NOT_AVAILABLE:
        case RD_KAFKA_RESP_ERR_NOT_COORDINATOR_FOR_GROUP:
        case RD_KAFKA_RESP_ERR_NOT_ENOUGH_REPLICAS:
        case RD_KAFKA_RESP_ERR_NOT_ENOUGH_REPLICAS_AFTER_APPEND:
            return true;
        default:
            return false;
    }
}

bool KafkaUtil::IsNetworkError(int err) {
    switch (err) {
        case RD_KAFKA_RESP_ERR__TRANSPORT:
        case RD_KAFKA_RESP_ERR__ALL_BROKERS_DOWN:
        case RD_KAFKA_RESP_ERR__DESTROY:
        case RD_KAFKA_RESP_ERR__TIMED_OUT:
        case RD_KAFKA_RESP_ERR_NETWORK_EXCEPTION:
            return true;
        default:
            return false;
    }
}

bool KafkaUtil::IsAuthError(int err) {
    switch (err) {
        case RD_KAFKA_RESP_ERR__AUTHENTICATION:
        case RD_KAFKA_RESP_ERR_TOPIC_AUTHORIZATION_FAILED:
        case RD_KAFKA_RESP_ERR_GROUP_AUTHORIZATION_FAILED:
            return true;
        default:
            return false;
    }
}

uint32_t KafkaUtil::CalculateBackoffDelay(uint32_t attempt, uint32_t baseDelayMs) {
    return baseDelayMs * (1 << std::min(attempt, 10u));
}

} // namespace logtail
