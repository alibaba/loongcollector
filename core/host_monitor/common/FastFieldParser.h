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

#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace logtail {

/**
 * @brief 高性能字段解析器 - 零拷贝、按需解析
 */
class FastFieldParser {
public:
    explicit FastFieldParser(std::string_view line, char delimiter = ' ')
        : mLine(line), mDelimiter(delimiter), mCurrentPos(0) {}

    /**
     * @brief 跳转到指定字段索引
     * @param index 字段索引（从0开始）
     * @return 是否成功跳转
     */
    bool SeekToField(size_t index);

    /**
     * @brief 获取当前字段的 string_view
     * @return 字段的 string_view，失败返回空
     */
    std::string_view GetCurrentField();

    /**
     * @brief 直接获取指定索引的字段
     * @param index 字段索引
     * @return 字段的 string_view，失败返回空
     */
    std::string_view GetField(size_t index);

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
        return ParseNumber<T>(field, defaultValue);
    }

    /**
     * @brief 批量获取多个字段
     * @param startIndex 起始字段索引
     * @param count 字段数量
     * @return 字段 string_view 的 vector
     */
    std::vector<std::string_view> GetFields(size_t startIndex, size_t count);

    /**
     * @brief 检查字段是否以指定前缀开始
     * @param index 字段索引
     * @param prefix 前缀
     * @return 是否匹配
     */
    bool FieldStartsWith(size_t index, std::string_view prefix);

    /**
     * @brief 重置解析器到行首
     */
    void Reset() { mCurrentPos = 0; }

    /**
     * @brief 获取字段总数（需要完整遍历）
     */
    size_t GetFieldCount();

    /**
     * @brief 批量解析数值字段 - 通用优化方法
     * @tparam T 目标数值类型
     * @param startIndex 起始字段索引
     * @param count 字段数量
     * @param defaultValue 解析失败时的默认值
     * @return 解析结果的 vector
     */
    template <typename T>
    std::vector<T> GetFieldsAs(size_t startIndex, size_t count, T defaultValue = T{}) {
        auto fields = GetFields(startIndex, count);
        std::vector<T> result;
        result.reserve(fields.size());

        for (const auto& field : fields) {
            result.push_back(ParseNumber<T>(field, defaultValue));
        }

        return result;
    }

private:
    std::string_view mLine;
    char mDelimiter;
    size_t mCurrentPos;

    /**
     * @brief 跳过连续的分隔符
     */
    void SkipDelimiters();

    /**
     * @brief 查找下一个分隔符位置
     */
    size_t FindNextDelimiter(size_t start);

    /**
     * @brief 高性能数值解析
     */
    template <typename T>
    T ParseNumber(std::string_view field, T defaultValue) {
        if constexpr (std::is_integral_v<T>) {
            T result;
            auto [ptr, ec] = std::from_chars(field.data(), field.data() + field.size(), result);
            return (ec == std::errc{}) ? result : defaultValue;
        } else if constexpr (std::is_floating_point_v<T>) {
            // 对于浮点数，直接使用strtod以保证兼容性
            // std::from_chars对浮点数支持在一些编译器中不完整
            std::string temp(field);
            char* end;
            double value = std::strtod(temp.c_str(), &end);
            return (end != temp.c_str()) ? static_cast<T>(value) : defaultValue;
        }
        return defaultValue;
    }
};

/**
 * @brief CPU 统计专用解析器 - 针对 /proc/stat 优化
 */
class CpuStatParser {
public:
    explicit CpuStatParser(std::string_view line) : mParser(line) {}

    /**
     * @brief 检查是否为 CPU 行
     */
    bool IsCpuLine() { return mParser.FieldStartsWith(0, "cpu"); }

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
            auto [ptr, ec] = std::from_chars(numberPart.data(), numberPart.data() + numberPart.size(), result);
            return (ec == std::errc{}) ? result : -1;
        }
        return -1;
    }

    /**
     * @brief 批量获取 CPU 统计数值 - 性能优化版本
     * 使用一次遍历获取所有字段，避免重复查找
     */
    template <typename T>
    void
    GetCpuStats(T& user, T& nice, T& system, T& idle, T& iowait, T& irq, T& softirq, T& steal, T& guest, T& guestNice) {
        // 一次性获取并解析字段1-10，只遍历一次字符串 - 大幅性能提升
        auto stats = mParser.GetFieldsAs<T>(1, 10, T{});

        // 安全检查：确保有足够的字段
        if (stats.size() >= 10) {
            user = stats[0];
            nice = stats[1];
            system = stats[2];
            idle = stats[3];
            iowait = stats[4];
            irq = stats[5];
            softirq = stats[6];
            steal = stats[7];
            guest = stats[8];
            guestNice = stats[9];
        } else {
            // 降级到逐个解析，保证兼容性
            user = mParser.GetFieldAs<T>(1, T{});
            nice = mParser.GetFieldAs<T>(2, T{});
            system = mParser.GetFieldAs<T>(3, T{});
            idle = mParser.GetFieldAs<T>(4, T{});
            iowait = mParser.GetFieldAs<T>(5, T{});
            irq = mParser.GetFieldAs<T>(6, T{});
            softirq = mParser.GetFieldAs<T>(7, T{});
            steal = mParser.GetFieldAs<T>(8, T{});
            guest = mParser.GetFieldAs<T>(9, T{});
            guestNice = mParser.GetFieldAs<T>(10, T{});
        }
    }

private:
    FastFieldParser mParser;
};

/**
 * @brief 网络设备统计专用解析器
 */
class NetDevParser {
public:
    explicit NetDevParser(std::string_view line) : mLine(line) {}

    /**
     * @brief 解析设备名和统计数据
     */
    bool ParseDeviceStats(std::string_view& deviceName, std::vector<uint64_t>& stats);

private:
    std::string_view mLine;
};

/**
 * @brief 便利的单行解析函数
 */
namespace FastParse {
/**
 * @brief 快速获取指定字段
 */
std::string_view GetField(std::string_view line, size_t index, char delimiter = ' ');

/**
 * @brief 快速解析数值字段
 */
template <typename T>
T GetFieldAs(std::string_view line, size_t index, T defaultValue = T{}, char delimiter = ' ') {
    FastFieldParser parser(line, delimiter);
    return parser.GetFieldAs<T>(index, defaultValue);
}

/**
 * @brief 批量解析数值字段 - 性能优化版本
 * @tparam T 目标数值类型
 * @param line 输入行
 * @param startIndex 起始字段索引
 * @param count 字段数量
 * @param defaultValue 解析失败时的默认值
 * @param delimiter 分隔符
 * @return 解析结果的 vector
 */
template <typename T>
std::vector<T>
GetFieldsAs(std::string_view line, size_t startIndex, size_t count, T defaultValue = T{}, char delimiter = ' ') {
    FastFieldParser parser(line, delimiter);
    return parser.GetFieldsAs<T>(startIndex, count, defaultValue);
}

/**
 * @brief 检查字段前缀
 */
bool FieldStartsWith(std::string_view line, size_t index, std::string_view prefix, char delimiter = ' ');
} // namespace FastParse

} // namespace logtail
