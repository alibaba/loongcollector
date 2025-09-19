/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <string>
#include <vector>
#include "collection_pipeline/queue/QueueKey.h"

namespace logtail {

// 前向声明
class CollectionPipelineContext;

/**
 * @brief journal输入插件的配置
 */
struct JournalConfig {
    // Journal读取配置
    std::string seekPosition;
    int cursorFlushPeriodMs = 5000;
    std::string cursorSeekFallback;

    // 过滤配置
    std::vector<std::string> units;          // Systemd服务单元过滤器
    std::vector<std::string> identifiers;    // Syslog标识符过滤器  
    std::vector<std::string> matchPatterns;  // 自定义匹配模式
    bool kernel = true;                      // 启用内核日志过滤器

    // 性能配置
    int resetIntervalSecond = 3600;          // 连接重置间隔
    int maxEntriesPerBatch = 1000;           // 每批最大条目数
    int waitTimeoutMs = 1000;                // 等待超时时间（毫秒）

    // 字段处理配置
    bool parsePriority = false;              // 解析优先级字段
    bool parseSyslogFacility = false;        // 解析syslog设施字段
    bool useJournalEventTime = true;         // 使用journal事件时间而非系统时间

    // 自定义journal路径（用于文件形式的journal）
    std::vector<std::string> journalPaths;

    // 上下文引用
    const CollectionPipelineContext* ctx = nullptr;
    
    // 运行时状态（在验证期间设置）
    mutable QueueKey queueKey = -1;  // 验证后缓存的队列键值（-1 = 未验证）
    mutable std::string lastSeekCheckpoint;  // 我们成功定位到的最后一个检查点
    mutable bool needsSeek = true;  // 下次读取时是否需要执行定位
    
    JournalConfig() = default;
};

} // namespace logtail 