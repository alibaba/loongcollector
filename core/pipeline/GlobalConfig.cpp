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

#include <json/json.h>

#include "common/ParamExtractor.h"
#include "pipeline/PipelineContext.h"
#include "pipeline/queue/ProcessQueueManager.h"

using namespace std;

namespace logtail {

const unordered_set<string> GlobalConfig::sNativeParam
    = {"TopicType", "TopicFormat", "ProcessPriority", "EnableTimestampNanosecond", "UsingOldContentTag"};

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

    // ProcessPriority
    uint32_t priority = 0;
    if (!GetOptionalUIntParam(config, "ProcessPriority", priority, errorMsg)) {
        PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                              ctx.GetAlarm(),
                              errorMsg,
                              mProcessPriority,
                              moduleName,
                              ctx.GetConfigName(),
                              ctx.GetProjectName(),
                              ctx.GetLogstoreName(),
                              ctx.GetRegion());
    } else if (priority > ProcessQueueManager::sMaxPriority) {
        PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                              ctx.GetAlarm(),
                              errorMsg,
                              mProcessPriority,
                              moduleName,
                              ctx.GetConfigName(),
                              ctx.GetProjectName(),
                              ctx.GetLogstoreName(),
                              ctx.GetRegion());
    } else {
        mProcessPriority = priority;
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

    return true;
}

} // namespace logtail
