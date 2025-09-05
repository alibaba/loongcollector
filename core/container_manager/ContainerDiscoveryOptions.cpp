// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "container_manager/ContainerDiscoveryOptions.h"

#include <boost/regex.hpp>

#include "collection_pipeline/CollectionPipeline.h"
#include "common/LogtailCommonFlags.h"
#include "common/ParamExtractor.h"


using namespace std;

DEFINE_FLAG_INT32(default_plugin_log_queue_size, "", 10);

namespace logtail {

bool ContainerFilterConfig::Init(const Json::Value& config,
                                 const CollectionPipelineContext& ctx,
                                 const string& pluginType) {
    string errorMsg;

    // K8pluginNamespaceRegex
    if (!GetOptionalStringParam(config, "ContainerFilters.K8sNamespaceRegex", mK8sNamespaceRegex, errorMsg)) {
        PARAM_WARNING_IGNORE(ctx.GetLogger(),
                             ctx.GetAlarm(),
                             errorMsg,
                             pluginType,
                             ctx.GetConfigName(),
                             ctx.GetProjectName(),
                             ctx.GetLogstoreName(),
                             ctx.GetRegion());
    }

    // K8sPodRegex
    if (!GetOptionalStringParam(config, "ContainerFilters.K8sPodRegex", mK8sPodRegex, errorMsg)) {
        PARAM_WARNING_IGNORE(ctx.GetLogger(),
                             ctx.GetAlarm(),
                             errorMsg,
                             pluginType,
                             ctx.GetConfigName(),
                             ctx.GetProjectName(),
                             ctx.GetLogstoreName(),
                             ctx.GetRegion());
    }

    // IncludeK8sLabel
    if (!GetOptionalMapParam(config, "ContainerFilters.IncludeK8sLabel", mIncludeK8sLabel, errorMsg)) {
        PARAM_WARNING_IGNORE(ctx.GetLogger(),
                             ctx.GetAlarm(),
                             errorMsg,
                             pluginType,
                             ctx.GetConfigName(),
                             ctx.GetProjectName(),
                             ctx.GetLogstoreName(),
                             ctx.GetRegion());
    }

    // ExcludeK8sLabel
    if (!GetOptionalMapParam(config, "ContainerFilters.ExcludeK8sLabel", mExcludeK8sLabel, errorMsg)) {
        PARAM_WARNING_IGNORE(ctx.GetLogger(),
                             ctx.GetAlarm(),
                             errorMsg,
                             pluginType,
                             ctx.GetConfigName(),
                             ctx.GetProjectName(),
                             ctx.GetLogstoreName(),
                             ctx.GetRegion());
    }

    // K8sContainerRegex
    if (!GetOptionalStringParam(config, "ContainerFilters.K8sContainerRegex", mK8sContainerRegex, errorMsg)) {
        PARAM_WARNING_IGNORE(ctx.GetLogger(),
                             ctx.GetAlarm(),
                             errorMsg,
                             pluginType,
                             ctx.GetConfigName(),
                             ctx.GetProjectName(),
                             ctx.GetLogstoreName(),
                             ctx.GetRegion());
    }

    // IncludeEnv
    if (!GetOptionalMapParam(config, "ContainerFilters.IncludeEnv", mIncludeEnv, errorMsg)) {
        PARAM_WARNING_IGNORE(ctx.GetLogger(),
                             ctx.GetAlarm(),
                             errorMsg,
                             pluginType,
                             ctx.GetConfigName(),
                             ctx.GetProjectName(),
                             ctx.GetLogstoreName(),
                             ctx.GetRegion());
    }

    // ExcludeEnv
    if (!GetOptionalMapParam(config, "ContainerFilters.ExcludeEnv", mExcludeEnv, errorMsg)) {
        PARAM_WARNING_IGNORE(ctx.GetLogger(),
                             ctx.GetAlarm(),
                             errorMsg,
                             pluginType,
                             ctx.GetConfigName(),
                             ctx.GetProjectName(),
                             ctx.GetLogstoreName(),
                             ctx.GetRegion());
    }

    // IncludeContainerLabel
    if (!GetOptionalMapParam(config, "ContainerFilters.IncludeContainerLabel", mIncludeContainerLabel, errorMsg)) {
        PARAM_WARNING_IGNORE(ctx.GetLogger(),
                             ctx.GetAlarm(),
                             errorMsg,
                             pluginType,
                             ctx.GetConfigName(),
                             ctx.GetProjectName(),
                             ctx.GetLogstoreName(),
                             ctx.GetRegion());
    }

    // ExcludeContainerLabel
    if (!GetOptionalMapParam(config, "ContainerFilters.ExcludeContainerLabel", mExcludeContainerLabel, errorMsg)) {
        PARAM_WARNING_IGNORE(ctx.GetLogger(),
                             ctx.GetAlarm(),
                             errorMsg,
                             pluginType,
                             ctx.GetConfigName(),
                             ctx.GetProjectName(),
                             ctx.GetLogstoreName(),
                             ctx.GetRegion());
    }
    return true;
}


bool SplitRegexFromMap(const std::unordered_map<std::string, std::string>& inputs, FieldFilter& fieldFilter) {
    std::unordered_map<std::string, std::string> staticResult;
    std::unordered_map<std::string, std::shared_ptr<boost::regex>> regexResult;

    for (const auto& input : inputs) {
        // 检查是否是有效的正则表达式：必须以^开头并且以$结尾
        bool isValidRegex = false;
        if (!input.second.empty() && input.second[0] == '^' && input.second.back() == '$') {
            isValidRegex = true;
        }

        if (isValidRegex) {
            try {
                // 编译正则表达式
                auto reg = std::make_shared<boost::regex>(input.second);
                regexResult[input.first] = reg;
            } catch (const std::exception& e) {
                return false;
            }
        } else {
            staticResult[input.first] = input.second;
        }
    }
    fieldFilter.mFieldsMap = staticResult;
    fieldFilter.mFieldsRegMap = regexResult;
    return true;
}

bool ContainerFilters::Init(const ContainerFilterConfig& config) {
    /// 为 K8sFilter 设置正则表达式
    try {
        if (!config.mK8sNamespaceRegex.empty()) {
            mK8SFilter.mNamespaceReg = std::make_shared<boost::regex>(config.mK8sNamespaceRegex);
        }

        if (!config.mK8sPodRegex.empty()) {
            mK8SFilter.mPodReg = std::make_shared<boost::regex>(config.mK8sPodRegex);
        }

        if (!config.mK8sContainerRegex.empty()) {
            mK8SFilter.mContainerReg = std::make_shared<boost::regex>(config.mK8sContainerRegex);
        }
    } catch (const boost::regex_error& e) {
        // Handle regex compilation errors gracefully
        return false;
    } catch (const std::exception& e) {
        // Handle other exceptions gracefully
        return false;
    }
    bool success = false;
    if (!config.mIncludeK8sLabel.empty()) {
        success = SplitRegexFromMap(config.mIncludeK8sLabel, mK8SFilter.mK8sLabelFilter.mIncludeFields);
        if (!success) {
            return success;
        }
    }
    if (!config.mExcludeK8sLabel.empty()) {
        success = SplitRegexFromMap(config.mExcludeK8sLabel, mK8SFilter.mK8sLabelFilter.mExcludeFields);
        if (!success) {
            return success;
        }
    }
    if (!config.mIncludeContainerLabel.empty()) {
        success = SplitRegexFromMap(config.mIncludeContainerLabel, mContainerLabelFilter.mIncludeFields);
        if (!success) {
            return success;
        }
    }
    if (!config.mExcludeContainerLabel.empty()) {
        success = SplitRegexFromMap(config.mExcludeContainerLabel, mContainerLabelFilter.mExcludeFields);
        if (!success) {
            return success;
        }
    }
    if (!config.mIncludeEnv.empty()) {
        success = SplitRegexFromMap(config.mIncludeEnv, mEnvFilter.mIncludeFields);
        if (!success) {
            return success;
        }
    }
    if (!config.mExcludeEnv.empty()) {
        success = SplitRegexFromMap(config.mExcludeEnv, mEnvFilter.mExcludeFields);
        if (!success) {
            return success;
        }
    }
    return true;
}


bool ContainerDiscoveryOptions::Init(const Json::Value& config,
                                     const CollectionPipelineContext& ctx,
                                     const string& pluginType) {
    string errorMsg;

    const char* key = "ContainerFilters";
    const Json::Value* itr = config.find(key, key + strlen(key));
    if (itr) {
        if (!itr->isObject()) {
            PARAM_WARNING_IGNORE(ctx.GetLogger(),
                                 ctx.GetAlarm(),
                                 "param ContainerFilters is not of type object",
                                 pluginType,
                                 ctx.GetConfigName(),
                                 ctx.GetProjectName(),
                                 ctx.GetLogstoreName(),
                                 ctx.GetRegion());
        } else {
            bool success = mContainerFilterConfig.Init(*itr, ctx, pluginType);
            if (!success) {
                return false;
            }
            success = mContainerFilters.Init(mContainerFilterConfig);
            if (!success) {
                return false;
            }
        }
    }

    // ExternalK8sLabelTag
    if (!GetOptionalMapParam(config, "ExternalK8sLabelTag", mExternalK8sLabelTag, errorMsg)) {
        PARAM_WARNING_IGNORE(ctx.GetLogger(),
                             ctx.GetAlarm(),
                             errorMsg,
                             pluginType,
                             ctx.GetConfigName(),
                             ctx.GetProjectName(),
                             ctx.GetLogstoreName(),
                             ctx.GetRegion());
    }

    // ExternalEnvTag
    if (!GetOptionalMapParam(config, "ExternalEnvTag", mExternalEnvTag, errorMsg)) {
        PARAM_WARNING_IGNORE(ctx.GetLogger(),
                             ctx.GetAlarm(),
                             errorMsg,
                             pluginType,
                             ctx.GetConfigName(),
                             ctx.GetProjectName(),
                             ctx.GetLogstoreName(),
                             ctx.GetRegion());
    }

    // CollectingContainersMeta
    if (!GetOptionalBoolParam(config, "CollectingContainersMeta", mCollectingContainersMeta, errorMsg)) {
        PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                              ctx.GetAlarm(),
                              errorMsg,
                              mCollectingContainersMeta,
                              pluginType,
                              ctx.GetConfigName(),
                              ctx.GetProjectName(),
                              ctx.GetLogstoreName(),
                              ctx.GetRegion());
    }

    return true;
}


void ContainerDiscoveryOptions::GetCustomExternalTags(
    const std::unordered_map<std::string, std::string>& containerEnvs,
    const std::unordered_map<std::string, std::string>& containerK8sLabels,
    std::vector<std::pair<std::string, std::string>>& tags) const {
    if (mExternalEnvTag.empty() && mExternalK8sLabelTag.empty()) {
        return;
    }

    // Process environment variable mappings
    for (const auto& envMapping : mExternalEnvTag) {
        const std::string& envKey = envMapping.first;
        const std::string& tagKey = envMapping.second;

        auto envIt = containerEnvs.find(envKey);
        if (envIt != containerEnvs.end()) {
            tags.emplace_back(tagKey, envIt->second);
        }
    }

    // Process K8s label mappings
    for (const auto& labelMapping : mExternalK8sLabelTag) {
        const std::string& labelKey = labelMapping.first;
        const std::string& tagKey = labelMapping.second;

        auto labelIt = containerK8sLabels.find(labelKey);
        if (labelIt != containerK8sLabels.end()) {
            tags.emplace_back(tagKey, labelIt->second);
        }
    }
}

} // namespace logtail
