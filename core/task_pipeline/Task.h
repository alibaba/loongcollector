/*
 * Copyright 2024 iLogtail Authors
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

#include <json/json.h>

#include <string>

#include "logger/Logger.h"
#include "monitor/AlarmManager.h"

namespace logtail {

class Task {
public:
    virtual ~Task() {}

    virtual const std::string& Name() const = 0;
    virtual bool Init(const Json::Value& config) = 0;
    virtual void Start() = 0;
    virtual void Stop(bool isRemoving) = 0;

protected:
    Logger::logger mLogger = sLogger;
    AlarmManager* mAlarm = AlarmManager::GetInstance();
};

} // namespace logtail
