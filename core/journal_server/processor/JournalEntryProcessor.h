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

#include <chrono>
#include <memory>
#include <string>

#include "common/JournalConfig.h"
#include "reader/JournalReader.h"

namespace logtail {

struct JournalConfig;
class JournalReader;
using QueueKey = int64_t;

/**
 * @brief 读取并处理journal条目
 *
 * @param configName 配置名称
 * @param config journal配置
 * @param journalReader journal reader指针
 * @param queueKey 队列键值，用于推送事件
 * @param hasPendingDataOut 输出参数，指示是否还有待处理数据（可能为 nullptr）
 * @param accumulatedEventGroup 输入输出参数，累积的eventGroup（可能为 nullptr）
 * @param accumulatedEntryCount 输入输出参数，累积的entry数量（可能为 nullptr）
 * @param accumulatedFirstCursor 输入输出参数，累积的第一个entry的cursor（可能为 nullptr）
 * @param timeoutTrigger 是否超时触发强制发送
 * @param lastBatchTimeOut 输入输出参数，上次批处理时间（可能为 nullptr）
 * @return 是否发送了eventGroup（true=已发送，false=累积到缓冲区）
 */
bool HandleJournalEntries(const std::string& configName,
                          const JournalConfig& config,
                          const std::shared_ptr<JournalReader>& journalReader,
                          QueueKey queueKey,
                          bool* hasPendingDataOut = nullptr,
                          std::shared_ptr<PipelineEventGroup>* accumulatedEventGroup = nullptr,
                          int* accumulatedEntryCount = nullptr,
                          std::string* accumulatedFirstCursor = nullptr,
                          bool timeoutTrigger = false,
                          std::chrono::steady_clock::time_point* lastBatchTimeOut = nullptr);

} // namespace logtail
