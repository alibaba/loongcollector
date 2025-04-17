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

#include "ebpf/plugin/ProcessCache.h"

#include "ebpf/type/table/DataTable.h"
#include "logger/Logger.h"
#include "type/table/ProcessTable.h"

namespace logtail {

// avoid infinite execve to cause memory leak
static long kMaxProcessCacheValueSourceBufferReuse = 100;
ProcessCacheValue* ProcessCacheValue::CloneContents() {
    auto* newValue = new ProcessCacheValue();
    if (GetSourceBuffer().use_count() < kMaxProcessCacheValueSourceBufferReuse) {
        newValue->mContents = mContents;
    } else {
        for (size_t i = 0; i < mContents.Size(); ++i) {
            StringBuffer cp = newValue->GetSourceBuffer()->CopyString(mContents[i]);
            newValue->mContents[i] = {cp.data, cp.size};
        }
    }
    return newValue;
}

ProcessCache::ProcessCache(size_t initCacheSize) {
    mCache.reserve(initCacheSize);
}

bool ProcessCache::Contains(const data_event_id& key) const {
    std::lock_guard<std::mutex> lock(mCacheMutex);
    return mCache.find(key) != mCache.end();
}

std::shared_ptr<ProcessCacheValue> ProcessCache::Lookup(const data_event_id& key) {
    std::lock_guard<std::mutex> lock(mCacheMutex);
    auto it = mCache.find(key);
    if (it != mCache.end()) {
        return it->second;
    }
    return nullptr;
}

size_t ProcessCache::Size() const {
    std::lock_guard<std::mutex> lock(mCacheMutex);
    return mCache.size();
}

void ProcessCache::removeCache(const data_event_id& key) {
    std::lock_guard<std::mutex> lock(mCacheMutex);
    mCache.erase(key);
}

void ProcessCache::AddCache(const data_event_id& key, std::shared_ptr<ProcessCacheValue>&& value) {
    value->IncRef();
    std::lock_guard<std::mutex> lock(mCacheMutex);
    mCache.emplace(key, std::move(value));
}

void ProcessCache::IncRef(const data_event_id& key) {
    auto v = Lookup(key);
    if (v) {
        v->IncRef();
    }
}

void ProcessCache::DecRef(const data_event_id& key, time_t curktime) {
    auto v = Lookup(key);
    if (v) {
        if (v->DecRef() == 0) {
            enqueueExpiredEntry(key, curktime);
        }
    }
}

void ProcessCache::enqueueExpiredEntry(const data_event_id& key, time_t curktime) {
    mCacheExpireQueue.emplace_back(ExitedEntry{curktime, key});
}

void ProcessCache::ClearCache() {
    std::lock_guard<std::mutex> lock(mCacheMutex);
    mCache.clear();
}


void ProcessCache::ClearExpiredCache(time_t ktime) {
    ktime -= kMaxCacheExpiredTimeout;
    if (mCacheExpireQueue.empty() || mCacheExpireQueue.front().time > ktime) {
        return;
    }
    while (!mCacheExpireQueue.empty() && mCacheExpireQueue.front().time <= ktime) {
        auto& key = mCacheExpireQueue.front().key;
        LOG_DEBUG(sLogger, ("[RecordExecveEvent][DUMP] clear expired cache pid", key.pid)("ktime", key.time));
        removeCache(key);
        mCacheExpireQueue.pop_front();
    }
}

void ProcessCache::PrintDebugInfo() {
    for (const auto& [key, value] : mCache) {
        LOG_ERROR(sLogger, ("[DUMP CACHE] pid", key.pid)("ktime", key.time));
    }
    for (const auto& entry : mCacheExpireQueue) {
        LOG_ERROR(sLogger,
                  ("[DUMP EXPIRE Q] pid", entry.key.pid)("ktime", entry.key.time)("enqueue ktime", entry.time));
    }
}

} // namespace logtail
