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

#include "JournalServer.h"

#include <chrono>

#include <utility>

#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "connection/JournalConnectionManager.h"
#include "connection/JournalConnectionGuard.h"
#include "checkpoint/JournalCheckpointManager.h"
#include "common/JournalConstants.h"
#include "collection_pipeline/queue/QueueKey.h"
#include "models/PipelineEventGroup.h"
#include "common/memory/SourceBuffer.h"
#include "common/TimeUtil.h"
#include "app_config/AppConfig.h"
#include "logger/Logger.h"
#include "runner/ProcessorRunner.h"
#include "reader/JournalEntry.h"
#include "common/Flags.h"

// Journal checkpoint 清理配置
DEFINE_FLAG_INT32(journal_checkpoint_cleanup_interval_sec, "cleanup interval for journal checkpoints in seconds, default 1 hour", 3600); // 默认1小时清理一次
DEFINE_FLAG_INT32(journal_checkpoint_expired_threshold_hours, "expired threshold for journal checkpoints in hours, default 24 hours", 24); // 默认24小时即过期

using namespace std;

namespace logtail {

// =============================================================================
// 1. JournalServer 生命周期管理 - Lifecycle Management
// =============================================================================

void JournalServer::Init() {
    mThreadRes = async(launch::async, &JournalServer::run, this);
    mStartTime = time(nullptr);
    LOG_INFO(sLogger, ("JournalServer initialized", ""));
}

void JournalServer::Stop() {
    if (!mThreadRes.valid()) {
        return;
    }
    {
        lock_guard<mutex> lock(mThreadRunningMux);
        mIsThreadRunning = false;
    }
    if (mThreadRes.valid()) {
        mThreadRes.get();
    }
    LOG_INFO(sLogger, ("JournalServer stopped", ""));
}

bool JournalServer::HasRegisteredPlugins() const {
    lock_guard<mutex> lock(mUpdateMux);
    return !mPipelineNameJournalConfigsMap.empty();
}

void JournalServer::AddJournalInput(const string& configName, size_t idx, const JournalConfig& config) {
    // 首先验证配置
    QueueKey queueKey = 0;
    if (!validateJournalConfig(configName, idx, config, queueKey)) {
        LOG_ERROR(sLogger, ("journal input validation failed", "config not added")("config", configName)("idx", idx));
        return;
    }
    
    // 验证成功后，缓存queueKey并添加配置
    JournalConfig validatedConfig = config;
    validatedConfig.queueKey = queueKey;
    
    {
        lock_guard<mutex> lock(mUpdateMux);
        mPipelineNameJournalConfigsMap[configName][idx] = validatedConfig;
        
        LOG_INFO(sLogger, ("journal input added after validation", "")("config", configName)("idx", idx)("ctx_valid", config.ctx != nullptr)("queue_key", queueKey)("total_pipelines", mPipelineNameJournalConfigsMap.size()));
    }
}

void JournalServer::RemoveJournalInput(const string& configName, size_t idx) {
    {
        lock_guard<mutex> lock(mUpdateMux);
        auto configItr = mPipelineNameJournalConfigsMap.find(configName);
        // 如果配置存在，则移除
        if (configItr != mPipelineNameJournalConfigsMap.end()) {
            configItr->second.erase(idx);
            // 如果配置为空，则移除整个配置
            if (configItr->second.empty()) {
                // 移除整个配置
                mPipelineNameJournalConfigsMap.erase(configItr);
            }
        }
    }
    
    // 移除config对应的连接
    JournalConnectionManager::GetInstance()->RemoveConnection(configName, idx);
    
    // 清理configName对应的所有checkpoints
    size_t clearedCheckpoints = JournalCheckpointManager::GetInstance().ClearConfigCheckpoints(configName);
    if (clearedCheckpoints > 0) {
        LOG_INFO(sLogger, ("config checkpoints cleared", "")("config", configName)("count", clearedCheckpoints));
    }
    
    LOG_INFO(sLogger, ("journal input removed with automatic connection and checkpoint cleanup", "")("config", configName)("idx", idx));
}

JournalConfig JournalServer::GetJournalConfig(const string& name, size_t idx) const {
    lock_guard<mutex> lock(mUpdateMux);
    auto configItr = mPipelineNameJournalConfigsMap.find(name);
    if (configItr != mPipelineNameJournalConfigsMap.end()) {
        // 如果配置存在，则获取idx对应的配置
        auto idxItr = configItr->second.find(idx);
        if (idxItr != configItr->second.end()) {
            return idxItr->second;
        }
    }
    // 如果配置不存在，则返回空配置
    return JournalConfig();
}


// =============================================================================
// 2. 主线程运行逻辑 - Main Thread Logic
// =============================================================================

void JournalServer::run() {
    LOG_INFO(sLogger, ("JournalServer thread", "started"));
    
    while (true) {
        {
            lock_guard<mutex> lock(mThreadRunningMux);
            if (!mIsThreadRunning) {
                break;
            }
        }
        // 核心处理逻辑：遍历所有配置并处理journal条目
        processJournalEntries();
        
        // 每次循环后短暂休眠，避免CPU占用过高
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // 定期清理过期的checkpoints（使用配置的间隔时间）
        static time_t sLastCleanup = time(nullptr);
        time_t currentTime = time(nullptr);
        if (currentTime - sLastCleanup >= INT32_FLAG(journal_checkpoint_cleanup_interval_sec)) {
            // 清理过期的checkpoints（使用配置的过期时间）
            LOG_INFO(sLogger, ("cleaning up expired journal checkpoints", "")
                     ("cleanup_interval_sec", INT32_FLAG(journal_checkpoint_cleanup_interval_sec))
                     ("expired_threshold_hours", INT32_FLAG(journal_checkpoint_expired_threshold_hours)));
            size_t cleanedCheckpoints = JournalCheckpointManager::GetInstance().CleanupExpiredCheckpoints(
                INT32_FLAG(journal_checkpoint_expired_threshold_hours));
            if (cleanedCheckpoints > 0) {
                LOG_INFO(sLogger, ("expired checkpoints cleaned", "")("count", cleanedCheckpoints));
            }
            sLastCleanup = currentTime;
        }
    }
    
    LOG_INFO(sLogger, ("JournalServer thread", "stopped"));
}

// =============================================================================
// 3. 主处理入口 - Main Processing Entry
// =============================================================================

// 遍历所有配置并处理journal条目
void JournalServer::processJournalEntries() {
    LOG_INFO(sLogger, ("processJournalEntries", "called"));
    // 获取当前配置的快照，避免长时间持有锁
    unordered_map<string, map<size_t, JournalConfig>> currentConfigs;
    {
        lock_guard<mutex> lock(mUpdateMux);
        currentConfigs = mPipelineNameJournalConfigsMap;
        LOG_INFO(sLogger, ("configurations loaded from map", "")("total_pipelines", mPipelineNameJournalConfigsMap.size()));
    }
    
    // 如果当前配置为空，则返回
    if (currentConfigs.empty()) {
        LOG_WARNING(sLogger, ("no journal configurations to process", "server may not have any registered journal inputs"));
        return;
    }
    
    LOG_INFO(sLogger, ("processing journal entries", "")("configs_count", currentConfigs.size()));
    
    // 处理每个配置
    for (auto& pipelineConfig : currentConfigs) {
        const string& configName = pipelineConfig.first;
        
        for (auto& idxConfig : pipelineConfig.second) {
            size_t idx = idxConfig.first;
            JournalConfig& config = idxConfig.second;
                        
            // 处理单个journal配置的主流程控制
            processJournalConfig(configName, idx, config);
        }
    }
}

// =============================================================================
// 4. 单配置处理流程 - Single Configuration Processing
// =============================================================================

// 处理单个journal配置的主流程控制
void JournalServer::processJournalConfig(const string& configName, size_t idx, JournalConfig& config) {
    LOG_INFO(sLogger, ("processJournalConfig", "started")("config", configName)("idx", idx)("ctx_valid", config.ctx != nullptr));

    // 使用已验证的queueKey（在addJournalInput时已验证并缓存）
    QueueKey queueKey = config.queueKey;
    if (queueKey == -1) {
        LOG_ERROR(sLogger, ("invalid queue key in config", "config not properly validated")("config", configName)("idx", idx));
        return;
    }
    
    // Step 1: 创建journal连接
    std::unique_ptr<JournalConnectionGuard> connectionGuard;
    bool isNewConnection = false;
    auto journalReader = setupJournalConnection(configName, idx, config, connectionGuard, isNewConnection);
    if (!journalReader) {
        // 连接失败，标记需要重新seek
        const_cast<JournalConfig&>(config).needsSeek = true;
        return;
    }
    
    // Step 2: 执行seek操作（仅在必要时）
    // 强制seek的条件：新连接
    bool forceSeek = isNewConnection;
    if (!performJournalSeek(configName, idx, config, journalReader, forceSeek)) {
        // Seek失败，标记需要重新seek以便下次尝试
        config.needsSeek = true;
        return;
    }
    
    // Step 3: 读取和处理journal条目
    readJournalEntriesForConfig(configName, idx, config, journalReader, queueKey);
}

// =============================================================================
// 5. 处理步骤函数 - Processing Step Functions  
// =============================================================================

// 验证journal配置的有效性和获取队列Key
bool JournalServer::validateJournalConfig(const string& configName, size_t idx, const JournalConfig& config, QueueKey& queueKey) {
    // 基本验证
    if (!config.ctx) {
        LOG_ERROR(sLogger, ("CRITICAL: no context available for journal config", "this indicates initialization problem")("config", configName)("idx", idx));
        return false;
    }
    
    // 从pipeline context获取queue key
    queueKey = config.ctx->GetProcessQueueKey();
    if (queueKey == -1) {
        LOG_WARNING(sLogger, ("no queue key available for journal config", "skip")("config", configName)("idx", idx));
        return false;
    }
    
    // 检查队列是否有效
    if (!ProcessQueueManager::GetInstance()->IsValidToPush(queueKey)) {
        // 队列无效，跳过该journal配置的处理
        return false;
    }
    
    return true;
}

// 设置并获取journal连接，返回可用的Reader
std::shared_ptr<SystemdJournalReader> JournalServer::setupJournalConnection(const string& configName, size_t idx, const JournalConfig& config, std::unique_ptr<JournalConnectionGuard>& connectionGuard, bool& isNewConnection) {
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

// 智能journal定位操作（仅在必要时执行seek）
bool JournalServer::performJournalSeek(const string& configName, size_t idx, JournalConfig& config, std::shared_ptr<SystemdJournalReader> journalReader, bool forceSeek) {
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

// 应用字段转换规则（重命名、过滤等）
void ApplyJournalFieldTransforms(JournalEntry& entry, const JournalConfig& config) {
    if (config.parsePriority) {
        // 查找PRIORITY字段
        auto it = entry.fields.find("PRIORITY");
        if (it != entry.fields.end()) {
            // 查找PRIORITY字段对应的值
            auto priorityIt = JournalConstants::kPriorityConversionMap.find(it->second);
            if (priorityIt != JournalConstants::kPriorityConversionMap.end()) {
                // 如果找到对应的值，则更新PRIORITY字段
                it->second = priorityIt->second;
            }
        }
    }
    
    if (config.parseSyslogFacility) {
        // 查找SYSLOG_FACILITY字段
        auto it = entry.fields.find("SYSLOG_FACILITY");
        if (it != entry.fields.end()) {
            // 查找SYSLOG_FACILITY字段对应的值
            auto facilityIt = JournalConstants::kSyslogFacilityString.find(it->second);
            if (facilityIt != JournalConstants::kSyslogFacilityString.end()) {
                // 如果找到对应的值，则更新SYSLOG_FACILITY字段
                it->second = facilityIt->second;
            }
        }
    }
}

// 批量读取和处理journal条目的主循环
void JournalServer::readJournalEntriesForConfig(const string& configName, size_t idx, const JournalConfig& config, 
                                        const std::shared_ptr<SystemdJournalReader>& journalReader, QueueKey queueKey) {
    int entryCount = 0;
    const int maxEntriesPerBatch = config.maxEntriesPerBatch;
    bool isFirstEntry = true;
    
    LOG_INFO(sLogger, ("starting to read journal entries", "")("config", configName)("idx", idx)("seek_position", config.seekPosition)("max_batch_size", maxEntriesPerBatch));
    
    while (entryCount < maxEntriesPerBatch) {
        // Step 1: 移动到下一个entry，如果需要的话
        // 如果需要的话，则移动到下一个entry
        if (!moveToNextJournalEntry(configName, idx, config, journalReader, isFirstEntry, entryCount)) {
            break;
        }
        
        // Step 2: 读取和验证entry
        // 读取和验证entry
        JournalEntry entry;
        if (!readAndValidateEntry(configName, idx, journalReader, entry)) {
            // 如果entry为空，则跳过
            if (entry.fields.empty() && !entry.cursor.empty()) {
                // 如果entry为空，但cursor有效，则计数并继续
                isFirstEntry = false;
                entryCount++;
                continue;
            }
            break;  // 连接错误或读取失败
        }
        
        // Step 3: 处理enrty (transform, create event, push)
        // 处理enrty (transform, create event, push)
        if (!createAndPushEventGroup(configName, idx, config, entry, queueKey)) {
            LOG_ERROR(sLogger, ("failed to process journal entry", "continue")("config", configName)("idx", idx));
        }
        
        entryCount++;
        isFirstEntry = false;
        
        // 处理完一条entry后更新cursor
        JournalCheckpointManager::GetInstance().SaveCheckpoint(configName, idx, entry.cursor);
    }
    
    // 如果处理了entry，则记录日志
    if (entryCount > 0) {
        LOG_INFO(sLogger, ("journal processing completed", "")("config", configName)("idx", idx)("entries_processed", entryCount));
    } else {
        LOG_WARNING(sLogger, ("no journal entries processed", "")("config", configName)("idx", idx)("seek_position", config.seekPosition));
    }
    
    // journal处理完成，连接保持打开状态以供后续使用
}

// 处理移动到下一个journal条目的逻辑
bool JournalServer::moveToNextJournalEntry(const string& configName, size_t idx, const JournalConfig& config, 
                                          const std::shared_ptr<SystemdJournalReader>& journalReader, bool isFirstEntry, int entryCount) {
    // 对于head模式或者非首次读取，需要调用Next()移动到下一条
    if (config.seekPosition == "head" || !isFirstEntry) {
        bool nextSuccess = journalReader->Next();
        if (!nextSuccess) {
            // 检查连接是否仍然有效
            if (!journalReader->IsOpen()) {
                LOG_WARNING(sLogger, ("journal connection closed during processing", "aborting batch")("config", configName)("idx", idx)("entries_processed", entryCount));
                return false;
            }
            
            // 使用wait功能等待新的entries
            if (!handleJournalWait(configName, idx, config, journalReader, entryCount)) {
                return false;
            }
        }
        // 已移动到下一个条目，继续处理
    }
    return true;
}

// 读取和验证journal条目
bool JournalServer::readAndValidateEntry(const string& configName, size_t idx, const std::shared_ptr<SystemdJournalReader>& journalReader, JournalEntry& entry) {
    // 在读取entry之前验证连接状态
    if (!journalReader->IsOpen()) {
        LOG_WARNING(sLogger, ("journal connection closed before GetEntry", "aborting batch")("config", configName)("idx", idx));
        return false;
    }
    
    // 读取当前entry
    bool getEntrySuccess = journalReader->GetEntry(entry);
    
    if (!getEntrySuccess) {
        LOG_WARNING(sLogger, ("failed to get journal entry", "skipping")("config", configName)("idx", idx));
        
        // 检查是否是连接问题导致的失败
        if (!journalReader->IsOpen()) {
            LOG_WARNING(sLogger, ("GetEntry failed due to closed connection", "aborting batch")("config", configName)("idx", idx));
            return false;
        }
        return false;  // Read failed but connection ok, caller should continue
    }
    
    // 检查entry是否为空
    if (entry.fields.empty()) {
        LOG_WARNING(sLogger, ("journal entry is empty", "no fields found")("config", configName)("idx", idx)("cursor", entry.cursor));
        return false;  // Empty entry, special handling needed by caller
    }
    
    LOG_INFO(sLogger, ("successfully read journal entry", "")("config", configName)("idx", idx)("cursor", entry.cursor.substr(0, 50))("fields_count", entry.fields.size()));
    return true;
}

// 创建事件组并推送到队列
bool JournalServer::createAndPushEventGroup(const string& configName, size_t idx, const JournalConfig& config, const JournalEntry& entry, QueueKey queueKey) {
    // 应用字段转换
    JournalEntry mutableEntry = entry;  // Make a mutable copy
    ApplyJournalFieldTransforms(mutableEntry, config);
    
    // 创建PipelineEventGroup并添加LogEvent
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    
    createLogEventFromJournal(mutableEntry, config, eventGroup);
    
    // 已创建LogEvent对象，包含所有字段和时间戳信息
    
    // 推送到处理队列
    if (!ProcessorRunner::GetInstance()->PushQueue(queueKey, idx, std::move(eventGroup))) {
        LOG_ERROR(sLogger, ("failed to push journal data to process queue", "discard data")
                  ("config", configName)("input idx", idx)("queue", queueKey));
        return false;
    }          
    // 成功推送到处理队列
    return true;
   
}

// =============================================================================
// 6. 底层辅助函数 - Low-level Helper Functions
// =============================================================================

// 处理journal等待逻辑，支持动态超时调整
bool JournalServer::handleJournalWait(const string& configName, size_t idx, const JournalConfig& config, 
                                    const std::shared_ptr<SystemdJournalReader>& journalReader, int entryCount) {
    // 动态调整等待时间：如果已经读到了一些entries，缩短等待时间以保持响应性
    int waitTimeout = (entryCount == 0) ? config.waitTimeoutMs : std::min(config.waitTimeoutMs / 4, 250);
    
    // 当前无可用条目，等待新条目到达（使用动态超时时间）
    
    int waitResult = journalReader->Wait(std::chrono::milliseconds(waitTimeout));
    if (waitResult > 0) {
        // 有新的数据可用，继续尝试读取
        // 等待后检测到新条目，继续处理
        
        // 在wait之后重新检查连接状态
        if (!journalReader->IsOpen()) {
            LOG_WARNING(sLogger, ("journal connection closed during wait", "aborting batch")("config", configName)("idx", idx));
            return false;
        }
        
        bool nextSuccess = journalReader->Next();
        if (!nextSuccess) {
            // 等待显示有新数据，但Next()失败，可能连接有问题
            if (!journalReader->IsOpen()) {
                LOG_WARNING(sLogger, ("connection lost after wait", "")("config", configName)("idx", idx));
            }
            return false;
        }
        return true;
    }
    if (waitResult == 0) {
        // 等待超时，没有新数据（可能未发现新条目，或批次已完成）
        return false;
    }
    // 错误，可能是连接被重置或其他问题
    LOG_WARNING(sLogger, ("wait failed", "")("config", configName)("idx", idx)("wait_result", waitResult)("entries_processed", entryCount));
    
    if (!journalReader->IsOpen()) {
        LOG_WARNING(sLogger, ("connection lost during wait operation", "")("config", configName)("idx", idx));
    }
    return false;
}

// 从JournalEntry创建LogEvent，包含时间戳处理
LogEvent* JournalServer::createLogEventFromJournal(const JournalEntry& entry, const JournalConfig& config, PipelineEventGroup& eventGroup) {
    LogEvent* logEvent = eventGroup.AddLogEvent();
    
    // 设置所有journal字段到LogEvent
    for (const auto& field : entry.fields) {
        logEvent->SetContent(field.first, field.second);
    }
    
    // 添加时间戳字段（始终透出），保持和go版本一致
    logEvent->SetContent("_realtime_timestamp_", std::to_string(entry.realtimeTimestamp));
    logEvent->SetContent("_monotonic_timestamp_", std::to_string(entry.monotonicTimestamp));
    
    // 设置时间戳
    if (config.useJournalEventTime && entry.realtimeTimestamp > 0) {
        // journal的realtimeTimestamp是微秒，需要转换为秒和纳秒
        uint64_t seconds = entry.realtimeTimestamp / 1000000;
        uint64_t nanoseconds = (entry.realtimeTimestamp % 1000000) * 1000;
        logEvent->SetTimestamp(seconds, nanoseconds);
    } else {
        // 使用当前时间（保持纳秒精度，应用秒级时间自动调整）
        auto currentTime = GetCurrentLogtailTime();
        time_t adjustedSeconds = currentTime.tv_sec;
        time_t adjustedNanoSeconds = currentTime.tv_nsec;

        if (AppConfig::GetInstance()->EnableLogTimeAutoAdjust()) {
            adjustedSeconds += GetTimeDelta();
            adjustedNanoSeconds += GetTimeDelta()*1000000;
            // 时间已调整，应用了时间偏移量
        }
        logEvent->SetTimestamp(adjustedSeconds, adjustedNanoSeconds);
    }
    
    return logEvent;
}

// =============================================================================
// 7. 配置管理 - Configuration Management
// =============================================================================

#ifdef APSARA_UNIT_TEST_MAIN
void JournalServer::clear() {
    lock_guard<mutex> lock(mUpdateMux);
    mPipelineNameJournalConfigsMap.clear();
    // Note: Checkpoint cleanup is handled by JournalCheckpointManager
}
#endif

} // namespace logtail 