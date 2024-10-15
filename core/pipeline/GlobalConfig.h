/*
 * Copyright 2023 iLogtail Authors
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

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace logtail {

class PipelineContext;

struct GlobalConfig {
    enum class TopicType { NONE, FILEPATH, MACHINE_GROUP_TOPIC, CUSTOM, DEFAULT };

    static const std::unordered_set<std::string> sNativeParam;

    bool Init(const Json::Value& config, const PipelineContext& ctx, Json::Value& extendedParams);

    TopicType mTopicType = TopicType::NONE;
    std::string mTopicFormat;
    uint32_t mProcessPriority = 0;
    bool mEnableTimestampNanosecond = false;
    bool mUsingOldContentTag = false;
    std::unordered_map<std::string, std::string> mPipelineMetaTagKey;
    std::unordered_map<std::string, std::string> mAgentEnvMetaTagKey;
};

} // namespace logtail
