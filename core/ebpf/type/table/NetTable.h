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

#pragma once

#include "ebpf/type/table/BaseElements.h"
#include "ebpf/type/table/DataTable.h"

namespace logtail {
namespace ebpf {

static constexpr DataElement kNetMetricsElements[] = {
    kHostName, // host
    kAppId, // pid
    kIp, // server ip
    kAppName, // service

    kNamespace,
    kPodName,
    kWorkloadKind,
    kWorkloadName,

    kPeerNamespace,
    kPeerPodName,
    kPeerWorkloadKind,
    kPeerWorkloadName,
};

static constexpr size_t kNetMetricsNum = std::size(kNetMetricsElements);

inline constexpr auto kNetMetricsTable = DataTableSchema("net_metrics", "net metrics table", kNetMetricsElements);

static constexpr DataElement kNetElements[] = {
    kIp,
    kAppId,

    kLocalAddr,
    kRemoteAddr,
    kRemotePort,
};

static constexpr auto kNetTable = DataTableSchema("net_record", "net events", kNetElements);

} // namespace ebpf
} // namespace logtail
