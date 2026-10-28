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

constexpr DataElement kDBSystemName = {
    "db_system_name",
    "db_system_name", // metric
    "db.system.name", // span
    "db.system.name", // log
    "db system name", // description or display name
};

constexpr DataElement kDBStatement = {
    "db_statement",
    "db_statement", // metric
    "db.statement", // span
    "db.statement", // log
    "db statement", // description or display name
};

constexpr DataElement kDBResponseStatusCode = {
    "db_response_status_code",
    "db_response_status_code", // metric
    "db.response.status_code", // span
    "db.response.status_code", // log
    "db response status code", // description or display name
};

constexpr DataElement kDBResponseMessage = {
    "db_response_message",
    "db_response_message", // metric
    "db.response.message", // span
    "db.response.message", // log
    "db response message", // description or display name
};

} // namespace logtail::ebpf
