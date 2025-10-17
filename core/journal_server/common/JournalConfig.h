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

#include <algorithm>
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
    // 注意：与Go插件不同已移除resetIntervalSecond配置，连接永远不重建
    int maxEntriesPerBatch = 1000;           // 每批最大条目数

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
    
    JournalConfig() = default;
    
    /**
     * @brief 验证和修正配置值，确保所有字段都在有效范围内
     * @return 修正的配置项数量
     */
    int ValidateAndFixConfig() {
        int fixedCount = 0;
        
        // 验证数值字段的范围
        
        if (cursorFlushPeriodMs <= 0) {
            cursorFlushPeriodMs = 5000;  // 默认5秒
            fixedCount++;
        } else if (cursorFlushPeriodMs > 300000) {  // 最大5分钟
            cursorFlushPeriodMs = 300000;
            fixedCount++;
        }
        
        if (maxEntriesPerBatch <= 0) {
            maxEntriesPerBatch = 1000;  // 默认1000条
            fixedCount++;
        } else if (maxEntriesPerBatch > 10000) {  // 最大10000条，防止内存爆炸
            maxEntriesPerBatch = 10000;
            fixedCount++;
        }
        
        // 验证seek位置
        if (seekPosition != "head" && seekPosition != "tail" && 
            seekPosition != "cursor" && seekPosition != "none") {
            seekPosition = "tail";  // 默认tail
            fixedCount++;
        }
        
        // 验证seek fallback
        if (cursorSeekFallback != "head" && cursorSeekFallback != "tail") {
            cursorSeekFallback = "tail";  // 默认tail
            fixedCount++;
        }
        
        // 验证字符串数组 - 移除空字符串
        auto removeEmpty = [&fixedCount](std::vector<std::string>& vec) {
            auto originalSize = vec.size();
            vec.erase(std::remove_if(vec.begin(), vec.end(), 
                                   [](const std::string& s) { return s.empty(); }), 
                     vec.end());
            if (vec.size() != originalSize) {
                fixedCount++;
            }
        };
        
        removeEmpty(units);
        removeEmpty(identifiers);
        removeEmpty(matchPatterns);
        removeEmpty(journalPaths);
        
        // 验证journal路径的有效性（如果指定了路径）
        if (!journalPaths.empty()) {
            std::vector<std::string> validPaths;
            for (const auto& path : journalPaths) {
                // 检查路径长度合理性
                if (path.length() > 0 && path.length() < 4096) {
                    validPaths.push_back(path);
                } else {
                    fixedCount++;
                }
            }
            if (validPaths.size() != journalPaths.size()) {
                journalPaths = std::move(validPaths);
            }
        }
        
        return fixedCount;
    }
    
    /**
     * @brief 检查配置是否有效
     * @return true 如果配置有效
     */
    bool IsValid() const {
        return cursorFlushPeriodMs > 0 && 
               maxEntriesPerBatch > 0 && 
               !seekPosition.empty() &&
               !cursorSeekFallback.empty();
    }
};

} // namespace logtail 