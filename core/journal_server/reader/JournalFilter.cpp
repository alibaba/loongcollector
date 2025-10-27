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

#include <sstream>

#include "../common/JournalUtils.h"
#include "absl/strings/match.h"
#include "logger/Logger.h"

namespace logtail {

bool JournalFilter::ApplyAllFilters(JournalReader* reader, const FilterConfig& config) {
    if (!reader) {
        LOG_ERROR(sLogger,
                  ("journal filter but journal reader is null", "")("config", config.mConfigName)("idx",
                                                                                                  config.mConfigIndex));
        return false;
    }

    LOG_INFO(sLogger,
             ("journal filter applying journal filters", "")("config", config.mConfigName)("idx", config.mConfigIndex)(
                 "description", GetConfigDescription(config)));

    // 验证配置
    if (!ValidateConfig(config)) {
        LOG_ERROR(sLogger,
                  ("journal filter with invalid filter config", "")("config", config.mConfigName)("idx",
                                                                                                  config.mConfigIndex));
        return false;
    }


    try {
        // 1. 应用units过滤（如果配置了）- 包含glob模式支持
        if (!AddUnitsFilter(reader, config.mUnits, config.mConfigName, config.mConfigIndex)) {
            LOG_ERROR(
                sLogger,
                ("journal filter units filter failed", "")("config", config.mConfigName)("idx", config.mConfigIndex));
            return false;
        }

        // 2. 应用自定义匹配模式（如果配置了）- 与Go版本顺序一致
        if (!AddMatchPatternsFilter(reader, config.mMatchPatterns, config.mConfigName, config.mConfigIndex)) {
            LOG_ERROR(sLogger,
                      ("journal filter match patterns filter failed",
                       "")("config", config.mConfigName)("idx", config.mConfigIndex));
            return false;
        }

        // 3. 应用kernel过滤（如果配置了units且启用kernel）
        // 与Golang版本保持一致：只有在配置了units且enableKernel=true时才添加kernel过滤器
        if (!config.mUnits.empty() && config.mEnableKernel) {
            if (!AddKernelFilter(reader, config.mConfigName, config.mConfigIndex)) {
                LOG_ERROR(sLogger,
                          ("journal filter kernel filter failed", "")("config",
                                                                      config.mConfigName)("idx", config.mConfigIndex));
                return false;
            }
        }

        // 4. 应用identifiers过滤（如果配置了）- 与Go版本顺序一致
        if (!AddIdentifiersFilter(reader, config.mIdentifiers, config.mConfigName, config.mConfigIndex)) {
            LOG_ERROR(sLogger,
                      ("journal filter identifiers filter failed", "")("config",
                                                                       config.mConfigName)("idx", config.mConfigIndex));
            return false;
        }

        LOG_INFO(sLogger,
                 ("journal filter all filters applied successfully",
                  "")("config", config.mConfigName)("idx", config.mConfigIndex));
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR(sLogger,
                  ("journal filter exception during filter application",
                   e.what())("config", config.mConfigName)("idx", config.mConfigIndex));
        return false;
    }
}

bool JournalFilter::AddUnitsFilter(JournalReader* reader,
                                   const std::vector<std::string>& units,
                                   const std::string& configName,
                                   size_t configIndex) {
    if (units.empty()) {
        // 没有units配置，不需要过滤
        return true;
    }

    std::vector<std::string> patterns;

    // 处理每个单元 - 直接添加具体单元或收集glob模式
    for (const auto& unit : units) {
        try {
            // 将任意字符串转换为有效的systemd单元名称（转换路径、添加后缀等）
            std::string mangledUnit = JournalUtils::UnitNameMangle(unit, ".service");

            if (JournalUtils::StringIsGlob(mangledUnit)) {
                // 收集glob模式以备后续处理
                patterns.push_back(mangledUnit);
            } else {
                // 立即添加具体单元匹配
                if (!addSingleUnitMatches(reader, mangledUnit, configName, configIndex)) {
                    LOG_ERROR(sLogger,
                              ("journal filter failed to add unit filter",
                               mangledUnit)("original", unit)("config", configName)("idx", configIndex));
                    return false;
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR(sLogger,
                      ("journal filter unit name mangle failed",
                       e.what())("unit", unit)("config", configName)("idx", configIndex));
            return false;
        }
    }

    // 现在处理glob模式（如果有的话）
    if (!patterns.empty()) {
        LOG_INFO(sLogger,
                 ("journal filter processing glob patterns",
                  "")("config", configName)("idx", configIndex)("patterns_count", patterns.size()));

        std::vector<std::string> matchedUnits = getPossibleUnits(reader, JournalUtils::kSystemUnits, patterns);
        LOG_INFO(sLogger,
                 ("journal filter glob patterns matched units",
                  "")("config", configName)("idx", configIndex)("matched_count", matchedUnits.size()));

        for (const auto& matchedUnit : matchedUnits) {
            if (!addSingleUnitMatches(reader, matchedUnit, configName, configIndex)) {
                LOG_ERROR(sLogger,
                          ("journal filter failed to add glob-matched unit filter",
                           matchedUnit)("config", configName)("idx", configIndex));
                return false;
            }
        }
    }

    LOG_INFO(sLogger,
             ("journal filter units filter completed", "")("config", configName)("idx", configIndex)("units_processed",
                                                                                                     units.size()));
    return true;
}

bool JournalFilter::addSingleUnitMatches(JournalReader* reader,
                                         const std::string& unit,
                                         const std::string& configName,
                                         size_t configIndex) {
    // 基于Go版本的addMatchesForUnit实现多维度日志收集逻辑
    // 参考：plugins/input/journal/unit.go:105-147

    try {
        // 1. 查找服务本身（_SYSTEMD_UNIT=unit）的消息
        if (!reader->AddMatch("_SYSTEMD_UNIT", unit)) {
            LOG_WARNING(sLogger,
                        ("journal filter failed to add unit match", unit)("config", configName)("idx", configIndex));
            return false;
        }

        // 2. 查找服务的coredump（_UID=0 + COREDUMP_UNIT=unit）
        // MESSAGE_ID "fc2e22bc6ee647b6b90729ab34a250b1" 是systemd预定义的coredump消息标识符
        // systemd-coredump服务在记录进程崩溃信息时使用这个标准ID
        if (!reader->AddDisjunction() || !reader->AddMatch("MESSAGE_ID", "fc2e22bc6ee647b6b90729ab34a250b1")
            || !reader->AddMatch("_UID", "0") || !reader->AddMatch("COREDUMP_UNIT", unit)) {
            LOG_WARNING(
                sLogger,
                ("journal filter failed to add coredump match", unit)("config", configName)("idx", configIndex));
            return false;
        }

        // 3. 查找PID 1（systemd关于此服务的消息）关于此服务的消息
        if (!reader->AddDisjunction() || !reader->AddMatch("_PID", "1") || !reader->AddMatch("UNIT", unit)) {
            LOG_WARNING(sLogger,
                        ("journal filter failed to add PID1 match", unit)("config", configName)("idx", configIndex));
            return false;
        }

        // 4. 查找授权守护进程（_UID=0 + OBJECT_SYSTEMD_UNIT=unit）关于此服务的消息
        if (!reader->AddDisjunction() || !reader->AddMatch("_UID", "0")
            || !reader->AddMatch("OBJECT_SYSTEMD_UNIT", unit)) {
            LOG_WARNING(sLogger,
                        ("journal filter failed to add daemon match", unit)("config", configName)("idx", configIndex));
            return false;
        }

        // 5. 显示属于slice(slice本身是一个容器，需要展开其下所有服务的日志)的所有消息
        if (absl::StrContains(unit, ".slice")) {
            if (!reader->AddDisjunction() || !reader->AddMatch("_SYSTEMD_SLICE", unit)) {
                LOG_WARNING(
                    sLogger,
                    ("journal filter failed to add slice match", unit)("config", configName)("idx", configIndex));
                return false;
            }
        }

        // 6. 为此unit添加最终的析取
        if (!reader->AddDisjunction()) {
            LOG_WARNING(
                sLogger,
                ("journal filter failed to add final disjunction", unit)("config", configName)("idx", configIndex));
            return false;
        }
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR(sLogger,
                  ("journal filter exception adding unit filter",
                   e.what())("unit", unit)("config", configName)("idx", configIndex));
        return false;
    }
}

bool JournalFilter::AddIdentifiersFilter(JournalReader* reader,
                                         const std::vector<std::string>& identifiers,
                                         const std::string& configName,
                                         size_t configIndex) {
    if (identifiers.empty()) {
        // 没有identifiers配置，不需要过滤
        return true;
    }

    for (const auto& identifier : identifiers) {
        if (!reader->AddMatch("SYSLOG_IDENTIFIER", identifier)) {
            LOG_WARNING(sLogger,
                        ("journal filter failed to add identifier match", identifier)("config",
                                                                                      configName)("idx", configIndex));
            return false;
        }

        // 与Go版本保持一致，每个identifier后都调用AddDisjunction
        if (!reader->AddDisjunction()) {
            LOG_WARNING(sLogger,
                        ("journal filter failed to add identifier disjunction",
                         identifier)("config", configName)("idx", configIndex));
            return false;
        }
    }

    LOG_INFO(sLogger,
             ("journal filter identifiers filter completed",
              "")("config", configName)("idx", configIndex)("identifiers_processed", identifiers.size()));
    return true;
}

bool JournalFilter::AddKernelFilter(JournalReader* reader, const std::string& configName, size_t configIndex) {
    if (!reader->AddMatch("_TRANSPORT", "kernel")) {
        LOG_WARNING(
            sLogger,
            ("journal filter failed to add kernel transport match", "")("config", configName)("idx", configIndex));
        return false;
    }

    // 添加AddDisjunction()以与其他过滤器形成OR逻辑关系，与Golang版本保持一致
    if (!reader->AddDisjunction()) {
        LOG_WARNING(sLogger,
                    ("journal filter failed to add kernel disjunction", "")("config", configName)("idx", configIndex));
        return false;
    }

    LOG_INFO(sLogger, ("journal filter kernel filter added", "")("config", configName)("idx", configIndex));

    return true;
}

bool JournalFilter::AddMatchPatternsFilter(JournalReader* reader,
                                           const std::vector<std::string>& patterns,
                                           const std::string& configName,
                                           size_t configIndex) {
    if (patterns.empty()) {
        // 没有自定义模式，不需要过滤
        return true;
    }

    // 与Go版本相同的方式处理模式 - 直接传递完整模式给AddMatch
    for (const auto& pattern : patterns) {
        if (pattern.empty()) {
            LOG_WARNING(sLogger,
                        ("journal filter empty pattern skipped", "")("config", configName)("idx", configIndex));
            continue;
        }

        // 解析FIELD=VALUE格式的模式以匹配Go版本行为
        size_t equalPos = pattern.find('=');
        if (equalPos == std::string::npos) {
            LOG_WARNING(
                sLogger,
                ("journal filter pattern missing '=' separator", pattern)("config", configName)("idx", configIndex));
            continue;
        }

        std::string field = pattern.substr(0, equalPos);
        std::string value = pattern.substr(equalPos + 1);

        if (!reader->AddMatch(field, value)) {
            LOG_ERROR(
                sLogger,
                ("journal filter failed to add pattern match", pattern)("config", configName)("idx", configIndex));
            return false;
        }

        if (!reader->AddDisjunction()) {
            LOG_ERROR(sLogger,
                      ("journal filter failed to add pattern disjunction", pattern)("config", configName)("idx",
                                                                                                          configIndex));
            return false;
        }
    }

    LOG_INFO(sLogger,
             ("journal filter match patterns filter completed",
              "")("config", configName)("idx", configIndex)("patterns_processed", patterns.size()));
    return true;
}

bool JournalFilter::ValidateConfig(const FilterConfig& config) {
    // 基本验证
    if (config.mConfigName.empty()) {
        LOG_WARNING(sLogger, ("journal filter config missing configName", ""));
        return false;
    }

    // 验证units配置
    for (const auto& unit : config.mUnits) {
        if (unit.empty()) {
            LOG_WARNING(sLogger, ("journal filter empty unit name in config", "")("config", config.mConfigName));
            return false;
        }
    }

    // 验证identifiers配置
    for (const auto& identifier : config.mIdentifiers) {
        if (identifier.empty()) {
            LOG_WARNING(sLogger, ("journal filter empty identifier in config", "")("config", config.mConfigName));
            return false;
        }
    }

    // 验证matchPatterns配置
    for (const auto& pattern : config.mMatchPatterns) {
        if (pattern.empty() || !absl::StrContains(pattern, '=')) {
            LOG_WARNING(sLogger, ("journal filter invalid match pattern", pattern)("config", config.mConfigName));
            return false;
        }
    }

    return true;
}

// ============================================================================
// 辅助函数实现（基于Go版本）
// ============================================================================

std::vector<std::string> JournalFilter::getPossibleUnits(JournalReader* reader,
                                                         const std::vector<std::string>& fields,
                                                         const std::vector<std::string>& patterns) {
    std::vector<std::string> found;
    std::vector<std::string> possibles;

    // 从所有字段获取唯一值
    for (const auto& field : fields) {
        std::vector<std::string> vals = reader->GetUniqueValues(field);
        possibles.insert(possibles.end(), vals.begin(), vals.end());
    }

    // 根据模式过滤可能的列表
    for (const auto& possible : possibles) {
        for (const auto& pattern : patterns) {
            if (JournalUtils::MatchPattern(pattern, possible)) {
                found.push_back(possible);
                break; // 找到此可能项的匹配，无需检查其他模式
            }
        }
    }

    return found;
}

std::string JournalFilter::GetConfigDescription(const FilterConfig& config) {
    std::ostringstream oss;
    oss << "Filter[";

    bool first = true;

    if (!config.mUnits.empty()) {
        if (!first) {
            oss << ", ";
        }
        oss << "units(" << config.mUnits.size() << ")";
        first = false;
    }

    if (!config.mIdentifiers.empty()) {
        if (!first) {
            oss << ", ";
        }
        oss << "identifiers(" << config.mIdentifiers.size() << ")";
        first = false;
    }

    if (config.mEnableKernel) {
        if (!first) {
            oss << ", ";
        }
        oss << "kernel";
        first = false;
    }

    if (!config.mMatchPatterns.empty()) {
        if (!first) {
            oss << ", ";
        }
        oss << "patterns(" << config.mMatchPatterns.size() << ")";
        first = false;
    }

    if (first) {
        oss << "no-filters";
    }

    oss << "]";
    return oss.str();
}

} // namespace logtail
