/*
 * Copyright 2022 iLogtail Authors
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

#include <algorithm>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#include "boost/lexical_cast.hpp"
#pragma GCC diagnostic pop
#include <charconv>

#include <string>
#include <vector>

#include "boost/regex.hpp"

#include "common/StringView.h"

namespace logtail {

inline bool StartWith(const std::string& input, StringView pattern) {
    return input.find(pattern.data(), 0, pattern.size()) == 0;
}

inline bool EndWith(const std::string& input, const std::string& pattern) {
    auto inputLen = input.length();
    auto patternLen = pattern.length();
    if (patternLen > inputLen) {
        return false;
    }

    auto pos = input.rfind(pattern);
    return pos != std::string::npos && (pos == inputLen - patternLen);
}

std::string ToLowerCaseString(const std::string& orig);
std::string ToUpperCaseString(const std::string& orig);

int StringCaseInsensitiveCmp(const std::string& s1, const std::string& s2);
int CStringNCaseInsensitiveCmp(const char* s1, const char* s2, size_t n);

inline std::string LeftTrimString(const std::string& str, const char trimChar = ' ') {
    auto s = str;
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [trimChar](int ch) { return trimChar != ch; }));
    return s;
}
inline std::string RightTrimString(const std::string& str, const char trimChar = ' ') {
    auto s = str;
    s.erase(std::find_if(s.rbegin(), s.rend(), [trimChar](int ch) { return trimChar != ch; }).base(), s.end());
    return s;
}
inline std::string TrimString(const std::string& str, const char leftTrimChar = ' ', const char rightTrimChar = ' ') {
    return RightTrimString(LeftTrimString(str, leftTrimChar), rightTrimChar);
}

template <typename T>
inline std::string ToString(const T& value) {
    return std::to_string(value);
}
inline std::string ToString(const std::string& str) {
    return str;
}
inline std::string ToString(const char* str) {
    if (str == nullptr) {
        return "";
    }
    return std::string(str);
}
inline std::string ToString(char* str) {
    return ToString(const_cast<const char*>(str));
}
inline std::string ToString(bool value) {
    return value ? "true" : "false";
}
std::string ToString(const std::vector<std::string>& vec);

template <typename T>
std::string ToHexString(const T& value) {
    uint32_t size = sizeof(T) * 8;
    T valueCopy = value;
    std::string str;
    do {
        uint8_t n = valueCopy & 0x0f;
        char c = static_cast<char>(n < 10 ? ('0' + n) : ('A' + n - 10));
        str.insert(str.begin(), c);
    } while ((valueCopy >>= 4) && (size -= 4));
    return str;
}
template <>
std::string ToHexString(const std::string& value);

// Split string by delimiter.
std::vector<std::string> SplitString(const std::string& str, const std::string& delim = " ");

// This method's behaviors is not like SplitString(string, string),
// The difference is below method use the whole delim as a separator,
// and will scan the target str from begin to end and we drop "".
// @Return: vector of substring split by delim, without ""
std::vector<std::string> StringSpliter(const std::string& str, const std::string& delim);

// Replaces all @src in @raw to @dst.
void ReplaceString(std::string& raw, const std::string& src, const std::string& dst);

// Boost regex utility.
bool BoostRegexSearch(const char* buffer,
                      const boost::regex& reg,
                      std::string& exception,
                      boost::match_results<const char*>& what,
                      boost::match_flag_type flags = boost::match_default);
bool BoostRegexMatch(const char* buffer,
                     size_t length,
                     const boost::regex& reg,
                     std::string& exception,
                     boost::match_results<const char*>& what,
                     boost::match_flag_type flags = boost::match_default);
bool BoostRegexMatch(const char* buffer, size_t size, const boost::regex& reg, std::string& exception);
bool BoostRegexMatch(const char* buffer, const boost::regex& reg, std::string& exception);
bool BoostRegexSearch(const char* buffer, size_t size, const boost::regex& reg, std::string& exception);
bool BoostRegexSearch(const char* buffer, const boost::regex& reg, std::string& exception);

// GetLittelEndianValue32 converts @buffer in little endian to uint32_t.
uint32_t GetLittelEndianValue32(const uint8_t* buffer);

bool ExtractTopics(const std::string& val,
                   const std::string& topicFormat,
                   std::vector<std::string>& keys,
                   std::vector<std::string>& values);

bool NormalizeTopicRegFormat(std::string& regStr);

void RemoveFilePathTrailingSlash(std::string& path);

bool IsInt(const char* sz);

inline bool IsInt(const std::string& str) {
    return IsInt(str.c_str());
}

#if defined(_MSC_VER)
// TODO: Test it.
#define FNM_PATHNAME 0
int fnmatch(const char* pattern, const char* dirPath, int flag);
#endif

// trim from start (returns a new string_view)
static inline StringView Ltrim(StringView s, const StringView blank = " \t\n\r\f\v") {
    s.remove_prefix(std::min(s.find_first_not_of(blank), s.size()));
    return s;
}

// trim from end (returns a new string_view)
static inline StringView Rtrim(StringView s, const StringView blank = " \t\n\r\f\v") {
    s.remove_suffix(std::min(s.size() - s.find_last_not_of(blank) - 1, s.size()));
    return s;
}

// trim from both ends (returns a new string_view)
static inline StringView Trim(StringView s) {
    return Ltrim(Rtrim(s));
}

static constexpr StringView kNullSv("\0", 1);

class StringViewSplitterIterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = StringView;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    StringViewSplitterIterator() = default;

    StringViewSplitterIterator(StringView str, StringView delimiter) : mStr(str), mDelimiter(delimiter), mPos(0) {
        findNext();
    }

    value_type operator*() { return mField; }

    pointer operator->() { return &mField; }

    StringViewSplitterIterator& operator++() {
        findNext();
        return *this;
    }

    StringViewSplitterIterator operator++(int) {
        StringViewSplitterIterator tmp = *this;
        ++(*this);
        return tmp;
    }

    friend bool operator==(const StringViewSplitterIterator& a, const StringViewSplitterIterator& b) {
        return a.mPos == b.mPos;
    }

    friend bool operator!=(const StringViewSplitterIterator& a, const StringViewSplitterIterator& b) {
        return !(a == b);
    }

private:
    void findNext() {
        if (mPos == StringView::npos) {
            mField = {};
            return;
        }

        size_t end = 0;
        if (mDelimiter.empty()) {
            end = mPos + 1;
        } else {
            end = mStr.find(mDelimiter, mPos);
        }
        if (end == StringView::npos) {
            if (mPos <= mStr.size()) { // last field
                mField = mStr.substr(mPos);
                mPos = mStr.size() + 1;
            } else { // equivalent to end
                mField = {};
                mPos = StringView::npos;
            }
        } else {
            mField = mStr.substr(mPos, end - mPos);
            mPos = end + mDelimiter.size();
        }
    }

    StringView mStr;
    StringView mDelimiter;
    StringView mField;
    size_t mPos = StringView::npos;
};

class StringViewSplitter {
public:
    using value_type = StringView;
    using iterator = StringViewSplitterIterator;

    StringViewSplitter(StringView str, StringView delimiter) : mStr(str), mDelimiter(delimiter) {}

    iterator begin() const { return iterator(mStr, mDelimiter); }

    iterator end() const { return iterator(); }

private:
    StringView mStr;
    StringView mDelimiter;
};

template <class T>
bool StringTo(const char* first, const char* last, T& val, int base = 10) {
    if (first == nullptr || first >= last) {
        return false; // 空字符串，转换失败
    }

    auto convresult = std::from_chars(first, last, val, base);
    if (convresult.ec != std::errc() || convresult.ptr != last) {
        return false;
    }
    return true;
}

template <>
inline bool StringTo<double>(const char* first, const char* last, double& val, [[maybe_unused]] int base) {
    if (first == nullptr || first >= last) {
        return false; // 空字符串，转换失败
    }

    // 重置 errno 以检测转换错误
    errno = 0;
    char* end = nullptr;
    val = std::strtod(first, &end);

    // 检查转换是否成功
    if (end != last) {
        return false; // 没有完全转换所有字符
    }

    if (errno == ERANGE) {
        return false; // 超出范围
    }
    return true;
}

template <>
inline bool StringTo<float>(const char* first, const char* last, float& val, [[maybe_unused]] int base) {
    double result{};
    if (!StringTo(first, last, result)) {
        return false;
    }
    // 检查结果是否在 float 的范围内
    if (result > std::numeric_limits<float>::max() || result < std::numeric_limits<float>::lowest()) {
        return false; // 超出 float 范围
    }

    val = static_cast<float>(result);
    return true;
}

template <>
inline bool StringTo<bool>(const char* first, const char* last, bool& val, [[maybe_unused]] int base) {
    // 先检查长度是否为4
    if (first == nullptr || last - first != 4) {
        val = false;
    } else { // 直接比较每个字符（忽略大小写）
        val = (std::tolower(static_cast<unsigned char>(first[0])) == 't'
               && std::tolower(static_cast<unsigned char>(first[1])) == 'r'
               && std::tolower(static_cast<unsigned char>(first[2])) == 'u'
               && std::tolower(static_cast<unsigned char>(first[3])) == 'e');
    }
    return true;
}

template <>
inline bool StringTo<std::string>(const char* first, const char* last, std::string& val, [[maybe_unused]] int base) {
    if (first == nullptr || first >= last) {
        return false; // 空字符串，转换失败
    }
    val.assign(first, last);
    return true;
}

template <class T>
bool StringTo(const char* str, T& val, int base = 10) {
    if (!str) {
        return false;
    }
    return StringTo(str, str + strlen(str), val, base);
}

template <class T>
bool StringTo(const std::string& str, T& val, int base = 10) {
    return StringTo(str.data(), str.data() + str.size(), val, base);
}

template <class T>
bool StringTo(const std::string_view& str, T& val, int base = 10) {
    return StringTo(str.data(), str.data() + str.size(), val, base);
}

template <class T>
bool StringTo(const StringView& str, T& val, int base = 10) {
    return StringTo(str.data(), str.data() + str.size(), val, base);
}

} // namespace logtail
