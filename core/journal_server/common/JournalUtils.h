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

#include <map>
#include <string>
#include <vector>

namespace logtail {
class JournalUtils {
public:
    // Syslog facility conversion mapping table
    static const std::map<std::string, std::string> kSyslogFacilityString;

    // Priority conversion mapping table
    static const std::map<std::string, std::string> kPriorityConversionMap;

    // Filter unit name processing constants
    static constexpr size_t kUnitNameMax = 256;
    static constexpr const char* kGlobChars = "*?[";
    static constexpr const char* kUppercaseLetters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static constexpr const char* kLowercaseLetters = "abcdefghijklmnopqrstuvwxyz";
    static constexpr const char* kDigits = "0123456789";

    static const std::string kLetters;
    static const std::string kValidChars;
    static const std::string kValidCharsWithAt;
    static const std::string kValidCharsGlob;

    // System unit fields and types
    static const std::vector<std::string> kSystemUnits;
    static const std::vector<std::string> kUnitTypes;

    static bool IsStringGlob(const std::string& name);
    static bool InCharset(const std::string& s, const std::string& charset);
    static bool IsDevicePath(const std::string& path);
    static bool IsPathAbsolute(const std::string& path);
    static bool MatchPattern(const std::string& pattern, const std::string& string);
    static bool IsUnitSuffixValid(const std::string& suffix);
    static bool IsUnitNameValid(const std::string& name);
    static std::string DoEscapeMangle(const std::string& name);
    /**
     * @example
     * UnitNameMangle("/dev/sda1", ".service") → "sda1.device"
     * UnitNameMangle("/home", ".service") → "home.mount"
     * UnitNameMangle("my-app", ".service") → "my-app.service"
     * UnitNameMangle("*.service", ".service") → "*.service" (keep glob unchanged)
     */
    static std::string UnitNameMangle(const std::string& name, const std::string& suffix);

    JournalUtils(const JournalUtils&) = delete;
    JournalUtils& operator=(const JournalUtils&) = delete;
    JournalUtils(JournalUtils&&) = delete;
    JournalUtils& operator=(JournalUtils&&) = delete;

private:
    JournalUtils() = default;
    ~JournalUtils() = default;
};

} // namespace logtail
