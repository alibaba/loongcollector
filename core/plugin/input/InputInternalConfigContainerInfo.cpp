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

#include "plugin/input/InputInternalConfigContainerInfo.h"


namespace logtail {

const std::string InputInternalConfigContainerInfo::sName = "input_internal_config_container_info";

bool InputInternalConfigContainerInfo::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    return true;
}

bool InputInternalConfigContainerInfo::Start() {
    ContainerManager::GetInstance()->UpdateConfigContainerInfoPipeline(mContext, mIndex);
    return true;
}

bool InputInternalConfigContainerInfo::Stop(bool isPipelineRemoving) {
    if (isPipelineRemoving) {
        ContainerManager::GetInstance()->RemoveConfigContainerInfoPipeline();
    }
    return true;
}

} // namespace logtail
