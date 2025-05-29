/*
 * Copyright 2025 iLogtail Authors
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

#include <mutex>

#include "ContainerInfo.h"
#include "common/LRUCache.h"
#include "common/StringView.h"

namespace logtail {

class ContainerMetadata {
public:
    explicit ContainerMetadata(size_t cidCacheSize = 1024);
    ContainerMetadata(const ContainerMetadata&) = delete;
    ContainerMetadata& operator=(const ContainerMetadata&) = delete;

    static ContainerMetadata& GetInstance() {
        static ContainerMetadata sInstance(1024);
        return sInstance;
    }

    bool Enable();
    std::shared_ptr<ContainerMeta> GetInfoByContainerId(const StringView& containerId);

private:
    lru11::Cache<StringView,
                 std::shared_ptr<ContainerMeta>,
                 std::mutex,
                 std::unordered_map<
                     StringView,
                     typename std::list<lru11::KeyValuePair<StringView, std::shared_ptr<ContainerMeta>>>::iterator,
                     StringViewHash,
                     StringViewEqual>>
        mContainerCache;
};

} // namespace logtail
