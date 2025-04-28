// Copyright 2025 iLogtail Authors
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

#pragma once

#include "ebpf/type/table/BaseElements.h"
#include "ebpf/type/table/DataTable.h"

namespace logtail::ebpf {

inline constexpr DataElement kConnTrackerElements[] = {
    kHostName,
    kAppName,
    kAppId,
    kPodName,
    kPodIp,
    kNamespace,
    kWorkloadKind,
    kWorkloadName,
    kPeerAppName,
    kPeerPodName,
    kPeerPodIp,
    kPeerNamespace,
    kPeerWorkloadKind,
    kPeerWorkloadName,
    kPeerServiceName,
    kProtocol,
    kLocalAddr,
    kRemoteAddr,
    kRemotePort,
    kRpcType,
    kCallKind,
    kCallType,
    kDestId,
    kEndpoint,
    kFd,
    kProcessId,
    kContainerId,
    kTraceRole,
    kIp,
    kRemoteIp,
    kNetNs,
    kFamily,
    kStartTsNs,
};

inline constexpr size_t kConnTrackerElementsTableSize = std::size(kConnTrackerElements);

inline constexpr auto kConnTrackerTable = DataTableSchema("conn_tracker_table", "", kConnTrackerElements);

inline constexpr DataElement kAppMetricsElements[] = {
    kHostName, // host
    kAppId, // pid
    kIp, // server ip
    kAppName, // service

    kNamespace,
    kWorkloadKind,
    kWorkloadName,

    kProtocol,
    kRpc,
    kDestId,
    kRpcType,
    kCallType,
    kCallKind,
    kEndpoint,
};

inline constexpr size_t kAppMetricsNum = std::size(kAppMetricsElements);

inline constexpr auto kAppMetricsTable = DataTableSchema("app_metrics", "app metrics table", kAppMetricsElements);

} // namespace logtail::ebpf
