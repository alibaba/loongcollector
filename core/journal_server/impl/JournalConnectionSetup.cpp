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

#include "JournalServerCore.h"

#include <memory>

#include "connection/JournalConnectionManager.h"
#include "connection/JournalConnectionGuard.h"
#include "checkpoint/JournalCheckpointManager.h"
#include "reader/JournalReader.h"
#include "logger/Logger.h"

using namespace std;

namespace logtail::impl {

// =============================================================================
// Connection 函数实现
// =============================================================================

std::shared_ptr<SystemdJournalReader> SetupJournalConnection(const string& configName, size_t idx, const JournalConfig& config, std::unique_ptr<JournalConnectionGuard>& connectionGuard, bool& isNewConnection) {
    LOG_INFO(sLogger, ("getting guarded journal connection from manager", "")("config", configName)("idx", idx));
    
    // 记录连接获取前的连接数，用于判断是否创建了新连接
    size_t connectionCountBefore = JournalConnectionManager::GetInstance()->GetConnectionCount();
    
    connectionGuard = JournalConnectionManager::GetInstance()->GetGuardedConnection(configName, idx, config);
    
    if (!connectionGuard) {
        LOG_ERROR(sLogger, ("failed to get guarded journal connection", "skip processing")("config", configName)("idx", idx));
        isNewConnection = false;
        return nullptr;
    }
    // 获取journal reader
    auto journalReader = connectionGuard->GetReader();
    if (!journalReader) {
        LOG_ERROR(sLogger, ("failed to get journal reader from guard", "skip processing")("config", configName)("idx", idx));
        isNewConnection = false;
        return nullptr;
    }
    // 检查journal reader是否打开
    if (!journalReader->IsOpen()) {
        LOG_ERROR(sLogger, ("journal reader not open", "skip processing")("config", configName)("idx", idx));
        isNewConnection = false;
        return nullptr;
    }
    
    // 检查是否创建了新连接
    size_t connectionCountAfter = JournalConnectionManager::GetInstance()->GetConnectionCount();
    isNewConnection = (connectionCountAfter > connectionCountBefore);
    
    LOG_INFO(sLogger, ("guarded journal connection obtained successfully", "")("config", configName)("idx", idx)("is_new_connection", isNewConnection));
    return journalReader;
}

bool PerformJournalSeek(const string& configName, size_t idx, JournalConfig& config, std::shared_ptr<SystemdJournalReader> journalReader, bool forceSeek) {
    // 获取当前checkpoint
    string currentCheckpoint = JournalCheckpointManager::GetInstance().GetCheckpoint(configName, idx);
    
    // 判断是否需要执行seek操作
    bool shouldSeek = forceSeek || config.needsSeek || 
                      (config.lastSeekCheckpoint != currentCheckpoint);
    
    if (!shouldSeek) {
        // 跳过seek操作，位置未改变，无需重新定位
        return true; 
    }
    
    LOG_INFO(sLogger, ("performing journal seek", "")("config", configName)("idx", idx)("reason", forceSeek ? "forced" : (config.needsSeek ? "required" : "checkpoint_changed"))("current_checkpoint", currentCheckpoint.substr(0, 50)));
    
    bool seekSuccess = false;
    
    // 首先尝试使用cursor
    if (!currentCheckpoint.empty() && config.seekPosition == "cursor") {
        LOG_INFO(sLogger, ("seeking to checkpoint cursor", currentCheckpoint.substr(0, 50))("config", configName)("idx", idx));
        seekSuccess = journalReader->SeekCursor(currentCheckpoint);
        if (!seekSuccess) {
            LOG_WARNING(sLogger, ("failed to seek to checkpoint, using fallback position", config.cursorSeekFallback)("config", configName)("idx", idx));
        }
    }
    
    // 如果cursor失败或者不使用cursor，按seekPosition处理
    if (!seekSuccess) {
        if (config.seekPosition == "head" || (config.seekPosition == "cursor" && config.cursorSeekFallback == "head")) {
            LOG_INFO(sLogger, ("seeking to journal head", "")("config", configName)("idx", idx));
            seekSuccess = journalReader->SeekHead();
        } else {
            LOG_INFO(sLogger, ("seeking to journal tail", "")("config", configName)("idx", idx));
            seekSuccess = journalReader->SeekTail();
            
            // tail定位后需要回退到最后一条实际记录
            if (seekSuccess) {
                if (journalReader->Previous()) {
                    LOG_INFO(sLogger, ("moved to last actual entry after tail seek", "")("config", configName)("idx", idx));
                } else {
                    LOG_INFO(sLogger, ("no entries found after tail seek", "")("config", configName)("idx", idx));
                    return false;
                }
            }
        }
    }
    // 如果seek失败，则返回false
    if (!seekSuccess) {
        LOG_ERROR(sLogger, ("failed to seek to position", config.seekPosition)("config", configName)("idx", idx));
        return false;
    }
    
    // 更新seek状态
    config.lastSeekCheckpoint = currentCheckpoint;
    config.needsSeek = false;
    
    // 完成Seek返回
    return true;
}

} // namespace logtail::impl 