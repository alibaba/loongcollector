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

#pragma once

#include <memory>
#include <string>

#include "../JournalServer.h"

namespace logtail {

// Forward declarations
class SystemdJournalReader;
struct JournalEntry;
class PipelineEventGroup;
class LogEvent;

namespace impl {

// =============================================================================
// 配置处理编排
// =============================================================================

/**
 * @brief 验证journal配置的有效性和获取队列Key
 */
bool ValidateJournalConfig(const std::string& configName, size_t idx, 
                          const JournalConfig& config, QueueKey& queueKey);

/**
 * @brief 遍历所有配置并处理journal条目
 */
void ProcessJournalEntries(JournalServer* server);

/**
 * @brief 处理单个journal配置的主流程控制
 */
void ProcessJournalConfig(const std::string& configName, 
                         size_t idx, JournalConfig& config);

// =============================================================================
// Connection 建立连接
// =============================================================================

/**
 * @brief 设置并获取journal连接，返回可用的Reader
 */
std::shared_ptr<SystemdJournalReader> SetupJournalConnection(
    const std::string& configName, size_t idx, const JournalConfig& config,
    bool& isNewConnection);

/**
 * @brief 智能journal定位操作（仅在必要时执行seek）
 */
bool PerformJournalSeek(const std::string& configName, size_t idx, JournalConfig& config,
                       const std::shared_ptr<SystemdJournalReader>& journalReader, bool forceSeek = false);

// =============================================================================
// Reader 读取条目
// =============================================================================

/**
 * @brief 批量读取和处理journal条目的主循环
 */
void ReadJournalEntries(const std::string& configName, size_t idx, const JournalConfig& config,
                       const std::shared_ptr<SystemdJournalReader>& journalReader, QueueKey queueKey);

/**
 * @brief 移动到下一个journal条目
 */
bool MoveToNextJournalEntry(const std::string& configName, size_t idx, const JournalConfig& config,
                           const std::shared_ptr<SystemdJournalReader>& journalReader, 
                           bool isFirstEntry, int entryCount);

/**
 * @brief 读取和验证journal条目
 */
bool ReadAndValidateEntry(const std::string& configName, size_t idx,
                         const std::shared_ptr<SystemdJournalReader>& journalReader, JournalEntry& entry);

/**
 * @brief 处理journal等待逻辑，支持动态超时调整
 */
bool HandleJournalWait(const std::string& configName, size_t idx, const JournalConfig& config,
                      const std::shared_ptr<SystemdJournalReader>& journalReader, int entryCount);

/**
 * @brief 从JournalEntry创建LogEvent，包含时间戳处理
 */
LogEvent* CreateLogEventFromJournal(const JournalEntry& entry, const JournalConfig& config, 
                                   PipelineEventGroup& eventGroup);

/**
 * @brief 创建事件组并推送到队列
 */
bool CreateAndPushEventGroup(const std::string& configName, size_t idx, const JournalConfig& config,
                            const JournalEntry& entry, QueueKey queueKey);

/**
 * @brief 应用字段转换规则（重命名、过滤等）
 */
void ApplyJournalFieldTransforms(JournalEntry& entry, const JournalConfig& config);

} // namespace impl
} // namespace logtail 