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

#include "../reader/JournalReader.h"

namespace logtail {

/**
 * @brief Journal过滤器管理类
 * 
 * 负责处理所有journal相关的过滤逻辑：
 * - Units过滤：指定systemd unit过滤，不配置则全采集
 * - Identifiers过滤：指定syslog identifier过滤，同units逻辑
 * - Kernel过滤：kernel日志过滤，默认启用
 * - 自定义匹配模式过滤
 */
class JournalFilter {
public:
    /**
     * @brief 过滤器配置结构
     */
    struct FilterConfig {
        std::vector<std::string> units;           // systemd units过滤列表，空则全采集
        std::vector<std::string> identifiers;    // syslog identifiers过滤列表，空则全采集
        std::vector<std::string> matchPatterns;  // 自定义匹配模式
        bool enableKernel = true;                 // 是否采集kernel日志，默认true
        
        // 用于调试和日志的配置信息
        std::string configName;
        size_t configIndex = 0;
    };

    /**
     * @brief 应用所有过滤器到journal reader
     * @param reader journal reader实例
     * @param config 过滤器配置
     * @return 成功返回true，失败返回false
     */
    static bool ApplyAllFilters(JournalReader* reader, const FilterConfig& config);

    /**
     * @brief Units过滤：根据systemd unit配置过滤
     * 
     * 不配置units则全采集，配置了则只采集指定的units
     * 支持以下类型的匹配：
     * - 服务本身的消息：_SYSTEMD_UNIT=unit
     * - 服务的coredump：MESSAGE_ID + COREDUMP_UNIT  
     * - PID1关于服务的消息：_PID=1 + UNIT
     * - 守护进程关于服务的消息：_UID=0 + OBJECT_SYSTEMD_UNIT
     * - slice相关消息：_SYSTEMD_SLICE
     * 
     * @param reader journal reader实例
     * @param units units列表
     * @param configName 配置名称（用于日志）
     * @param configIndex 配置索引（用于日志）
     * @return 成功返回true，失败返回false
     */
    static bool AddUnitsFilter(JournalReader* reader, 
                               const std::vector<std::string>& units,
                               const std::string& configName, 
                               size_t configIndex);

    /**
     * @brief Identifiers过滤：根据syslog identifier配置过滤
     * 
     * 不配置identifiers则全采集，配置了则只采集指定的identifiers
     * 匹配SYSLOG_IDENTIFIER字段
     * 
     * @param reader journal reader实例
     * @param identifiers identifiers列表
     * @param configName 配置名称（用于日志）
     * @param configIndex 配置索引（用于日志）
     * @return 成功返回true，失败返回false
     */
    static bool AddIdentifiersFilter(JournalReader* reader,
                                     const std::vector<std::string>& identifiers,
                                     const std::string& configName,
                                     size_t configIndex);

    /**
     * @brief Kernel过滤：采集kernel日志
     * 
     * 默认启用kernel日志采集
     * 匹配_TRANSPORT=kernel
     * 
     * @param reader journal reader实例
     * @param configName 配置名称（用于日志）
     * @param configIndex 配置索引（用于日志）
     * @return 成功返回true，失败返回false
     */
    static bool AddKernelFilter(JournalReader* reader,
                                const std::string& configName,
                                size_t configIndex);

    /**
     * @brief 自定义匹配模式过滤
     * 
     * 支持自定义的journal匹配模式
     * 每个pattern直接传递给journal的AddMatch
     * 
     * @param reader journal reader实例
     * @param patterns 匹配模式列表
     * @param configName 配置名称（用于日志）
     * @param configIndex 配置索引（用于日志）
     * @return 成功返回true，失败返回false
     */
    static bool AddMatchPatternsFilter(JournalReader* reader,
                                       const std::vector<std::string>& patterns,
                                       const std::string& configName,
                                       size_t configIndex);

    /**
     * @brief 验证过滤器配置的有效性
     * @param config 过滤器配置
     * @return 配置有效返回true，无效返回false
     */
    static bool ValidateConfig(const FilterConfig& config);

    /**
     * @brief 获取过滤器配置的描述信息（用于日志）
     * @param config 过滤器配置
     * @return 配置描述字符串
     */
    static std::string GetConfigDescription(const FilterConfig& config);

private:
    JournalFilter() = default;
    ~JournalFilter() = default;

    // 辅助方法
    static bool addSingleUnitMatches(JournalReader* reader, 
                                     const std::string& unit,
                                     const std::string& configName,
                                     size_t configIndex);
};

} // namespace logtail 