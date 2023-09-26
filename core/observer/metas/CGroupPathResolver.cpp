// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "CGroupPathResolver.h"
#include "common/util.h"
#include "common/FileSystemUtil.h"
#include "logger/Logger.h"
#include "common/ExceptionBase.h"
#include <vector>
#include "StringTools.h"

namespace logtail {
static const char* containerTypeStr[] = {"docker", "cri-containerd", "crio", "kubepods", "unknow"};
static const char* containerTypeRegexStr[] = {"docker", "cri-containerd", "crio", "\\S+", "unknow"};

CONTAINER_TYPE ExtractContainerType(const std::string& path) {
    if (path.find(containerTypeStr[CONTAINER_TYPE_DOCKER]) != std::string::npos) {
        return CONTAINER_TYPE_DOCKER;
    } else if (path.find(containerTypeStr[CONTAINER_TYPE_CRI_CONTAINERD]) != std::string::npos) {
        return CONTAINER_TYPE_CRI_CONTAINERD;
    } else if (path.find(containerTypeStr[CONTAINER_TYPE_CRIO]) != std::string::npos) {
        return CONTAINER_TYPE_CRIO;
    } else if (path.find(containerTypeStr[CONTAINER_TYPE_K8S_OTHERS]) != std::string::npos) {
        // "/sys/fs/cgroup/cpu,cpuacct/kubepods.slice/kubepods-burstable.slice/"
        return CONTAINER_TYPE_K8S_OTHERS;
    }
    return CONTAINER_TYPE_UNKNOWN;
}

KubernetesCGroupPathMatcher* GetCGroupMatcher(const std::string& path, CONTAINER_TYPE type) {
    const std::string podRegex = "([0-9a-f]{8}[-_][0-9a-f]{4}[-_][0-9a-f]{4}[-_][0-9a-f]{4}[-_][0-9a-f]{12})";
    const std::string containerRegex = "([0-9a-f]{64})";
    auto* matcher = new KubernetesCGroupPathMatcher();
    // STANDARD:
    // kubepods.slice/kubepods-pod2b801b7a_5266_4386_864e_45ed71136371.slice/cri-containerd-20e061fc708d3b66dfe257b19552b34a1307a7347ed6b5bd0d8c5e76afb1a870.scope/cgroup.procs
    matcher->setGuaranteedPathRegex(boost::regex("^kubepods.slice\\/kubepods-pod" + podRegex + ".slice\\/"
                                                 + containerTypeRegexStr[type] + "-" + containerRegex
                                                 + "\\.scope\\/cgroup\\.procs$"));
    // STANDARD:
    // kubepods.slice/kubepods-besteffort.slice/kubepods-besteffort-pod0d206349_0faf_445c_8c3f_2d2153784f15.slice/cri-containerd-efd08a78ad94af4408bcdb097fbcb603a31a40e4d74907f72ff14c3264ee7e85.scope/cgroup.procs
    matcher->setBesteffortPathRegex(boost::regex("^kubepods.slice\\/kubepods-besteffort.slice\\/kubepods-besteffort-pod"
                                                 + podRegex + ".slice\\/" + containerTypeRegexStr[type] + "-"
                                                 + containerRegex + "\\.scope\\/cgroup\\.procs$"));
    // STANDARD:
    // kubepods.slice/kubepods-burstable.slice/kubepods-burstable-podee10fb7d_d989_47b3_bc2a_e9ffbe767849.slice/cri-containerd-4591321a5d841ce6a60a777223cf7fe872d1af0ca721e76a5cf20985056771f7.scope/cgroup.procs
    matcher->setBurstablePathRegex(boost::regex("^kubepods.slice\\/kubepods-burstable.slice\\/kubepods-burstable-pod"
                                                + podRegex + ".slice\\/" + containerTypeRegexStr[type] + "-"
                                                + containerRegex + "\\.scope\\/cgroup\\.procs$"));
    if (matcher->IsMatch(path)) {
        return matcher;
    }
    // GKE:
    // kubepods/pod8dbc5577-d0e2-4706-8787-57d52c03ddf2/14011c7d92a9e513dfd69211da0413dbf319a5e45a02b354ba6e98e10272542d/cgroup.procs
    matcher->setGuaranteedPathRegex(
        boost::regex("^kubepods\\/pod" + podRegex + "\\/" + containerRegex + "\\/cgroup\\.procs$"));
    // GKE:
    // kubepods/besteffort/pod8dbc5577-d0e2-4706-8787-57d52c03ddf2/14011c7d92a9e513dfd69211da0413dbf319a5e45a02b354ba6e98e10272542d/cgroup.procs
    matcher->setBesteffortPathRegex(
        boost::regex("^kubepods\\/besteffort\\/pod" + podRegex + "\\/" + containerRegex + "\\/cgroup\\.procs$"));
    // GKE:
    // kubepods/burstable/pod8dbc5577-d0e2-4706-8787-57d52c03ddf2/14011c7d92a9e513dfd69211da0413dbf319a5e45a02b354ba6e98e10272542d/cgroup.procs
    matcher->setBurstablePathRegex(
        boost::regex("^kubepods\\/burstable\\/pod" + podRegex + "\\/" + containerRegex + "\\/cgroup\\.procs$"));
    if (matcher->IsMatch(path)) {
        return matcher;
    }
    // pure docker  :
    // /sys/fs/cgroup/cpu/docker/1ad2ce5889acb209e1576339741b1e504480db77d3771365e95b3bbd6fe91120/cgroup.procs
    matcher->setGuaranteedPathRegex(boost::regex("^docker\\/" + containerRegex + "\\/cgroup\\.procs$"));
    if (matcher->IsMatch(path)) {
        return matcher;
    }
    delete matcher;
    return nullptr;
}

std::string CGroupBasePath(const std::string& sysfsPath) {
    const std::vector<std::string> cgroup_dirs = {"cpu,cpuacct", "cpu", "pids"};
    std::string basePath = sysfsPath;
    basePath.append("/cgroup/");
    for (auto& dir : cgroup_dirs) {
        std::string innerPath = basePath;
        innerPath.append(dir);
        if (logtail::IsAccessibleDirectory(innerPath)) {
            return innerPath;
        }
    }
    throw ExceptionBase(std::string("cannot find the base cgroup path'"));
}

void resolveAllCGroupProcsPaths(const std::string& basePath, std::vector<std::string>& allPaths) {
    fsutil::Dir dir(basePath);
    if (!dir.Open()) {
        LOG_DEBUG(sLogger, ("Open dir fail", basePath)("errno", GetErrno()));
        return;
    }
    while (auto ent = dir.ReadNext(false)) {
        if (ent.IsRegFile()) {
            static std::string sCgroupProcesFileName = "cgroup.procs";
            if (ent.Name() == sCgroupProcesFileName) {
                std::string fullPath = basePath + "/cgroup.procs";
                allPaths.push_back(fullPath);
            }
            continue;
        }
        if (ent.IsDir()) {
            resolveAllCGroupProcsPaths(basePath + "/" + ent.Name(), allPaths);
        }
    }
}

void ResolveAllCGroupProcsPaths(const std::string& basePath, std::vector<std::string>& allPaths) {
    // resolve standard or OpenShift kubernetes cgroup procs paths
    resolveAllCGroupProcsPaths(basePath + "/kubepods.slice", allPaths);
    if (!allPaths.empty())
        return;
    // resolve GKE kubernetes cgroup procs paths
    resolveAllCGroupProcsPaths(basePath + "/kubepods", allPaths);
    if (!allPaths.empty())
        return;
    // resolve docker cgroup procs paths
    resolveAllCGroupProcsPaths(basePath + "/docker", allPaths);
    if (!allPaths.empty())
        return;
    resolveAllCGroupProcsPaths(basePath, allPaths);
}


bool KubernetesCGroupPathMatcher::IsMatch(const std::string& path) {
    std::string exception;
    if (path.find("burstable") != std::string::npos) {
        return BoostRegexMatch(path.c_str(), this->burstablePathRegex, exception);
    } else if (path.find("besteffort") != std::string::npos) {
        return BoostRegexMatch(path.c_str(), this->besteffortPathRegex, exception);
    } else {
        return BoostRegexMatch(path.c_str(), this->guaranteedPathRegex, exception);
    }
}

void KubernetesCGroupPathMatcher::ExtractProcessMeta(const std::string& path,
                                                     std::string& containerID,
                                                     std::string& podID) {
    std::string exception;
    boost::match_results<const char*> what;
    bool res;
    if (path.find("burstable") != std::string::npos) {
        res = BoostRegexSearch(path.c_str(), this->burstablePathRegex, exception, what);
    } else if (path.find("besteffort") != std::string::npos) {
        res = BoostRegexSearch(path.c_str(), this->besteffortPathRegex, exception, what);
    } else {
        res = BoostRegexSearch(path.c_str(), this->guaranteedPathRegex, exception, what);
    }
    if (!res) {
        LOG_DEBUG(sLogger, ("extract contianer meta error", exception)("path", path));
        return;
    }
    if (what.size() == 3) {
        podID = what[1].str();
        containerID = what[2].str();
    } else if (what.size() == 2) {
        containerID = what[1].str();
    }
}

std::string ExtractPodWorkloadName(const std::string& podName) {
    if (podName.empty()) {
        return podName;
    }
    static boost::regex deploymentPodReg(R"(^([\w\-]+)\-([0-9a-z]{9,10})\-([0-9a-z]{5})$)");
    static boost::regex setPodReg(R"(^([\w\-]+)\-([0-9a-z]{5})$)");
    static boost::regex statefulSetPodReg(R"(^([\w\-]+)\-(\d+)$)");
    std::string exception;
    boost::match_results<const char*> what;
    std::string pod;
    if (BoostRegexSearch(podName.c_str(), deploymentPodReg, exception, what)) {
        if (what.size() > 1) {
            return what[1].str();
        }
    } else if (BoostRegexSearch(podName.c_str(), setPodReg, exception, what)) {
        if (what.size() > 1) {
            return what[1].str();
        }
    } else if (BoostRegexSearch(podName.c_str(), statefulSetPodReg, exception, what)) {
        if (what.size() > 1) {
            return what[1].str();
        }
    }
    LOG_DEBUG(sLogger, ("extract workload name error", exception)("podName", podName));
    return podName;
}

std::string ReadPidCgroupPath(uint32_t pid, const std::string& procPath) {
    std::string cgroupPath = std::string(procPath).append(std::to_string(pid)).append("/cgroup");
    std::string content;
    LOG_DEBUG(sLogger, ("read pid cgroup path", cgroupPath));
    if (!ReadFileContent(cgroupPath, content) || content.empty()) {
        return "";
    }
    std::vector<std::string> pidStrs = SplitString(content, "\n");
    for (const auto& item : pidStrs) {
        if (item.empty()) {
            continue;
        }
        std::vector<std::string> parts = SplitString(item, ":");
        if (parts.size() != 3) {
            continue;
        }
        if ((parts[1] == "cpu,cpuacct" || parts[1] == "pids") && !parts[2].empty()) {
            parts[2].erase(parts[2].begin());
            return parts[2] + "/cgroup.procs";
        }
    }
    LOG_DEBUG(sLogger, ("read pid cgroup path error, pid", pid)("procPath", procPath));
    return "";
}


std::string ContainerTypeToString(CONTAINER_TYPE ct) {
    const static std::string sContainerTypeValue[] = {"docker", "cri_contianerd", "crio", "k8s_others", "unknown"};
    return sContainerTypeValue[ct];
}
} // namespace logtail