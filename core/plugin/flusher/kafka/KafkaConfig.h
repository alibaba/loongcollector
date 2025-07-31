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
    std::string ClientID;
    uint32_t TimeoutMs = 3000;
    uint32_t Retries = 3;
    uint32_t BatchNumMessages = 1000;
    uint32_t LingerMs = 0;
    std::map<std::string, std::string> CustomConfig;

    bool Load(const Json::Value& config, std::string& errorMsg) {
        if (!GetMandatoryListParam<std::string>(config, "Brokers", Brokers, errorMsg)) {
            return false;
        }

        if (!GetMandatoryStringParam(config, "Topic", Topic, errorMsg)) {
            return false;
        }

        GetOptionalStringParam(config, "ClientID", ClientID, errorMsg);
        GetOptionalUIntParam(config, "TimeoutMs", TimeoutMs, errorMsg);
        GetOptionalUIntParam(config, "Retries", Retries, errorMsg);
        GetOptionalUIntParam(config, "BatchNumMessages", BatchNumMessages, errorMsg);
        GetOptionalUIntParam(config, "LingerMs", LingerMs, errorMsg);

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
