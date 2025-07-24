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

#include "plugin/input/InputForward.h"

#include <algorithm>

#include "common/ParamExtractor.h"
#include "forward/GrpcInputManager.h"
#include "forward/loongsuite/LoongSuiteForwardService.h"
#include "logger/Logger.h"

namespace logtail {

const std::string InputForward::sName = "input_forward";

const std::vector<std::string> InputForward::sSupportedProtocols = {
    "LoongSuite",
    // TODO: add more protocols here
};

bool InputForward::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    std::string errorMsg;

    if (!GetMandatoryStringParam(config, "Protocol", mProtocol, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    auto it = std::find(sSupportedProtocols.begin(), sSupportedProtocols.end(), mProtocol);
    if (it == sSupportedProtocols.end()) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           "Unsupported protocol '" + mProtocol,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    if (!GetMandatoryStringParam(config, "Endpoint", mEndpoint, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    const char* key = "MatchRule";
    const Json::Value* itr = config.find(key, key + strlen(key));
    if (itr && itr->isObject()) {
        if (!GetMandatoryStringParam(*itr, "Key", mMatchRule.key, errorMsg)) {
            PARAM_ERROR_RETURN(mContext->GetLogger(),
                               mContext->GetAlarm(),
                               errorMsg,
                               sName,
                               mContext->GetConfigName(),
                               mContext->GetProjectName(),
                               mContext->GetLogstoreName(),
                               mContext->GetRegion());
        }

        if (!GetMandatoryStringParam(*itr, "Value", mMatchRule.value, errorMsg)) {
            PARAM_ERROR_RETURN(mContext->GetLogger(),
                               mContext->GetAlarm(),
                               errorMsg,
                               sName,
                               mContext->GetConfigName(),
                               mContext->GetProjectName(),
                               mContext->GetLogstoreName(),
                               mContext->GetRegion());
        }
    } else {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           "Param MatchRule is required",
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    mConfigName = mContext->GetConfigName();

    LOG_INFO(
        sLogger,
        ("InputForward initialized", "success")("config", mConfigName)("endpoint", mEndpoint)("protocol", mProtocol));
    return true;
}

bool InputForward::Start() {
    Json::Value config;
    config["MatchRule"]["Key"] = mMatchRule.key;
    config["MatchRule"]["Value"] = mMatchRule.value;
    config["QueueKey"] = mContext->GetProcessQueueKey();
    config["InputIndex"] = static_cast<int>(mIndex);
    config["Protocol"] = mProtocol;

    bool result = false;

    if (mProtocol == "LoongSuite") {
        result = GrpcInputManager::GetInstance()->AddListenInput<LoongSuiteForwardServiceImpl>(
            mConfigName, mEndpoint, config);
    } else {
        LOG_WARNING(sLogger, ("Protocol not fully implemented, should not happen", mProtocol)("config", mConfigName));
    }

    if (!result) {
        LOG_ERROR(sLogger,
                  ("InputForward failed to start service", mEndpoint)("config", mConfigName)("protocol", mProtocol));
        return false;
    }

    LOG_INFO(sLogger, ("InputForward started successfully", mEndpoint)("config", mConfigName)("protocol", mProtocol));
    return true;
}

bool InputForward::Stop(bool isPipelineRemoving) {
    bool result
        = GrpcInputManager::GetInstance()->RemoveListenInput<LoongSuiteForwardServiceImpl>(mEndpoint, mConfigName);

    if (result) {
        LOG_INFO(sLogger, ("InputForward stopped successfully", mEndpoint)("config", mConfigName));
    } else {
        LOG_ERROR(sLogger, ("InputForward failed to stop service", mEndpoint)("config", mConfigName));
    }

    return result;
}

} // namespace logtail
