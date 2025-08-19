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

#include <json/json.h>

#include <map>
#include <string>
#include <vector>

#include "common/ParamExtractor.h"

namespace logtail {

struct KafkaConfig {
    std::vector<std::string> Brokers;
    std::string Topic;

    struct Producer {
        uint32_t QueueBufferingMaxKbytes = 1048576;
        uint32_t QueueBufferingMaxMessages = 100000;
        uint32_t LingerMs = 5;
        uint32_t BatchNumMessages = 10000;
        uint32_t BatchSize = 1000000;
        uint32_t MaxMessageBytes = 1000000;
    } Producer;

    struct Delivery {
        std::string Acks = "1";
        uint32_t RequestTimeoutMs = 30000;
        uint32_t MessageTimeoutMs = 300000;
        uint32_t MaxRetries = 2147483647;
        uint32_t RetryBackoffMs = 1000;
    } Delivery;

    std::map<std::string, std::string> CustomConfig;

    bool Load(const Json::Value& config, std::string& errorMsg) {
        if (!GetMandatoryListParam<std::string>(config, "Brokers", Brokers, errorMsg)) {
            return false;
        }

        if (!GetMandatoryStringParam(config, "Topic", Topic, errorMsg)) {
            return false;
        }

        GetOptionalUIntParam(config, "Producer.QueueBufferingMaxKbytes", Producer.QueueBufferingMaxKbytes, errorMsg);
        GetOptionalUIntParam(
            config, "Producer.QueueBufferingMaxMessages", Producer.QueueBufferingMaxMessages, errorMsg);
        GetOptionalUIntParam(config, "Producer.LingerMs", Producer.LingerMs, errorMsg);
        GetOptionalUIntParam(config, "Producer.BatchNumMessages", Producer.BatchNumMessages, errorMsg);
        GetOptionalUIntParam(config, "Producer.BatchSize", Producer.BatchSize, errorMsg);
        GetOptionalUIntParam(config, "Producer.MaxMessageBytes", Producer.MaxMessageBytes, errorMsg);

        GetOptionalStringParam(config, "Delivery.Acks", Delivery.Acks, errorMsg);
        GetOptionalUIntParam(config, "Delivery.RequestTimeoutMs", Delivery.RequestTimeoutMs, errorMsg);
        GetOptionalUIntParam(config, "Delivery.MessageTimeoutMs", Delivery.MessageTimeoutMs, errorMsg);
        GetOptionalUIntParam(config, "Delivery.MaxRetries", Delivery.MaxRetries, errorMsg);
        GetOptionalUIntParam(config, "Delivery.RetryBackoffMs", Delivery.RetryBackoffMs, errorMsg);

        if (config.isMember("Kafka") && config["Kafka"].isObject()) {
            const Json::Value& kafkaConfig = config["Kafka"];
            for (const auto& key : kafkaConfig.getMemberNames()) {
                CustomConfig[key] = kafkaConfig[key].asString();
            }
        }

        return true;
    }
};

} // namespace logtail
