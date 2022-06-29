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

#ifndef __OAS_SHENNONG_REGEX_FILTER_VALUE_NODE_H__
#define __OAS_SHENNONG_REGEX_FILTER_VALUE_NODE_H__

#include <boost/regex.hpp>
#include "BaseFilterNode.h"
#include "common/util.h"
#include "app_config/AppConfig.h"
#include "profiler/LogtailAlarm.h"
#include "logger/Logger.h"

namespace logtail {

class RegexFilterValueNode : public BaseFilterNode {
public:
    RegexFilterValueNode(const std::string& key, const std::string& exp)
        : BaseFilterNode(VALUE_NODE), key(key), reg(exp) {}

    virtual ~RegexFilterValueNode() {}

public:
    virtual bool Match(const sls_logs::Log& log, const LogGroupContext& context) {
        for (int i = 0; i < log.contents_size(); ++i) {
            const sls_logs::Log_Content& content = log.contents(i);
            if (content.key() != key) {
                continue;
            }

            std::string exception;
            bool result = BoostRegexMatch(content.value().c_str(), reg, exception);
            if (!result && !exception.empty() && AppConfig::GetInstance()->IsLogParseAlarmValid()) {
                LOG_ERROR(sLogger, ("regex_match in Filter fail", exception));
                if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
                    context.SendAlarm(REGEX_MATCH_ALARM, "regex_match in Filter fail:" + exception);
                }
            }
            return result;
        }
        return false;
    }

private:
    std::string key;
    boost::regex reg;
};

} // namespace logtail

#endif //__OAS_SHENNONG_REGEX_FILTER_VALUE_NODE_H__
