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

#include <sstream>
#include <algorithm>
#include <regex>
#include <filesystem>

#include "absl/strings/match.h"
#include "JournalConstants.h"

namespace logtail {

// ============================================================================
// 字符串和路径工具函数实现
// ============================================================================

// 检查字符串是否包含glob模式字符（*、?、[）
bool JournalUtils::StringIsGlob(const std::string& name) {
    return name.find_first_of(JournalConstants::kGlobChars) != std::string::npos;
}

// 检查字符串中的所有字符是否都在指定的字符集中
bool JournalUtils::InCharset(const std::string& s, const std::string& charset) {
    return std::all_of(s.begin(), s.end(), [&charset](char c) {
        return absl::StrContains(charset, c);
    });
}

// 检查路径是否是设备路径（以/dev/或/sys/开头）
bool JournalUtils::IsDevicePath(const std::string& path) {
    return path.length() >= 5 && (path.substr(0, 5) == "/dev/" || path.substr(0, 5) == "/sys/");
}

// 检查路径是否是绝对路径（以/开头）
bool JournalUtils::PathIsAbsolute(const std::string& path) {
    return !path.empty() && path[0] == '/';
}

// 使用glob模式匹配字符串
bool JournalUtils::MatchPattern(const std::string& pattern, const std::string& string) {
    // 使用正则表达式进行简单的glob匹配
    // 将glob模式转换为正则表达式模式
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
// Systemd单元相关工具函数实现
// ============================================================================

// 检查单元后缀是否有效（必须以.开头且在有效类型列表中）
bool JournalUtils::UnitSuffixIsValid(const std::string& suffix) {
    if (suffix.empty() || suffix[0] != '.') {
        return false;
    }
    
    return std::any_of(JournalConstants::kUnitTypes.begin(), JournalConstants::kUnitTypes.end(),
                       [&suffix](const std::string& validSuffix) {
                           return suffix == validSuffix;
                       });
}

// 检查单元名称是否有效
bool JournalUtils::UnitNameIsValid(const std::string& name) {
    if (name.length() >= JournalConstants::kUnitNameMax) {
        return false;
    }
    
    size_t dot = name.find('.');
    if (dot == std::string::npos) {
        return false; // 必须有点号（即后缀）
    }
    
    std::string suffix = name.substr(dot);
    if (!UnitSuffixIsValid(suffix)) {
        return false;
    }
    
    // 检查单元名称是否只包含有效字符（字母、数字、特殊符号和@）
    if (!InCharset(name, JournalConstants::kValidCharsWithAt)) {
        return false;
    }
    
    size_t at = name.find('@');
    if (at == 0) {
        return false; // 不能以'@'开头
    }
    
    // 普通单元（非模板或实例）或模板/实例单元  
    if (at == std::string::npos || (at > 0 && dot >= at + 1)) {
        return true;
    }
    
    return false;
}

/**
 * @brief 转义单元名称中的无效字符
 * 
 * 将字符串中的无效字符进行转义处理，使其符合systemd单元名称规范：
 * - 将斜杠 '/' 转换为破折号 '-'
 * - 将其他无效字符转换为十六进制转义序列 "\xNN"
 * - 保留有效字符不变
 * 
 * @param name 需要转义的字符串
 * @return 转义后的字符串
 */
std::string JournalUtils::DoEscapeMangle(const std::string& name) {
    std::string mangled;
    for (char c : name) {
        if (c == '/') {
            mangled += '-';
        } else if (!absl::StrContains(JournalConstants::kValidChars, c)) {
            // 转换为十六进制转义序列
            std::ostringstream oss;
            oss << "\\x" << std::hex << static_cast<unsigned char>(c);
            mangled += oss.str();
        } else {
            mangled += c;
        }
    }
    return mangled;
}

/**
 * @brief 将任意字符串转换为有效的systemd单元名称
 * 
 * 这个函数实现了systemd单元名称的标准化转换规则，支持以下转换：
 * 1. 设备路径（如/dev/sda）→ 设备单元（sda.device）
 * 2. 挂载路径（如/home）→ 挂载单元（home.mount）
 * 3. 普通字符串 → 添加指定后缀的单元名称
 * 4. 处理无效字符的转义
 * 5. 保持glob模式和已有效单元名称不变
 * 
 * @param name 待转换的字符串（可以是路径、服务名等）
 * @param suffix 默认后缀（如".service"）
 * @return 标准化的systemd单元名称
 * @throws std::invalid_argument 如果输入为空、以点号开头或后缀无效
 * 
 * @example
 * UnitNameMangle("/dev/sda1", ".service") → "sda1.device"
 * UnitNameMangle("/home", ".service") → "home.mount"  
 * UnitNameMangle("my-app", ".service") → "my-app.service"
 * UnitNameMangle("*.service", ".service") → "*.service" (保持glob不变)
 */
std::string JournalUtils::UnitNameMangle(const std::string& name, const std::string& suffix) {
    // 不能为空或以点号开头
    if (name.empty() || name[0] == '.') {
        throw std::invalid_argument("unit name can't be empty or begin with a dot");
    }
    
    if (!UnitSuffixIsValid(suffix)) {
        throw std::invalid_argument("unit name has an invalid suffix");
    }
    
    // 已经是完全有效的单元名称？
    if (UnitNameIsValid(name)) {
        return name;
    }
    
    // 已经是完全有效的glob表达式？如果是，则无需处理...
    if (StringIsGlob(name) && InCharset(name, JournalConstants::kValidCharsGlob)) {
        return name;
    }
    
    // 如果是设备路径，转换为.device单元
    if (IsDevicePath(name)) {
        // 截取路径并在末尾添加.device
        std::filesystem::path p(name);
        return p.filename().string() + ".device";
    }
    
    // 如果是绝对路径，转换为.mount单元
    if (PathIsAbsolute(name)) {
        // 截取路径并在末尾添加.mount  
        std::filesystem::path p(name);
        return p.filename().string() + ".mount";
    }
    
    std::string escaped = DoEscapeMangle(name);
    
    // 如果没有后缀则添加后缀，但仅当这不是glob模式时，
    // 这样我们可以允许"foo.*"作为有效的glob模式。
    if (!StringIsGlob(escaped) && !absl::StrContains(escaped, '.')) {
        return escaped + suffix;
    }
    
    return escaped;
}

} // namespace logtail