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


#include "config/feedbacker/ConfigFeedbackReceiver.h"

#include <unordered_map>

namespace logtail {

ConfigFeedbackReceiver& ConfigFeedbackReceiver::GetInstance() {
    static ConfigFeedbackReceiver instance;
    return instance;
}

void ConfigFeedbackReceiver::RegisterPipelineConfig(const std::string& name, ConfigFeedbackable* feedbackable) {
    std::lock_guard<std::mutex> lock(mMutex);
    mPipelineConfigFeedbackableMap[name] = feedbackable;
}

void ConfigFeedbackReceiver::RegisterInstanceConfig(const std::string& name, ConfigFeedbackable* feedbackable) {
    std::lock_guard<std::mutex> lock(mMutex);
    mInstanceConfigFeedbackableMap[name] = feedbackable;
}

void ConfigFeedbackReceiver::RegisterCommand(const std::string& type,
                                             const std::string& name,
                                             ConfigFeedbackable* feedbackable) {
    std::lock_guard<std::mutex> lock(mMutex);
    mCommandFeedbackableMap[GenerateCommandFeedBackKey(type, name)] = feedbackable;
}

void ConfigFeedbackReceiver::UnregisterPipelineConfig(const std::string& name) {
    std::lock_guard<std::mutex> lock(mMutex);
    mPipelineConfigFeedbackableMap.erase(name);
}

void ConfigFeedbackReceiver::UnregisterInstanceConfig(const std::string& name) {
    std::lock_guard<std::mutex> lock(mMutex);
    mInstanceConfigFeedbackableMap.erase(name);
}

void ConfigFeedbackReceiver::UnregisterCommand(const std::string& type, const std::string& name) {
    std::lock_guard<std::mutex> lock(mMutex);
    mCommandFeedbackableMap.erase(GenerateCommandFeedBackKey(type, name));
}

void ConfigFeedbackReceiver::FeedbackPipelineConfigStatus(const std::string& name, ConfigFeedbackStatus status) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto iter = mPipelineConfigFeedbackableMap.find(name);
    if (iter != mPipelineConfigFeedbackableMap.end()) {
        iter->second->FeedbackPipelineConfigStatus(name, status);
    }
}

void ConfigFeedbackReceiver::FeedbackInstanceConfigStatus(const std::string& name, ConfigFeedbackStatus status) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto iter = mInstanceConfigFeedbackableMap.find(name);
    if (iter != mInstanceConfigFeedbackableMap.end()) {
        iter->second->FeedbackInstanceConfigStatus(name, status);
    }
}

void ConfigFeedbackReceiver::FeedbackCommandConfigStatus(const std::string& type,
                                                         const std::string& name,
                                                         ConfigFeedbackStatus status) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto iter = mCommandFeedbackableMap.find(GenerateCommandFeedBackKey(type, name));
    if (iter != mCommandFeedbackableMap.end()) {
        iter->second->FeedbackCommandConfigStatus(type, name, status);
    }
}

std::string GenerateCommandFeedBackKey(const std::string& type, const std::string& name) {
    return type + '\1' + name;
}

} // namespace logtail
