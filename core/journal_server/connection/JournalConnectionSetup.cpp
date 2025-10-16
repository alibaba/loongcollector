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

#include "../reader/JournalReaderManager.h"
#include "../reader/JournalReader.h"
#include "logger/Logger.h"

using namespace std;

namespace logtail::impl {

// =============================================================================
// 建立连接
// =============================================================================

std::shared_ptr<SystemdJournalReader> SetupJournalConnection(const string& configName, size_t idx, const JournalConfig& config, bool& isNewConnection) {
    // 为配置创建独立的journal reader
    
    // 记录连接获取前的连接数，用于判断是否创建了新连接
    size_t connectionCountBefore = JournalReaderManager::GetInstance().GetConnectionCount();
    
    // 创建空的处理器（实际处理在其他地方）
    auto handler = [](const std::string&, size_t, const JournalEntry&) {
        // 空的处理器，仅用于连接建立
    };
    
    // 添加配置到管理器（会为每个配置创建独立的reader）
    bool added = JournalReaderManager::GetInstance().AddConfig(configName, idx, config, handler);
    if (!added) {
        LOG_ERROR(sLogger, ("failed to add config to manager", "skip processing")("config", configName)("idx", idx));
        isNewConnection = false;
        return nullptr;
    }
    
    // 获取刚创建的独立reader
    auto journalReader = JournalReaderManager::GetInstance().GetConnectionInfo(configName, idx);
    
    if (!journalReader) {
        LOG_ERROR(sLogger, ("failed to get journal reader", "skip processing")("config", configName)("idx", idx));
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
    size_t connectionCountAfter = JournalReaderManager::GetInstance().GetConnectionCount();
    isNewConnection = (connectionCountAfter > connectionCountBefore);
    
    LOG_INFO(sLogger, ("journal reader created successfully", "")("config", configName)("idx", idx)("is_new", isNewConnection));
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
        } else {
            // seek到开头
            seekSuccess = journalReader->SeekHead();
            LOG_INFO(sLogger, ("seek to head", "")("config", configName)("idx", idx)("success", seekSuccess));
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