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

#pragma once

#include <mutex>
#include <unordered_map>

#include "config/feedbacker/ConfigFeedbackable.h"

namespace logtail {

std::string GenerateCommandFeedBackKey(const std::string& type, const std::string& name);

class ConfigFeedbackReceiver {
public:
    static ConfigFeedbackReceiver& GetInstance();
    void RegisterPipelineConfig(const std::string& name, ConfigFeedbackable* feedbackable);
    void RegisterInstanceConfig(const std::string& name, ConfigFeedbackable* feedbackable);
    void RegisterCommand(const std::string& type, const std::string& name, ConfigFeedbackable* feedbackable);
    void UnregisterPipelineConfig(const std::string& name);
    void UnregisterInstanceConfig(const std::string& name);
    void UnregisterCommand(const std::string& type, const std::string& name);
    void FeedbackPipelineConfigStatus(const std::string& name, ConfigFeedbackStatus status);
    void FeedbackInstanceConfigStatus(const std::string& name, ConfigFeedbackStatus status);
    void FeedbackCommandConfigStatus(const std::string& type, const std::string& name, ConfigFeedbackStatus status);

private:
    ConfigFeedbackReceiver() {}
    std::mutex mMutex;
    std::unordered_map<std::string, ConfigFeedbackable*> mPipelineConfigFeedbackableMap;
    std::unordered_map<std::string, ConfigFeedbackable*> mInstanceConfigFeedbackableMap;
    std::unordered_map<std::string, ConfigFeedbackable*> mCommandFeedbackableMap;
};

} // namespace logtail
