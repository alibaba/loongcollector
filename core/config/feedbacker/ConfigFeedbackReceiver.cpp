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

void ConfigFeedbackReceiver::RegisterContinuousPipelineConfig(const std::string& name,
                                                              ConfigFeedbackable* feedbackable) {
    std::lock_guard<std::mutex> lock(mMutex);
    mContinuousPipelineConfigFeedbackableMap[name] = feedbackable;
}

void ConfigFeedbackReceiver::RegisterInstanceConfig(const std::string& name, ConfigFeedbackable* feedbackable) {
    std::lock_guard<std::mutex> lock(mMutex);
    mInstanceConfigFeedbackableMap[name] = feedbackable;
}

void ConfigFeedbackReceiver::RegisterOnetimePipelineConfig(const std::string& type,
                                                           const std::string& name,
                                                           ConfigFeedbackable* feedbackable) {
    std::lock_guard<std::mutex> lock(mMutex);
    mOnetimePipelineConfigFeedbackableMap[GenerateOnetimePipelineConfigFeedBackKey(type, name)] = feedbackable;
}

void ConfigFeedbackReceiver::UnregisterContinuousPipelineConfig(const std::string& name) {
    std::lock_guard<std::mutex> lock(mMutex);
    mContinuousPipelineConfigFeedbackableMap.erase(name);
}

void ConfigFeedbackReceiver::UnregisterInstanceConfig(const std::string& name) {
    std::lock_guard<std::mutex> lock(mMutex);
    mInstanceConfigFeedbackableMap.erase(name);
}

void ConfigFeedbackReceiver::UnregisterOnetimePipelineConfig(const std::string& type, const std::string& name) {
    std::lock_guard<std::mutex> lock(mMutex);
    mOnetimePipelineConfigFeedbackableMap.erase(GenerateOnetimePipelineConfigFeedBackKey(type, name));
}

void ConfigFeedbackReceiver::FeedbackContinuousPipelineConfigStatus(const std::string& name,
                                                                    ConfigFeedbackStatus status) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto iter = mContinuousPipelineConfigFeedbackableMap.find(name);
    if (iter != mContinuousPipelineConfigFeedbackableMap.end()) {
        iter->second->FeedbackContinuousPipelineConfigStatus(name, status);
    }
}

void ConfigFeedbackReceiver::FeedbackInstanceConfigStatus(const std::string& name, ConfigFeedbackStatus status) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto iter = mInstanceConfigFeedbackableMap.find(name);
    if (iter != mInstanceConfigFeedbackableMap.end()) {
        iter->second->FeedbackInstanceConfigStatus(name, status);
    }
}

void ConfigFeedbackReceiver::FeedbackOnetimePipelineConfigStatus(const std::string& type,
                                                                 const std::string& name,
                                                                 ConfigFeedbackStatus status) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto iter = mOnetimePipelineConfigFeedbackableMap.find(GenerateOnetimePipelineConfigFeedBackKey(type, name));
    if (iter != mOnetimePipelineConfigFeedbackableMap.end()) {
        iter->second->FeedbackOnetimePipelineConfigStatus(type, name, status);
    }
}

std::string GenerateOnetimePipelineConfigFeedBackKey(const std::string& type, const std::string& name) {
    return type + '\1' + name;
}

} // namespace logtail
