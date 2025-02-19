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

#include <algorithm>

#include "json/value.h"

#include "Flags.h"
#include "common/ParamExtractor.h"
#include "constants/EntityConstants.h"
#include "host_monitor/HostMonitorInputRunner.h"
#include "logger/Logger.h"

DEFINE_FLAG_INT32(host_monitor_collect_min_interval, "host monitor collect min interval", 15);

namespace logtail {

const std::string InputHostMeta::sName = "input_host_meta";

bool InputHostMeta::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    std::string errorMsg;
    if (!GetOptionalIntParam(config, "Interval", mInterval, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              false,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }
    mInterval = std::max(mInterval, INT32_FLAG(host_monitor_collect_min_interval));
    return true;
}

bool InputHostMeta::Start() {
    LOG_INFO(sLogger, ("input host meta start", mContext->GetConfigName()));
    HostMonitorInputRunner::GetInstance()->Init();
    HostMonitorInputRunner::GetInstance()->UpdateCollector(
        {"process_entity"}, mContext->GetProcessQueueKey(), mIndex, mInterval);
    return true;
}

bool InputHostMeta::Stop(bool isPipelineRemoving) {
    LOG_INFO(sLogger, ("input host meta stop", mContext->GetConfigName()));
    HostMonitorInputRunner::GetInstance()->RemoveCollector();
    return true;
}

} // namespace logtail
