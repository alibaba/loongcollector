/*
 * Copyright 2022 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <string>
#include <memory>
#include <vector>

#include "K8sMeta.h"
#include "common/util.h"
#include "logger/Logger.h"
#include "network/NetworkConfig.h"
#include "network/sources/ebpf/EBPFWrapper.h"

namespace logtail {

struct ProcessMeta {
    uint32_t PID;
    std::string ProcessCMD;

    K8sPodMeta Pod;
    ContainerMeta Container;

    void Clear() {
        ProcessCMD.clear();
        Pod.Clear();
        Container.Clear();
        mMetaInfo.clear();
        mPassFilterRules = 0;
    }

    std::string ToString() {
        std::string rst;
        std::vector<std::pair<std::string, std::string> >& meta = GetFormattedMeta();
        for (size_t i = 0; i < meta.size(); ++i) {
            rst.append("[").append(meta[i].first).append(", ").append(meta[i].second).append("]");
        }
        return rst;
    }

    std::vector<std::pair<std::string, std::string> >& GetFormattedMeta() {
        if (!mMetaInfo.empty()) {
            return mMetaInfo;
        }

        if (!Pod.PodName.empty()) {
            // k8s pod
            mMetaInfo.reserve(5);
            mMetaInfo.emplace_back(std::string("_pod_name_"), Pod.PodName);
            mMetaInfo.emplace_back(std::string("_namespace_"), Pod.NameSpace);
            mMetaInfo.emplace_back(std::string("_container_name_"), Container.ContainerName);
            mMetaInfo.emplace_back(std::string("_workload_name_"), Pod.WorkloadName);
            mMetaInfo.emplace_back(std::string("_running_mode_"), "kubernetes");
        } else if (!Container.ContainerName.empty()) {
            // container
            mMetaInfo.reserve(2);
            mMetaInfo.emplace_back(std::string("_container_name_"), Container.ContainerName);
            mMetaInfo.emplace_back(std::string("_running_mode_"), "container");
        } else {
            // process
            mMetaInfo.reserve(3);
            mMetaInfo.emplace_back(std::string("_process_pid_"), std::to_string(PID));
            mMetaInfo.emplace_back(std::string("_process_cmd_"), ProcessCMD);
            mMetaInfo.emplace_back(std::string("_running_mode_"), "host");
        }
        return mMetaInfo;
    }

    static bool isMapLabelsMatch(std::unordered_map<std::string, boost::regex>& includeLabels,
                                 std::unordered_map<std::string, boost::regex>& excludeLabels,
                                 std::unordered_map<std::string, std::string>& labels) {
        std::string exception;
        if (!includeLabels.empty()) {
            bool matchedFlag = false;
            for (const auto& item : includeLabels) {
                auto res = labels.find(item.first);
                if (res != labels.end()) {
                    if (res->second.empty() || BoostRegexMatch(res->second.c_str(), item.second, exception)) {
                        matchedFlag = true;
                        break;
                    }
                }
            }
            if (!matchedFlag) {
                return false;
            }
        }
        if (!excludeLabels.empty()) {
            for (const auto& item : excludeLabels) {
                auto res = labels.find(item.first);
                if (res != labels.end()) {
                    if (res->second.empty() || BoostRegexMatch(res->second.c_str(), item.second, exception)) {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    bool PassFilterRules() {
        if (this->mPassFilterRules != 0) {
            return mPassFilterRules > 0;
        }
        auto instance = NetworkConfig::GetInstance();
        std::string exception;
        if (!Pod.PodName.empty()) {
            if ((!instance->mIncludeNamespaceNameRegex.empty()
                 && !BoostRegexMatch(Pod.NameSpace.c_str(), instance->mIncludeNamespaceNameRegex, exception))
                || (!instance->mIncludePodNameRegex.empty()
                    && !BoostRegexMatch(Pod.PodName.c_str(), instance->mIncludePodNameRegex, exception))
                || (!instance->mExcludeNamespaceNameRegex.empty()
                    && BoostRegexMatch(Pod.NameSpace.c_str(), instance->mExcludeNamespaceNameRegex, exception))
                || (!instance->mExcludePodNameRegex.empty()
                    && BoostRegexMatch(Pod.PodName.c_str(), instance->mExcludePodNameRegex, exception))
                || (!isMapLabelsMatch(instance->mIncludeK8sLabels, instance->mExcludeK8sLabels, Pod.Labels))
                || (!instance->mIncludeContainerNameRegex.empty()
                    && !BoostRegexMatch(
                        Container.ContainerName.c_str(), instance->mIncludeContainerNameRegex, exception))
                || (!instance->mExcludeContainerNameRegex.empty()
                    && BoostRegexMatch(
                        Container.ContainerName.c_str(), instance->mExcludeContainerNameRegex, exception))
                || (!isMapLabelsMatch(
                    instance->mIncludeContainerLabels, instance->mExcludeContainerLabels, Container.Labels))
                || (!isMapLabelsMatch(instance->mIncludeEnvs, instance->mExcludeEnvs, Container.Envs))) {
                mPassFilterRules = -1;
                return false;
            }
        } else if (!Container.ContainerName.empty()) {
            if ((!instance->mIncludeContainerNameRegex.empty()
                 && !BoostRegexMatch(Container.ContainerName.c_str(), instance->mIncludeContainerNameRegex, exception))
                || (!instance->mExcludeContainerNameRegex.empty()
                    && BoostRegexMatch(
                        Container.ContainerName.c_str(), instance->mExcludeContainerNameRegex, exception))
                || (!isMapLabelsMatch(
                    instance->mIncludeContainerLabels, instance->mExcludeContainerLabels, Container.Labels))
                || (!isMapLabelsMatch(instance->mIncludeEnvs, instance->mExcludeEnvs, Container.Envs))) {
                mPassFilterRules = -1;
                return false;
            }
        } else {
            if ((!instance->mIncludeCmdRegex.empty()
                 && !BoostRegexMatch(ProcessCMD.c_str(), instance->mIncludeCmdRegex, exception))
                || (!instance->mExcludeCmdRegex.empty()
                    && BoostRegexMatch(ProcessCMD.c_str(), instance->mExcludeCmdRegex, exception))) {
                mPassFilterRules = -1;
                return false;
            }
        }
        mPassFilterRules = 1;
        return true;
    }

    void ResetFilter() { this->mPassFilterRules = 0; }

private:
    std::vector<std::pair<std::string, std::string> > mMetaInfo;
    int8_t mPassFilterRules = 0;
    friend class CGroupPathResolverUnittest;
};

typedef std::shared_ptr<ProcessMeta> ProcessMetaPtr;


} // namespace logtail
