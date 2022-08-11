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
#include <regex>
#include <vector>

#include "common/util.h"
#include "ProcessMeta.h"

namespace logtail {

enum CONTAINER_TYPE {
    CONTAINER_TYPE_DOCKER,
    CONTAINER_TYPE_CRI_CONTAINERD,
    CONTAINER_TYPE_CRIO,
    CONTAINER_TYPE_K8S_OTHERS,
    CONTAINER_TYPE_UNKNOWN
};


class KubernetesCGroupPathMatcher {
private:
    boost::regex guaranteedPathRegex;
    boost::regex besteffortPathRegex;
    boost::regex burstablePathRegex;

public:
    void setGuaranteedPathRegex(const boost::regex& regex) { KubernetesCGroupPathMatcher::guaranteedPathRegex = regex; }

    void setBesteffortPathRegex(const boost::regex& regex) { KubernetesCGroupPathMatcher::besteffortPathRegex = regex; }

    void setBurstablePathRegex(const boost::regex& regex) { KubernetesCGroupPathMatcher::burstablePathRegex = regex; }

    bool IsMatch(const std::string& path);

    void ExtractProcessMeta(const std::string& path, std::string& containerID, std::string& podID);
};

CONTAINER_TYPE ExtractContainerType(const std::string& path);

std::string CGroupBasePath(const std::string& sysfs_path);

void ResolveAllCGroupProcsPaths(const std::string& bashPath, std::vector<std::string>& allPaths);

KubernetesCGroupPathMatcher* GetCGroupMatcher(const std::string& path, CONTAINER_TYPE type);

std::string ExtractPodWorkloadName(const std::string& podName);

std::string ReadPidCgroupPath(uint32_t pid, const std::string& procPath = "/proc/");

std::string ContainerTypeToString(CONTAINER_TYPE ct);

} // namespace logtail