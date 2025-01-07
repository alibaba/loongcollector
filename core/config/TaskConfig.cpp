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

#include "config/TaskConfig.h"

#include "common/ParamExtractor.h"
#include "task_pipeline/TaskRegistry.h"

using namespace std;

namespace logtail {

bool TaskConfig::Parse() {
    string errorMsg;
    AlarmManager& alarm = *AlarmManager::GetInstance();

    if (!GetOptionalUIntParam(*mDetail, "createTime", mCreateTime, errorMsg)) {
        TASK_PARAM_WARNING_DEFAULT(sLogger, alarm, errorMsg, mCreateTime, noModule, mName);
    }

    auto& detail = (*mDetail)["task"];
    if (!detail.isObject()) {
        TASK_PARAM_ERROR_RETURN(sLogger, alarm, "task module is not of type object", noModule, mName);
    }

    string key = "Type";
    auto itr = detail.find(key.c_str(), key.c_str() + key.size());
    if (itr == nullptr) {
        TASK_PARAM_ERROR_RETURN(sLogger, alarm, "param task.Type is missing", noModule, mName);
    }
    if (!itr->isString()) {
        TASK_PARAM_ERROR_RETURN(sLogger, alarm, "param task.Type is not of type string", noModule, mName);
    }
    string pluginType = itr->asString();
    if (!TaskRegistry::GetInstance()->IsValidPlugin(pluginType)) {
        TASK_PARAM_ERROR_RETURN(sLogger, alarm, "unsupported task plugin", pluginType, mName);
    }
    return true;
}

} // namespace logtail
