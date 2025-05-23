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

#include <algorithm>
#include <iterator>
#include <mutex>

#include "ProcessCacheValue.h"
#include "logger/Logger.h"

namespace logtail {

ProcessCache::ProcessCache(size_t maxCacheSize) {
    mCache.reserve(maxCacheSize);
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

void ProcessCache::AddCache(const data_event_id& key, std::shared_ptr<ProcessCacheValue>& value) {
    value->IncRef();
    std::lock_guard<std::mutex> lock(mCacheMutex);
    mCache.emplace(key, value);
}

void ProcessCache::IncRef(const data_event_id& key, std::shared_ptr<ProcessCacheValue>& value) {
    if (value) {
        value->IncRef();
    }
}

void ProcessCache::DecRef(const data_event_id& key, std::shared_ptr<ProcessCacheValue>& value) {
    if (value) {
        if (value->DecRef() == 0 && value->LifeStage() != ProcessCacheValue::LifeStage::kDeleted) {
            value->SetLifeStage(ProcessCacheValue::LifeStage::kDeletePending);
            enqueueExpiredEntry(key, value);
        }
    }
}

void ProcessCache::enqueueExpiredEntry(const data_event_id& key, std::shared_ptr<ProcessCacheValue>& value) {
    std::lock_guard<std::mutex> lock(mCacheExpireQueueMutex);
    mCacheExpireQueue.push_back({key, value});
}

void ProcessCache::Clear() {
    std::lock_guard<std::mutex> lock(mCacheMutex);
    mCache.clear();
}

void ProcessCache::ClearExpiredCache() {
    {
        std::lock_guard<std::mutex> lock(mCacheExpireQueueMutex);
        mCacheExpireQueueProcessing.swap(mCacheExpireQueue);
    }
    if (mCacheExpireQueueProcessing.empty()) {
        return;
    }
    size_t nextQueueSize = 0;
    for (auto& entry : mCacheExpireQueueProcessing) {
        if (entry.value->LifeStage() == ProcessCacheValue::LifeStage::kDeleted) {
            LOG_WARNING(sLogger, ("clear expired cache twice pid", entry.key.pid)("ktime", entry.key.time));
            continue;
        }
        if (entry.value->RefCount() > 0) {
            entry.value->SetLifeStage(ProcessCacheValue::LifeStage::kInUse);
            continue;
        }
        if (entry.value->LifeStage() == ProcessCacheValue::LifeStage::kDeletePending) {
            entry.value->SetLifeStage(ProcessCacheValue::LifeStage::kDeleteReady);
            mCacheExpireQueueProcessing[nextQueueSize++] = entry;
            continue;
        }
        if (entry.value->LifeStage() == ProcessCacheValue::LifeStage::kDeleteReady) {
            entry.value->SetLifeStage(ProcessCacheValue::LifeStage::kDeleted);
            LOG_DEBUG(sLogger, ("clear expired cache pid", entry.key.pid)("ktime", entry.key.time));
            removeCache(entry.key);
        }
    }
    if (nextQueueSize > 0) {
        mCacheExpireQueue.insert(mCacheExpireQueue.end(),
                                 std::make_move_iterator(mCacheExpireQueueProcessing.begin()),
                                 std::make_move_iterator(mCacheExpireQueueProcessing.end()));
    }
    mCacheExpireQueueProcessing.clear();
}

void ProcessCache::ForceShrink() {
    std::vector<std::pair<int, data_event_id>> cacheToRemove;
    cacheToRemove.reserve(mCache.size());
    {
        std::lock_guard<std::mutex> lock(mCacheMutex);
        for (const auto& kv : mCache) {
            cacheToRemove.emplace_back(kv.second->RefCount(), kv.first);
        }
    }
    std::sort(cacheToRemove.begin(), cacheToRemove.end());
    cacheToRemove.resize(std::min(1UL, mCache.size() / 4));
    {
        std::lock_guard<std::mutex> lock(mCacheMutex);
        for (const auto& [refCount, key] : cacheToRemove) {
            mCache.erase(key);
            LOG_DEBUG(sLogger, ("[FORCE SHRINK] pid", key.pid)("ktime", key.time));
        }
    }
}

void ProcessCache::PrintDebugInfo() {
    for (const auto& [key, value] : mCache) {
        LOG_ERROR(sLogger, ("[DUMP CACHE] pid", key.pid)("ktime", key.time));
    }
    for (const auto& entry : mCacheExpireQueue) {
        LOG_ERROR(sLogger, ("[DUMP EXPIRE Q] pid", entry.key.pid)("ktime", entry.key.time));
    }
}

} // namespace logtail
