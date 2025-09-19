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

#include <unordered_map>
#include <map>

#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "logger/Logger.h"
#include "connection/JournalConnectionManager.h"
#include "connection/JournalConnectionGuard.h"

using namespace std;

namespace logtail::impl {

// =============================================================================
// 配置处理函数实现
// =============================================================================

bool ValidateJournalConfig(const string& configName, size_t idx, const JournalConfig& config, QueueKey& queueKey) {
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

void ProcessJournalEntries(JournalServer* server) {
    LOG_INFO(sLogger, ("processJournalEntries", "called"));
    // 获取当前配置的快照，避免长时间持有锁
    unordered_map<string, map<size_t, JournalConfig>> currentConfigs;
    {
        const auto& allConfigs = server->GetAllJournalConfigs();
        currentConfigs = allConfigs;
        LOG_INFO(sLogger, ("configurations loaded from map", "")("total_pipelines", allConfigs.size()));
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
            ProcessJournalConfig(configName, idx, config);
        }
    }
}

void ProcessJournalConfig(const string& configName, size_t idx, JournalConfig& config) {
    LOG_INFO(sLogger, ("processJournalConfig", "started")("config", configName)("idx", idx)("ctx_valid", config.ctx != nullptr));

    // 使用已验证的queueKey（在addJournalInput时已验证并缓存）
    QueueKey queueKey = config.queueKey;
    if (queueKey == -1) {
        LOG_ERROR(sLogger, ("invalid queue key in config", "config not properly validated")("config", configName)("idx", idx));
        return;
    }
    
    // Step 1: 建立journal连接
    std::unique_ptr<JournalConnectionGuard> connectionGuard;
    bool isNewConnection = false;
    auto journalReader = SetupJournalConnection(configName, idx, config, connectionGuard, isNewConnection);
    if (!journalReader) {
        // 连接失败，标记需要重新seek
        const_cast<JournalConfig&>(config).needsSeek = true;
        return;
    }
    
    // Step 2: 执行seek操作（仅在必要时）
    // 强制seek的条件：新连接
    bool forceSeek = isNewConnection;
    if (!PerformJournalSeek(configName, idx, config, journalReader, forceSeek)) {
        // Seek失败，标记需要重新seek以便下次尝试
        config.needsSeek = true;
        return;
    }
    
    // Step 3: 读取和处理journal条目
    ReadJournalEntries(configName, idx, config, journalReader, queueKey);
}

} // namespace logtail::impl 