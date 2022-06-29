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
#include <vector>
#include <string>
#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>

namespace logtail {

inline bool StartWith(const std::string& input, const std::string& pattern) {
    return input.find(pattern) == 0;
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
    return std::string(str);
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

template <typename T>
T StringTo(const std::string& str) {
    return boost::lexical_cast<T>(str);
}

// @return true if str is equal to "true", otherwise false.
template <>
bool StringTo<bool>(const std::string& str);

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
                     const boost::regex& reg,
                     std::string& exception,
                     boost::match_results<const char*>& what,
                     boost::match_flag_type flags = boost::match_default);
bool BoostRegexMatch(const char* buffer, const boost::regex& reg, std::string& exception);

// GetLittelEndianValue32 converts @buffer in little endian to uint32_t.
uint32_t GetLittelEndianValue32(const uint8_t* buffer);

bool ExtractTopics(const std::string& val,
                   const std::string& topicFormat,
                   std::vector<std::string>& keys,
                   std::vector<std::string>& values);

bool CheckTopicRegFormat(const std::string& regStr);

#if defined(_MSC_VER)
// TODO: Test it.
#define FNM_PATHNAME 0
int fnmatch(const char* pattern, const char* dirPath, int flag);
#endif

} // namespace logtail
