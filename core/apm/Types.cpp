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

#include "apm/Types.h"

#include "common/ParamExtractor.h"

namespace logtail::apm {

bool LanguageToEnum(std::string& language, APMLanguage& lang) {
    std::transform(language.begin(), language.end(), language.begin(), [](unsigned char c) { return std::toupper(c); });
    if (language == "JAVA") {
        lang = APMLanguage::kJava;
        return true;
    }
    if (language == "GOLANG") {
        lang = APMLanguage::kGolang;
        return true;
    }
    if (language == "PYTHON") {
        lang = APMLanguage::kPython;
        return true;
    }

    LOG_DEBUG(sLogger, ("unknown apm language", language));

    return false;
}

bool RuleTypeToEnum(std::string& ruleTypeStr, RuleType& ruleType) {
    std::transform(
        ruleTypeStr.begin(), ruleTypeStr.end(), ruleTypeStr.begin(), [](unsigned char c) { return std::toupper(c); });
    if (ruleTypeStr == "CWD") {
        ruleType = RuleType::kCwd;
        return true;
    }
    LOG_DEBUG(sLogger, ("unknown rule type", ruleTypeStr));
    ruleType = RuleType::kMax;
    return false;
}

bool RuleOperationToEnum(std::string& ruleOpStr, RuleOperation& ruleOp) {
    std::transform(
        ruleOpStr.begin(), ruleOpStr.end(), ruleOpStr.begin(), [](unsigned char c) { return std::toupper(c); });
    if (ruleOpStr == "EQ") {
        ruleOp = RuleOperation::kEq;
        return true;
    }
    LOG_DEBUG(sLogger, ("unknown rule operation", ruleOpStr));
    ruleOp = RuleOperation::kMax;
    return false;
}

bool InitApmAttachOption(const TaskPipelineContext* ctx,
                         const Json::Value& config,
                         std::unique_ptr<AttachConfig>& attachConfig,
                         const std::string& inputType) {
    std::string errorMsg;
    // AppId
    // Project
    if (!GetMandatoryStringParam(config, "AppId", attachConfig->mAppId, errorMsg)) {
        PARAM_ERROR_RETURN(ctx->GetLogger(), ctx->GetAlarm(), errorMsg, inputType, ctx->GetConfigName(), "", "", "");
    }

    // AppName
    if (!GetMandatoryStringParam(config, "AppName", attachConfig->mAppName, errorMsg)) {
        PARAM_ERROR_RETURN(ctx->GetLogger(), ctx->GetAlarm(), errorMsg, inputType, ctx->GetConfigName(), "", "", "");
    }

    // LicenseKey
    if (!GetMandatoryStringParam(config, "LicenseKey", attachConfig->mLicenseKey, errorMsg)) {
        PARAM_ERROR_RETURN(ctx->GetLogger(), ctx->GetAlarm(), errorMsg, inputType, ctx->GetConfigName(), "", "", "");
    }
    // ServiceId
    if (!GetMandatoryStringParam(config, "ServiceId", attachConfig->mServiceId, errorMsg)) {
        PARAM_ERROR_RETURN(ctx->GetLogger(), ctx->GetAlarm(), errorMsg, inputType, ctx->GetConfigName(), "", "", "");
    }
    // Workspace
    if (!GetMandatoryStringParam(config, "Workspace", attachConfig->mWorkspace, errorMsg)) {
        PARAM_ERROR_RETURN(ctx->GetLogger(), ctx->GetAlarm(), errorMsg, inputType, ctx->GetConfigName(), "", "", "");
    }

    // Language
    std::string language;
    if (!GetMandatoryStringParam(config, "Language", language, errorMsg)
        || !LanguageToEnum(language, attachConfig->mLanguage)) {
        PARAM_ERROR_RETURN(ctx->GetLogger(), ctx->GetAlarm(), errorMsg, inputType, ctx->GetConfigName(), "", "", "");
    }

    // CommandId
    if (!GetMandatoryStringParam(config, "CommandId", attachConfig->mCommandId, errorMsg)) {
        PARAM_ERROR_RETURN(ctx->GetLogger(), ctx->GetAlarm(), errorMsg, inputType, ctx->GetConfigName(), "", "", "");
    }

    // AgentVersion
    if (!GetMandatoryStringParam(config, "AgentVersion", attachConfig->mAgentVersion, errorMsg)) {
        PARAM_ERROR_RETURN(ctx->GetLogger(), ctx->GetAlarm(), errorMsg, inputType, ctx->GetConfigName(), "", "", "");
    }

    // MatchRules
    if (!IsValidList(config, "MatchRules", errorMsg)) {
        PARAM_ERROR_RETURN(ctx->GetLogger(), ctx->GetAlarm(), errorMsg, inputType, ctx->GetConfigName(), "", "", "");
    }

    Json::Value matchRules;
    matchRules = config["MatchRules"];
    std::vector<MatchRule> matchRuleVec;
    for (const auto& matchRule : matchRules) {
        MatchRule rule;
        // RuleType
        std::string ruleTypeStr;
        if (!GetMandatoryStringParam(matchRule, "RuleType", ruleTypeStr, errorMsg)
            || !RuleTypeToEnum(ruleTypeStr, rule.mRuleType)) {
            PARAM_ERROR_RETURN(
                ctx->GetLogger(), ctx->GetAlarm(), errorMsg, inputType, ctx->GetConfigName(), "", "", "");
        }
        // Operation
        std::string operationStr;
        if (!GetMandatoryStringParam(matchRule, "Operation", operationStr, errorMsg)
            || !RuleOperationToEnum(operationStr, rule.mOperation)) {
            PARAM_ERROR_RETURN(
                ctx->GetLogger(), ctx->GetAlarm(), errorMsg, inputType, ctx->GetConfigName(), "", "", "");
        }

        // Value
        if (!GetMandatoryStringParam(matchRule, "Value", rule.mVal, errorMsg)) {
            PARAM_ERROR_RETURN(
                ctx->GetLogger(), ctx->GetAlarm(), errorMsg, inputType, ctx->GetConfigName(), "", "", "");
        }
        matchRuleVec.push_back(rule);
    }
    attachConfig->mMatchRules = std::move(matchRuleVec);

    return true;
}

} // namespace logtail::apm
