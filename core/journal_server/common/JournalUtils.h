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

namespace logtail {

/**
 * @brief Journal相关的工具函数集合
 * 
 * 提供字符串处理、路径处理和systemd单元名称处理等通用工具函数
 * 这些函数基于Go版本实现，确保与现有逻辑保持一致
 */
class JournalUtils {
public:
    // ============================================================================
    // 字符串和路径工具函数
    // ============================================================================
    
    /**
     * @brief 检查字符串是否包含glob模式字符（*、?、[）
     * @param name 待检查的字符串
     * @return 包含glob字符返回true，否则返回false
     */
    static bool StringIsGlob(const std::string& name);
    
    /**
     * @brief 检查字符串中的所有字符是否都在指定的字符集中
     * @param s 待检查的字符串
     * @param charset 字符集
     * @return 所有字符都在字符集中返回true，否则返回false
     */
    static bool InCharset(const std::string& s, const std::string& charset);
    
    /**
     * @brief 检查路径是否是设备路径（以/dev/或/sys/开头）
     * @param path 待检查的路径
     * @return 是设备路径返回true，否则返回false
     */
    static bool IsDevicePath(const std::string& path);
    
    /**
     * @brief 检查路径是否是绝对路径（以/开头）
     * @param path 待检查的路径
     * @return 是绝对路径返回true，否则返回false
     */
    static bool PathIsAbsolute(const std::string& path);
    
    /**
     * @brief 使用glob模式匹配字符串
     * @param pattern glob模式字符串（支持*、?、[]）
     * @param string 待匹配的字符串
     * @return 匹配成功返回true，否则返回false
     */
    static bool MatchPattern(const std::string& pattern, const std::string& string);

    // ============================================================================
    // Systemd单元相关工具函数
    // ============================================================================
    
    /**
     * @brief 检查单元后缀是否有效（必须以.开头且在有效类型列表中）
     * @param suffix 单元后缀（如".service"、".socket"等）
     * @return 有效返回true，否则返回false
     */
    static bool UnitSuffixIsValid(const std::string& suffix);
    
    /**
     * @brief 检查单元名称是否有效
     * @param name systemd单元名称
     * @return 有效返回true，否则返回false
     */
    static bool UnitNameIsValid(const std::string& name);
    
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
    static std::string DoEscapeMangle(const std::string& name);
    
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
    static std::string UnitNameMangle(const std::string& name, const std::string& suffix);

    // Delete copy/move operations for utility class
    JournalUtils(const JournalUtils&) = delete;
    JournalUtils& operator=(const JournalUtils&) = delete;
    JournalUtils(JournalUtils&&) = delete;
    JournalUtils& operator=(JournalUtils&&) = delete;

private:
    JournalUtils() = default;
    ~JournalUtils() = default;
};

} // namespace logtail 