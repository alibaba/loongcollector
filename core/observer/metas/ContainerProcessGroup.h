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

#include "ProcessMeta.h"
#include "common/Thread.h"
#include "common/Lock.h"
#include "CGroupPathResolver.h"
#include "interface/statistics.h"

#include <network/protocols/ProtocolEventAggregators.h>
#include <sls_logs.pb.h>
#include <unordered_set>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <ostream>

namespace logtail {


struct ContainerProcessGroup {
    explicit ContainerProcessGroup(const ProcessMetaPtr& meta) : mMetaPtr(meta) { mAggregator.SetProcessMeta(meta); }

    void AddProcess(uint32_t pid) { mAllProcesses.insert(pid); }

    /**
     * @brief RemoveProcess
     * @param pid
     * @return all processes empty flag
     */
    bool RemoveProcess(uint32_t pid) {
        mAllProcesses.erase(pid);
        return mAllProcesses.empty();
    }

    void FlushOutMetrics(uint64_t timeNano,
                         std::vector<sls_logs::Log>& allData,
                         std::vector<std::pair<std::string, std::string>>& tags,
                         uint64_t interval);

    std::unordered_set<uint32_t> mAllProcesses;
    ProcessMetaPtr mMetaPtr;
    ProtocolEventAggregators mAggregator;
};

typedef std::shared_ptr<ContainerProcessGroup> ContainerProcessGroupPtr;

/**
 * @brief The ContainerProcessGroupManager class
 * 用来管理所有的容器进程组。内部管理所有pid对应的container映射关系，并支持动态/按需更新。工作主要如下：
 * * 新的PID创建时，判断其所属的ContainerProcessGroup，返回ContainerProcessGroup实例（不存在创建）
 * * 当PID不属于某个cgroup时，返回一个新的ContainerProcessGroup
 * * 当Container所有的进程销毁时，销毁对应的ContainerProcessGroup实例
 */
class ContainerProcessGroupManager {
public:
    static ContainerProcessGroupManager* GetInstance() {
        static ContainerProcessGroupManager* sManager = new ContainerProcessGroupManager;
        return sManager;
    }

    bool Init(const std::string& cgroupPath = "/sys/fs");

    void ResetFilterProcessMeta() {
        for (const auto& item : this->mProcessMetaMap) {
            item.second->ResetFilter();
        }
    }


    /**
     * @brief GetProcessMeta
     * @param pid
     * @return
     */
    ProcessMetaPtr GetProcessMeta(uint32_t pid);


    /**
     * @brief GetContainerProcessGroupPtr 判断其所属的ContainerProcessGroup，返回ContainerProcessGroup实例（不存在创建）
     * @note 当PID不属于某个cgroup时，返回一个新的ContainerProcessGroup
     * @param pid
     * @return
     */
    ContainerProcessGroupPtr GetContainerProcessGroupPtr(const ProcessMetaPtr& meta, uint32_t pid) {
        const std::string& containerID = meta->Container.ContainerID;
        if (containerID.empty()) {
            auto findIter = mPureProcessGroupMap.find(meta->PID);
            if (findIter != mPureProcessGroupMap.end()) {
                return findIter->second;
            }
            ContainerProcessGroupPtr newPtr(new ContainerProcessGroup(meta));
            mPureProcessGroupMap.insert(std::make_pair(meta->PID, newPtr));
            return newPtr;
        }
        auto findIter = mContainerProcessGroupMap.find(containerID);
        if (findIter != mContainerProcessGroupMap.end()) {
            findIter->second->AddProcess(pid);
            return findIter->second;
        }
        ContainerProcessGroupPtr newPtr(new ContainerProcessGroup(meta));
        newPtr->AddProcess(pid);
        mContainerProcessGroupMap.insert(std::make_pair(containerID, newPtr));
        return newPtr;
    }

    /**
     * @brief OnProcessDestroy 当进程销毁（一段时间没有数据流动也算销毁）的时候需要调用
     * @note 不能使用meta中的PID，meta可能属于容器内的其他进程
     * @param pid
     */
    void OnProcessDestroy(ProcessMeta* meta, uint32_t pid) {
        const std::string& containerID = meta->Container.ContainerID;
        if (containerID.empty()) {
            // only delete pid without container
            mProcessMetaMap.erase(pid);
            mPureProcessGroupMap.erase(pid);
        } else {
            auto findRst = mContainerProcessGroupMap.find(containerID);
            if (findRst != mContainerProcessGroupMap.end()) {
                bool needDelete = findRst->second->RemoveProcess(pid);
                if (needDelete) {
                    mContainerProcessGroupMap.erase(findRst);
                }
            }
        }
    }

    void FlushOutMetrics(std::vector<sls_logs::Log>& allData,
                         std::vector<std::pair<std::string, std::string>>& tags,
                         uint64_t interval);


    void FlushMetas();

    std::string GetContainerType() { return ContainerTypeToString(this->mContainerType); };

protected:
    void FlushPids(const std::unordered_set<uint32_t>& existedPids);

    // 0 means success, -1 means ignored path,
    int32_t
    ParseCgroupPath(const std::string& path, std::vector<int32_t>& pids, std::string& containerID, std::string& podID);

    int32_t DetectContainerType(const std::string& path);

    bool readCmdline(uint32_t pid, std::string& cmdLine);

private:
    ContainerProcessGroupManager() { mProcessMetaStatistic = ProcessMetaStatistic::GetInstance(); }

    ProcessMetaStatistic* mProcessMetaStatistic;
    // 从ContainerCenter同步过来的所有容器对应PID列表
    std::unordered_map<uint32_t, ProcessMetaPtr> mProcessMetaMap;
    uint32_t mLastNormalProcessMetaUpdateTime = 0;

    // 保存所有已经创建的GroupPtr
    // 这两个Map只有在实际有数据到的时候才会初始化，否则是空的
    // ContainerProcessGroupPtr 中除了引用ProcessMeta外，还有最关键的Aggregator信息
    // 需要注意，这两个Map的GC尤为重要
    std::unordered_map<uint32_t, ContainerProcessGroupPtr> mPureProcessGroupMap;
    std::unordered_map<std::string, ContainerProcessGroupPtr> mContainerProcessGroupMap;


    std::string mGgoupBasePath;
    // matcher and containerType would be kept in the whole life cycle;
    KubernetesCGroupPathMatcher* mMatcher = NULL;
    CONTAINER_TYPE mContainerType = CONTAINER_TYPE_UNKNOWN;
};

} // namespace logtail
