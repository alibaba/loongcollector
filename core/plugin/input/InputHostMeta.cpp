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

#include "InputHostMeta.h"

#include <memory>
#include <utility>

#include "HostMonitorInputRunner.h"
#include "Logger.h"
#include "PluginRegistry.h"
#include "ProcessorInstance.h"
#include "constants/EntityConstants.h"
#include "json/value.h"
#include "pipeline/Pipeline.h"
#include "plugin/processor/inner/ProcessorHostMetaNative.h"

namespace logtail {

const std::string InputHostMeta::sName = "input_host_meta";

bool InputHostMeta::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    return CreateInnerProcessors(config);
}

bool InputHostMeta::Start() {
    LOG_INFO(sLogger, ("input host meta start", mContext->GetConfigName()));
    HostMonitorInputRunner::GetInstance()->Init();
    HostMonitorInputRunner::GetInstance()->UpdateCollector(
        mContext->GetConfigName(), {"process"}, mContext->GetProcessQueueKey(), mIndex);
    return true;
}

bool InputHostMeta::Stop(bool isPipelineRemoving) {
    LOG_INFO(sLogger, ("input host meta stop", mContext->GetConfigName()));
    if (isPipelineRemoving) {
        HostMonitorInputRunner::GetInstance()->RemoveCollector(mContext->GetConfigName());
    }
    return true;
}

bool InputHostMeta::CreateInnerProcessors(const Json::Value& config) {
    std::unique_ptr<ProcessorInstance> processor;
    {
        processor = PluginRegistry::GetInstance()->CreateProcessor(ProcessorHostMetaNative::sName,
                                                                   mContext->GetPipeline().GenNextPluginMeta(false));
        Json::Value detail;
        if (!processor->Init(detail, *mContext)) {
            return false;
        }
        mInnerProcessors.emplace_back(std::move(processor));
    }
    return true;
}

} // namespace logtail
