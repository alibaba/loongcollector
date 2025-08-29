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

#include "collection_pipeline/CollectionPipeline.h"
#include "common/LogtailCommonFlags.h"
#include "common/ParamExtractor.h"
#include <boost/regex.hpp>


using namespace std;

DEFINE_FLAG_INT32(default_plugin_log_queue_size, "", 10);

namespace logtail {

bool ContainerFilterConfig::Init(const Json::Value& config, const CollectionPipelineContext& ctx, const string& pluginType) {
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

    for (const auto& input: inputs) {
        // 检查前缀
        if (input.second.empty() || input.second[0] == '^') {
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

bool ContainerFilterConfig::GetContainerFilters(ContainerFilters& filters) {
    /// 为 K8sFilter 设置正则表达式
    if (!mK8sNamespaceRegex.empty()) {
        filters.mK8SFilter.mNamespaceReg = std::make_shared<boost::regex>(mK8sNamespaceRegex);
    }

    if (!mK8sPodRegex.empty()) {
        filters.mK8SFilter.mPodReg = std::make_shared<boost::regex>(mK8sPodRegex);
    }

    if (!mK8sContainerRegex.empty()) {
        filters.mK8SFilter.mContainerReg = std::make_shared<boost::regex>(mK8sContainerRegex);
    }
    bool success = false;
    if (!mIncludeK8sLabel.empty()) {
        success = SplitRegexFromMap(mIncludeK8sLabel, filters.mK8SFilter.mK8sLabelFilter.mIncludeFields);
        if (!success) {
            return success;
        }
    }
    if (!mExcludeK8sLabel.empty()) {
        success = SplitRegexFromMap(mExcludeK8sLabel, filters.mK8SFilter.mK8sLabelFilter.mExcludeFields);
        if (!success) {
            return success;
        }
    }
    if (!mIncludeContainerLabel.empty()) {
        success = SplitRegexFromMap(mIncludeContainerLabel, filters.mContainerLabelFilter.mIncludeFields);
        if (!success) {
            return success;
        }
    }
    if (!mExcludeContainerLabel.empty()) {
        success = SplitRegexFromMap(mExcludeContainerLabel, filters.mContainerLabelFilter.mExcludeFields);
        if (!success) {
            return success;
        }
    }
    if (!mIncludeEnv.empty()) {
        success = SplitRegexFromMap(mIncludeEnv, filters.mEnvFilter.mIncludeFields);
        if (!success) {
            return success;
        }
    }
    if (!mExcludeEnv.empty()) {
        success = SplitRegexFromMap(mExcludeEnv, filters.mEnvFilter.mExcludeFields);
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
            mContainerFilterConfig.Init(*itr, ctx, pluginType);
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
} // namespace logtail
