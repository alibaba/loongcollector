/*
 * Copyright 2022 iLogtail Authors
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

#include "file_server/checkpoint/FileSystemUuid.h"

#include <fstream>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <vector>

#if defined(__linux__)
#include <limits.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#endif

#include "common/LogtailCommonFlags.h"

namespace logtail {
namespace {

std::mutex gCacheMutex;
std::unordered_map<uint64_t, std::string> gCache;
std::function<std::string(uint64_t)> gResolver;

std::string ReadFirstLine(const std::string& path) {
    std::ifstream input(path.c_str());
    if (!input) {
        return "";
    }
    std::string line;
    if (!std::getline(input, line)) {
        return "";
    }
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')) {
        line.pop_back();
    }
    return line;
}

#if defined(__linux__)
std::string DeviceNameFromDev(uint64_t dev) {
    const std::string link
        = "/sys/dev/block/" + std::to_string(gnu_dev_major(dev)) + ":" + std::to_string(gnu_dev_minor(dev));
    char buf[PATH_MAX + 1];
    const ssize_t len = readlink(link.c_str(), buf, PATH_MAX);
    if (len <= 0) {
        return "";
    }
    buf[len] = '\0';
    const std::string target(buf);
    const size_t slash = target.find_last_of('/');
    if (slash == std::string::npos) {
        return target;
    }
    return target.substr(slash + 1);
}

std::string FsTypeForDev(uint64_t dev) {
    const std::string majMin = std::to_string(gnu_dev_major(dev)) + ":" + std::to_string(gnu_dev_minor(dev));
    std::ifstream input("/proc/self/mountinfo");
    if (!input) {
        return "";
    }
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream fields(line);
        std::string mountId;
        std::string parentId;
        std::string majMinField;
        if (!(fields >> mountId >> parentId >> majMinField) || majMinField != majMin) {
            continue;
        }
        const size_t dash = line.find(" - ");
        if (dash == std::string::npos) {
            continue;
        }
        std::istringstream tail(line.substr(dash + 3));
        std::string fsType;
        if (tail >> fsType) {
            return fsType;
        }
    }
    return "";
}

std::string ReadUuidFromSysfs(uint64_t dev) {
    const std::string deviceName = DeviceNameFromDev(dev);
    if (deviceName.empty()) {
        return "";
    }
    const std::string fsType = FsTypeForDev(dev);
    std::vector<std::string> sysTypes;
    if (fsType == "ext2" || fsType == "ext3" || fsType == "ext4") {
        sysTypes.push_back("ext4");
    } else if (fsType == "xfs") {
        sysTypes.push_back("xfs");
    } else if (fsType.empty()) {
        sysTypes.push_back("ext4");
        sysTypes.push_back("xfs");
    } else {
        return "";
    }

    std::vector<std::string> roots;
    roots.push_back("/sys/fs");
    const std::string hostRoot = STRING_FLAG(default_container_host_path);
    if (!hostRoot.empty()) {
        roots.push_back(hostRoot + "/sys/fs");
    }
    for (size_t i = 0; i < roots.size(); ++i) {
        for (size_t j = 0; j < sysTypes.size(); ++j) {
            const std::string uuid = ReadFirstLine(roots[i] + "/" + sysTypes[j] + "/" + deviceName + "/uuid");
            if (!uuid.empty()) {
                return uuid;
            }
        }
    }
    return "";
}
#endif

} // namespace

std::string GetFileSystemUuid(uint64_t dev) {
    std::function<std::string(uint64_t)> resolver;
    {
        std::lock_guard<std::mutex> guard(gCacheMutex);
        resolver = gResolver;
        if (!resolver) {
            const auto cached = gCache.find(dev);
            if (cached != gCache.end()) {
                return cached->second;
            }
        }
    }
    if (resolver) {
        return resolver(dev);
    }
    std::string uuid;
#if defined(__linux__)
    uuid = ReadUuidFromSysfs(dev);
#endif
    std::lock_guard<std::mutex> guard(gCacheMutex);
    gCache[dev] = uuid;
    return uuid;
}

void SetFileSystemUuidResolverForTest(const std::function<std::string(uint64_t)>& resolver) {
    std::lock_guard<std::mutex> guard(gCacheMutex);
    gResolver = resolver;
    gCache.clear();
}

void ClearFileSystemUuidCacheForTest() {
    std::lock_guard<std::mutex> guard(gCacheMutex);
    gResolver = std::function<std::string(uint64_t)>();
    gCache.clear();
}

} // namespace logtail
