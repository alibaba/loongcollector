// Copyright 2025 iLogtail Authors
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

#include "plugin/input/InputHostMonitor.h"

#include <sys/types.h>

#include "StringTools.h"
#include "common/ParamExtractor.h"
#include "host_monitor/HostMonitorInputRunner.h"
#include "host_monitor/collector/CPUCollector.h"

namespace logtail {

const std::string InputHostMonitor::sName = "input_host_monitor";
const uint32_t kMinInterval = 5; // seconds
const uint32_t kDefaultInterval = 15; // seconds

bool InputHostMonitor::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    std::string errorMsg;
    mInterval = kDefaultInterval;
    if (!GetOptionalUIntParam(config, "Interval", mInterval, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mInterval,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }
    if (mInterval < kMinInterval) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              "uint param Interval is smaller than" + ToString(kMinInterval),
                              mInterval,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
        mInterval = kMinInterval;
    }

    // TODO: add more collectors
    // cpu
    bool enableCPU = true;
    if (!GetOptionalBoolParam(config, "EnableCPU", enableCPU, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
    if (enableCPU) {
        mCollectors.push_back(CPUCollector::sName);
    }
    return true;
}

bool InputHostMonitor::Start() {
    HostMonitorInputRunner::GetInstance()->Init();
    HostMonitorInputRunner::GetInstance()->UpdateCollector(
        mCollectors, std::vector(mCollectors.size(), mInterval), mContext->GetProcessQueueKey(), mIndex);
    return true;
}

bool InputHostMonitor::Stop(bool isPipelineRemoving) {
    HostMonitorInputRunner::GetInstance()->RemoveCollector(mCollectors);
    return true;
}

} // namespace logtail
