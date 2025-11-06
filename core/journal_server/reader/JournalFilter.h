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

#include "JournalReader.h"

namespace logtail {
class JournalFilter {
public:
    struct FilterConfig {
        std::vector<std::string> mUnits;
        std::vector<std::string> mIdentifiers;
        std::vector<std::string> mMatchPatterns;
        bool mEnableKernel = true;

        std::string mConfigName;
        size_t mConfigIndex = 0;
    };

    static bool ApplyAllFilters(JournalReader* reader, const FilterConfig& config);
    static bool AddUnitsFilter(JournalReader* reader,
                               const std::vector<std::string>& units,
                               const std::string& configName,
                               size_t configIndex);
    static bool AddIdentifiersFilter(JournalReader* reader,
                                     const std::vector<std::string>& identifiers,
                                     const std::string& configName,
                                     size_t configIndex);
    static bool AddKernelFilter(JournalReader* reader, const std::string& configName, size_t configIndex);
    static bool AddMatchPatternsFilter(JournalReader* reader,
                                       const std::vector<std::string>& patterns,
                                       const std::string& configName,
                                       size_t configIndex);
    static bool ValidateConfig(const FilterConfig& config);
    static std::string GetConfigDescription(const FilterConfig& config);

    JournalFilter(const JournalFilter&) = delete;
    JournalFilter& operator=(const JournalFilter&) = delete;
    JournalFilter(JournalFilter&&) = delete;
    JournalFilter& operator=(JournalFilter&&) = delete;

private:
    JournalFilter() = default;
    ~JournalFilter() = default;

    static bool addSingleUnitMatches(JournalReader* reader,
                                     const std::string& unit,
                                     const std::string& configName,
                                     size_t configIndex);

    // Glob pattern support functions
    static std::vector<std::string> getPossibleUnits(JournalReader* reader,
                                                     const std::vector<std::string>& fields,
                                                     const std::vector<std::string>& patterns);
};

} // namespace logtail
