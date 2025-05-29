/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <string>
#include <vector>
#include <memory>

#include "json/json.h"

namespace logtail::apm {

enum APMLanguage {
    kJava,
    kPython,
    kGolang,
};

enum Operation {
    kEq,
};

enum RuleType {
    kCwd,
};

struct MatchRule {
    RuleType mRuleType;
    Operation mOperation;
    std::string mVal;
};

struct AttachConfig {
    APMLanguage mLanguage;
    std::string mCommandId;
    std::string mLicenseKey;
    std::string mAppName;
    std::string mAppId;
    std::string mAgentVersion;
    std::string mServiceId;
    std::string mWorkspace;
    std::vector<MatchRule> mMatchRules;
};

bool InitObserverNetworkOption(const Json::Value& config, std::unique_ptr<AttachConfig>& attachConfig);

}
