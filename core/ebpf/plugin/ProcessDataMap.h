
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
#include <cstddef>

#include <mutex>
#include <unordered_map>

namespace logtail::ebpf {

struct DataEventIdHash {
    std::size_t operator()(const data_event_id& deid) const { return deid.pid ^ ((deid.time >> 12) << 16); }
};

struct DataEventIdEqual {
    bool operator()(const data_event_id& lhs, const data_event_id& rhs) const {
        return lhs.pid == rhs.pid && lhs.time == rhs.time;
    }
};

class ProcessDataMap {
public:
    explicit ProcessDataMap(size_t maxSize) : mMaxSize(maxSize) {}

    size_t Size() const;
    // thread-safe
    void DataAdd(msg_data* data);
    void Clear();
    // thread-safe
    std::string DataGetAndRemove(const data_event_desc* desc);

    void PrintDebugInfo();

    inline static constexpr time_t kMaxCacheExpiredTimeoutNs = std::chrono::seconds(30) / std::chrono::nanoseconds(1);

private:
    // NOT thread-safe
    void clearExpiredData(time_t ktime);

    size_t mMaxSize;
    using DataEventMap = std::unordered_map<data_event_id, std::string, DataEventIdHash, DataEventIdEqual>;
    mutable std::mutex mDataMapMutex;
    DataEventMap mDataMap; // TODO：ebpf中也没区分filename和args，如果两者都超长会导致filename被覆盖为args
    std::chrono::time_point<std::chrono::system_clock> mLastClearTime;
};

} // namespace logtail::ebpf
