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

#include "ebpf/type/table/DataTable.h"
#include "ebpf/type/table/BaseElements.h"

namespace logtail{
namespace ebpf {

static constexpr DataElement kNetMetricsElements [] = {

    kHost, // host
    kAppId, // pid
    kIp, // server ip
    kAppName, // service

    kWorkloadKind,
    kWorkloadName,
    kPeerWorkloadKind,
    kPeerWorkloadName,
    /* non-aggregate keys */
};

static constexpr auto kNetMetricsTable =
    DataTableSchema("net_metrics", "net metrics table", kNetMetricsElements);

static constexpr size_t kNetMetricsNum = std::size(kNetMetricsElements);

static constexpr DataElement kNetElements[] = {
    kIp, 
    kAppId,

    kLocalAddr,
    kRemoteAddr,
    kRemotePort,
};

static constexpr auto kNetTable =
    DataTableSchema("net_record", "net events", kNetElements);

} 
}
