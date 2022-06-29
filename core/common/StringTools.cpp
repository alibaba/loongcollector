// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "StringTools.h"
#include <boost/algorithm/string.hpp>
#include <boost/xpressive/xpressive.hpp>
#include "logger/Logger.h"
#if defined(_MSC_VER)
#include <Shlwapi.h>
#endif
using namespace std;

namespace logtail {

std::string ToLowerCaseString(const std::string& orig) {
    auto copy = orig;
    std::transform(copy.begin(), copy.end(), copy.begin(), ::tolower);
    return copy;
}

std::string ToString(const std::vector<std::string>& vec) {
    if (vec.empty())
        return "";

    auto iter = vec.begin();
    std::string ret = *iter++;
    for (; iter != vec.end(); ++iter) {
        ret += "," + *iter;
    }
    return ret;
}

template <>
bool StringTo<bool>(const std::string& str) {
    return str == "true";
}

std::vector<std::string> SplitString(const std::string& str, const std::string& delim) {
    std::vector<std::string> tokens;
    boost::split(tokens, str, boost::is_any_of(delim));
    return tokens;
}

std::vector<std::string> StringSpliter(const std::string& str, const std::string& delim) {
    std::vector<std::string> tokens;
    size_t prev = 0, pos = 0;
    do {
        pos = str.find(delim, prev);
        if (pos == std::string::npos)
            pos = str.length();
        auto token = str.substr(prev, pos - prev);
        if (!token.empty())
            tokens.push_back(token);
        prev = pos + delim.length();
    } while (pos < str.length() && prev < str.length());
    return std::move(tokens);
}

void ReplaceString(std::string& raw, const std::string& src, const std::string& dst) {
    std::string::size_type pos = 0u;
    while ((pos = raw.find(src, pos)) != std::string::npos) {
        raw.replace(pos, src.length(), dst);
        pos += dst.length();
    }
}

#if defined(_MSC_VER)
// fnmatch on Windows only supports *, does not support bracket (such as *.[Ll][Oo][Gg]).
int fnmatch(const char* pattern, const char* path, int flag) {
    return PathMatchSpec(path, pattern) ? 0 : 1;
}
#endif

bool BoostRegexSearch(const char* buffer,
                      const boost::regex& reg,
                      std::string& exception,
                      boost::match_results<const char*>& what,
                      boost::match_flag_type flags) {
    try {
        if (boost::regex_search(buffer, what, reg, flags))
            return true;
        else
            return false;
    } catch (boost::regex_error& e) {
        exception.append("regex_error code: ");
        exception.append(ToString(e.code()));
        exception.append("; buffer: ");
        exception.append(buffer);
        return false;
    } catch (std::exception& e) {
        exception.append("exception message: ");
        exception.append(e.what());
        exception.append("; buffer: ");
        exception.append(buffer);
        return false;
    } catch (...) {
        exception.append("unknown exception; buffer: ");
        exception.append(buffer);
        return false;
    }
}

bool BoostRegexMatch(const char* buffer,
                     const boost::regex& reg,
                     string& exception,
                     boost::match_results<const char*>& what,
                     boost::match_flag_type flags) {
    try {
        if (boost::regex_match(buffer, what, reg, flags))
            return true;
        else
            return false;
    } catch (boost::regex_error& e) {
        exception.append("regex_error code: ");
        exception.append(ToString(e.code()));
        exception.append("; buffer: ");
        exception.append(buffer);
        return false;
    } catch (std::exception& e) {
        exception.append("exception message: ");
        exception.append(e.what());
        exception.append("; buffer: ");
        exception.append(buffer);
        return false;
    } catch (...) {
        exception.append("unknown exception; buffer: ");
        exception.append(buffer);
        return false;
    }
}

bool BoostRegexMatch(const char* buffer, const boost::regex& reg, string& exception) {
    try {
        if (boost::regex_match(buffer, reg))
            return true;
        else
            return false;
    } catch (boost::regex_error& e) {
        exception.append("regex_error code is ");
        exception.append(ToString(e.code()));
        exception.append("; buffer is ");
        exception.append(buffer);
        return false;
    } catch (std::exception& e) {
        exception.append("exception message is ");
        exception.append(e.what());
        exception.append("; buffer is ");
        exception.append(buffer);
        return false;
    } catch (...) {
        exception.append("unknown exception; buffer is ");
        exception.append(buffer);
        return false;
    }
}

uint32_t GetLittelEndianValue32(const uint8_t* buffer) {
    return buffer[3] << 24 | buffer[2] << 16 | buffer[1] << 8 | buffer[0];
}

std::vector<std::string> GetTopicNames(const std::string& topicFormat) {
    std::vector<std::string> result;
    try {
        boost::regex regex("\\?P<([^>]+)>");
        boost::sregex_token_iterator iter(topicFormat.begin(), topicFormat.end(), regex, 0);
        boost::sregex_token_iterator end;

        for (; iter != end; ++iter) {
            std::string val = *iter;
            if (val.size() > (size_t)4) {
                result.push_back(val.substr(3, val.size() - 4));
            }
        }
    } catch (...) {
        LOG_ERROR(sLogger, ("get topic name error", topicFormat));
    }
    return result;
}

bool ExtractTopics(const std::string& val,
                   const std::string& topicFormat,
                   std::vector<std::string>& keys,
                   std::vector<std::string>& values) {
    try {
        boost::xpressive::sregex regex = boost::xpressive::sregex::compile(topicFormat);
        boost::xpressive::smatch what;

        if (boost::xpressive::regex_match(val, what, regex)) {
            for (size_t i = 1; i < what.size(); ++i) {
                values.push_back(what[i]);
            }
        } else {
            return false;
        }
        keys = GetTopicNames(topicFormat);
        if (keys.size() < values.size()) {
            for (size_t i = keys.size(); i < values.size(); ++i) {
                keys.push_back(std::string("__topic_") + ToString(i + 1) + "__");
            }
        }
    } catch (...) {
        LOG_ERROR(sLogger, ("get topic name error", topicFormat));
        return false;
    }
    return true;
}

bool CheckTopicRegFormat(const std::string& regStr) {
    try {
        boost::regex reg(regStr);
        return true;
    } catch (...) {
    }

    try {
        boost::xpressive::sregex::compile(regStr);
    } catch (...) {
        return false;
    }
    return true;
}

} // namespace logtail
