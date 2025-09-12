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

#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/plugin/PluginRegistry.h"
#include "common/ParamExtractor.h"
#include "forward/GrpcInputManager.h"
#include "forward/loongsuite/LoongSuiteForwardService.h"
#include "logger/Logger.h"
#include "plugin/processor/inner/ProcessorParseFromPBNative.h"

namespace logtail {

const std::string InputForward::sName = "input_forward";

const std::unordered_set<std::string> InputForward::sSupportedProtocols = {
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

    if (sSupportedProtocols.find(mProtocol) == sSupportedProtocols.end()) {
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

    mConfigName = mContext->GetConfigName();
    mForwardConfig["InputIndex"] = static_cast<int>(mIndex);
    mForwardConfig["Protocol"] = mProtocol;

    LOG_INFO(
        sLogger,
        ("InputForward initialized", "success")("config", mConfigName)("endpoint", mEndpoint)("protocol", mProtocol));
    return true;
}

bool InputForward::Start() {
    bool result = false;
    mForwardConfig["QueueKey"] = mContext->GetProcessQueueKey();

    std::unique_ptr<ProcessorInstance> processor;
    if (mProtocol == "LoongSuite") {
        result = GrpcInputManager::GetInstance()->AddListenInput<LoongSuiteForwardServiceImpl>(
            mConfigName, mEndpoint, mForwardConfig);
        processor = PluginRegistry::GetInstance()->CreateProcessor(ProcessorParseFromPBNative::sName,
                                                                   mContext->GetPipeline().GenNextPluginMeta(false));
        if (!processor->Init(mForwardConfig, *mContext)) {
            LOG_ERROR(sLogger, ("InputForward failed to init processor", mProtocol)("config", mConfigName));
            return false;
        }
        mInnerProcessors.emplace_back(std::move(processor));
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
    bool result = GrpcInputManager::GetInstance()->RemoveListenInput(mConfigName, mEndpoint, mForwardConfig);

    if (result) {
        LOG_INFO(sLogger, ("InputForward stopped successfully", mEndpoint)("config", mConfigName));
    } else {
        LOG_ERROR(sLogger, ("InputForward failed to stop service", mEndpoint)("config", mConfigName));
    }

    return result;
}

} // namespace logtail
