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
#include <algorithm>
#include <regex>
#include <filesystem>

#include "absl/strings/match.h"
#include "logger/Logger.h"
#include "../common/JournalConstants.h"

namespace logtail {

bool JournalFilter::ApplyAllFilters(JournalReader* reader, const FilterConfig& config) {
    if (!reader) {
        LOG_ERROR(sLogger, ("journal reader is null", "")("config", config.configName)("idx", config.configIndex));
        return false;
    }

    LOG_INFO(sLogger,
             ("applying journal filters",
              "")("config", config.configName)("idx", config.configIndex)("description", GetConfigDescription(config)));

    // 验证配置
    if (!ValidateConfig(config)) {
        LOG_ERROR(sLogger, ("invalid filter config", "")("config", config.configName)("idx", config.configIndex));
        return false;
    }


    try {
        // 1. 应用units过滤（如果配置了）- 包含glob模式支持
        if (!config.units.empty()) {
            LOG_INFO(sLogger,
                     ("applying units filter",
                      "")("config", config.configName)("idx", config.configIndex)("units_count", config.units.size()));
            if (!AddUnitsFilter(reader, config.units, config.configName, config.configIndex)) {
                LOG_ERROR(sLogger, ("units filter failed", "")("config", config.configName)("idx", config.configIndex));
                return false;
            }
        }

        // 2. 应用自定义匹配模式（如果配置了）- 与Go版本顺序一致
        if (!config.matchPatterns.empty()) {
            LOG_INFO(sLogger,
                     ("applying match patterns filter", "")("config", config.configName)("idx", config.configIndex)(
                         "patterns_count", config.matchPatterns.size()));
            if (!AddMatchPatternsFilter(reader, config.matchPatterns, config.configName, config.configIndex)) {
                LOG_ERROR(sLogger,
                          ("match patterns filter failed", "")("config", config.configName)("idx", config.configIndex));
                return false;
            }
        }

        // 3. 应用kernel过滤（如果配置了units且启用kernel）
        // 与Golang版本保持一致：只有在配置了units且enableKernel=true时才添加kernel过滤器
        if (!config.units.empty() && config.enableKernel) {
            LOG_INFO(sLogger, ("applying kernel filter", "")("config", config.configName)("idx", config.configIndex));
            if (!AddKernelFilter(reader, config.configName, config.configIndex)) {
                LOG_ERROR(sLogger,
                          ("kernel filter failed", "")("config", config.configName)("idx", config.configIndex));
                return false;
            }
        }

        // 4. 应用identifiers过滤（如果配置了）- 与Go版本顺序一致
        if (!config.identifiers.empty()) {
            LOG_INFO(sLogger,
                     ("applying identifiers filter", "")("config", config.configName)("idx", config.configIndex)(
                         "identifiers_count", config.identifiers.size()));
            if (!AddIdentifiersFilter(reader, config.identifiers, config.configName, config.configIndex)) {
                LOG_ERROR(sLogger,
                          ("identifiers filter failed", "")("config", config.configName)("idx", config.configIndex));
                return false;
            }
        }

        // 如果没有配置任何过滤器，记录警告（可能表示配置错误）
        if (config.units.empty() && config.identifiers.empty() && !config.enableKernel
            && config.matchPatterns.empty()) {
            LOG_WARNING(sLogger,
                        ("no filters configured, will collect all journal entries",
                         "")("config", config.configName)("idx", config.configIndex));
        }

        // 如果只有kernel过滤器，提醒用户这会只收集内核消息
        if (config.units.empty() && config.identifiers.empty() && config.enableKernel && config.matchPatterns.empty()) {
            LOG_INFO(sLogger,
                     ("kernel-only filter active, will collect only kernel messages",
                      "")("config", config.configName)("idx", config.configIndex));
        }

        LOG_INFO(sLogger,
                 ("all filters applied successfully", "")("config", config.configName)("idx", config.configIndex));
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR(
            sLogger,
            ("exception during filter application", e.what())("config", config.configName)("idx", config.configIndex));
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

    LOG_INFO(sLogger,
             ("adding units filter", "")("config", configName)("idx", configIndex)("units_count", units.size()));

    std::vector<std::string> patterns;
    
    // Process each unit - add specific units or collect glob patterns  
    for (const auto& unit : units) {
        try {
            // Mangle unit name (convert paths, add suffix, etc.)
            std::string mangledUnit = unitNameMangle(unit, ".service");
            
            if (stringIsGlob(mangledUnit)) {
                // Collect glob patterns for later processing
                patterns.push_back(mangledUnit);
                LOG_DEBUG(sLogger, ("glob pattern collected", mangledUnit)("original", unit)("config", configName)("idx", configIndex));
            } else {
                // Add specific unit matches immediately
                if (!addSingleUnitMatches(reader, mangledUnit, configName, configIndex)) {
                    LOG_ERROR(sLogger, ("failed to add unit filter", mangledUnit)("original", unit)("config", configName)("idx", configIndex));
                    return false;
                }
                LOG_DEBUG(sLogger, ("specific unit filter added", mangledUnit)("original", unit)("config", configName)("idx", configIndex));
            }
        } catch (const std::exception& e) {
            LOG_ERROR(sLogger, ("unit name mangle failed", e.what())("unit", unit)("config", configName)("idx", configIndex));
            return false;
        }
    }

    // Now process glob patterns if any
    if (!patterns.empty()) {
        LOG_INFO(sLogger, ("processing glob patterns", "")("config", configName)("idx", configIndex)("patterns_count", patterns.size()));
        
        std::vector<std::string> matchedUnits = getPossibleUnits(reader, JournalConstants::kSystemUnits, patterns);
        LOG_INFO(sLogger, ("glob patterns matched units", "")("config", configName)("idx", configIndex)("matched_count", matchedUnits.size()));
        
        for (const auto& matchedUnit : matchedUnits) {
            if (!addSingleUnitMatches(reader, matchedUnit, configName, configIndex)) {
                LOG_ERROR(sLogger, ("failed to add glob-matched unit filter", matchedUnit)("config", configName)("idx", configIndex));
                return false;
            }
            LOG_DEBUG(sLogger, ("glob-matched unit filter added", matchedUnit)("config", configName)("idx", configIndex));
        }
    }

    LOG_INFO(sLogger,
             ("units filter completed", "")("config", configName)("idx", configIndex)("units_processed", units.size()));
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
        if (!reader->AddDisjunction() || !reader->AddMatch("MESSAGE_ID", "fc2e22bc6ee647b6b90729ab34a250b1")
            || !reader->AddMatch("_UID", "0") || !reader->AddMatch("COREDUMP_UNIT", unit)) {
            LOG_WARNING(sLogger, ("failed to add coredump match", unit)("config", configName)("idx", configIndex));
            return false;
        }

        // 3. 查找PID 1关于此服务的消息
        if (!reader->AddDisjunction() || !reader->AddMatch("_PID", "1") || !reader->AddMatch("UNIT", unit)) {
            LOG_WARNING(sLogger, ("failed to add PID1 match", unit)("config", configName)("idx", configIndex));
            return false;
        }

        // 4. 查找授权守护进程关于此服务的消息
        if (!reader->AddDisjunction() || !reader->AddMatch("_UID", "0")
            || !reader->AddMatch("OBJECT_SYSTEMD_UNIT", unit)) {
            LOG_WARNING(sLogger, ("failed to add daemon match", unit)("config", configName)("idx", configIndex));
            return false;
        }

        // 5. 显示属于slice的所有消息
        if (absl::StrContains(unit, ".slice")) {
            if (!reader->AddDisjunction() || !reader->AddMatch("_SYSTEMD_SLICE", unit)) {
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
        LOG_ERROR(sLogger,
                  ("exception adding unit filter", e.what())("unit", unit)("config", configName)("idx", configIndex));
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

    LOG_INFO(sLogger,
             ("adding identifiers filter", "")("config", configName)("idx", configIndex)("identifiers_count",
                                                                                         identifiers.size()));

    for (const auto& identifier : identifiers) {
        if (!reader->AddMatch("SYSLOG_IDENTIFIER", identifier)) {
            LOG_WARNING(sLogger,
                        ("failed to add identifier match", identifier)("config", configName)("idx", configIndex));
            return false;
        }

        // 与Go版本保持一致，每个identifier后都调用AddDisjunction
        if (!reader->AddDisjunction()) {
            LOG_WARNING(sLogger,
                        ("failed to add identifier disjunction", identifier)("config", configName)("idx", configIndex));
            return false;
        }

        LOG_DEBUG(sLogger, ("identifier filter added", identifier)("config", configName)("idx", configIndex));
    }

    LOG_INFO(sLogger,
             ("identifiers filter completed", "")("config", configName)("idx", configIndex)("identifiers_processed",
                                                                                            identifiers.size()));
    return true;
}

bool JournalFilter::AddKernelFilter(JournalReader* reader, const std::string& configName, size_t configIndex) {
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

    LOG_INFO(sLogger,
             ("adding match patterns filter", "")("config", configName)("idx", configIndex)("patterns_count",
                                                                                            patterns.size()));

    // Process patterns same as Go version - directly pass complete pattern to AddMatch
    for (const auto& pattern : patterns) {
        if (pattern.empty()) {
            LOG_WARNING(sLogger, ("empty pattern skipped", "")("config", configName)("idx", configIndex));
            continue;
        }

        // Parse pattern in FIELD=VALUE format to match Go version behavior
        size_t equalPos = pattern.find('=');
        if (equalPos == std::string::npos) {
            LOG_WARNING(sLogger, ("pattern missing '=' separator", pattern)("config", configName)("idx", configIndex));
            continue;
        }
        
        std::string field = pattern.substr(0, equalPos);
        std::string value = pattern.substr(equalPos + 1);
        
        if (!reader->AddMatch(field, value)) {
            LOG_ERROR(sLogger, ("failed to add pattern match", pattern)("config", configName)("idx", configIndex));
            return false;
        }

        if (!reader->AddDisjunction()) {
            LOG_ERROR(sLogger,
                      ("failed to add pattern disjunction", pattern)("config", configName)("idx", configIndex));
            return false;
        }

        LOG_DEBUG(sLogger, ("pattern filter added", pattern)("config", configName)("idx", configIndex));
    }

    LOG_INFO(sLogger,
             ("match patterns filter completed", "")("config", configName)("idx", configIndex)("patterns_processed",
                                                                                               patterns.size()));
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
        if (pattern.empty() || !absl::StrContains(pattern, '=')) {
            LOG_WARNING(sLogger, ("invalid match pattern", pattern)("config", config.configName));
            return false;
        }
    }

    return true;
}

// ============================================================================
// Helper Functions Implementation (based on Go version)
// ============================================================================

bool JournalFilter::stringIsGlob(const std::string& name) {
    return name.find_first_of(JournalConstants::kGlobChars) != std::string::npos;
}

bool JournalFilter::inCharset(const std::string& s, const std::string& charset) {
    for (char c : s) {
        if (charset.find(c) == std::string::npos) {
            return false;
        }
    }
    return true;
}

bool JournalFilter::isDevicePath(const std::string& path) {
    return path.length() >= 5 && (path.substr(0, 5) == "/dev/" || path.substr(0, 5) == "/sys/");
}

bool JournalFilter::pathIsAbsolute(const std::string& path) {
    return !path.empty() && path[0] == '/';
}

bool JournalFilter::unitSuffixIsValid(const std::string& suffix) {
    if (suffix.empty() || suffix[0] != '.') {
        return false;
    }
    
    for (const auto& validSuffix : JournalConstants::kUnitTypes) {
        if (suffix == validSuffix) {
            return true;
        }
    }
    return false;
}

bool JournalFilter::unitNameIsValid(const std::string& name) {
    if (name.length() >= JournalConstants::kUnitNameMax) {
        return false;
    }
    
    size_t dot = name.find('.');
    if (dot == std::string::npos) {
        return false; // Must have a dot (i.e. suffix)
    }
    
    std::string suffix = name.substr(dot);
    if (!unitSuffixIsValid(suffix)) {
        return false;
    }
    
    if (!inCharset(name, JournalConstants::kValidCharsWithAt)) {
        return false;
    }
    
    size_t at = name.find('@');
    if (at == 0) {
        return false; // Can't start with '@'
    }
    
    // Plain unit (not a template or instance) or a template or instance  
    if (at == std::string::npos || (at > 0 && dot >= at + 1)) {
        return true;
    }
    
    return false;
}

std::string JournalFilter::doEscapeMangle(const std::string& name) {
    std::string mangled;
    for (char c : name) {
        if (c == '/') {
            mangled += '-';
        } else if (JournalConstants::kValidChars.find(c) == std::string::npos) {
            // Convert to hex escape sequence
            std::ostringstream oss;
            oss << "\\x" << std::hex << static_cast<unsigned char>(c);
            mangled += oss.str();
        } else {
            mangled += c;
        }
    }
    return mangled;
}

std::string JournalFilter::unitNameMangle(const std::string& name, const std::string& suffix) {
    // Can't be empty or begin with a dot
    if (name.empty() || name[0] == '.') {
        throw std::invalid_argument("unit name can't be empty or begin with a dot");
    }
    
    if (!unitSuffixIsValid(suffix)) {
        throw std::invalid_argument("unit name has an invalid suffix");
    }
    
    // Already a fully valid unit name?
    if (unitNameIsValid(name)) {
        return name;
    }
    
    // Already a fully valid globbing expression? If so, no mangling is necessary either...
    if (stringIsGlob(name) && inCharset(name, JournalConstants::kValidCharsGlob)) {
        return name;
    }
    
    if (isDevicePath(name)) {
        // chop off path and put .device on the end
        std::filesystem::path p(name);
        return p.filename().string() + ".device";
    }
    
    if (pathIsAbsolute(name)) {
        // chop path and put .mount on the end  
        std::filesystem::path p(name);
        return p.filename().string() + ".mount";
    }
    
    std::string escaped = doEscapeMangle(name);
    
    // Append a suffix if it doesn't have any, but only if this is not a glob,
    // so that we can allow "foo.*" as a valid glob.
    if (!stringIsGlob(escaped) && escaped.find('.') == std::string::npos) {
        return escaped + suffix;
    }
    
    return escaped;
}

bool JournalFilter::matchPattern(const std::string& pattern, const std::string& string) {
    // Simple glob matching using regex 
    // Convert glob pattern to regex pattern
    std::string regexPattern;
    regexPattern.reserve(pattern.length() * 2);
    
    for (char c : pattern) {
        switch (c) {
            case '*':
                regexPattern += ".*";
                break;
            case '?':
                regexPattern += ".";
                break;
            case '[':
            case ']':
                regexPattern += c;
                break;
            case '.':
            case '^':
            case '$':
            case '+':
            case '{':
            case '}':
            case '|':
            case '(':
            case ')':
            case '\\':
                regexPattern += '\\';
                regexPattern += c;
                break;
            default:
                regexPattern += c;
                break;
        }
    }
    
    try {
        std::regex regex(regexPattern);
        return std::regex_match(string, regex);
    } catch (const std::regex_error&) {
        return false;
    }
}

std::vector<std::string> JournalFilter::getPossibleUnits(JournalReader* reader,
                                                         const std::vector<std::string>& fields,
                                                         const std::vector<std::string>& patterns) {
    std::vector<std::string> found;
    std::vector<std::string> possibles;
    
    // Get unique values from all fields
    for (const auto& field : fields) {
        std::vector<std::string> vals = reader->GetUniqueValues(field);
        possibles.insert(possibles.end(), vals.begin(), vals.end());
    }
    
    // Filter possibles list against patterns
    for (const auto& possible : possibles) {
        for (const auto& pattern : patterns) {
            if (matchPattern(pattern, possible)) {
                found.push_back(possible);
                break; // Found match for this possible, no need to check other patterns
            }
        }
    }
    
    return found;
}

std::string JournalFilter::GetConfigDescription(const FilterConfig& config) {
    std::ostringstream oss;
    oss << "Filter[";

    bool first = true;

    if (!config.units.empty()) {
        if (!first) {
            oss << ", ";
        }
        oss << "units(" << config.units.size() << ")";
        first = false;
    }

    if (!config.identifiers.empty()) {
        if (!first) {
            oss << ", ";
        }
        oss << "identifiers(" << config.identifiers.size() << ")";
        first = false;
    }

    if (config.enableKernel) {
        if (!first) {
            oss << ", ";
        }
        oss << "kernel";
        first = false;
    }

    if (!config.matchPatterns.empty()) {
        if (!first) {
            oss << ", ";
        }
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
