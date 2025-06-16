
// Copyright 2025 LoongCollector Authors
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

#include <coolbpf/security/data_msg.h>

#include <mutex>

#include "common/ProcParser.h"
#include "ebpf/plugin/ProcessCacheValue.h"
#include "ebpf/plugin/ProcessDataMap.h"

namespace logtail {

class ProcessCache {
public:
    explicit ProcessCache(size_t maxCacheSize, ProcParser& procParser);

    // thread-safe
    bool Contains(const data_event_id& key) const;

    // thread-safe
    std::shared_ptr<ProcessCacheValue> Lookup(const data_event_id& key);

    size_t Size() const;

    // thread-safe, only single write call, but contention with read
    // will init ref count to 1
    void AddCache(const data_event_id& key, std::shared_ptr<ProcessCacheValue>& value);

    // will inc ref count by 1
    void IncRef(const data_event_id& key, std::shared_ptr<ProcessCacheValue>& value);
    // will dec ref count by 1, and if ref count is 0, will enqueueExpiredEntry
    void DecRef(const data_event_id& key, std::shared_ptr<ProcessCacheValue>& value);
    // thread-safe, only single write call, but contention with read
    void Clear();
    // NOT thread-safe, only single write call, no contention with read
    void ClearExpiredCache();

    void ForceShrink();

    void PrintDebugInfo();

private:
    // thread-safe, only single write call, but contention with read
    void removeCache(const data_event_id& key);
    // NOT thread-safe, only single write call, no contention with read
    void enqueueExpiredEntry(const data_event_id& key, std::shared_ptr<ProcessCacheValue>& value);

    ProcParser mProcParser;
    using ExecveEventMap = std::
        unordered_map<data_event_id, std::shared_ptr<ProcessCacheValue>, ebpf::DataEventIdHash, ebpf::DataEventIdEqual>;
    mutable std::mutex mCacheMutex;
    ExecveEventMap mCache;

    struct ExitedEntry {
        data_event_id key;
        std::shared_ptr<ProcessCacheValue> value;
    };

    std::mutex mCacheExpireQueueMutex;
    std::vector<ExitedEntry> mCacheExpireQueue;
    std::vector<ExitedEntry> mCacheExpireQueueProcessing;

    int64_t mLastForceShrinkTimeSec = 0;
};

} // namespace logtail
