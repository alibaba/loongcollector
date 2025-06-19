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

#include <memory>
#include <string>
#include <vector>

#include "json/json.h"

#include "task_pipeline/TaskPipelineContext.h"

namespace logtail::apm {

enum class APMLanguage { kJava, kPython, kGolang, kMax };

enum class RuleOperation {
    kEq,
    kMax,
};

enum class RuleType {
    kCwd,
    kMax,
};

enum class CommandType {
    kInstall,
    kUpdate,
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
    CommandType mCommandType;
    std::string mLicenseKey;
    std::string mAppName;
    std::string mAppId;
    std::string mAgentVersion;
    std::string mServiceId;
    std::string mWorkspace;
    std::vector<MatchRule> mMatchRules;
};

enum class ApmAttachStatus {
    kInProgress,
    kSucceed,
    kFailed,
    kNeedRetry,
    kUnknown,
};

struct AttachContext {
    explicit AttachContext(std::shared_ptr<AttachConfig>& config) : mAttachConfig(config) {}
    std::shared_ptr<AttachConfig> mAttachConfig;
    ApmAttachStatus mStatus = ApmAttachStatus::kInProgress;
};

bool InitApmAttachOption(const TaskPipelineContext* ctx,
                         const Json::Value& config,
                         std::shared_ptr<AttachConfig>& attachConfig,
                         const std::string& inputType);

} // namespace logtail::apm
