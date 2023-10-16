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

#include "ContainerProcessGroup.h"
#include "common/TimeUtil.h"
#include "logger/Logger.h"
#include "CGroupPathResolver.h"
#include "monitor/LogtailAlarm.h"
#include "FileSystemUtil.h"
#include "StringTools.h"
#include "go_pipeline/LogtailPlugin.h"
#include "interface/statistics.h"
#include <stdlib.h>

DEFINE_FLAG_INT32(sls_observer_process_update_interval, "SLS Observer Process Update Interval", 300);


namespace logtail {

void ContainerProcessGroup::FlushOutMetrics(uint64_t timeNano,
                                            std::vector<sls_logs::Log>& allData,
                                            std::vector<std::pair<std::string, std::string>>& tags,
                                            uint64_t interval) {
    auto& metaTags = mMetaPtr->GetFormattedMeta();
    mAggregator.FlushOutMetrics(timeNano, allData, metaTags, tags, interval);
}

void ContainerProcessGroupManager::FlushOutMetrics(std::vector<sls_logs::Log>& allData,
                                                   std::vector<std::pair<std::string, std::string>>& tags,
                                                   uint64_t interval) {
    uint64_t timeNano = GetCurrentTimeInNanoSeconds();
    for (auto& iter : mPureProcessGroupMap) {
        iter.second->FlushOutMetrics(timeNano, allData, tags, interval);
    }
    for (auto& iter : mContainerProcessGroupMap) {
        iter.second->FlushOutMetrics(timeNano, allData, tags, interval);
    }
}


bool ContainerProcessGroupManager::Init(const std::string& cgroupPath) {
    if (!this->mGgoupBasePath.empty()) {
        return true;
    }
    LOG_INFO(sLogger, ("try to find a valid cgroup path", "begin"));
    try {
        this->mGgoupBasePath = CGroupBasePath(cgroupPath);
    } catch (const std::exception& e) {
        LOG_ERROR(sLogger, ("fail to find cgroup path", e.what()));
        LogtailAlarm::GetInstance()->SendAlarm(OBSERVER_INIT_ALARM,
                                               "fail to find cgroup path:" + std::string(e.what()));
        return false;
    }
    LOG_INFO(sLogger, ("find a valid cgroup path", "success")("path", this->mGgoupBasePath));
    return true;
}


int32_t ContainerProcessGroupManager::ParseCgroupPath(const std::string& path,
                                                      std::vector<int32_t>& pids,
                                                      std::string& containerID,
                                                      std::string& podID) {
    std::string content;
    if (!ReadFileContent(path, content)) {
        return -2;
    }
    if (content.empty()) {
        return -1;
    }
    std::vector<std::string> pidStrs = SplitString(content, "\n");
    if (pidStrs.empty() || pidStrs[0].empty()) {
        return -1;
    }
    for (size_t i = 0; i < pidStrs.size(); ++i) {
        pids.push_back(strtol(pidStrs[i].c_str(), NULL, 10));
    }
    if (path.size() <= mGgoupBasePath.size()) {
        return -3;
    }
    mMatcher->ExtractProcessMeta(path.substr(mGgoupBasePath.size() + 1), containerID, podID);
    if (containerID.empty()) {
        return -4;
    }
    return 0;
}

int32_t ContainerProcessGroupManager::DetectContainerType(const std::string& procsPath) {
    mContainerType = ExtractContainerType(procsPath);
    if (mContainerType == CONTAINER_TYPE_UNKNOWN) {
        return -1;
    }
    if (procsPath.size() <= mGgoupBasePath.size()) {
        mContainerType = CONTAINER_TYPE_UNKNOWN;
        return -2;
    }
    mMatcher = GetCGroupMatcher(procsPath.substr(mGgoupBasePath.size() + 1), mContainerType);
    if (mMatcher == NULL) {
        mContainerType = CONTAINER_TYPE_UNKNOWN;
        return -3;
    }
    LOG_INFO(sLogger, ("detect container type by cgroup path success, type", mContainerType)("path", procsPath));
    return 0;
}

void ContainerProcessGroupManager::FlushMetas() {
    // ProcessMetaStatistic is the gauge value, so must clear history data before fetching meta.
    ProcessMetaStatistic::Clear();
    std::vector<std::string> paths;
    std::unordered_set<uint32_t> pidSet;
    if (!this->mGgoupBasePath.empty()) {
        ResolveAllCGroupProcsPaths(this->mGgoupBasePath, paths);
    }
    if (paths.empty()) {
        LOG_WARNING(sLogger, ("try to find process meta because of no valid cgroup proc paths", ""));
        FlushPids(pidSet);
        return;
    }
    if (mContainerType == CONTAINER_TYPE_UNKNOWN) {
        // paths may have :
        //        1. /sys/fs/cgroup/cpu,cpuacct/kubepods.slice/cgroup.procs
        //        2.
        //        /sys/fs/cgroup/cpu,cpuacct/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-pode4cd4327_698e_42e0_b6bb_54d32685bcf5.slice/docker-d1eefc8c91baea08621aa21b07abf73b2a565016326732793113f0ba31cc41a1.scope/cgroup.procs
        // we should use max length's path to detect exact container type
        size_t maxLenIndex = 0;
        size_t maxLen = 0;
        for (size_t i = 0; i < paths.size(); ++i) {
            if (paths[i].size() > maxLen) {
                maxLen = paths[i].size();
                maxLenIndex = i;
            }
        }
        int32_t detectRst = DetectContainerType(paths[maxLenIndex]);
        if (detectRst != 0) {
            LOG_ERROR(sLogger, ("unknown container type from path", paths[maxLenIndex])("result", detectRst));
            FlushPids(pidSet);
            return;
        }
    }
    size_t ignoredPathCount = 0;
    for (auto& p : paths) {
        std::vector<int32_t> pids;
        std::string containerID;
        std::string podID;
        int32_t rst = ParseCgroupPath(p, pids, containerID, podID);
        if (rst == -1) {
            ignoredPathCount++;
            LOG_DEBUG(sLogger, ("parse cgroup path ignored, rst", rst)("path", p));
            continue;
        }
        if (rst < -1) {
            mProcessMetaStatistic->mCgroupPathParseFailCount++;
            LOG_DEBUG(sLogger, ("parse cgroup path failed, rst", rst)("path", p));
            continue;
        }
        bool containerMetaFetched = false;
        K8sContainerMeta containerMeta;
        for (const auto& pid : pids) {
            if (pid == 0) {
                continue;
            }
            pidSet.insert(pid);
            ProcessMetaPtr& metaPtr = mProcessMetaMap[pid];
            if (metaPtr.get() == nullptr) {
                metaPtr.reset(new ProcessMeta);
            }
            ProcessMeta* meta = metaPtr.get();
            if (meta->Pod.PodUUID != podID || meta->Container.ContainerID != containerID
                || meta->Container.ContainerName.empty()) {
                meta->Clear();
                meta->PID = pid;
                meta->Container.ContainerID = containerID;
                meta->Pod.PodUUID = podID;
                if (!containerMetaFetched) {
                    containerMetaFetched = true;
                    containerMeta = LogtailPlugin::GetInstance()->GetContainerMeta(containerID);
                    mProcessMetaStatistic->mFetchContainerMetaCount++;
                    if (containerMeta.ContainerName.empty()) {
                        mProcessMetaStatistic->mFetchContainerMetaFailCount++;
                    }
                    LOG_DEBUG(sLogger,
                              ("flushMeta get container meta for pid",
                               pid)("id", containerID)("meta", containerMeta.ToString()));
                }
                meta->Pod.NameSpace = containerMeta.K8sNamespace;
                meta->Pod.PodName = containerMeta.PodName;
                meta->Pod.WorkloadName = ExtractPodWorkloadName(containerMeta.PodName);
                meta->Container.ContainerName = containerMeta.ContainerName;
                meta->Container.Image = containerMeta.Image;
                meta->Pod.Labels = containerMeta.k8sLabels;
                meta->Container.Labels = containerMeta.containerLabels;
                meta->Container.Envs = containerMeta.envs;
            }
        }
    }
    mProcessMetaStatistic->mCgroupPathTotalCount = paths.size() - ignoredPathCount;
    mProcessMetaStatistic->mWatchProcessCount = pidSet.size();
    LOG_INFO(sLogger, ("flush meta success", mProcessMetaStatistic->ToString()));
    FlushPids(pidSet);
}

void ContainerProcessGroupManager::FlushPids(const std::unordered_set<uint32_t>& existedPids) {
    uint32_t nowTime = time(NULL);
    if (nowTime - mLastNormalProcessMetaUpdateTime >= (uint32_t)INT32_FLAG(sls_observer_process_update_interval)) {
        mLastNormalProcessMetaUpdateTime = nowTime;
        for (auto iter = mProcessMetaMap.begin(); iter != mProcessMetaMap.end();) {
            // do not delete pid 0
            if (iter->first == 0) {
                ++iter;
                continue;
            }
            if (existedPids.find(iter->first) != existedPids.end()) {
                // LOG_INFO(sLogger, (std::to_string(iter->first), iter->second->ToString()));
                ++iter;
                continue;
            }
            // check pid exists. if exists, update it; else delete it
            std::string cmdLine;
            if (!readCmdline(iter->first, cmdLine)) {
                iter = mProcessMetaMap.erase(iter);
            } else {
                auto& metaPtr = iter->second;
                if (metaPtr->ProcessCMD != cmdLine) {
                    // clear k8s info
                    metaPtr->Clear();
                    metaPtr->ProcessCMD = cmdLine;
                }
                ++iter;
            }
        }
    }
}

ProcessMetaPtr ContainerProcessGroupManager::GetProcessMeta(uint32_t pid) {
    auto findIter = mProcessMetaMap.find(pid);
    if (findIter != mProcessMetaMap.end()) {
        // todo 还需要检查插入时间，如果超过几分钟，还需要刷新一次PID列表
        return findIter->second;
    }
    auto path = ReadPidCgroupPath(pid);
    if (!path.empty()) {
        std::string containerID, podID;
        std::vector<int32_t> pids;
        auto res = this->ParseCgroupPath(path, pids, containerID, podID);
        K8sContainerMeta containerMeta;
        if (res == 0 && std::find(pids.begin(), pids.end(), pid) != pids.end()) {
            ++mProcessMetaStatistic->mCgroupPathTotalCount;
            mProcessMetaStatistic->mWatchProcessCount += pids.size();
            for (size_t i = 0; i < pids.size(); ++i) {
                if (i == 0) {
                    containerMeta = LogtailPlugin::GetInstance()->GetContainerMeta(containerID);
                    ++mProcessMetaStatistic->mFetchContainerMetaCount;
                    if (containerMeta.ContainerName.empty()) {
                        ++mProcessMetaStatistic->mFetchContainerMetaFailCount;
                    }
                    LOG_DEBUG(sLogger,
                              ("getMeta get container meta for pid", pid)("id", containerID)("meta",
                                                                                             containerMeta.ToString()));
                }
                ProcessMetaPtr meta = mProcessMetaMap.find(pids[i]) == mProcessMetaMap.end()
                    ? std::make_shared<ProcessMeta>()
                    : mProcessMetaMap[pids[i]];
                meta->Clear();
                meta->PID = pids[i];
                meta->Container.ContainerID = containerID;
                meta->Pod.PodUUID = podID;
                meta->Pod.NameSpace = containerMeta.K8sNamespace;
                meta->Pod.PodName = containerMeta.PodName;
                meta->Pod.WorkloadName = ExtractPodWorkloadName(containerMeta.PodName);
                meta->Container.ContainerName = containerMeta.ContainerName;
                meta->Container.Image = containerMeta.Image;
                meta->Pod.Labels = containerMeta.k8sLabels;
                meta->Container.Labels = containerMeta.containerLabels;
                meta->Container.Envs = containerMeta.envs;
                LOG_DEBUG(sLogger, ("getMeta insert process meta with container meta", pid));
                mProcessMetaMap.insert(std::make_pair(pids[i], meta));
            }
            return mProcessMetaMap[pid];
        } else if (res < -1) {
            ++mProcessMetaStatistic->mCgroupPathTotalCount;
            ++mProcessMetaStatistic->mCgroupPathParseFailCount;
        }
    }
    ProcessMetaPtr meta(new ProcessMeta);
    meta->PID = pid;
    std::string cmdLine;
    if (readCmdline(pid, cmdLine)) {
        meta->ProcessCMD = cmdLine;
    }
    ++mProcessMetaStatistic->mWatchProcessCount;
    LOG_DEBUG(sLogger, ("getMeta insert process meta with cmd", pid));
    mProcessMetaMap.insert(std::make_pair(pid, meta));
    return meta;
}

bool ContainerProcessGroupManager::readCmdline(uint32_t pid, std::string& cmdLine) {
    std::string cmdLinePath = std::string("/proc/").append(std::to_string(pid)).append("/cmdline");
    if (!ReadFileContent(cmdLinePath, cmdLine, 1024)) {
        return false;
    }
    for (char& i : cmdLine) {
        if (i == '\0')
            *(&i) = ' ';
    }
    auto position = cmdLine.find(' ');
    if (position != std::string::npos) {
        auto sub = cmdLine.substr(0, position);
        if (sub.find("java") == std::string::npos && sub.find("python") == std::string::npos) {
            cmdLine = std::move(sub);
        }
    }
    return true;
}
} // namespace logtail
