// Copyright 2024 iLogtail Authors
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

#include "pipeline/route/Condition.h"

#include "common/ParamExtractor.h"

using namespace std;

namespace logtail {

bool EventTypeCondition::Init(const Json::Value& config, const PipelineContext& ctx) {
    string errorMsg;
    string value;
    if (!GetMandatoryStringParam(config, "Match.Value", value, errorMsg)) {
        PARAM_ERROR_RETURN(ctx.GetLogger(),
                           ctx.GetAlarm(),
                           errorMsg,
                           noModule,
                           ctx.GetConfigName(),
                           ctx.GetProjectName(),
                           ctx.GetLogstoreName(),
                           ctx.GetRegion());
    }
    if (value == "log") {
        mType = PipelineEvent::Type::LOG;
    } else if (value == "metric") {
        mType = PipelineEvent::Type::METRIC;
    } else if (value == "trace") {
        mType = PipelineEvent::Type::SPAN;
    } else {
        PARAM_ERROR_RETURN(ctx.GetLogger(),
                           ctx.GetAlarm(),
                           "param Match.Value is not valid",
                           noModule,
                           ctx.GetConfigName(),
                           ctx.GetProjectName(),
                           ctx.GetLogstoreName(),
                           ctx.GetRegion());
    }
    return true;
}

bool EventTypeCondition::Check(const PipelineEventGroup& g) const {
    if (g.GetEvents().empty()) {
        return false;
    }
    return g.GetEvents()[0]->GetType() == mType;
}

bool TagCondition::Init(const Json::Value& config, const PipelineContext& ctx) {
    string errorMsg;

    // Key
    if (!GetMandatoryStringParam(config, "Match.Key", mKey, errorMsg)) {
        PARAM_ERROR_RETURN(ctx.GetLogger(),
                           ctx.GetAlarm(),
                           errorMsg,
                           noModule,
                           ctx.GetConfigName(),
                           ctx.GetProjectName(),
                           ctx.GetLogstoreName(),
                           ctx.GetRegion());
    }

    // Value
    if (!GetMandatoryStringParam(config, "Match.Value", mValue, errorMsg)) {
        PARAM_ERROR_RETURN(ctx.GetLogger(),
                           ctx.GetAlarm(),
                           errorMsg,
                           noModule,
                           ctx.GetConfigName(),
                           ctx.GetProjectName(),
                           ctx.GetLogstoreName(),
                           ctx.GetRegion());
    }

    return true;
}

bool TagCondition::Check(const PipelineEventGroup& g) const {
    return g.GetTag(mKey) == mValue;
}

bool Condition::Init(const Json::Value& config, const PipelineContext& ctx) {
    string errorMsg;

    if (!config.isObject()) {
        PARAM_ERROR_RETURN(ctx.GetLogger(),
                           ctx.GetAlarm(),
                           "param Match is not of type object",
                           noModule,
                           ctx.GetConfigName(),
                           ctx.GetProjectName(),
                           ctx.GetLogstoreName(),
                           ctx.GetRegion());
    }

    // Type
    string type;
    if (!GetMandatoryStringParam(config, "Type", type, errorMsg)) {
        PARAM_ERROR_RETURN(ctx.GetLogger(),
                           ctx.GetAlarm(),
                           errorMsg,
                           noModule,
                           ctx.GetConfigName(),
                           ctx.GetProjectName(),
                           ctx.GetLogstoreName(),
                           ctx.GetRegion());
    } else if (type == "event_type") {
        mType = Type::EVENT_TYPE;
    } else if (type == "tag") {
        mType = Type::TAG;
    } else {
        PARAM_ERROR_RETURN(ctx.GetLogger(),
                           ctx.GetAlarm(),
                           "string param Match.Type is not valid",
                           noModule,
                           ctx.GetConfigName(),
                           ctx.GetProjectName(),
                           ctx.GetLogstoreName(),
                           ctx.GetRegion());
    }

    switch (mType) {
        case Type::EVENT_TYPE:
            if (!mDetail.emplace<EventTypeCondition>().Init(config, ctx)) {
                return false;
            }
            break;
        case Type::TAG:
            if (!mDetail.emplace<TagCondition>().Init(config, ctx)) {
                return false;
            }
            break;
        default:
            return false;
    }

    return true;
}

bool Condition::Check(const PipelineEventGroup& g) const {
    switch (mType) {
        case Type::EVENT_TYPE:
            return get_if<EventTypeCondition>(&mDetail)->Check(g);
        case Type::TAG:
            return get_if<TagCondition>(&mDetail)->Check(g);
        default:
            return false;
    }
}

} // namespace logtail
