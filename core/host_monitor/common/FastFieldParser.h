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

#include <charconv>
#include <cstdint>

#include <string>
#include <type_traits>
#include <vector>

#include "common/StringTools.h"

namespace logtail {

/**
 * @brief 高性能字段解析器 - 零拷贝、按需解析
 */
class FastFieldParser {
public:
    explicit FastFieldParser(StringView line, char delimiter = ' ')
        : mLine(line),
          mDelimiterChar(delimiter),
          mDelimiter(StringView(&mDelimiterChar, 1)),
          mSplitter(mLine, mDelimiter, true) {}

    /**
     * @brief 直接获取指定索引的字段
     * @param index 字段索引
     * @return 字段的 StringView，失败返回空
     */
    StringView GetField(size_t index);

    /**
     * @brief 直接解析指定字段为数值类型
     * @tparam T 目标数值类型
     * @param index 字段索引
     * @param defaultValue 解析失败时的默认值
     * @return 解析结果
     */
    template <typename T>
    T GetFieldAs(size_t index, T defaultValue = T{}) {
        auto field = GetField(index);
        if (field.empty()) {
            return defaultValue;
        }
        T result;
        return StringTo(field, result) ? result : defaultValue;
    }

    /**
     * @brief 获取字段迭代器开始位置
     * @return StringViewSplitterIterator
     */
    StringViewSplitterIterator begin() const { return mSplitter.begin(); }

    /**
     * @brief 获取字段迭代器结束位置
     * @return StringViewSplitterIterator
     */
    StringViewSplitterIterator end() const { return mSplitter.end(); }

    /**
     * @brief 检查字段是否以指定前缀开始
     * @param index 字段索引
     * @param prefix 前缀
     * @return 是否匹配
     */
    bool FieldStartsWith(size_t index, StringView prefix);


    /**
     * @brief 获取字段总数（需要完整遍历）
     */
    size_t GetFieldCount();

private:
    StringView mLine;
    char mDelimiterChar;
    StringView mDelimiter;
    StringViewSplitter mSplitter;
};

/**
 * @brief CPU 统计专用解析器 - 针对 /proc/stat 优化
 */
class CpuStatParser {
public:
    explicit CpuStatParser(StringView line) : mParser(line) {}

    /**
     * @brief 检查是否为 CPU 行
     */
    bool IsCpuLine() {
        auto field = mParser.GetField(0);
        if (field.size() < 3 || field.substr(0, 3) != "cpu") {
            return false;
        }

        // Check if it's exactly "cpu" (total CPU stats)
        if (field == "cpu") {
            return true;
        }

        // Check if remaining characters after "cpu" are all digits
        int index;
        auto numberPart = field.substr(3);
        if (numberPart.empty()) {
            return false;
        }
        if (!StringTo(numberPart, index)) {
            return false;
        }
        return true;
    }

    /**
     * @brief 获取 CPU 索引（-1 表示总体 CPU）
     */
    int GetCpuIndex() {
        auto field = mParser.GetField(0);
        if (field == "cpu") {
            return -1;
        }
        if (field.size() > 3 && field.substr(0, 3) == "cpu") {
            // 解析 cpu 后面的数字部分
            auto numberPart = field.substr(3);
            if (numberPart.empty()) {
                return -1;
            }

            int result;
            if (!StringTo(numberPart, result)) {
                return -2;
            }
            return result;
        }
        return -2;
    }

    /**
     * @brief 批量获取 CPU 统计数值 - 性能优化版本
     * 使用一次遍历获取所有字段，避免重复查找
     */
    template <typename T>
    void
    GetCpuStats(T& user, T& nice, T& system, T& idle, T& iowait, T& irq, T& softirq, T& steal, T& guest, T& guestNice) {
        // 使用迭代器遍历字段1-10
        auto iter = mParser.begin();
        auto end = mParser.end();

        // 跳过字段0 (cpu名称)
        if (iter != end)
            ++iter;

        // 按顺序读取10个统计字段
        auto parseField = [](StringViewSplitterIterator& it, StringViewSplitterIterator& end) -> T {
            if (it != end) {
                T result;
                return StringTo(*it++, result) ? result : T{};
            }
            return T{};
        };

        user = parseField(iter, end);
        nice = parseField(iter, end);
        system = parseField(iter, end);
        idle = parseField(iter, end);
        iowait = parseField(iter, end);
        irq = parseField(iter, end);
        softirq = parseField(iter, end);
        steal = parseField(iter, end);
        guest = parseField(iter, end);
        guestNice = parseField(iter, end);
    }

private:
    FastFieldParser mParser;
};

/**
 * @brief 网络设备统计专用解析器
 */
class NetDevParser {
public:
    explicit NetDevParser(StringView line) : mLine(line) {}

    /**
     * @brief 解析设备名和统计数据
     */
    bool ParseDeviceStats(StringView& deviceName, std::vector<uint64_t>& stats);

private:
    StringView mLine;
};

/**
 * @brief 便利的单行解析函数
 */
namespace FastParse {
/**
 * @brief 快速获取指定字段
 */
StringView GetField(StringView line, size_t index, char delimiter = ' ');

/**
 * @brief 快速解析数值字段
 */
template <typename T>
T GetFieldAs(StringView line, size_t index, T defaultValue = T{}, char delimiter = ' ') {
    FastFieldParser parser(line, delimiter);
    return parser.GetFieldAs<T>(index, defaultValue);
}

/**
 * @brief 检查字段前缀
 */
bool FieldStartsWith(StringView line, size_t index, StringView prefix, char delimiter = ' ');
} // namespace FastParse

} // namespace logtail
