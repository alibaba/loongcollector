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

#include <string>
#include <memory>

namespace logtail {
    struct JournalConfig;
    class SystemdJournalReader;
}

namespace logtail::impl {

/**
 * @brief 建立journal连接
 * @param configName 配置名称
 * @param idx 配置索引
 * @param config journal配置
 * @param isNewConnection [输出] 是否创建了新连接
 * @return journal reader指针，失败返回nullptr
 */
std::shared_ptr<SystemdJournalReader> SetupJournalConnection(
    const std::string& configName, 
    size_t idx, 
    const JournalConfig& config, 
    bool& isNewConnection);

/**
 * @brief 执行journal seek操作
 * @param configName 配置名称
 * @param idx 配置索引
 * @param config journal配置（会修改needsSeek标记）
 * @param journalReader journal reader指针
 * @param forceSeek 是否强制执行seek（忽略needsSeek标记）
 * @return seek是否成功
 */
bool PerformJournalSeek(
    const std::string& configName, 
    size_t idx, 
    JournalConfig& config, 
    const std::shared_ptr<SystemdJournalReader>& journalReader, 
    bool forceSeek);

} // namespace logtail::impl

