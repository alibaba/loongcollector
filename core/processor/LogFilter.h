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
#include <unordered_map>
#include <boost/regex.hpp>
#include <vector>
#include <string>
#include "log_pb/sls_logs.pb.h"
#include "BaseFilterNode.h"

namespace logtail {

class Config;

struct LogFilterRule {
    std::vector<std::string> FilterKeys;
    std::vector<boost::regex> FilterRegs;
};

class LogFilter {
private:
    std::unordered_map<std::string, LogFilterRule*> mFilters;

    bool IsMatched(const sls_logs::Log& log, const LogFilterRule& rule, const LogGroupContext& context);

    static void CastOneSensitiveWord(sls_logs::Log_Content* pContent, const Config* pConfig);
    LogFilter() {}

    ~LogFilter() {
        std::unordered_map<std::string, LogFilterRule*>::iterator it = mFilters.begin();
        for (; it != mFilters.end(); ++it) {
            delete it->second;
        }
        mFilters.clear();
    }

public:
    static LogFilter* Instance() {
        static LogFilter sFilter;
        return &sFilter;
    }

    bool InitFilter(const std::string& configName);

    std::vector<int32_t>
    Filter(const std::string& projectName, const std::string& region, const sls_logs::LogGroup& logGroup);

    std::vector<int32_t>
    Filter(const sls_logs::LogGroup& logGroup, const LogFilterRule* filterRule, const LogGroupContext& context);

    std::vector<int32_t>
    Filter(const sls_logs::LogGroup& logGroup, BaseFilterNodePtr node, const LogGroupContext& context);

    static void CastSensitiveWords(sls_logs::LogGroup& logGroup, const Config* pConfig);

#ifdef APSARA_UNIT_TEST_MAIN
    friend class LogFilterUnittest;
#endif
};

// Replace non-UTF8 bytes in @str to ' '.
void FilterNoneUtf8(const std::string& str);

} // namespace logtail
