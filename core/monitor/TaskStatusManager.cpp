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
#include "monitor/TaskStatusManager.h"

#include "common/ParamExtractor.h"
#include "monitor/SelfMonitorServer.h"
#include "plugin/input/InputStaticFile.h"
#include "provider/Provider.h"

namespace logtail {

// task content keys
const std::string TASK_CONTENT_KEY_TASK_TYPE = "task_type";
const std::string TASK_CONTENT_KEY_PROJECT = "project";
const std::string TASK_CONTENT_KEY_CONFIG_NAME = "config_name";
const std::string TASK_CONTENT_KEY_STATUS = "status";
const std::string TASK_CONTENT_KEY_START_TIME = "start_time";
const std::string TASK_CONTENT_KEY_EXPIRE_TIME = "expire_time";

// task types
const std::string& TASK_TYPE_STATIC_FILE = InputStaticFile::sName;

LogEvent* TaskStatusManager::AddTaskStatus(const std::string& region) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mTaskStatusMap.find(region) == mTaskStatusMap.end()) {
        PipelineEventGroup pipelineEventGroup(std::make_shared<SourceBuffer>());
        // metadata for flusher(region, dataType
        pipelineEventGroup.SetMetadata(EventGroupMetaKey::INTERNAL_DATA_TARGET_REGION, region);
        pipelineEventGroup.SetMetadata(EventGroupMetaKey::INTERNAL_DATA_TYPE,
                                       SelfMonitorServer::INTERNAL_DATA_TYPE_TASK_STATUS);
        mTaskStatusMap.emplace(region, std::move(pipelineEventGroup));
    }
    return mTaskStatusMap.at(region).AddLogEvent();
}

void TaskStatusManager::FlushTaskStatus(std::vector<PipelineEventGroup>& pipelineEventGroupList) {
    std::lock_guard<std::mutex> lock(mMutex);
    for (auto& [key, pipelineEventGroup] : mTaskStatusMap) {
        if (pipelineEventGroup.GetEvents().size() <= 0) {
            continue;
        }
        pipelineEventGroupList.emplace_back(std::move(pipelineEventGroup));
    }
    mTaskStatusMap.clear();
}

} // namespace logtail
