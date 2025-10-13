/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cstdint>

#include <string>

#include "json/json.h"

#include "logger/Logger.h"
#include "models/PipelineEventGroup.h"
#include "monitor/AlarmManager.h"

namespace logtail {

class TaskPipeline;

class TaskPipelineContext {
public:
    TaskPipelineContext() {}
    TaskPipelineContext(const TaskPipelineContext&) = delete;
    TaskPipelineContext(TaskPipelineContext&&) = delete;
    TaskPipelineContext operator=(const TaskPipelineContext&) = delete;
    TaskPipelineContext operator=(TaskPipelineContext&&) = delete;

    const std::string& GetConfigName() const { return mConfigName; }
    void SetConfigName(const std::string& configName) { mConfigName = configName; }
    uint32_t GetCreateTime() const { return mCreateTime; }
    void SetCreateTime(uint32_t time) { mCreateTime = time; }

    const Logger::logger& GetLogger() const { return mLogger; }
    AlarmManager& GetAlarm() const { return *mAlarm; };

private:
    std::string mConfigName;
    uint32_t mCreateTime;

    Logger::logger mLogger = sLogger;
    AlarmManager* mAlarm = AlarmManager::GetInstance();
};

} // namespace logtail
