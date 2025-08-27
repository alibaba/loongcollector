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
#include "plugin/flusher/kafka/KafkaUtil.h"

namespace logtail {

struct KafkaConfig {
    std::vector<std::string> Brokers;
    std::string Topic;

    std::string KafkaVersion;


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

        if (!GetMandatoryStringParam(config, "KafkaVersion", KafkaVersion, errorMsg)) {
            return false;
        }

        KafkaUtil::Version parsed;
        if (!KafkaUtil::ParseKafkaVersion(KafkaVersion, parsed)) {
            errorMsg = "invalid KafkaVersion format, expected x.y.z[.n]";
            return false;
        }

        if (config.isMember("Producer") && config["Producer"].isObject()) {
            const Json::Value& producerConfig = config["Producer"];
            GetOptionalUIntParam(producerConfig, "QueueBufferingMaxKbytes", Producer.QueueBufferingMaxKbytes, errorMsg);
            GetOptionalUIntParam(
                producerConfig, "QueueBufferingMaxMessages", Producer.QueueBufferingMaxMessages, errorMsg);
            GetOptionalUIntParam(producerConfig, "LingerMs", Producer.LingerMs, errorMsg);
            GetOptionalUIntParam(producerConfig, "BatchNumMessages", Producer.BatchNumMessages, errorMsg);
            GetOptionalUIntParam(producerConfig, "BatchSize", Producer.BatchSize, errorMsg);
            GetOptionalUIntParam(producerConfig, "MaxMessageBytes", Producer.MaxMessageBytes, errorMsg);
        }


        if (config.isMember("Delivery") && config["Delivery"].isObject()) {
            const Json::Value& deliveryConfig = config["Delivery"];
            GetOptionalStringParam(deliveryConfig, "Acks", Delivery.Acks, errorMsg);
            GetOptionalUIntParam(deliveryConfig, "RequestTimeoutMs", Delivery.RequestTimeoutMs, errorMsg);
            GetOptionalUIntParam(deliveryConfig, "MessageTimeoutMs", Delivery.MessageTimeoutMs, errorMsg);
            GetOptionalUIntParam(deliveryConfig, "MaxRetries", Delivery.MaxRetries, errorMsg);
            GetOptionalUIntParam(deliveryConfig, "RetryBackoffMs", Delivery.RetryBackoffMs, errorMsg);
        }


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
