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

constexpr DataElement kStatusCode = {
    "status_code",
    "statusCode", // metric
    "http.status.code", // span
    "http.status.code", // log
    "status code",
};

static constexpr DataElement kHTTPElements[] = {
    kIp, // agg key
    kAppId, // agg key
    kProtocol, // agg key 
    kRpc, // agg key
    kDestId, // agg key
    kContainerId, // agg key
    kRpcType,
    kCallType,
    kCallKind,
    kEndpoint,
    kStatusCode,
    kStartTsNs,
    kEndTsNs
};

static constexpr auto kHTTPTable =
    DataTableSchema("http_record", "HTTP request-response pair events", kHTTPElements);

static constexpr int kHTTPPathIdx = kHTTPTable.ColIndex("path");

}