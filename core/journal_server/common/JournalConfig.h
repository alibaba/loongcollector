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
    std::string mSeekPosition;
    int mCursorFlushPeriodMs = 5000;
    std::string mCursorSeekFallback;

    // 过滤配置
    std::vector<std::string> mUnits; // Systemd服务单元过滤器
    std::vector<std::string> mIdentifiers; // Syslog标识符过滤器
    std::vector<std::string> mMatchPatterns; // 自定义匹配模式
    bool mKernel = true; // 启用内核日志过滤器

    // 性能配置
    // 注意：与Go插件不同已移除resetIntervalSecond配置，连接永远不重建
    int mMaxEntriesPerBatch = 1000; // 每批最大条目数

    // 字段处理配置
    bool mParsePriority = false; // 解析优先级字段
    bool mParseSyslogFacility = false; // 解析syslog设施字段
    bool mUseJournalEventTime = true; // 使用journal事件时间而非系统时间

    // 自定义journal路径（用于文件形式的journal）
    std::vector<std::string> mJournalPaths;

    // 上下文引用
    const CollectionPipelineContext* mCtx = nullptr;

    // 运行时状态（在验证期间设置）
    mutable QueueKey mQueueKey = -1; // 验证后缓存的队列键值（-1 = 未验证）

    JournalConfig() = default;

    /**
     * @brief 验证和修正配置值，确保所有字段都在有效范围内
     * @return 修正的配置项数量
     */
    int ValidateAndFixConfig() {
        int fixedCount = 0;

        // 验证数值字段的范围

        if (mCursorFlushPeriodMs <= 0) {
            mCursorFlushPeriodMs = 5000; // 默认5秒
            fixedCount++;
        } else if (mCursorFlushPeriodMs > 300000) { // 最大5分钟
            mCursorFlushPeriodMs = 300000;
            fixedCount++;
        }

        if (mMaxEntriesPerBatch <= 0) {
            mMaxEntriesPerBatch = 1000; // 默认1000条
            fixedCount++;
        } else if (mMaxEntriesPerBatch > 10000) { // 最大10000条，防止内存爆炸
            mMaxEntriesPerBatch = 10000;
            fixedCount++;
        }

        // 验证seek位置
        if (mSeekPosition != "head" && mSeekPosition != "tail" && mSeekPosition != "cursor"
            && mSeekPosition != "none") {
            mSeekPosition = "tail"; // 默认tail
            fixedCount++;
        }

        // 验证seek fallback（默认head）
        if (mCursorSeekFallback != "head" && mCursorSeekFallback != "tail") {
            mCursorSeekFallback = "head"; // 默认head
            fixedCount++;
        }

        // 验证字符串数组 - 移除空字符串
        auto removeEmpty = [&fixedCount](std::vector<std::string>& vec) {
            auto originalSize = vec.size();
            vec.erase(std::remove_if(vec.begin(), vec.end(), [](const std::string& s) { return s.empty(); }),
                      vec.end());
            if (vec.size() != originalSize) {
                fixedCount++;
            }
        };

        removeEmpty(mUnits);
        removeEmpty(mIdentifiers);
        removeEmpty(mMatchPatterns);
        removeEmpty(mJournalPaths);

        // 验证journal路径的有效性（如果指定了路径）
        if (!mJournalPaths.empty()) {
            std::vector<std::string> validPaths;
            for (const auto& path : mJournalPaths) {
                // 检查路径长度合理性
                if (path.length() > 0 && path.length() < 4096) {
                    validPaths.push_back(path);
                } else {
                    fixedCount++;
                }
            }
            if (validPaths.size() != mJournalPaths.size()) {
                mJournalPaths = std::move(validPaths);
            }
        }

        return fixedCount;
    }

    /**
     * @brief 检查配置是否有效
     * @return true 如果配置有效
     */
    bool IsValid() const {
        return mCursorFlushPeriodMs > 0 && mMaxEntriesPerBatch > 0 && !mSeekPosition.empty()
            && !mCursorSeekFallback.empty();
    }
};

} // namespace logtail
