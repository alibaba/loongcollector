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

#include "common/JournalConfig.h"
#include "reader/JournalReader.h"

namespace logtail {

struct JournalConfig;
class SystemdJournalReader;
using QueueKey = int64_t;

/**
 * @brief 读取并处理journal条目
 *
 * 从journal reader中读取条目，应用字段转换，创建日志事件并推送到处理队列。
 * 会根据配置的maxEntriesPerBatch限制每批读取的条目数量。
 *
 * @param configName 配置名称
 * @param config journal配置
 * @param journalReader journal reader指针
 * @param queueKey 队列键值，用于推送事件
 */
void ReadJournalEntries(const std::string& configName,
                        const JournalConfig& config,
                        const std::shared_ptr<SystemdJournalReader>& journalReader,
                        QueueKey queueKey);

} // namespace logtail
