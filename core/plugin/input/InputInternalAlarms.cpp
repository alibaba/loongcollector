/*
 * Copyright 2024 iLogtail Authors
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

#include "plugin/input/InputInternalAlarms.h"

#include "monitor/SelfMonitorServer.h"

namespace logtail {

const std::string InputInternalAlarms::sName = "input_internal_alarms";

bool InputInternalAlarms::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    return true;
}

bool InputInternalAlarms::Start() {
    SelfMonitorServer::GetInstance()->UpdateAlarmPipeline(mContext, mIndex);
    return true;
}

bool InputInternalAlarms::Stop(bool isPipelineRemoving) {
    if (isPipelineRemoving) {
        SelfMonitorServer::GetInstance()->RemoveAlarmPipeline();
    }
    return true;
}

} // namespace logtail
