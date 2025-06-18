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

#include "plugin/input/InputApmAgentInject.h"

#include <memory>

#include "apm/ApmInjectRunner.h"
#include "apm/Types.h"

namespace logtail {

const std::string ApmAgentInjectTask::sName = "apm_agent_inject";

bool ApmAgentInjectTask::Init(const Json::Value& config) {
    apm::ApmInjectRunner::GetInstance()->Init();
    mAttachConfig = std::make_unique<apm::AttachConfig>();
    return apm::InitApmAttachOption(mContext, config, mAttachConfig, ApmAgentInjectTask::sName);
}

void ApmAgentInjectTask::Start() {
    apm::ApmInjectRunner::GetInstance()->InjectApmAgent(mContext, std::move(mAttachConfig));
}

void ApmAgentInjectTask::Stop(bool isRemoving) {
    if (isRemoving) {
        apm::ApmInjectRunner::GetInstance()->RemoveApmAgent(mContext);
    }
}

} // namespace logtail
