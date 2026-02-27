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

    bool IsEmpty() const { return mFieldsMap.empty() && mFieldsRegMap.empty(); }

    std::string ToString() const {
        std::stringstream ss;
        ss << "{";
        bool first = true;
        for (const auto& pair : mFieldsMap) {
            if (!first) {
                ss << ", ";
            }
            ss << pair.first << "=" << pair.second;
            first = false;
        }
        for (const auto& pair : mFieldsRegMap) {
            if (!first) {
                ss << ", ";
            }
            ss << pair.first << "=" << (pair.second ? pair.second->str() : "null");
            first = false;
        }
        ss << "}";
        return ss.str();
    }
};

struct MatchCriteriaFilter {
    // 包含和排除的标签
    FieldFilter mIncludeFields;
    FieldFilter mExcludeFields;

    bool IsEmpty() const { return mIncludeFields.IsEmpty() && mExcludeFields.IsEmpty(); }

    std::string ToString() const {
        std::stringstream ss;
        ss << "IncludeFields: " << mIncludeFields.ToString() << ", ExcludeFields: " << mExcludeFields.ToString();
        return ss.str();
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

    std::string ToString() const {
        std::stringstream ss;
        ss << "NamespaceReg: " << (mNamespaceReg ? mNamespaceReg->str() : "null")
           << ", PodReg: " << (mPodReg ? mPodReg->str() : "null")
           << ", ContainerReg: " << (mContainerReg ? mContainerReg->str() : "null")
           << ", K8sLabelFilter: " << mK8sLabelFilter.ToString();
        return ss.str();
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

    bool Init(const ContainerFilterConfig& config, std::string& exception);

    std::string ToString() const {
        std::stringstream ss;
        ss << "K8SFilter: " << mK8SFilter.ToString() << ", EnvFilter: " << mEnvFilter.ToString()
           << ", ContainerLabelFilter: " << mContainerLabelFilter.ToString();
        return ss.str();
    }
};


struct MatchedContainerInfo {
    std::string DataType;
    std::string Project;
    std::string Logstore;
    std::string ConfigName;
    std::string PathNotExistInputContainerIDs;
    std::string PathExistInputContainerIDs;
    std::string SourceAddress;
    std::string InputType;
    std::string InputIsContainerFile;
    std::string FlusherType;
    std::string FlusherTargetAddress;

    std::string ToString() const {
        std::stringstream ss;
        ss << "DataType: " << DataType << std::endl;
        ss << "Project: " << Project << std::endl;
        ss << "Logstore: " << Logstore << std::endl;
        ss << "ConfigName: " << ConfigName << std::endl;
        ss << "PathNotExistInputContainerIDs: " << PathNotExistInputContainerIDs << std::endl;
        ss << "PathExistInputContainerIDs: " << PathExistInputContainerIDs << std::endl;
        ss << "SourceAddress: " << SourceAddress << std::endl;
        ss << "InputType: " << InputType << std::endl;
        ss << "InputIsContainerFile: " << InputIsContainerFile << std::endl;
        ss << "FlusherType: " << FlusherType << std::endl;
        ss << "FlusherTargetAddress: " << FlusherTargetAddress << std::endl;
        return ss.str();
    }
};

struct ContainerDiscoveryOptions {
    ContainerFilterConfig mContainerFilterConfig;
    ContainerFilters mContainerFilters;
    std::unordered_map<std::string, std::string> mExternalK8sLabelTag;
    std::unordered_map<std::string, std::string> mExternalEnvTag;
    // 启用容器元信息预览
    bool mCollectingContainersMeta = false;
    bool mIsStdio = false;

    std::shared_ptr<MatchedContainerInfo> mMatchedContainerInfo;


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
