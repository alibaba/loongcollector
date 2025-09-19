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

namespace logtail {

// Forward declarations
class JournalConnectionInfo;
class SystemdJournalReader;

/**
 * @brief RAII连接守护类，自动管理连接使用计数
 * 
 * 确保在连接使用期间不会被意外重置或清理。
 * 
 * 工作原理：
 * 1. 构造时增加连接的使用计数
 * 2. 析构时减少连接的使用计数
 * 3. 连接管理器检查使用计数，避免清理正在使用的连接
 * 
 * 用法示例：
 * ```cpp
 * {
 *     auto guard = connectionManager->GetGuardedConnection(name, idx, config);
 *     auto reader = guard->GetReader();
 *     
 *     // 在这个作用域内，连接受到保护
 *     int result = reader->Wait(1000ms);
 *     bool success = reader->GetEntry(entry);
 * } // guard析构，自动释放保护
 * ```
 */
class JournalConnectionGuard {
public:
    /**
     * @brief 构造守护对象并增加使用计数
     * @param connection 要保护的连接对象
     */
    explicit JournalConnectionGuard(std::shared_ptr<JournalConnectionInfo> connection);
    
    /**
     * @brief 析构守护对象并减少使用计数
     */
    ~JournalConnectionGuard();
    
    // 禁用拷贝和移动，防止使用计数错乱
    JournalConnectionGuard(const JournalConnectionGuard&) = delete;
    JournalConnectionGuard& operator=(const JournalConnectionGuard&) = delete;
    JournalConnectionGuard(JournalConnectionGuard&&) = delete;
    JournalConnectionGuard& operator=(JournalConnectionGuard&&) = delete;
    
    /**
     * @brief 获取受保护的journal reader
     * @return journal reader指针，如果连接无效则返回nullptr
     */
    std::shared_ptr<SystemdJournalReader> GetReader();
    
    /**
     * @brief 检查受保护的连接是否有效
     * @return true如果连接有效且打开
     */
    [[nodiscard]] bool IsValid() const;
    
    /**
     * @brief 获取受保护的连接对象
     * @return 连接对象指针
     */
    std::shared_ptr<JournalConnectionInfo> GetConnection() const {
        return mConnection;
    }
    
private:
    std::shared_ptr<JournalConnectionInfo> mConnection;
};

} // namespace logtail 