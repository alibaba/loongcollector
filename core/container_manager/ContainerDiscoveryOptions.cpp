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

#include "common/LogtailCommonFlags.h"
#include "common/ParamExtractor.h"
#include "pipeline/Pipeline.h"

using namespace std;

DEFINE_FLAG_INT32(default_plugin_log_queue_size, "", 10);

namespace logtail {

bool ContainerFilters::Init(const Json::Value& config, const PipelineContext& ctx, const string& pluginType) {
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

bool ContainerDiscoveryOptions::Init(const Json::Value& config, const PipelineContext& ctx, const string& pluginType) {
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
            mContainerFilters.Init(*itr, ctx, pluginType);
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

void ContainerDiscoveryOptions::GenerateContainerMetaFetchingGoPipeline(Json::Value& res,
                                                                        const FileDiscoveryOptions* fileDiscovery,
                                                                        const PluginInstance::PluginMeta& pluginMeta) const {
    Json::Value plugin(Json::objectValue);
    Json::Value detail(Json::objectValue);
    Json::Value object(Json::objectValue);

    auto ConvertMapToJsonObj = [&](const char* key, const unordered_map<string, string>& map) {
        if (!map.empty()) {
            object.clear();
            for (const auto& item : map) {
                object[item.first] = Json::Value(item.second);
            }
            detail[key] = object;
        }
    };

    if (fileDiscovery) {
        if (!fileDiscovery->GetWildcardPaths().empty()) {
            detail["LogPath"] = Json::Value(fileDiscovery->GetWildcardPaths()[0]);
            detail["MaxDepth"] = Json::Value(static_cast<int32_t>(fileDiscovery->GetWildcardPaths().size())
                                             + fileDiscovery->mMaxDirSearchDepth - 1);
        } else {
            detail["LogPath"] = Json::Value(fileDiscovery->GetBasePath());
            detail["MaxDepth"] = Json::Value(fileDiscovery->mMaxDirSearchDepth);
        }
        detail["FilePattern"] = Json::Value(fileDiscovery->GetFilePattern());
    }
    // 传递给 metric_container_info 的配置
    // 容器过滤
    if (!mContainerFilters.mK8sNamespaceRegex.empty()) {
        detail["K8sNamespaceRegex"] = Json::Value(mContainerFilters.mK8sNamespaceRegex);
    }
    if (!mContainerFilters.mK8sPodRegex.empty()) {
        detail["K8sPodRegex"] = Json::Value(mContainerFilters.mK8sPodRegex);
    }
    if (!mContainerFilters.mK8sContainerRegex.empty()) {
        detail["K8sContainerRegex"] = Json::Value(mContainerFilters.mK8sContainerRegex);
    }
    ConvertMapToJsonObj("IncludeK8sLabel", mContainerFilters.mIncludeK8sLabel);
    ConvertMapToJsonObj("ExcludeK8sLabel", mContainerFilters.mExcludeK8sLabel);
    ConvertMapToJsonObj("IncludeEnv", mContainerFilters.mIncludeEnv);
    ConvertMapToJsonObj("ExcludeEnv", mContainerFilters.mExcludeEnv);
    ConvertMapToJsonObj("IncludeContainerLabel", mContainerFilters.mIncludeContainerLabel);
    ConvertMapToJsonObj("ExcludeContainerLabel", mContainerFilters.mExcludeContainerLabel);
    // 日志标签富化
    ConvertMapToJsonObj("ExternalK8sLabelTag", mExternalK8sLabelTag);
    ConvertMapToJsonObj("ExternalEnvTag", mExternalEnvTag);
    // 启用容器元信息预览
    if (mCollectingContainersMeta) {
        detail["CollectingContainersMeta"] = Json::Value(true);
    }
    plugin["type"] = Json::Value(Pipeline::GenPluginTypeWithID("metric_container_info", pluginMeta.mPluginID));
    plugin["detail"] = detail;

    res["inputs"].append(plugin);
    // these param will be overriden if the same param appears in the global module of config, which will be parsed
    // later.
    res["global"]["DefaultLogQueueSize"] = Json::Value(INT32_FLAG(default_plugin_log_queue_size));
    res["global"]["AlwaysOnline"] = Json::Value(true);
}

} // namespace logtail
