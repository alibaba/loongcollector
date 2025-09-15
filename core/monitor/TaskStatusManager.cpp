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
#include "provider/Provider.h"
#include "plugin/input/InputStaticFile.h"

namespace logtail {

const std::string& TASK_TYPE_STATIC_FILE = InputStaticFile::sName;

LogEvent* TaskStatusManager::AddTaskStatus(const std::string& taskType, const std::string& region) {
    std::string key = taskType + "_" + region;
    std::lock_guard<std::mutex> lock(mMutex);
    if (mTaskStatusMap.find(key) == mTaskStatusMap.end()) {
        PipelineEventGroup pipelineEventGroup(std::make_shared<SourceBuffer>());
        // metadata for flusher(region, dataType
        pipelineEventGroup.SetMetadata(EventGroupMetaKey::INTERNAL_DATA_TARGET_REGION, region);
        pipelineEventGroup.SetMetadata(EventGroupMetaKey::INTERNAL_DATA_TYPE,
                                       SelfMonitorServer::INTERNAL_DATA_TYPE_TASK_STATUS);
        // tags for query(task_type)
        pipelineEventGroup.SetTagNoCopy(LOG_RESERVED_KEY_TASK_TYPE, taskType);
        mTaskStatusMap.emplace(key, std::move(pipelineEventGroup));
    }
    return mTaskStatusMap.at(key).AddLogEvent();
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
