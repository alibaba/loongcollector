/*
 * Copyright 2023 iLogtail Authors
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

#include <json/json.h>

#include <string>
#include <unordered_map>
#include <utility>

#include "file_server/FileDiscoveryOptions.h"
#include "pipeline/PipelineContext.h"
#include "pipeline/plugin/instance/PluginInstance.h"

namespace logtail {

struct ContainerFilters {
    std::string mK8sNamespaceRegex;
    std::string mK8sPodRegex;
    std::unordered_map<std::string, std::string> mIncludeK8sLabel;
    std::unordered_map<std::string, std::string> mExcludeK8sLabel;
    std::string mK8sContainerRegex;
    std::unordered_map<std::string, std::string> mIncludeEnv;
    std::unordered_map<std::string, std::string> mExcludeEnv;
    std::unordered_map<std::string, std::string> mIncludeContainerLabel;
    std::unordered_map<std::string, std::string> mExcludeContainerLabel;

    bool Init(const Json::Value& config, const PipelineContext& ctx, const std::string& pluginType);
};

struct ContainerDiscoveryOptions {
    ContainerFilters mContainerFilters;
    std::unordered_map<std::string, std::string> mExternalK8sLabelTag;
    std::unordered_map<std::string, std::string> mExternalEnvTag;
    // 启用容器元信息预览
    bool mCollectingContainersMeta = false;

    bool Init(const Json::Value& config, const PipelineContext& ctx, const std::string& pluginType);
    void GenerateContainerMetaFetchingGoPipeline(Json::Value& res,
                                                 const FileDiscoveryOptions* fileDiscovery = nullptr,
                                                 const PluginInstance::PluginMeta& pluginMeta = {"0"}) const;
};

using ContainerDiscoveryConfig = std::pair<const ContainerDiscoveryOptions*, const PipelineContext*>;

} // namespace logtail
