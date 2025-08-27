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

#include "journal_server/JournalServer.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <utility>

#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "collection_pipeline/queue/QueueKey.h"
#include "models/PipelineEventGroup.h"
#include "common/Flags.h"
#include "common/LogtailCommonFlags.h"
#include "common/memory/SourceBuffer.h"
#include "logger/Logger.h"
#include "runner/ProcessorRunner.h"

DEFINE_FLAG_INT32(journal_server_checkpoint_dump_interval_sec, "", 10);

using namespace std;

namespace logtail {

void JournalServer::Init() {
    mThreadRes = async(launch::async, &JournalServer::Run, this);
    mStartTime = time(nullptr);
}

void JournalServer::Stop() {
    if (!mThreadRes.valid()) {
        return;
    }
    {
        lock_guard<mutex> lock(mThreadRunningMux);
        mIsThreadRunning = false;
    }
    mStopCV.notify_all();

    future_status s = mThreadRes.wait_for(chrono::seconds(1));
    if (s == future_status::ready) {
        LOG_INFO(sLogger, ("journal server", "stopped successfully"));
    } else {
        LOG_WARNING(sLogger, ("journal server", "forced to stopped"));
    }
}

bool JournalServer::HasRegisteredPlugins() const {
    lock_guard<mutex> lock(mUpdateMux);
    return !mPipelineNameJournalConfigsMap.empty();
}

void JournalServer::ClearUnusedCheckpoints() {
    if (mIsUnusedCheckpointsCleared || time(nullptr) - mStartTime < INT32_FLAG(unused_checkpoints_clear_interval_sec)) {
        return;
    }
    
    // Clear checkpoints for deleted configurations
    lock_guard<mutex> lock(mUpdateMux);
    for (const auto& deletedInput : mDeletedInputs) {
        const auto& configName = deletedInput.first;
        size_t idx = deletedInput.second;
        
        auto configItr = mPipelineNameJournalConfigsMap.find(configName);
        if (configItr != mPipelineNameJournalConfigsMap.end()) {
            configItr->second.erase(idx);
            if (configItr->second.empty()) {
                mPipelineNameJournalConfigsMap.erase(configItr);
            }
        }
        
        // Clear checkpoint
        auto checkpointItr = mJournalCheckpoints.find(configName);
        if (checkpointItr != mJournalCheckpoints.end()) {
            checkpointItr->second.erase(idx);
            if (checkpointItr->second.empty()) {
                mJournalCheckpoints.erase(checkpointItr);
            }
        }
    }
    
    mDeletedInputs.clear();
    mIsUnusedCheckpointsCleared = true;
}

void JournalServer::AddJournalInput(const string& configName, size_t idx, const JournalConfig& config) {
    {
        lock_guard<mutex> lock(mUpdateMux);
        mPipelineNameJournalConfigsMap[configName][idx] = config;
        mAddedInputs.emplace(configName, idx);
    }
    
    // Save initial checkpoint if not exists
    if (GetJournalCheckpoint(configName, idx).empty()) {
        SaveJournalCheckpoint(configName, idx, "");
    }
}

void JournalServer::RemoveJournalInput(const string& configName, size_t idx) {
    {
        lock_guard<mutex> lock(mUpdateMux);
        auto configItr = mPipelineNameJournalConfigsMap.find(configName);
        if (configItr != mPipelineNameJournalConfigsMap.end()) {
            configItr->second.erase(idx);
            if (configItr->second.empty()) {
                mPipelineNameJournalConfigsMap.erase(configItr);
            }
        }
        mDeletedInputs.emplace(configName, idx);
    }
    
    // Clear checkpoint
    ClearJournalCheckpoint(configName, idx);
}

JournalConfig JournalServer::GetJournalConfig(const string& name, size_t idx) const {
    lock_guard<mutex> lock(mUpdateMux);
    auto configItr = mPipelineNameJournalConfigsMap.find(name);
    if (configItr != mPipelineNameJournalConfigsMap.end()) {
        auto idxItr = configItr->second.find(idx);
        if (idxItr != configItr->second.end()) {
            return idxItr->second;
        }
    }
    return JournalConfig();
}

void JournalServer::SaveJournalCheckpoint(const string& configName, size_t idx, const string& cursor) {
    lock_guard<mutex> lock(mUpdateMux);
    mJournalCheckpoints[configName][idx] = cursor;
}

std::string JournalServer::GetJournalCheckpoint(const std::string& configName, size_t idx) const {
    lock_guard<mutex> lock(mUpdateMux);
    auto configItr = mJournalCheckpoints.find(configName);
    if (configItr != mJournalCheckpoints.end()) {
        auto idxItr = configItr->second.find(idx);
        if (idxItr != configItr->second.end()) {
            return idxItr->second;
        }
    }
    return "";
}

void JournalServer::ClearJournalCheckpoint(const string& configName, size_t idx) {
    lock_guard<mutex> lock(mUpdateMux);
    auto configItr = mJournalCheckpoints.find(configName);
    if (configItr != mJournalCheckpoints.end()) {
        configItr->second.erase(idx);
        if (configItr->second.empty()) {
            mJournalCheckpoints.erase(configItr);
        }
    }
}

void JournalServer::Run() {
    LOG_INFO(sLogger, ("journal server", "started"));
    unique_lock<mutex> lock(mThreadRunningMux);
    time_t lastDumpCheckpointTime = time(nullptr);
    
    while (mIsThreadRunning) {
        lock.unlock();
        
        // Process journal entries for all registered configurations
        ProcessJournalEntries();
        
        // Periodically dump checkpoints
        auto cur = time(nullptr);
        if (cur - lastDumpCheckpointTime >= INT32_FLAG(journal_server_checkpoint_dump_interval_sec)) {
            // TODO: Implement checkpoint persistence to disk
            lastDumpCheckpointTime = cur;
        }
        
        lock.lock();
        if (mStopCV.wait_for(lock, chrono::milliseconds(100), [this]() { return !mIsThreadRunning; })) {
            return;
        }
    }
}

void JournalServer::ProcessJournalEntries() {
    // Get current configurations snapshot to avoid long lock holding
    unordered_map<string, map<size_t, JournalConfig>> currentConfigs;
    {
        lock_guard<mutex> lock(mUpdateMux);
        currentConfigs = mPipelineNameJournalConfigsMap;
    }
    
    // Process each configuration
    for (const auto& pipelineConfig : currentConfigs) {
        const string& configName = pipelineConfig.first;
        
        for (const auto& idxConfig : pipelineConfig.second) {
            size_t idx = idxConfig.first;
            const JournalConfig& config = idxConfig.second;
            
            // Check if this input has been deleted
            {
                lock_guard<mutex> lock(mUpdateMux);
                if (mDeletedInputs.find(make_pair(configName, idx)) != mDeletedInputs.end()) {
                    continue;
                }
            }
            
            // Process this journal configuration
            ProcessJournalConfig(configName, idx, config);
        }
    }
}

void JournalServer::ProcessJournalConfig(const string& configName, size_t idx, const JournalConfig& config) {
    if (!config.ctx) {
        LOG_WARNING(sLogger, ("no context available for journal config", "skip")("config", configName)("idx", idx));
        return;
    }
    
    // Get queue key from pipeline context
    QueueKey queueKey = config.ctx->GetProcessQueueKey();
    if (queueKey == -1) {
        LOG_WARNING(sLogger, ("no queue key available for journal config", "skip")("config", configName)("idx", idx));
        return;
    }
    
    // Check if queue is valid
    if (!ProcessQueueManager::GetInstance()->IsValidToPush(queueKey)) {
        LOG_DEBUG(sLogger, ("queue not valid for journal config", "skip")("config", configName)("idx", idx)("queue", queueKey));
        return;
    }
    
    // TODO: Implement actual journal reading logic here
    // This is a placeholder for the actual journal processing
    
    // For now, we'll create a simple event group to demonstrate the flow
    // In real implementation, this would:
    // 1. Read from systemd journal based on config
    // 2. Convert journal entries to PipelineEvent objects
    // 3. Group them into PipelineEventGroup
    // 4. Send to processing queue
    
    try {
        // Create a simple event group (placeholder)
        // In real implementation, this would be populated with actual journal data
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        auto group = std::make_unique<PipelineEventGroup>(sourceBuffer);
        
        // Add some metadata to the group using EventGroupMetaKey
        // For now, we'll use a simple approach - in real implementation,
        // you would create actual journal events and add them to the group
        
        // Send to processing queue
        if (!ProcessorRunner::GetInstance()->PushQueue(queueKey, idx, std::move(*group))) {
            LOG_ERROR(sLogger, 
                      ("failed to push journal data to process queue", "discard data")("config", configName)(
                          "input idx", idx)("queue", queueKey));
        } else {
            LOG_DEBUG(sLogger, 
                      ("successfully pushed journal data to process queue", "")("config", configName)(
                          "input idx", idx)("queue", queueKey));
        }
        
        // Update checkpoint (placeholder - would be actual cursor position)
        SaveJournalCheckpoint(configName, idx, "placeholder_cursor");
        
    } catch (const exception& e) {
        LOG_ERROR(sLogger, 
                  ("exception while processing journal config", e.what())("config", configName)(
                      "input idx", idx));
    }
}

#ifdef APSARA_UNIT_TEST_MAIN
void JournalServer::Clear() {
    lock_guard<mutex> lock(mUpdateMux);
    mPipelineNameJournalConfigsMap.clear();
    mJournalCheckpoints.clear();
    mAddedInputs.clear();
    mDeletedInputs.clear();
}
#endif

} // namespace logtail 