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
#include <string>
#include <json/json.h>
#include "processor/BaseFilterNode.h"

namespace logtail {

class Config;

// UserLogConfigParser offers utility to help ConfigManager parse content in user_log_config.json.
class UserLogConfigParser {
public:
    // ParseAdvancedConfig parses all advanced configs.
    // @throw If parse failed.
    static void ParseAdvancedConfig(const Json::Value& val, Config& cfg);

private:
    // ParseBlacklist parses blacklist configuration from @advancedVal, and assign them into @cfg.
    // @return if everything is ok, empty is returned, otherwise, returns exception string.
    static std::string ParseBlacklist(const Json::Value& advancedVal, Config& cfg);

    static BaseFilterNodePtr ParseExpressionFromJSON(const Json::Value& value);
    static bool GetOperatorType(const std::string& type, FilterOperator& op);
    static bool GetNodeFuncType(const std::string& type, FilterNodeFunctionType& func);

#if defined(APSARA_UNIT_TEST_MAIN)
    friend class LogFilterUnittest;
#endif
};

} // namespace logtail
