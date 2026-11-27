/*
 * Copyright 2024 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cstdint>

#include <string>

namespace logtail {

// Read the CPU hard limit (quota / period) of the cgroup hierarchy rooted at @root, in cores.
// Tries cgroup v2 (cpu.max) first, then v1 (cpu.cfs_quota_us / cpu.cfs_period_us).
// Returns false when the limit is absent, unlimited or malformed; the caller keeps its fallback.
bool ReadCgroupCpuLimit(const std::string& root, float& cores);

// Read the memory hard limit of the cgroup hierarchy rooted at @root, in MiB.
// Tries cgroup v2 (memory.max) first, then v1 (memory.limit_in_bytes).
// Returns false when the limit is absent, unlimited or malformed; the caller keeps its fallback.
bool ReadCgroupMemLimit(const std::string& root, int64_t& memMiB);

} // namespace logtail
