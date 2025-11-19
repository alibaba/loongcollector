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

#include "JournalUtils.h"

#include <algorithm>
#include <filesystem>
#include <regex>
#include <sstream>

#include "absl/strings/match.h"

namespace logtail {

// ============================================================================
// Journal-related constant definitions
// ============================================================================

// Syslog facility conversion mapping table
const std::map<std::string, std::string> JournalUtils::kSyslogFacilityString = {
    {"0", "kernel"},         {"1", "user"},         {"2", "mail"},         {"3", "daemon"},     {"4", "auth"},
    {"5", "syslog"},         {"6", "line printer"}, {"7", "network news"}, {"8", "uucp"},       {"9", "clock daemon"},
    {"10", "security/auth"}, {"11", "ftp"},         {"12", "ntp"},         {"13", "log audit"}, {"14", "log alert"},
    {"15", "clock daemon"},  {"16", "local0"},      {"17", "local1"},      {"18", "local2"},    {"19", "local3"},
    {"20", "local4"},        {"21", "local5"},      {"22", "local6"},      {"23", "local7"}};

// Priority conversion mapping table
const std::map<std::string, std::string> JournalUtils::kPriorityConversionMap = {{"0", "emergency"},
                                                                                 {"1", "alert"},
                                                                                 {"2", "critical"},
                                                                                 {"3", "error"},
                                                                                 {"4", "warning"},
                                                                                 {"5", "notice"},
                                                                                 {"6", "informational"},
                                                                                 {"7", "debug"}};

// Unit name processing constants
const std::string JournalUtils::kLetters = std::string(kLowercaseLetters) + std::string(kUppercaseLetters);
const std::string JournalUtils::kValidChars = std::string(kDigits) + kLetters + ":-_.\\";
const std::string JournalUtils::kValidCharsWithAt = "@" + kValidChars;
const std::string JournalUtils::kValidCharsGlob = kValidCharsWithAt + "[]!-*?";

const std::vector<std::string> JournalUtils::kSystemUnits
    = {"_SYSTEMD_UNIT", "COREDUMP_UNIT", "UNIT", "OBJECT_SYSTEMD_UNIT", "_SYSTEMD_SLICE"};

const std::vector<std::string> JournalUtils::kUnitTypes = {".service",
                                                           ".socket",
                                                           ".target",
                                                           ".device",
                                                           ".mount",
                                                           ".automount",
                                                           ".swap",
                                                           ".path",
                                                           ".timer",
                                                           ".snapshot",
                                                           ".slice",
                                                           ".scope"};

// ============================================================================
// String and path utility function implementations
// ============================================================================

bool JournalUtils::IsStringGlob(const std::string& name) {
    return name.find_first_of(kGlobChars) != std::string::npos;
}

bool JournalUtils::InCharset(const std::string& s, const std::string& charset) {
    return std::all_of(s.begin(), s.end(), [&charset](char c) { return absl::StrContains(charset, c); });
}

bool JournalUtils::IsDevicePath(const std::string& path) {
    return path.length() >= 5 && (path.substr(0, 5) == "/dev/" || path.substr(0, 5) == "/sys/");
}

bool JournalUtils::IsPathAbsolute(const std::string& path) {
    return !path.empty() && path[0] == '/';
}

bool JournalUtils::MatchPattern(const std::string& pattern, const std::string& string) {
    // Use regular expressions for simple glob matching
    // Convert glob pattern to regular expression pattern
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

// ============================================================================
// Systemd unit-related utility function implementations
// ============================================================================

bool JournalUtils::IsUnitSuffixValid(const std::string& suffix) {
    if (suffix.empty() || suffix[0] != '.') {
        return false;
    }

    return std::any_of(kUnitTypes.begin(), kUnitTypes.end(), [&suffix](const std::string& validSuffix) {
        return suffix == validSuffix;
    });
}

bool JournalUtils::IsUnitNameValid(const std::string& name) {
    if (name.length() >= kUnitNameMax) {
        return false;
    }

    size_t dot = name.find('.');
    if (dot == std::string::npos) {
        return false; // Must have a dot (i.e., suffix)
    }

    std::string suffix = name.substr(dot);
    if (!IsUnitSuffixValid(suffix)) {
        return false;
    }

    // Check if unit name contains only valid characters (letters, digits, special symbols, and @)
    if (!InCharset(name, kValidCharsWithAt)) {
        return false;
    }

    size_t at = name.find('@');
    if (at == 0) {
        return false; // Cannot start with '@'
    }

    // Regular unit (non-template or instance) or template/instance unit
    if (at == std::string::npos || (at > 0 && dot >= at + 1)) {
        return true;
    }

    return false;
}

std::string JournalUtils::DoEscapeMangle(const std::string& name) {
    std::string mangled;
    for (char c : name) {
        if (c == '/') {
            mangled += '-';
        } else if (!absl::StrContains(kValidChars, c)) {
            // Convert to hexadecimal escape sequence
            std::ostringstream oss;
            oss << "\\x" << std::hex << static_cast<unsigned char>(c);
            mangled += oss.str();
        } else {
            mangled += c;
        }
    }
    return mangled;
}

std::string JournalUtils::UnitNameMangle(const std::string& name, const std::string& suffix) {
    // Cannot be empty or start with a dot
    if (name.empty() || name[0] == '.') {
        throw std::invalid_argument("unit name can't be empty or begin with a dot");
    }

    if (!IsUnitSuffixValid(suffix)) {
        throw std::invalid_argument("unit name has an invalid suffix");
    }

    if (IsUnitNameValid(name)) {
        return name;
    }

    if (IsStringGlob(name) && InCharset(name, kValidCharsGlob)) {
        return name;
    }

    // If it's a device path, convert to .device unit
    if (IsDevicePath(name)) {
        // Extract path and append .device at the end
        std::filesystem::path p(name);
        return p.filename().string() + ".device";
    }

    // If it's an absolute path, convert to .mount unit
    if (IsPathAbsolute(name)) {
        // Extract path and append .mount at the end
        std::filesystem::path p(name);
        return p.filename().string() + ".mount";
    }

    std::string escaped = DoEscapeMangle(name);

    // If no suffix exists, add suffix, but only when this is not a glob pattern,
    // so we can allow "foo.*" as a valid glob pattern.
    if (!IsStringGlob(escaped) && !absl::StrContains(escaped, '.')) {
        return escaped + suffix;
    }

    return escaped;
}

} // namespace logtail
