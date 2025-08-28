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

#include "JournalFilter.h"
#include "logger/Logger.h"
#include <sstream>

namespace logtail {

bool JournalFilter::ApplyAllFilters(JournalReader* reader, const FilterConfig& config) {
    if (!reader) {
        LOG_ERROR(sLogger, ("journal reader is null", "")("config", config.configName)("idx", config.configIndex));
        return false;
    }

    LOG_INFO(sLogger, ("applying journal filters", "")("config", config.configName)("idx", config.configIndex)
             ("description", GetConfigDescription(config)));

    // 验证配置
    if (!ValidateConfig(config)) {
        LOG_ERROR(sLogger, ("invalid filter config", "")("config", config.configName)("idx", config.configIndex));
        return false;
    }



    try {
        // 1. 应用units过滤（如果配置了）
        if (!config.units.empty()) {
            LOG_INFO(sLogger, ("applying units filter", "")("config", config.configName)("idx", config.configIndex)("units_count", config.units.size()));
            if (!AddUnitsFilter(reader, config.units, config.configName, config.configIndex)) {
                LOG_ERROR(sLogger, ("units filter failed", "")("config", config.configName)("idx", config.configIndex));
                return false;
            }
        }

        // 2. 应用identifiers过滤（如果配置了）
        if (!config.identifiers.empty()) {
            LOG_INFO(sLogger, ("applying identifiers filter", "")("config", config.configName)("idx", config.configIndex)("identifiers_count", config.identifiers.size()));
            if (!AddIdentifiersFilter(reader, config.identifiers, config.configName, config.configIndex)) {
                LOG_ERROR(sLogger, ("identifiers filter failed", "")("config", config.configName)("idx", config.configIndex));
                return false;
            }
        }

        // 3. 应用kernel过滤（如果配置了units且启用kernel）
        // 与Golang版本保持一致：只有在配置了units且enableKernel=true时才添加kernel过滤器
        if (!config.units.empty() && config.enableKernel) {
            LOG_INFO(sLogger, ("applying kernel filter", "")("config", config.configName)("idx", config.configIndex));
            if (!AddKernelFilter(reader, config.configName, config.configIndex)) {
                LOG_ERROR(sLogger, ("kernel filter failed", "")("config", config.configName)("idx", config.configIndex));
                return false;
            }
        }

        // 4. 应用自定义匹配模式（如果配置了）
        if (!config.matchPatterns.empty()) {
            LOG_INFO(sLogger, ("applying match patterns filter", "")("config", config.configName)("idx", config.configIndex)("patterns_count", config.matchPatterns.size()));
            if (!AddMatchPatternsFilter(reader, config.matchPatterns, config.configName, config.configIndex)) {
                LOG_ERROR(sLogger, ("match patterns filter failed", "")("config", config.configName)("idx", config.configIndex));
                return false;
            }
        }

        // 如果没有配置任何过滤器，记录警告（可能表示配置错误）
        if (config.units.empty() && config.identifiers.empty() && 
            !config.enableKernel && config.matchPatterns.empty()) {
            LOG_WARNING(sLogger, ("no filters configured, will collect all journal entries", "")
                        ("config", config.configName)("idx", config.configIndex));
        }

        // 如果只有kernel过滤器，提醒用户这会只收集内核消息
        if (config.units.empty() && config.identifiers.empty() && 
            config.enableKernel && config.matchPatterns.empty()) {
            LOG_INFO(sLogger, ("kernel-only filter active, will collect only kernel messages", "")
                     ("config", config.configName)("idx", config.configIndex));
        }

        LOG_INFO(sLogger, ("all filters applied successfully", "")("config", config.configName)("idx", config.configIndex));
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR(sLogger, ("exception during filter application", e.what())
                  ("config", config.configName)("idx", config.configIndex));
        return false;
    }
}

bool JournalFilter::AddUnitsFilter(JournalReader* reader, 
                                   const std::vector<std::string>& units,
                                   const std::string& configName, 
                                   size_t configIndex) {
    
    if (units.empty()) {
        return true; // 没有units配置，不需要过滤
    }

    LOG_INFO(sLogger, ("adding units filter", "")("config", configName)("idx", configIndex)("units_count", units.size()));

    for (const auto& unit : units) {
        if (!addSingleUnitMatches(reader, unit, configName, configIndex)) {
            LOG_ERROR(sLogger, ("failed to add unit filter", unit)("config", configName)("idx", configIndex));
            return false;
        }
        LOG_DEBUG(sLogger, ("unit filter added", unit)("config", configName)("idx", configIndex));
    }

    LOG_INFO(sLogger, ("units filter completed", "")("config", configName)("idx", configIndex)("units_processed", units.size()));
    return true;
}

bool JournalFilter::addSingleUnitMatches(JournalReader* reader, 
                                         const std::string& unit,
                                         const std::string& configName,
                                         size_t configIndex) {
    
    // 基于Go版本的addMatchesForUnit实现
    // 参考：plugins/input/journal/unit.go:105-147

    try {
        // 1. 查找服务本身的消息
        if (!reader->AddMatch("_SYSTEMD_UNIT", unit)) {
            LOG_WARNING(sLogger, ("failed to add unit match", unit)("config", configName)("idx", configIndex));
            return false;
        }
        
        // 2. 查找服务的coredump
        if (!reader->AddDisjunction() ||
            !reader->AddMatch("MESSAGE_ID", "fc2e22bc6ee647b6b90729ab34a250b1") ||
            !reader->AddMatch("_UID", "0") ||
            !reader->AddMatch("COREDUMP_UNIT", unit)) {
            LOG_WARNING(sLogger, ("failed to add coredump match", unit)("config", configName)("idx", configIndex));
            return false;
        }
        
        // 3. 查找PID 1关于此服务的消息
        if (!reader->AddDisjunction() ||
            !reader->AddMatch("_PID", "1") ||
            !reader->AddMatch("UNIT", unit)) {
            LOG_WARNING(sLogger, ("failed to add PID1 match", unit)("config", configName)("idx", configIndex));
            return false;
        }
        
        // 4. 查找授权守护进程关于此服务的消息
        if (!reader->AddDisjunction() ||
            !reader->AddMatch("_UID", "0") ||
            !reader->AddMatch("OBJECT_SYSTEMD_UNIT", unit)) {
            LOG_WARNING(sLogger, ("failed to add daemon match", unit)("config", configName)("idx", configIndex));
            return false;
        }
        
        // 5. 显示属于slice的所有消息
        if (unit.find(".slice") != std::string::npos) {
            if (!reader->AddDisjunction() ||
                !reader->AddMatch("_SYSTEMD_SLICE", unit)) {
                LOG_WARNING(sLogger, ("failed to add slice match", unit)("config", configName)("idx", configIndex));
                return false;
            }
        }
        
        // 6. 为此unit添加最终的析取
        if (!reader->AddDisjunction()) {
            LOG_WARNING(sLogger, ("failed to add final disjunction", unit)("config", configName)("idx", configIndex));
            return false;
        }
        
        LOG_DEBUG(sLogger, ("comprehensive unit filter added", unit)("config", configName)("idx", configIndex));
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR(sLogger, ("exception adding unit filter", e.what())("unit", unit)("config", configName)("idx", configIndex));
        return false;
    }
}

bool JournalFilter::AddIdentifiersFilter(JournalReader* reader,
                                         const std::vector<std::string>& identifiers,
                                         const std::string& configName,
                                         size_t configIndex) {
    
    if (identifiers.empty()) {
        return true; // 没有identifiers配置，不需要过滤
    }

    LOG_INFO(sLogger, ("adding identifiers filter", "")("config", configName)("idx", configIndex)("identifiers_count", identifiers.size()));

    for (const auto& identifier : identifiers) {
        if (!reader->AddMatch("SYSLOG_IDENTIFIER", identifier)) {
            LOG_WARNING(sLogger, ("failed to add identifier match", identifier)("config", configName)("idx", configIndex));
            return false;
        }
        
        // 与Go版本保持一致，每个identifier后都调用AddDisjunction
        if (!reader->AddDisjunction()) {
            LOG_WARNING(sLogger, ("failed to add identifier disjunction", identifier)("config", configName)("idx", configIndex));
            return false;
        }
        
        LOG_DEBUG(sLogger, ("identifier filter added", identifier)("config", configName)("idx", configIndex));
    }
    
    LOG_INFO(sLogger, ("identifiers filter completed", "")("config", configName)("idx", configIndex)("identifiers_processed", identifiers.size()));
    return true;
}

bool JournalFilter::AddKernelFilter(JournalReader* reader,
                                    const std::string& configName,
                                    size_t configIndex) {
    
    LOG_INFO(sLogger, ("adding kernel filter", "")("config", configName)("idx", configIndex));

    if (!reader->AddMatch("_TRANSPORT", "kernel")) {
        LOG_WARNING(sLogger, ("failed to add kernel transport match", "")("config", configName)("idx", configIndex));
        return false;
    }
    
    // 添加AddDisjunction()以与其他过滤器形成OR逻辑关系，与Golang版本保持一致
    if (!reader->AddDisjunction()) {
        LOG_WARNING(sLogger, ("failed to add kernel disjunction", "")("config", configName)("idx", configIndex));
        return false;
    }
    
    LOG_DEBUG(sLogger, ("kernel filter added", "")("config", configName)("idx", configIndex));
    return true;
}

bool JournalFilter::AddMatchPatternsFilter(JournalReader* reader,
                                           const std::vector<std::string>& patterns,
                                           const std::string& configName,
                                           size_t configIndex) {
    
    if (patterns.empty()) {
        return true; // 没有自定义模式，不需要过滤
    }

    LOG_INFO(sLogger, ("adding match patterns filter", "")("config", configName)("idx", configIndex)("patterns_count", patterns.size()));

    for (const auto& pattern : patterns) {
        // 解析模式，支持FIELD=VALUE格式
        size_t equalPos = pattern.find('=');
        if (equalPos == std::string::npos) {
            LOG_WARNING(sLogger, ("invalid pattern format, expected FIELD=VALUE", pattern)("config", configName)("idx", configIndex));
            continue;
        }

        std::string field = pattern.substr(0, equalPos);
        std::string value = pattern.substr(equalPos + 1);

        if (!reader->AddMatch(field, value)) {
            LOG_WARNING(sLogger, ("failed to add pattern match", pattern)("config", configName)("idx", configIndex));
            return false;
        }
        
        if (!reader->AddDisjunction()) {
            LOG_WARNING(sLogger, ("failed to add pattern disjunction", pattern)("config", configName)("idx", configIndex));
            return false;
        }
        
        LOG_DEBUG(sLogger, ("pattern filter added", pattern)("config", configName)("idx", configIndex));
    }
    
    LOG_INFO(sLogger, ("match patterns filter completed", "")("config", configName)("idx", configIndex)("patterns_processed", patterns.size()));
    return true;
}

bool JournalFilter::ValidateConfig(const FilterConfig& config) {
    // 基本验证
    if (config.configName.empty()) {
        LOG_WARNING(sLogger, ("filter config missing configName", ""));
        return false;
    }

    // 验证units配置
    for (const auto& unit : config.units) {
        if (unit.empty()) {
            LOG_WARNING(sLogger, ("empty unit name in config", "")("config", config.configName));
            return false;
        }
    }

    // 验证identifiers配置
    for (const auto& identifier : config.identifiers) {
        if (identifier.empty()) {
            LOG_WARNING(sLogger, ("empty identifier in config", "")("config", config.configName));
            return false;
        }
    }

    // 验证matchPatterns配置
    for (const auto& pattern : config.matchPatterns) {
        if (pattern.empty() || pattern.find('=') == std::string::npos) {
            LOG_WARNING(sLogger, ("invalid match pattern", pattern)("config", config.configName));
            return false;
        }
    }

    return true;
}

std::string JournalFilter::GetConfigDescription(const FilterConfig& config) {
    std::ostringstream oss;
    oss << "Filter[";
    
    bool first = true;
    
    if (!config.units.empty()) {
        if (!first) oss << ", ";
        oss << "units(" << config.units.size() << ")";
        first = false;
    }
    
    if (!config.identifiers.empty()) {
        if (!first) oss << ", ";
        oss << "identifiers(" << config.identifiers.size() << ")";
        first = false;
    }
    
    if (config.enableKernel) {
        if (!first) oss << ", ";
        oss << "kernel";
        first = false;
    }
    
    if (!config.matchPatterns.empty()) {
        if (!first) oss << ", ";
        oss << "patterns(" << config.matchPatterns.size() << ")";
        first = false;
    }
    
    if (first) {
        oss << "no-filters";
    }
    
    oss << "]";
    return oss.str();
}

} // namespace logtail 