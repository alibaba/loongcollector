
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

#include <coolbpf/security/data_msg.h>
#include <cstdint>
#include <spdlog/fmt/bundled/format.h>

#include <atomic>
#include <deque>
#include <mutex>

#include "StringView.h"
#include "common/memory/SourceBuffer.h"
#include "ebpf/type/table/ProcessTable.h"

namespace logtail {

static constexpr size_t kInitCacheSize = 65536UL;
static constexpr auto kOneMinuteNanoseconds = std::chrono::minutes(1) / std::chrono::nanoseconds(1);
static constexpr time_t kMaxCacheExpiredTimeout = kOneMinuteNanoseconds;

static constexpr size_t kMaxInt32Width = 11;
static constexpr size_t kMaxInt64Width = 20;

/* This class is optimized for generate process log event
   Constent set copies only value in SourceBuffer as all keys are predefined StringViews
 */
class ProcessCacheValue {
public:
    ProcessCacheValue();
    const StringView& operator[](const ebpf::DataElement& key) const;

    ProcessCacheValue* CloneContents();

    void SetContentNoCopy(const ebpf::DataElement& key, const StringView& val);
    void SetContent(const ebpf::DataElement& key, const StringView& val);
    void SetContent(const ebpf::DataElement& key, const std::string& val);
    void SetContent(const ebpf::DataElement& key, const char* data, size_t len);
    void SetContent(const ebpf::DataElement& key, int32_t val);
    void SetContent(const ebpf::DataElement& key, uint32_t val);
    void SetContent(const ebpf::DataElement& key, int64_t val);
    void SetContent(const ebpf::DataElement& key, uint64_t val);
    void SetContent(const ebpf::DataElement& key, long long val);
    void SetContent(const ebpf::DataElement& key, unsigned long long val);

    std::array<StringView, ebpf::kProcessCacheTableSize> mContents;
    std::shared_ptr<SourceBuffer> mSourceBuffer;
    uint32_t mPPid = 0;
    uint64_t mPKtime = 0;
    int mRefCount = 0;
};

struct DataEventIdHash {
    std::size_t operator()(const data_event_id& deid) const { return deid.pid ^ ((deid.time >> 12) << 16); }
};

struct DataEventIdEqual {
    bool operator()(const data_event_id& lhs, const data_event_id& rhs) const {
        return lhs.pid == rhs.pid && lhs.time == rhs.time;
    }
};

class ProcessCache {
public:
    explicit ProcessCache(size_t initCacheSize = kInitCacheSize);

    // thread-safe
    bool Contains(const data_event_id& key) const;

    // thread-safe
    std::shared_ptr<ProcessCacheValue> Lookup(const data_event_id& key);

    size_t Size() const;

    // thread-safe, only single write call, but contention with read
    // will init ref count to 1
    void AddCache(const data_event_id& key, std::shared_ptr<ProcessCacheValue>&& value);
    // NOT thread-safe, only single write call, no contention with read
    // will inc ref count by 1
    void IncRef(const data_event_id& key);
    // NOT thread-safe, only single write call, no contention with read
    // will dec ref count by 1, and if ref count is 0, will enqueueExpiredEntry
    void DecRef(const data_event_id& key, time_t curktime);
    // thread-safe, only single write call, but contention with read
    void ClearCache();
    // NOT thread-safe, only single write call, no contention with read
    void ClearExpiredCache(time_t ktime);

private:
    // thread-safe, only single write call, but contention with read
    void removeCache(const data_event_id& key);
    // NOT thread-safe, only single write call, no contention with read
    void enqueueExpiredEntry(const data_event_id& key, time_t curktime);

    using ExecveEventMap
        = std::unordered_map<data_event_id, std::shared_ptr<ProcessCacheValue>, DataEventIdHash, DataEventIdEqual>;
    mutable std::mutex mCacheMutex;
    ExecveEventMap mCache;
    struct ExitedEntry {
        time_t time;
        data_event_id key;
    };
    std::deque<ExitedEntry> mCacheExpireQueue;
};

} // namespace logtail
