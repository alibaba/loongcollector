/*
 * Copyright 2023 iLogtail Authors
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

#include "LabelingK8sMetadata.h"

#include <vector>
#include <string>
#include "common/ParamExtractor.h"
#include "logger/Logger.h"
#include "models/MetricEvent.h"
#include "models/SpanEvent.h"
#include "monitor/metric_constants/MetricConstants.h"
#include <boost/utility/string_view.hpp>
#include "K8sMetadata.h"

using logtail::StringView; 

namespace logtail {

void LabelingK8sMetadata::AddLabelToLogGroup(PipelineEventGroup& logGroup) {
    if (logGroup.GetEvents().empty()) {
        return;
    }
    EventsContainer& events = logGroup.MutableEvents();
    std::vector<std::string> containerVec;
    std::vector<std::string> remoteIpVec;
    std::vector<size_t>  cotainerNotTag;
    for (size_t rIdx = 0; rIdx < events.size(); ++rIdx) {
        if (!ProcessEvent(events[rIdx], containerVec, remoteIpVec)) {
            cotainerNotTag.push_back(rIdx);
        }
    }
    auto& k8sMetadata = K8sMetadata::GetInstance();
    if (containerVec.empty() || (!k8sMetadata.GetByContainerIdsFromServer(containerVec))) {
        return;
    }
    if (remoteIpVec.empty() || (!k8sMetadata.GetByIpsFromServer(remoteIpVec))) {
        return;
    }
    for (size_t i = 0; i < cotainerNotTag.size(); ++i) {
        ProcessEvent(events[i], containerVec, remoteIpVec);
    }
    return;
}

bool LabelingK8sMetadata::ProcessEvent(PipelineEventPtr& e, std::vector<std::string>& containerVec, std::vector<std::string>& remoteIpVec) {
    if (!IsSupportedEvent(e)) {
        return true;
    }

    if (e.Is<MetricEvent>()) {
        return AddLabels(e.Cast<MetricEvent>(), containerVec, remoteIpVec);
    } else if (e.Is<SpanEvent>()) {
        return AddLabels(e.Cast<SpanEvent>(), containerVec, remoteIpVec);
    }

    return true;
}

template <typename Event>
bool LabelingK8sMetadata::AddLabels(Event& e, std::vector<std::string>& containerVec, std::vector<std::string>& remoteIpVec) {
    bool res = true;
    
    auto& k8sMetadata = K8sMetadata::GetInstance();
    StringView containerIdViewKey(containerIdKey);
    StringView containerIdView = e.HasTag(containerIdViewKey) ? e.GetTag(containerIdViewKey) : StringView{};
    if (!containerIdView.empty()) {
        std::string containerId(containerIdView);
        std::shared_ptr<k8sContainerInfo> containerInfo = k8sMetadata.GetInfoByContainerIdFromCache(containerId);
        if (containerInfo == nullptr) {
            containerVec.push_back(containerId);
            res = false;
        } else {
            e.SetTag(workloadNameKey, containerInfo->workloadName);
            e.SetTag(workloadKindKey, containerInfo->workloadKind);
            e.SetTag(namespaceKey, containerInfo->k8sNamespace);
            e.SetTag(serviceNameKey, containerInfo->serviceName);
            e.SetTag(pidKey, containerInfo->appId);
        }
    }
    StringView ipView(remoteIpKey);
    StringView remoteIpView = e.HasTag(ipView) ? e.GetTag(ipView) : StringView{};
    if (!remoteIpView.empty()) {
        std::string remoteIp(remoteIpView);
        std::shared_ptr<k8sContainerInfo> ipInfo = k8sMetadata.GetInfoByIpFromCache(remoteIp);
        if (ipInfo == nullptr) {
            remoteIpVec.push_back(remoteIp);
            res = false;
        } else {
            e.SetTag(peerWorkloadNameKey, ipInfo->workloadName);
            e.SetTag(peerWorkloadKindKey, ipInfo->workloadKind);
            e.SetTag(peerNamespaceKey, ipInfo->k8sNamespace);
        }
    }
    return res;
}

bool LabelingK8sMetadata::IsSupportedEvent(const PipelineEventPtr& e) const {
    return e.Is<MetricEvent>() || e.Is<SpanEvent>();
}

} // namespace logtail
