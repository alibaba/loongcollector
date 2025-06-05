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
#include "task_pipeline/TaskPipelineContext.h"

namespace logtail::apm {

enum class APMLanguage {
    kJava,
    kPython,
    kGolang,
    kMax
};

enum class RuleOperation {
    kEq,
    kMax,
};

enum class RuleType {
    kCwd,
    kMax,
};

struct MatchRule {
    RuleType mRuleType;
    RuleOperation mOperation;
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

bool InitApmAttachOption(const TaskPipelineContext* ctx, const Json::Value& config, std::unique_ptr<AttachConfig>& attachConfig, const std::string& inputType);

}
