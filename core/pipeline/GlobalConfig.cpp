// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "pipeline/GlobalConfig.h"

#include <string>
#include <unordered_map>

#include "json/json.h"

#include "TagConstants.h"
#include "common/ParamExtractor.h"
#include "pipeline/PipelineContext.h"
#include "pipeline/queue/ProcessQueueManager.h"

using namespace std;

namespace logtail {

const unordered_set<string> GlobalConfig::sNativeParam = {"TopicType",
                                                          "TopicFormat",
                                                          "Priority",
                                                          "EnableTimestampNanosecond",
                                                          "UsingOldContentTag",
                                                          "PipelineMetaTagKey",
                                                          "AgentEnvMetaTagKey"};

bool GlobalConfig::Init(const Json::Value& config, const PipelineContext& ctx, Json::Value& extendedParams) {
    const string moduleName = "global";
    string errorMsg;

    // TopicType
    string topicType;
    if (!GetOptionalStringParam(config, "TopicType", topicType, errorMsg)) {
        PARAM_WARNING_IGNORE(ctx.GetLogger(),
                             ctx.GetAlarm(),
                             errorMsg,
                             moduleName,
                             ctx.GetConfigName(),
                             ctx.GetProjectName(),
                             ctx.GetLogstoreName(),
                             ctx.GetRegion());
    } else if (topicType == "custom") {
        mTopicType = TopicType::CUSTOM;
    } else if (topicType == "machine_group_topic") {
        mTopicType = TopicType::MACHINE_GROUP_TOPIC;
    } else if (topicType == "filepath") {
        mTopicType = TopicType::FILEPATH;
    } else if (topicType == "default") {
        mTopicType = TopicType::DEFAULT;
    } else if (!topicType.empty() && topicType != "none") {
        PARAM_WARNING_IGNORE(ctx.GetLogger(),
                             ctx.GetAlarm(),
                             "string param TopicType is not valid",
                             moduleName,
                             ctx.GetConfigName(),
                             ctx.GetProjectName(),
                             ctx.GetLogstoreName(),
                             ctx.GetRegion());
    }

    // TopicFormat
    if (mTopicType == TopicType::CUSTOM || mTopicType == TopicType::MACHINE_GROUP_TOPIC
        || mTopicType == TopicType::FILEPATH) {
        if (!GetMandatoryStringParam(config, "TopicFormat", mTopicFormat, errorMsg)) {
            mTopicType = TopicType::NONE;
            LOG_WARNING(
                ctx.GetLogger(),
                ("problem encountered in config parsing", errorMsg)("action", "ignore param TopicType and TopicFormat")(
                    "module", moduleName)("config", ctx.GetConfigName()));
            ctx.GetAlarm().SendAlarm(CATEGORY_CONFIG_ALARM,
                                     errorMsg
                                         + ": ignore param TopicType and TopicFormat, config: " + ctx.GetConfigName(),
                                     ctx.GetProjectName(),
                                     ctx.GetLogstoreName(),
                                     ctx.GetRegion());
        } else if (mTopicType == TopicType::FILEPATH && !NormalizeTopicRegFormat(mTopicFormat)) {
            mTopicType = TopicType::NONE;
            mTopicFormat.clear();
            LOG_WARNING(ctx.GetLogger(),
                        ("problem encountered in config parsing",
                         "string param TopicFormat is not valid")("action", "ignore param TopicType and TopicFormat")(
                            "module", moduleName)("config", ctx.GetConfigName()));
            ctx.GetAlarm().SendAlarm(
                CATEGORY_CONFIG_ALARM,
                "string param TopicFormat is not valid: ignore param TopicType and TopicFormat, config: "
                    + ctx.GetConfigName(),
                ctx.GetProjectName(),
                ctx.GetLogstoreName(),
                ctx.GetRegion());
        }
    }

    // Priority
    uint32_t priority = 1;
    if (!GetOptionalUIntParam(config, "Priority", priority, errorMsg)) {
        PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                              ctx.GetAlarm(),
                              errorMsg,
                              mPriority,
                              moduleName,
                              ctx.GetConfigName(),
                              ctx.GetProjectName(),
                              ctx.GetLogstoreName(),
                              ctx.GetRegion());
    } else if (priority > ProcessQueueManager::sMaxPriority) {
        PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                              ctx.GetAlarm(),
                              "param Priority is out of range",
                              ProcessQueueManager::sMaxPriority,
                              moduleName,
                              ctx.GetConfigName(),
                              ctx.GetProjectName(),
                              ctx.GetLogstoreName(),
                              ctx.GetRegion());
        mPriority = ProcessQueueManager::sMaxPriority;
    } else {
        mPriority = priority;
    }

    // EnableTimestampNanosecond
    if (!GetOptionalBoolParam(config, "EnableTimestampNanosecond", mEnableTimestampNanosecond, errorMsg)) {
        PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                              ctx.GetAlarm(),
                              errorMsg,
                              mEnableTimestampNanosecond,
                              moduleName,
                              ctx.GetConfigName(),
                              ctx.GetProjectName(),
                              ctx.GetLogstoreName(),
                              ctx.GetRegion());
    }

    // UsingOldContentTag
    if (!GetOptionalBoolParam(config, "UsingOldContentTag", mUsingOldContentTag, errorMsg)) {
        PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                              ctx.GetAlarm(),
                              errorMsg,
                              mUsingOldContentTag,
                              moduleName,
                              ctx.GetConfigName(),
                              ctx.GetProjectName(),
                              ctx.GetLogstoreName(),
                              ctx.GetRegion());
    }

    for (auto itr = config.begin(); itr != config.end(); ++itr) {
        if (sNativeParam.find(itr.name()) == sNativeParam.end()) {
            extendedParams[itr.name()] = *itr;
        }
    }

    // PipelineMetaTagKey
    unordered_map<string, string> tagKeys;
    if (!GetOptionalMapParam(config, "PipelineMetaTagKey", tagKeys, errorMsg)) {
        PARAM_WARNING_IGNORE(ctx.GetLogger(),
                             ctx.GetAlarm(),
                             errorMsg,
                             moduleName,
                             ctx.GetConfigName(),
                             ctx.GetProjectName(),
                             ctx.GetLogstoreName(),
                             ctx.GetRegion());
    }
#ifdef __ENTERPRISE__
    mPipelineMetaTagKey[TagKey::AGENT_TAG] = DEFAULT_CONFIG_TAG_KEY_VALUE;
#endif
    for (const auto& kv : tagKeys) {
        if (kv.first == "HOST_NAME") {
            mPipelineMetaTagKey[TagKey::HOST_NAME] = kv.second;
        } else if (kv.first == "HOST_ID") {
            mPipelineMetaTagKey[TagKey::HOST_ID] = kv.second;
        } else if (kv.first == "CLOUD_PROVIDER") {
            mPipelineMetaTagKey[TagKey::CLOUD_PROVIDER] = kv.second;
        }
#ifdef __ENTERPRISE__
        else if (kv.first == "AGENT_TAG") {
            mPipelineMetaTagKey[TagKey::AGENT_TAG] = kv.second;
        }
#else
        else if (kv.first == "HOST_IP") {
            mPipelineMetaTagKey[TagKey::HOST_IP] = kv.second;
        }
#endif
    }

#ifdef __ENTERPRISE__
    // AgentEnvMetaTagKey
    const std::string key = "AgentEnvMetaTagKey";
    const Json::Value* itr = config.find(key.c_str(), key.c_str() + key.length());
    if (itr) {
        mEnableAgentEnvMetaTagControl = true;
    }
    if (!GetOptionalMapParam(config, "AgentEnvMetaTagKey", mAgentEnvMetaTagKey, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }
#endif
    return true;
}

Json::Value GlobalConfig::GetPipelineMetaTagKeyJsonValue() const {
    Json::Value json;
    for (const auto& kv : mPipelineMetaTagKey) {
        json[kv.first] = kv.second;
    }
    return json;
}

} // namespace logtail
