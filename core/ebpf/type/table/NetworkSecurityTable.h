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

static constexpr DataElement kNetworkSecurityElements[] = {
    kExecId,
    kProcessId,
    kUid,
    kUser,
    kBinary,
    kArguments,
    kCWD,
    kKtime,
    kCapInheritable,
    kCapPermitted,
    kCapEffective,
    kParentProcessId,
    kParentUid,
    kParentUser,
    kParentBinary,
    kParentArguments,
    kParentCWD,
    kParentKtime,
    kParentCapInheritable,
    kParentCapPermitted,
    kParentCapEffective,
    kContainerId, // ?
    kEventTime,
    kCallName,
    kEventType,

    // for network
    kSaddr,
    kSport,
    kDaddr,
    kDport,
    kState,
    kL4Protocol,
};

static constexpr size_t kNetworkSecurityTableSize = std::size(kNetworkSecurityElements);

static constexpr auto kNetworkSecurityTable = DataTableSchema("network_security_table", "", kNetworkSecurityElements);

} // namespace ebpf
} // namespace logtail
