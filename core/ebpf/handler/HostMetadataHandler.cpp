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

#include "ebpf/handler/HostMetadataHandler.h"
#include "ebpf/include/export.h"
#include "ebpf/Config.h"
#include "pipeline/PipelineContext.h"
#include "pipeline/queue/ProcessQueueItem.h"
#include "pipeline/queue/ProcessQueueManager.h"
#include "metadata/K8sMetadata.h"
#include "common/magic_enum.hpp"

namespace logtail {
namespace ebpf {

HostMetadataHandler::HostMetadataHandler(const logtail::PipelineContext* ctx, QueueKey key, uint32_t idx, int intervalSec)
    : AbstractHandler(ctx, key, idx), mIntervalSec(intervalSec) {
    mFlag = true;
    // TODO @qianlu.kk we need to move this into start function
    mReporter = std::thread(&HostMetadataHandler::ReportAgentInfo, this);
}

HostMetadataHandler::~HostMetadataHandler() {
    mFlag = false;
    if (mReporter.joinable()) {
        mReporter.join();
    }
}

const std::string pidKey = "pid";
const std::string appNameKey = "appName";
const std::string ipKey = "ip";
const std::string hostNameKey = "hostname";
const std::string agentVersionKey = "agentVersion";
const std::string startTimestampKey = "startTimeStamp";
const std::string dataTypeKey = "data_type";
const std::string agentInfoStr = "agent_info";

void HostMetadataHandler::ReportAgentInfo() {
    while(mFlag) {
        std::shared_ptr<SourceBuffer> sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        eventGroup.SetTag(dataTypeKey, agentInfoStr);
        auto nowSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        {
            ReadLock lk(mLock);
            for (const auto& [ip, spi] : mHostPods) {
                // generate agentinfo
                // per min
                LOG_DEBUG(sLogger, ("AGENT_INFO appid", spi->mAppId)
                    ("appname", spi->mAppName)
                    ("ip", spi->mPodIp) ("hostname", spi->mPodName) ("agentVersion", "1.0.0") ("startTs", std::to_string(spi->mStartTime)));
                auto logEvent = eventGroup.AddLogEvent();
                logEvent->SetContent(pidKey, spi->mAppId);
                logEvent->SetContent(appNameKey, spi->mAppName);
                logEvent->SetContent(ipKey, spi->mPodIp);
                logEvent->SetContent(hostNameKey, spi->mPodName);
                logEvent->SetContent(agentVersionKey, "1.0.0");
                logEvent->SetContent(startTimestampKey, std::to_string(spi->mStartTime));
                logEvent->SetTimestamp(nowSec);

                // every 15s

                // TODO @qianlu.kk generate host tags
                // auto metricEvent = eventGroup.AddMetricEvent();
                // metricEvent->SetName("arms_tag_entity");
                // metricEvent->SetTagNoCopy("hostname", "");
                // metricEvent->SetValue(UntypedSingleValue{1.0});
            }
        }

        {
            ReadLock lk(mCtxLock);
            if (!mCtx) {
                continue;
            }

            std::unique_ptr<ProcessQueueItem> item = std::make_unique<ProcessQueueItem>(std::move(eventGroup), mPluginIdx);
            auto res = ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item));
            if (res == QueueStatus::OK) {
                LOG_WARNING(sLogger, ("[AgentInfo] push queue failed! status", magic_enum::enum_name(res)));
            } else {
                LOG_DEBUG(sLogger, ("[AgentInfo] push queue success!", ""));
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(mIntervalSec));
    }
}

bool HostMetadataHandler::handle(uint32_t pluginIndex, std::vector<std::string>& podIpVec) {
    std::vector<std::string> newContainerIds;
    std::vector<std::string> expiredContainerIds;
    std::unordered_set<std::string> newPodIps;
    std::vector<std::string> addedContainers;
    std::vector<std::string> removedContainers;

    std::unordered_map<std::string, std::unique_ptr<SimplePodInfo>> currentHostPods;
    std::unordered_set<std::string> currentContainers;


    for (const auto& ip : podIpVec) {
        auto podInfo = K8sMetadata::GetInstance().GetInfoByIpFromCache(ip);
        if (!podInfo || podInfo->appId == "") {
            // filter appid ...
            LOG_DEBUG(sLogger, (ip, "cannot fetch pod metadata or doesn't have arms label"));
            continue;
        }

        std::unique_ptr<SimplePodInfo> spi = std::make_unique<SimplePodInfo>(
            uint64_t(podInfo->timestamp), 
            podInfo->appId,
            podInfo->appName,
            podInfo->podIp, 
            podInfo->podName,
            podInfo->containerIds);
        currentHostPods[ip] = std::move(spi);
        std::string cids;
        for (auto& cid : podInfo->containerIds) {
            cids += cid + ",";
            currentContainers.insert(cid);
            if (!mCids.count(cid)) {
                // if cid doesn't exist in last cid set
                addedContainers.push_back(cid);
            }
        }
        LOG_DEBUG(sLogger, ("appId", podInfo->appId) ("appName", podInfo->appName) ("podIp", podInfo->podIp) ("podName", podInfo->podName) ("containerId", cids));
    }

    LOG_DEBUG(sLogger, ("begin to update local host metadata, pod list size:", newPodIps.size()) ("input pod list size", podIpVec.size()) ("host pod list size", mHostPods.size()));

    for (auto& cid : mCids) {
        if (!currentContainers.count(cid)) {
            // if cid doesn't exist in current container ids list
            removedContainers.push_back(cid);
        }
    }

    // update cids ...
    mCids = std::move(currentContainers);

    {
        WriteLock lk(mLock);
        // update cache ... 
        mHostPods = std::move(currentHostPods);
    }

    std::string addPodsStr;
    for (auto& cid : addedContainers) {
        addPodsStr += cid;
        addPodsStr += ",";
    }

    std::string removePodsStr;
    for (auto& cid : removedContainers) {
        removePodsStr += cid;
        removePodsStr += ",";
    }

    std::string hostPodsStr;
    for (auto& pod : mHostPods) {
        hostPodsStr += pod.first;
        hostPodsStr += ",";
    }
    if (removedContainers.size() || addedContainers.size()) {
        ObserverNetworkOption ops;
        ops.mDisableCids = removedContainers;
        ops.mEnableCids = addedContainers;
        ops.mEnableCidFilter = true;
        bool ret = mUpdateFunc(PluginType::NETWORK_OBSERVE, &ops);
        LOG_DEBUG(sLogger, ("after update, self pod list size", mHostPods.size()) 
            ("add cids", addPodsStr) 
            ("remove cids", removePodsStr) 
            ("host pods ip", hostPodsStr) 
            ("ret", ret) ("ops.mEnableCids.size", ops.mEnableCids.size()) ("ops.mDisableCids.size", ops.mDisableCids.size()));
        return ret;
    }    

    return true;
}

}
}
