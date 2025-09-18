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

using namespace std;

namespace logtail {

void ConfigFeedbackReceiver::RegisterContinuousPipelineConfig(const string& name, ConfigFeedbackable* feedbackable) {
    lock_guard<mutex> lock(mMutex);
    mContinuousPipelineConfigFeedbackableMap[name] = feedbackable;
}

void ConfigFeedbackReceiver::RegisterOnetimePipelineConfig(const string& name, ConfigFeedbackable* feedbackable) {
    lock_guard<mutex> lock(mMutex);
    mOnetimePipelineConfigFeedbackableMap[name] = feedbackable;
}

void ConfigFeedbackReceiver::RegisterInstanceConfig(const string& name, ConfigFeedbackable* feedbackable) {
    lock_guard<mutex> lock(mMutex);
    mInstanceConfigFeedbackableMap[name] = feedbackable;
}

void ConfigFeedbackReceiver::UnregisterContinuousPipelineConfig(const string& name) {
    lock_guard<mutex> lock(mMutex);
    mContinuousPipelineConfigFeedbackableMap.erase(name);
}

void ConfigFeedbackReceiver::UnregisterOnetimePipelineConfig(const string& name) {
    lock_guard<mutex> lock(mMutex);
    mOnetimePipelineConfigFeedbackableMap.erase(name);
}

void ConfigFeedbackReceiver::UnregisterInstanceConfig(const string& name) {
    lock_guard<mutex> lock(mMutex);
    mInstanceConfigFeedbackableMap.erase(name);
}

void ConfigFeedbackReceiver::FeedbackContinuousPipelineConfigStatus(const string& name, ConfigFeedbackStatus status) {
    ConfigFeedbackable* feedbackable = nullptr;
    {
        lock_guard<mutex> lock(mMutex);
        auto iter = mContinuousPipelineConfigFeedbackableMap.find(name);
        if (iter != mContinuousPipelineConfigFeedbackableMap.end()) {
            feedbackable = iter->second;
        }
    }
    if (feedbackable) {
        feedbackable->FeedbackContinuousPipelineConfigStatus(name, status);
    }
}

void ConfigFeedbackReceiver::FeedbackOnetimePipelineConfigStatus(const string& name, ConfigFeedbackStatus status) {
    ConfigFeedbackable* feedbackable = nullptr;
    {
        lock_guard<mutex> lock(mMutex);
        auto iter = mOnetimePipelineConfigFeedbackableMap.find(name);
        if (iter != mOnetimePipelineConfigFeedbackableMap.end()) {
            feedbackable = iter->second;
        }
    }
    if (feedbackable) {
        feedbackable->FeedbackOnetimePipelineConfigStatus(name, status);
    }
}

void ConfigFeedbackReceiver::FeedbackInstanceConfigStatus(const string& name, ConfigFeedbackStatus status) {
    ConfigFeedbackable* feedbackable = nullptr;
    {
        lock_guard<mutex> lock(mMutex);
        auto iter = mInstanceConfigFeedbackableMap.find(name);
        if (iter != mInstanceConfigFeedbackableMap.end()) {
            feedbackable = iter->second;
        }
    }
    if (feedbackable) {
        feedbackable->FeedbackInstanceConfigStatus(name, status);
    }
}

} // namespace logtail
