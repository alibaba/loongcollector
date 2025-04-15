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

constexpr DataElement kStatusCode = {
    "status_code",
    "status", // metric
    "http.status.code", // span
    "http.status.code", // log
    "status code",
};

constexpr DataElement kHTTPMethod = {
    "method",
    "http_method", // metric
    "http.method", // span
    "http.method", // log
    "http method",
};

constexpr DataElement kHTTPPath = {
    "path",
    "http_path", // metric
    "http.path", // span
    "http.path", // log
    "http path",
};

constexpr DataElement kHTTPVersion = {
    "version",
    "http_version", // metric
    "version", // span
    "http.version", // log
    "http version",
};

constexpr DataElement kHTTPReqBody = {
    "request_body",
    "http_req_body", // metric
    "req.body", // span
    "http.req.body", // log
    "http req.body",
};

constexpr DataElement kHTTPRespBody = {
    "response_body",
    "http_resp_body", // metric
    "resp.body", // span
    "http.resp.body", // log
    "http resp.body",
};

constexpr DataElement kHTTPReqBodySize = {
    "request_body_size",
    "http_req_body_size", // metric
    "http.req.body.size", // span
    "http.req.body.size", // log
    "http req.body.size",
};

constexpr DataElement kHTTPRespBodySize = {
    "response_body_size",
    "http_resp_body_size", // metric
    "http.resp.body.size", // span
    "http.resp.body.size", // log
    "http resp.body.size",
};

constexpr DataElement kHTTPReqHeader = {
    "request_header",
    "http_req_header", // metric
    "http.req.header", // span
    "http.req.header", // log
    "http req.header",
};

constexpr DataElement kHTTPRespHeader = {
    "response_header",
    "http_resp_header", // metric
    "http.resp.header", // span
    "http.resp.header", // log
    "http resp.header",
};

} // namespace logtail::ebpf
