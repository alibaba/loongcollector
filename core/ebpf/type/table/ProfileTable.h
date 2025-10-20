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

constexpr DataElement kPid = {
    "pid",
    "pid", // metric
    "pid", // span
    "pid", // log
    "process pid",
};

constexpr DataElement kComm = {
    "comm",
    "comm", // metric
    "comm", // span
    "comm", // log
    "process command name",
};

constexpr DataElement kStack = {
    "stack",
    "stack", // metric
    "stack", // span
    "stack", // log
    "call stack",
};

constexpr DataElement kCnt = {
    "cnt",
    "cnt", // metric
    "cnt", // span
    "cnt", // log
    "call stack count",
};

constexpr DataElement kTraceId = {
    "trace_id",
    "trace_id", // metric
    "trace_id", // span
    "trace_id", // log
    "call stack trace id",
};

} // namespace logtail::ebpf