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

constexpr DataElement kProfileID = {
    "profileID",
    "profile_id", // metric
    "profile.id", // span
    "profile.id", // log
    "profile ID",
};

constexpr DataElement kProfileDataType = {
    "dataType",
    "data_type", // metric
    "data.type", // span
    "data.type", // log
    "profile data type",
};

constexpr DataElement kProfileLanguage = {
    "language",
    "language", // metric
    "language", // span
    "language", // log
    "programming language",
};

constexpr DataElement kName = {
    "name",
    "name", // metric
    "name", // span
    "name", // log
    "profile name",
};

constexpr DataElement kStack = {
    "stack",
    "stack", // metric
    "stack", // span
    "stack", // log
    "call stack",
};

constexpr DataElement kStackID = {
    "stackID",
    "stackID", // metric
    "stackID", // span
    "stackID", // log
    "call stack ID",
};

constexpr DataElement kType = {
    "type",
    "type", // metric
    "type", // span
    "type", // log
    "profile type",
};

constexpr DataElement kTypeCN = {
    "type_cn",
    "type_cn", // metric
    "type_cn", // span
    "type_cn", // log
    "profile type in Chinese",
};

constexpr DataElement kUnits = {
    "units",
    "units", // metric
    "units", // span
    "units", // log
    "profile units",
};

constexpr DataElement kVal = {
    "val",
    "val", // metric
    "val", // span
    "val", // log
    "profile value",
};

constexpr DataElement kValueTypes = {
    "valueTypes",
    "valueTypes", // metric
    "valueTypes", // span
    "valueTypes", // log
    "profile value types",
};

constexpr DataElement kValueTypesCN = {
    "valueTypes_cn",
    "valueTypes_cn", // metric
    "valueTypes_cn", // span
    "valueTypes_cn", // log
    "profile value types in Chinese",
};

constexpr DataElement kLabels = {
    "labels",
    "labels", // metric
    "labels", // span
    "labels", // log
    "profile labels",
};

} // namespace logtail::ebpf
