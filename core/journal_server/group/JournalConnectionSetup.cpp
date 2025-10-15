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

#include "JournalConnectionSetup.h"

#include <memory>

#include "JournalConfigGroupManager.h"
#include "checkpoint/JournalCheckpointManager.h"
#include "reader/JournalReader.h"
#include "logger/Logger.h"

using namespace std;

namespace logtail::impl {

// =============================================================================
// 建立连接
// =============================================================================

std::shared_ptr<SystemdJournalReader> SetupJournalConnection(const string& configName, size_t idx, const JournalConfig& config, bool& isNewConnection) {
    // Getting journal connection from manager for config: configName, idx: idx
    
    // 记录连接获取前的连接数，用于判断是否创建了新连接
    size_t connectionCountBefore = JournalConfigGroupManager::GetInstance().GetConnectionCount();
    
    // 注意：这里需要创建一个临时的配置处理器
    auto handler = [](const std::string&, size_t, const JournalEntry&) {
        // 空的处理器，仅用于连接建立
    };
    
    // 添加配置到分组管理器
    bool added = JournalConfigGroupManager::GetInstance().AddConfig(configName, idx, config, handler);
    if (!added) {
        LOG_ERROR(sLogger, ("failed to add config to group manager", "skip processing")("config", configName)("idx", idx));
        isNewConnection = false;
        return nullptr;
    }
    
    auto journalReader = JournalConfigGroupManager::GetInstance().GetConnectionInfo(configName, idx);
    
    if (!journalReader) {
        LOG_ERROR(sLogger, ("failed to get journal connection", "skip processing")("config", configName)("idx", idx));
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
    size_t connectionCountAfter = JournalConfigGroupManager::GetInstance().GetConnectionCount();
    isNewConnection = (connectionCountAfter > connectionCountBefore);
    
    // Journal connection obtained successfully for config: configName, idx: idx, is_new_connection: isNewConnection
    return journalReader;
}

bool PerformJournalSeek(const string& configName, size_t idx, JournalConfig& config, const std::shared_ptr<SystemdJournalReader>& journalReader, bool forceSeek) {
    try {
        // 检查是否需要执行seek操作
        bool shouldSeek = forceSeek || config.needsSeek;
        if (!shouldSeek) {
            // 如果不需要seek，直接返回成功
            return true;
        }
        
        bool seekSuccess = false;
        
        if (config.seekPosition == "tail") {
            // seek到末尾
            seekSuccess = journalReader->SeekTail();
            LOG_INFO(sLogger, ("seek to tail", "")("config", configName)("idx", idx)("success", seekSuccess));
            
            // SeekTail()后需要调用Previous()才能读取到实际的日志条目
            if (seekSuccess) {
                bool prevSuccess = journalReader->Previous();
                LOG_INFO(sLogger, ("seek to previous after tail", "")("config", configName)("idx", idx)("success", prevSuccess));
                // Previous()失败也是正常的（比如journal为空），不影响整体成功
            }
        } else if (config.seekPosition == "head") {
            // seek到开头
            seekSuccess = journalReader->SeekHead();
            LOG_INFO(sLogger, ("seek to head", "")("config", configName)("idx", idx)("success", seekSuccess));
        } else {
            // 尝试从checkpoint加载cursor并seek
            string checkpointCursor = JournalCheckpointManager::GetInstance().GetCheckpoint(configName, idx);
            if (!checkpointCursor.empty()) {
                // 有checkpoint，seek到指定位置
                seekSuccess = journalReader->SeekCursor(checkpointCursor);
                LOG_INFO(sLogger, ("seek to checkpoint cursor", "")("config", configName)("idx", idx)("cursor", checkpointCursor)("success", seekSuccess));
                
                if (!seekSuccess) {
                    // 如果cursor seek失败，fallback到head
                    LOG_WARNING(sLogger, ("checkpoint cursor seek failed, falling back to head", "")("config", configName)("idx", idx)("cursor", checkpointCursor));
                    seekSuccess = journalReader->SeekHead();
                }
            } else {
                // 没有checkpoint，默认从head开始
                seekSuccess = journalReader->SeekHead();
                LOG_INFO(sLogger, ("no checkpoint found, seek to head", "")("config", configName)("idx", idx)("success", seekSuccess));
            }
        }
        
        if (!seekSuccess) {
            LOG_ERROR(sLogger, ("journal seek failed", "")("config", configName)("idx", idx)("seek_position", config.seekPosition));
            return false;
        }
        
        // Seek成功，清除needsSeek标记
        config.needsSeek = false;
        LOG_DEBUG(sLogger, ("journal seek completed successfully", "")("config", configName)("idx", idx)("needsSeek", false));
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR(sLogger, ("exception during journal seek operation", e.what())("config", configName)("idx", idx)("seek_position", config.seekPosition));
        return false;
    } catch (...) {
        LOG_ERROR(sLogger, ("unknown exception during journal seek operation", "")("config", configName)("idx", idx)("seek_position", config.seekPosition));
        return false;
    }
}

} // namespace logtail::impl 