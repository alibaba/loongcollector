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

#include <boost/unordered/concurrent_flat_map.hpp>
#include <chrono>
#include <iterator>
#include <mutex>

#include "ProcessCacheValue.h"
#include "common/TimeKeeper.h"
#include "logger/Logger.h"

namespace logtail {

ProcessCache::ProcessCache(size_t maxCacheSize, ProcParser& procParser) : mProcParser(procParser) {
    mCache = std::make_unique<ExecveEventMap>(maxCacheSize, ebpf::DataEventIdHash{}, ebpf::DataEventIdEqual{});
}

ProcessCache::~ProcessCache() = default;

bool ProcessCache::Contains(const data_event_id& key) const {
    if (!mCache) {
        return false;
    }
    size_t found = mCache->cvisit(key, [](const auto& /* element */) {});
    return found == 1;
}

std::shared_ptr<ProcessCacheValue> ProcessCache::Lookup(const data_event_id& key) {
    if (!mCache) {
        return nullptr;
    }

    std::shared_ptr<ProcessCacheValue> result = nullptr;
    size_t found = mCache->cvisit(key, [&result](const auto& element) { result = element.second; });

    return (found == 1) ? result : nullptr;
}

size_t ProcessCache::Size() const {
    if (!mCache) {
        return 0;
    }
    return mCache->size();
}

void ProcessCache::removeCache(const data_event_id& key) {
    if (!mCache) {
        return;
    }
    mCache->erase(key);
}

void ProcessCache::AddCache(const data_event_id& key, std::shared_ptr<ProcessCacheValue>& value) {
    if (!mCache) {
        return;
    }
    value->IncRef();
    mCache->insert(std::make_pair(key, value));
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
    if (!mCache) {
        return;
    }
    mCache->clear();
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
    if (mLastForceShrinkTimeSec != 0 && mLastForceShrinkTimeSec > TimeKeeper::GetInstance()->NowSec() - 120) {
        return;
    }
    if (!mCache) {
        return;
    }

    auto validProcs = mProcParser.GetAllPids();
    auto minKtime = TimeKeeper::GetInstance()->KtimeNs()
        - std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::minutes(2)).count();

    std::vector<data_event_id> cacheToRemove;
    mCache->visit_all([&](const auto& element) {
        const auto& [k, v] = element;
        if (validProcs.count(k.pid) == 0U && minKtime > time_t(k.time)) {
            cacheToRemove.emplace_back(k);
        }
    });

    for (const auto& key : cacheToRemove) {
        mCache->erase(key);
        LOG_ERROR(sLogger, ("[FORCE SHRINK] pid", key.pid)("ktime", key.time));
    }

    mLastForceShrinkTimeSec = TimeKeeper::GetInstance()->NowSec();
}

void ProcessCache::PrintDebugInfo() {
    if (mCache) {
        mCache->cvisit_all([](const auto& element) {
            const auto& [key, value] = element;
            LOG_ERROR(sLogger, ("[DUMP CACHE] pid", key.pid)("ktime", key.time));
        });
    }
    for (const auto& entry : mCacheExpireQueue) {
        LOG_ERROR(sLogger, ("[DUMP EXPIRE Q] pid", entry.key.pid)("ktime", entry.key.time));
    }
}

} // namespace logtail
