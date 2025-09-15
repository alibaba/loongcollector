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

#include <boost/regex.hpp>
#include <string>
#include <unordered_map>
#include <utility>

#include "json/json.h"

#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/plugin/instance/PluginInstance.h"

namespace logtail {

class FileDiscoveryOptions;

struct FieldFilter {
    std::unordered_map<std::string, std::string> mFieldsMap;
    std::unordered_map<std::string, std::shared_ptr<boost::regex>> mFieldsRegMap;

    bool IsEmpty() const {
        return mFieldsMap.empty() && mFieldsRegMap.empty();
    }
};

struct MatchCriteriaFilter {
    // 包含和排除的标签
    FieldFilter mIncludeFields;
    FieldFilter mExcludeFields;

    bool IsEmpty() const {
        return mIncludeFields.IsEmpty() && mExcludeFields.IsEmpty();
    }
};

struct K8sFilter {
    std::shared_ptr<boost::regex> mNamespaceReg;
    std::shared_ptr<boost::regex> mPodReg;
    std::shared_ptr<boost::regex> mContainerReg;

    MatchCriteriaFilter mK8sLabelFilter;

    bool IsEmpty() const {
        return mNamespaceReg == nullptr && mPodReg == nullptr && mContainerReg == nullptr && mK8sLabelFilter.IsEmpty();
    }
};

struct ContainerFilterConfig {
    std::string mK8sNamespaceRegex;
    std::string mK8sPodRegex;
    std::string mK8sContainerRegex;


    std::unordered_map<std::string, std::string> mIncludeK8sLabel;
    std::unordered_map<std::string, std::string> mExcludeK8sLabel;

    std::unordered_map<std::string, std::string> mIncludeEnv;
    std::unordered_map<std::string, std::string> mExcludeEnv;

    std::unordered_map<std::string, std::string> mIncludeContainerLabel;
    std::unordered_map<std::string, std::string> mExcludeContainerLabel;

    bool Init(const Json::Value& config, const CollectionPipelineContext& ctx, const std::string& pluginType);
};

struct ContainerFilters {
    K8sFilter mK8SFilter;
    MatchCriteriaFilter mEnvFilter;
    MatchCriteriaFilter mContainerLabelFilter;

    bool Init(const ContainerFilterConfig& config);
};


struct ContainerDiscoveryOptions {
    ContainerFilterConfig mContainerFilterConfig;
    ContainerFilters mContainerFilters;
    std::unordered_map<std::string, std::string> mExternalK8sLabelTag;
    std::unordered_map<std::string, std::string> mExternalEnvTag;
    // 启用容器元信息预览
    bool mCollectingContainersMeta = false;

    bool Init(const Json::Value& config, const CollectionPipelineContext& ctx, const std::string& pluginType);

    // Fill external tags into the provided vector based on container info and tag mappings
    void GetCustomExternalTags(const std::unordered_map<std::string, std::string>& containerEnvs,
                               const std::unordered_map<std::string, std::string>& containerK8sLabels,
                               std::vector<std::pair<std::string, std::string>>& tags) const;

    void GenerateContainerMetaFetchingGoPipeline(Json::Value& res,
                                                 const FileDiscoveryOptions* fileDiscovery = nullptr,
                                                 const PluginInstance::PluginMeta& pluginMeta = {"0"}) const;
};

using ContainerDiscoveryConfig = std::pair<const ContainerDiscoveryOptions*, const CollectionPipelineContext*>;

} // namespace logtail
