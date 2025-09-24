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

    std::string Version = "1.0.0";

    std::string PartitionerType;
    std::vector<std::string> HashKeys;
    std::string Partitioner;

    uint32_t QueueBufferingMaxKbytes = 1048576;
    uint32_t QueueBufferingMaxMessages = 100000;

    uint32_t BulkFlushFrequency = 0;
    uint32_t BulkMaxSize = 2048;
    uint32_t MaxMessageBytes = 1000000;

    int32_t RequiredAcks = 1;
    uint32_t Timeout = 30000;
    uint32_t MessageTimeoutMs = 300000;
    uint32_t MaxRetries = 3;
    uint32_t RetryBackoffMs = 100;

    std::map<std::string, std::string> CustomConfig;

    bool Load(const Json::Value& config, std::string& errorMsg) {
        if (!GetMandatoryListParam<std::string>(config, "Brokers", Brokers, errorMsg)) {
            return false;
        }

        if (!GetMandatoryStringParam(config, "Topic", Topic, errorMsg)) {
            return false;
        }

        std::string versionStr;
        if (!GetOptionalStringParam(config, "Version", versionStr, errorMsg)) {
            return false;
        }
        if (versionStr.empty()) {
            GetOptionalStringParam(config, "KafkaVersion", versionStr, errorMsg);
        }
        if (!versionStr.empty()) {
            Version = versionStr;
        }

        KafkaUtil::Version parsed;
        if (!KafkaUtil::ParseKafkaVersion(Version, parsed)) {
            errorMsg = "invalid Version format, expected x.y.z[.n]";
            return false;
        }

        GetOptionalUIntParam(config, "BulkFlushFrequency", BulkFlushFrequency, errorMsg);
        GetOptionalUIntParam(config, "BulkMaxSize", BulkMaxSize, errorMsg);
        GetOptionalUIntParam(config, "MaxMessageBytes", MaxMessageBytes, errorMsg);
        GetOptionalIntParam(config, "RequiredAcks", RequiredAcks, errorMsg);
        GetOptionalUIntParam(config, "Timeout", Timeout, errorMsg);
        GetOptionalUIntParam(config, "MessageTimeoutMs", MessageTimeoutMs, errorMsg);
        GetOptionalUIntParam(config, "MaxRetries", MaxRetries, errorMsg);
        GetOptionalUIntParam(config, "RetryBackoffMs", RetryBackoffMs, errorMsg);

        GetOptionalUIntParam(config, "QueueBufferingMaxKbytes", QueueBufferingMaxKbytes, errorMsg);
        GetOptionalUIntParam(config, "QueueBufferingMaxMessages", QueueBufferingMaxMessages, errorMsg);


        GetOptionalStringParam(config, "PartitionerType", PartitionerType, errorMsg);
        GetOptionalListParam<std::string>(config, "HashKeys", HashKeys, errorMsg);

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
