// Copyright 2024 iLogtail Authors
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

#include "collection_pipeline/batch/TimeoutFlushManager.h"

using namespace std;

namespace logtail {

void TimeoutFlushManager::UpdateRecord(
    const string& config, size_t index, size_t key, uint32_t timeoutSecs, Flusher* f) {
    lock_guard<mutex> lock(mTimeoutRecordsMux);
    auto& item = mTimeoutRecords[config];
    auto it = item.find({index, key});
    if (it == item.end()) {
        item.try_emplace({index, key}, f, key, timeoutSecs);
    } else {
        it->second.Update();
    }
}

void TimeoutFlushManager::FlushTimeoutBatch() {
    multimap<string, pair<Flusher*, size_t>> records;
    {
        lock_guard<mutex> lock(mTimeoutRecordsMux);
        for (auto& item : mTimeoutRecords) {
            for (auto it = item.second.begin(); it != item.second.end();) {
                if (time(nullptr) - it->second.mUpdateTime >= it->second.mTimeoutSecs) {
                    // cannot flush here, since flush may also update record, which might invalidate map iterator and
                    // lead to deadlock
                    records.emplace(item.first, make_pair(it->second.mFlusher, it->second.mKey));
                    it = item.second.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
    {
        lock_guard<mutex> lock(mDeletedFlushersMux);
        for (auto& item : records) {
            if (mDeletedFlushers.find(make_pair(item.first, item.second.first)) == mDeletedFlushers.end()) {
                item.second.first->Flush(item.second.second);
            }
        }
        mDeletedFlushers.clear();
    }
}

void TimeoutFlushManager::UnregisterFlushers(const string& config,
                                             const vector<unique_ptr<FlusherInstance>>& flushers) {
    {
        lock_guard<mutex> lock(mTimeoutRecordsMux);
        mTimeoutRecords.erase(config);
    }
    {
        lock_guard<mutex> lock(mDeletedFlushersMux);
        for (const auto& flusher : flushers) {
            mDeletedFlushers.emplace(make_pair(config, flusher->GetPlugin()));
        }
    }
}

void TimeoutFlushManager::RegisterFlushers(const string& config, const vector<unique_ptr<FlusherInstance>>& flushers) {
    lock_guard<mutex> lock(mDeletedFlushersMux);
    for (const auto& flusher : flushers) {
        mDeletedFlushers.erase(make_pair(config, flusher->GetPlugin()));
    }
}

} // namespace logtail
