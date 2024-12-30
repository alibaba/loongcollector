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

#include <string.h>

#include <boost/algorithm/string.hpp>
#include <boost/exception/all.hpp>
#include <boost/filesystem.hpp>

#include "logger/Logger.h"
#if defined(_MSC_VER)
#include <Shlwapi.h>
#else
#include <strings.h>
#endif
using namespace std;

namespace logtail {

std::string ToLowerCaseString(const std::string& orig) {
    auto copy = orig;
    std::transform(copy.begin(), copy.end(), copy.begin(), ::tolower);
    return copy;
}

std::string ToUpperCaseString(const std::string& orig) {
    auto copy = orig;
    std::transform(copy.begin(), copy.end(), copy.begin(), ::toupper);
    return copy;
}

int StringCaseInsensitiveCmp(const std::string& s1, const std::string& s2) {
#if defined(_MSC_VER)
    return _stricmp(s1.c_str(), s2.c_str());
#else
    return strcasecmp(s1.c_str(), s2.c_str());
#endif
}

int CStringNCaseInsensitiveCmp(const char* s1, const char* s2, size_t n) {
#if defined(_MSC_VER)
    return _strnicmp(s1, s2, n);
#else
    return strncasecmp(s1, s2, n);
#endif
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
    return tokens;
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
        exception.append("regex_error: ");
        exception.append(ToString(e.what()));
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
                     size_t length,
                     const boost::regex& reg,
                     string& exception,
                     boost::match_results<const char*>& what,
                     boost::match_flag_type flags) {
    try {
        if (boost::regex_match(buffer, buffer + length, what, reg, flags))
            return true;
        else
            return false;
    } catch (boost::regex_error& e) {
        exception.append("regex_error: ");
        exception.append(ToString(e.what()));
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

bool BoostRegexMatch(const char* buffer, size_t size, const boost::regex& reg, string& exception) {
    try {
        if (boost::regex_match(buffer, buffer + size, reg))
            return true;
        else
            return false;
    } catch (boost::regex_error& e) {
        exception.append("regex_error: ");
        exception.append(ToString(e.what()));
        exception.append("; buffer: ");
        exception.append(buffer);
        return false;
    } catch (std::exception& e) {
        exception.append("exception message: ");
        exception.append(e.what());
        exception.append("; buffer ");
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
        exception.append("regex_error: ");
        exception.append(ToString(e.what()));
        exception.append("; buffer is ");
        exception.append(buffer);
        return false;
    } catch (std::exception& e) {
        exception.append("exception message: ");
        exception.append(e.what());
        exception.append("; buffer ");
        exception.append(buffer);
        return false;
    } catch (...) {
        exception.append("unknown exception; buffer: ");
        exception.append(buffer);
        return false;
    }
}

bool BoostRegexSearch(const char* buffer, size_t size, const boost::regex& reg, string& exception) {
    try {
        boost::match_results<const char*> what;
        if (boost::regex_search(buffer, buffer + size, what, reg, boost::match_continuous)) {
            return true;
        } else {
            return false;
        }
    } catch (boost::regex_error& e) {
        exception.append("regex_error: ");
        exception.append(ToString(e.what()));
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

bool BoostRegexSearch(const char* buffer, const boost::regex& reg, string& exception) {
    try {
        boost::match_results<const char*> what;
        if (boost::regex_search(buffer, what, reg, boost::match_continuous)) {
            return true;
        } else {
            return false;
        }
    } catch (boost::regex_error& e) {
        exception.append("regex_error: ");
        exception.append(ToString(e.what()));
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

uint32_t GetLittelEndianValue32(const uint8_t* buffer) {
    return buffer[3] << 24 | buffer[2] << 16 | buffer[1] << 8 | buffer[0];
}

std::vector<std::string> GetTopicNames(const boost::regex& regex) {
    static boost::regex topicNameRegex(R"(^\?<([^>]+)>)");
    // static boost::regex topicNameRegex(R".(.+).");
    std::vector<std::string> result;
    try {
        auto subExpCount = regex.mark_count();
        for (size_t i = 0; i < subExpCount; ++i) {
            auto subExp = regex.subexpression(i);
            boost::match_results<const char*> what;
            if (boost::regex_search(subExp.first + 1, subExp.second, what, topicNameRegex)) {
                result.emplace_back(what[1].str());
            } else {
                result.emplace_back(std::string("__topic_") + ToString(i + 1) + "__");
            }
        }
    } catch (boost::regex_error& e) {
        LOG_ERROR(sLogger, ("get topic name error", e.what())("topic format", regex.str()));
    } catch (std::exception& e) {
        LOG_ERROR(sLogger, ("get topic name error", e.what())("topic format", regex.str()));
    } catch (...) {
        LOG_ERROR(sLogger, ("get topic name error", "unknown")("topic format", regex.str()));
    }
    return result;
}

bool ExtractTopics(const std::string& val,
                   const std::string& topicFormat,
                   std::vector<std::string>& keys,
                   std::vector<std::string>& values) {
    static boost::regex groupNameRegex("\\?P?<[^>]+>");
    try {
        boost::regex regex(topicFormat,
                           boost::regex::save_subexpression_location); // TODO: do not need to compile every time
        keys = GetTopicNames(regex);
        boost::smatch what;
        if (boost::regex_match(val, what, regex)) {
            for (size_t i = 1; i < what.size(); ++i) {
                values.push_back(what[i]);
            }
        } else {
            return false;
        }
        if (keys.size() < values.size()) {
            for (size_t i = keys.size(); i < values.size(); ++i) {
                keys.push_back(std::string("__topic_") + ToString(i + 1) + "__");
            }
        }
    } catch (boost::regex_error& e) {
        LOG_ERROR(sLogger, ("get extract topic error", e.what())("topic format", topicFormat)("val", val));
    } catch (std::exception& e) {
        LOG_ERROR(sLogger, ("get extract topic error", e.what())("topic format", topicFormat)("val", val));
        return false;
    } catch (...) {
        LOG_ERROR(sLogger, ("get extract topic error", "unknown")("topic format", topicFormat)("val", val));
        return false;
    }
    return true;
}

bool NormalizeTopicRegFormat(std::string& topicFormat) {
    static boost::regex groupNameRegex(R"(\?P<([^>]+)>)");
    try {
        topicFormat = boost::regex_replace(topicFormat, groupNameRegex, R"(\?<$1>)");
        boost::regex reg(topicFormat);
        return true;
    } catch (...) {
        return false;
    }
    return true;
}

bool isRootDirectory(const std::string& pathStr) {
    boost::filesystem::path path(pathStr);
    return path.has_root_directory() && !path.has_parent_path();
}

void RemoveFilePathTrailingSlash(std::string& filePath) {
    if (isRootDirectory(filePath)) {
        return;
    }
    boost::filesystem::path path(filePath);
    path.remove_trailing_separator();
    filePath = path.string();
}

bool IsInt(const char* sz) {
    bool ok = (sz != nullptr && *sz != '\0');
    for (auto* it = sz; ok && *it; ++it) {
        ok = (0 != std::isdigit(*it));
    }
    return ok;
}

} // namespace logtail
