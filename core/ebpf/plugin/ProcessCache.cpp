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

#include <chrono>
#include <iterator>
#include <mutex>

#include "ProcessCacheValue.h"
#include "common/TimeKeeper.h"
#include "logger/Logger.h"

namespace logtail {

ProcessCache::ProcessCache(size_t maxCacheSize, ProcParser& procParser) : mProcParser(procParser) {
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

void ProcessCache::IncRef([[maybe_unused]] const data_event_id& key, std::shared_ptr<ProcessCacheValue>& value) {
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
        mCacheExpireQueueProcessing.resize(nextQueueSize);
        mCacheExpireQueue.insert(mCacheExpireQueue.end(),
                                 std::make_move_iterator(mCacheExpireQueueProcessing.begin()),
                                 std::make_move_iterator(mCacheExpireQueueProcessing.end()));
    }
    mCacheExpireQueueProcessing.clear();
}

void ProcessCache::ForceShrink() {
    if (mLastForceShrinkTimeSec < TimeKeeper::GetInstance()->NowSec() - 120) {
        return;
    }
    auto validProcs = mProcParser.GetAllPids();
    auto minKtime = TimeKeeper::GetInstance()->KtimeNs()
        - std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::minutes(2)).count();
    std::vector<data_event_id> cacheToRemove;
    cacheToRemove.reserve(mCache.size() - validProcs.size());
    {
        std::lock_guard<std::mutex> lock(mCacheMutex);
        for (const auto& [k, v] : mCache) {
            if (validProcs.count(k.pid) == 0U && minKtime > time_t(k.time)) {
                cacheToRemove.emplace_back(k);
            }
        }
        for (const auto& key : cacheToRemove) {
            mCache.erase(key);
            LOG_ERROR(sLogger, ("[FORCE SHRINK] pid", key.pid)("ktime", key.time));
        }
    }
    mLastForceShrinkTimeSec = TimeKeeper::GetInstance()->NowSec();
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
