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

#include "common/CgroupUtil.h"

#if defined(__linux__)

#include <cmath>

#include <vector>

#include "common/FileSystemUtil.h"
#include "common/StringTools.h"

namespace logtail {
namespace {

bool ReadTrimmedCgroupFile(const std::string& path, std::string& content) {
    if (ReadFileContent(path, content, kDefaultMaxFileSize) != FileReadResult::kOK) {
        return false;
    }
    content = TrimSpace(content);
    return !content.empty();
}

bool CoresFromQuotaPeriod(int64_t quota, int64_t period, float& cores) {
    if (quota <= 0 || period <= 0) {
        return false;
    }
    double c = static_cast<double>(quota) / static_cast<double>(period);
    if (!std::isfinite(c) || c <= 0) {
        return false;
    }
    cores = static_cast<float>(c);
    return true;
}

bool MemMiBFromBytes(int64_t bytes, int64_t& memMiB) {
    // Unlimited sentinels: v1 PAGE_COUNTER_MAX (9223372036854771712) and old-kernel LONG_MAX.
    if (bytes <= 0 || bytes >= (int64_t(1) << 62)) {
        return false;
    }
    memMiB = bytes / (1024 * 1024);
    return memMiB > 0;
}

// v2 cpu.max content: "<quota> <period>", where quota may be "max" (unlimited).
bool ParseCpuMax(const std::string& content, float& cores) {
    std::vector<StringView> tokens;
    for (const auto& token : StringViewSplitter(content, " ", true)) {
        tokens.push_back(token);
    }
    if (tokens.size() != 2 || tokens[0] == StringView("max")) {
        return false;
    }
    int64_t quota = 0;
    int64_t period = 0;
    if (!StringTo(tokens[0], quota) || !StringTo(tokens[1], period)) {
        return false;
    }
    return CoresFromQuotaPeriod(quota, period, cores);
}

bool ParseMemBytes(const std::string& content, int64_t& memMiB) {
    if (content == "max") {
        return false;
    }
    int64_t bytes = 0;
    if (!StringTo(content, bytes)) {
        return false;
    }
    return MemMiBFromBytes(bytes, memMiB);
}

} // namespace

bool ReadCgroupCpuLimit(const std::string& root, float& cores) {
    if (root.empty()) {
        return false;
    }
    // cgroup v2 unified hierarchy. If cpu.max exists, the cpu controller is on v2; do not fall back.
    std::string content;
    if (ReadTrimmedCgroupFile(PathJoin(root, "cpu.max"), content)) {
        return ParseCpuMax(content, cores);
    }
    // cgroup v1: the cpu controller may be mounted alone or co-mounted with cpuacct.
    for (const std::string sub : {"cpu", "cpu,cpuacct"}) {
        const std::string cpuDir = PathJoin(root, sub);
        std::string quotaContent;
        if (!ReadTrimmedCgroupFile(PathJoin(cpuDir, "cpu.cfs_quota_us"), quotaContent)) {
            continue;
        }
        int64_t quota = 0;
        if (!StringTo(quotaContent, quota) || quota == -1) {
            return false; // malformed or unlimited
        }
        std::string periodContent;
        int64_t period = 0;
        if (!ReadTrimmedCgroupFile(PathJoin(cpuDir, "cpu.cfs_period_us"), periodContent)
            || !StringTo(periodContent, period)) {
            return false;
        }
        return CoresFromQuotaPeriod(quota, period, cores);
    }
    return false;
}

bool ReadCgroupMemLimit(const std::string& root, int64_t& memMiB) {
    if (root.empty()) {
        return false;
    }
    std::string content;
    // cgroup v2.
    if (ReadTrimmedCgroupFile(PathJoin(root, "memory.max"), content)) {
        return ParseMemBytes(content, memMiB);
    }
    // cgroup v1.
    if (ReadTrimmedCgroupFile(PathJoin(PathJoin(root, "memory"), "memory.limit_in_bytes"), content)) {
        return ParseMemBytes(content, memMiB);
    }
    return false;
}

} // namespace logtail

#else

namespace logtail {

bool ReadCgroupCpuLimit(const std::string&, float&) {
    return false;
}

bool ReadCgroupMemLimit(const std::string&, int64_t&) {
    return false;
}

} // namespace logtail

#endif
