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

namespace logtail {
namespace ebpf {

inline constexpr DataElement kProcessCacheElements[] = {
    kExecId, kProcessId, kUid, kUser, kBinary, kArguments, kCWD, kKtime, kCapInheritable, kCapPermitted, kCapEffective};

inline constexpr size_t kProcessCacheTableSize = std::size(kProcessCacheElements);

inline constexpr auto kProcessCacheTable = DataTableSchema("process_cache_table", "", kProcessCacheElements);

// inline constexpr DataElement kProcessSecurityElements[] = {kExecId,
//                                                            kProcessId,
//                                                            kUid,
//                                                            kUser,
//                                                            kBinary,
//                                                            kArguments,
//                                                            kCWD,
//                                                            kKtime,
//                                                            kCapInheritable,
//                                                            kCapPermitted,
//                                                            kCapEffective,
//                                                            kParentProcessId,
//                                                            kParentUid,
//                                                            kParentUser,
//                                                            kParentBinary,
//                                                            kParentArguments,
//                                                            kParentCWD,
//                                                            kParentKtime,
//                                                            kParentCapInheritable,
//                                                            kParentCapPermitted,
//                                                            kParentCapEffective,
//                                                            kContainerId,
//                                                            kEventTime,
//                                                            kCallName,
//                                                            kEventType};

// inline constexpr size_t kProcessSecurityTableSize = std::size(kProcessSecurityElements);

// inline constexpr auto kProcessSecurityTable = DataTableSchema("process_security_table", "",
// kProcessSecurityElements);

} // namespace ebpf
} // namespace logtail
