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

    // Validate configuration
    if (!ValidateConfig(config)) {
        LOG_ERROR(sLogger,
                  ("journal filter with invalid filter config", "")("config", config.mConfigName)("idx",
                                                                                                  config.mConfigIndex));
        return false;
    }

    if (!AddUnitsFilter(reader, config.mUnits, config.mConfigName, config.mConfigIndex)) {
        LOG_ERROR(sLogger,
                  ("journal filter units filter failed", "")("config", config.mConfigName)("idx", config.mConfigIndex));
        return false;
    }

    if (!AddMatchPatternsFilter(reader, config.mMatchPatterns, config.mConfigName, config.mConfigIndex)) {
        LOG_ERROR(sLogger,
                  ("journal filter match patterns filter failed", "")("config",
                                                                      config.mConfigName)("idx", config.mConfigIndex));
        return false;
    }

    // Only add kernel filter when units are configured and enableKernel=true
    if (!config.mUnits.empty() && config.mEnableKernel) {
        if (!AddKernelFilter(reader, config.mConfigName, config.mConfigIndex)) {
            LOG_ERROR(
                sLogger,
                ("journal filter kernel filter failed", "")("config", config.mConfigName)("idx", config.mConfigIndex));
            return false;
        }
    }

    if (!AddIdentifiersFilter(reader, config.mIdentifiers, config.mConfigName, config.mConfigIndex)) {
        LOG_ERROR(
            sLogger,
            ("journal filter identifiers filter failed", "")("config", config.mConfigName)("idx", config.mConfigIndex));
        return false;
    }


    LOG_INFO(sLogger,
             ("journal filter all filters applied successfully", "")("config", config.mConfigName)(
                 "idx", config.mConfigIndex)("description", GetConfigDescription(config)));
    return true;
}

bool JournalFilter::AddUnitsFilter(JournalReader* reader,
                                   const std::vector<std::string>& units,
                                   const std::string& configName,
                                   size_t configIndex) {
    if (units.empty()) {
        return true;
    }

    std::vector<std::string> patterns;

    // Process each unit - directly add concrete units or collect glob patterns
    for (const auto& unit : units) {
        // Convert any string to a valid systemd unit name (convert paths, add suffixes, etc.)
        std::string mangledUnit = JournalUtils::UnitNameMangle(unit, ".service");

        if (JournalUtils::IsStringGlob(mangledUnit)) {
            // Collect glob patterns for later processing
            patterns.push_back(mangledUnit);
        } else {
            // Immediately add concrete unit matches
            if (!addSingleUnitMatches(reader, mangledUnit, configName, configIndex)) {
                LOG_ERROR(sLogger,
                          ("journal filter failed to add unit filter",
                           mangledUnit)("original", unit)("config", configName)("idx", configIndex));
                return false;
            }
        }
    }

    // Process glob patterns (if any)
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
    // 1. Match messages from the service itself (_SYSTEMD_UNIT=unit)
    if (!reader->AddMatch("_SYSTEMD_UNIT", unit)) {
        LOG_WARNING(sLogger,
                    ("journal filter failed to add unit match", unit)("config", configName)("idx", configIndex));
        return false;
    }

    // 2. Match service coredumps (_UID=0 + COREDUMP_UNIT=unit)
    // MESSAGE_ID "fc2e22bc6ee647b6b90729ab34a250b1" is a systemd predefined coredump message identifier
    if (!reader->AddDisjunction() || !reader->AddMatch("MESSAGE_ID", "fc2e22bc6ee647b6b90729ab34a250b1")
        || !reader->AddMatch("_UID", "0") || !reader->AddMatch("COREDUMP_UNIT", unit)) {
        LOG_WARNING(sLogger,
                    ("journal filter failed to add coredump match", unit)("config", configName)("idx", configIndex));
        return false;
    }

    // 3. Match messages from PID 1 (systemd) about this service
    if (!reader->AddDisjunction() || !reader->AddMatch("_PID", "1") || !reader->AddMatch("UNIT", unit)) {
        LOG_WARNING(sLogger,
                    ("journal filter failed to add PID1 match", unit)("config", configName)("idx", configIndex));
        return false;
    }

    // 4. Match authorized daemon messages (_UID=0 + OBJECT_SYSTEMD_UNIT=unit)
    if (!reader->AddDisjunction() || !reader->AddMatch("_UID", "0") || !reader->AddMatch("OBJECT_SYSTEMD_UNIT", unit)) {
        LOG_WARNING(sLogger,
                    ("journal filter failed to add daemon match", unit)("config", configName)("idx", configIndex));
        return false;
    }

    // 5. Show all messages belonging to the slice (slice is a container, need to expand logs of all services under it)
    if (absl::StrContains(unit, ".slice")) {
        if (!reader->AddDisjunction() || !reader->AddMatch("_SYSTEMD_SLICE", unit)) {
            LOG_WARNING(sLogger,
                        ("journal filter failed to add slice match", unit)("config", configName)("idx", configIndex));
            return false;
        }
    }

    // 6. Add final disjunction for this unit
    if (!reader->AddDisjunction()) {
        LOG_WARNING(sLogger,
                    ("journal filter failed to add final disjunction", unit)("config", configName)("idx", configIndex));
        return false;
    }
    return true;
}

bool JournalFilter::AddIdentifiersFilter(JournalReader* reader,
                                         const std::vector<std::string>& identifiers,
                                         const std::string& configName,
                                         size_t configIndex) {
    if (identifiers.empty()) {
        // No identifiers configured, no filtering needed
        return true;
    }

    for (const auto& identifier : identifiers) {
        if (!reader->AddMatch("SYSLOG_IDENTIFIER", identifier)) {
            LOG_WARNING(sLogger,
                        ("journal filter failed to add identifier match", identifier)("config",
                                                                                      configName)("idx", configIndex));
            return false;
        }

        // Call AddDisjunction after each identifier
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

    // Add AddDisjunction() to form OR logic relationship with other filters
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
        return true;
    }

    // Directly pass complete patterns to AddMatch
    for (const auto& pattern : patterns) {
        if (pattern.empty()) {
            LOG_WARNING(sLogger,
                        ("journal filter empty pattern skipped", "")("config", configName)("idx", configIndex));
            continue;
        }

        // Parse FIELD=VALUE format patterns to match Go version behavior
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
    // Basic validation
    if (config.mConfigName.empty()) {
        LOG_WARNING(sLogger, ("journal filter config missing configName", ""));
        return false;
    }

    // Validate units configuration
    for (const auto& unit : config.mUnits) {
        if (unit.empty() || unit[0] == '.') {
            LOG_WARNING(sLogger, ("journal filter invalid unit name in config", unit)("config", config.mConfigName));
            return false;
        }
    }

    // Validate identifiers configuration
    for (const auto& identifier : config.mIdentifiers) {
        if (identifier.empty()) {
            LOG_WARNING(sLogger, ("journal filter empty identifier in config", "")("config", config.mConfigName));
            return false;
        }
    }

    // Validate matchPatterns configuration
    for (const auto& pattern : config.mMatchPatterns) {
        if (pattern.empty() || !absl::StrContains(pattern, '=')) {
            LOG_WARNING(sLogger, ("journal filter invalid match pattern", pattern)("config", config.mConfigName));
            return false;
        }
    }

    return true;
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

    // Filter possible list according to patterns
    for (const auto& possible : possibles) {
        for (const auto& pattern : patterns) {
            if (JournalUtils::MatchPattern(pattern, possible)) {
                found.push_back(possible);
                break; // Found a match for this possible item, no need to check other patterns
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
