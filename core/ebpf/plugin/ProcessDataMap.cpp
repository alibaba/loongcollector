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

#include "ebpf/plugin/ProcessDataMap.h"

#include <ctime>

#include <chrono>

#include "TimeKeeper.h"
#include "logger/Logger.h"

namespace logtail::ebpf {

constexpr size_t kDataArgOffset = offsetof(msg_data, arg);

size_t ProcessDataMap::Size() const {
    std::lock_guard<std::mutex> lk(mDataMapMutex);
    return mDataMap.size();
}

void ProcessDataMap::DataAdd(msg_data* dataPtr) {
    if (dataPtr->common.size < kDataArgOffset) {
        LOG_ERROR(sLogger,
                  ("size is negative, dataPtr.common.size", dataPtr->common.size)("arg offset", kDataArgOffset));
        return;
    }
    size_t size = dataPtr->common.size - kDataArgOffset;
    if (size <= MSG_DATA_ARG_LEN) {
        time_t ktimeUpperBound = TimeKeeper::GetInstance()->KtimeNs()
            + std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(10)).count();
        if (time_t(dataPtr->id.time) > ktimeUpperBound) {
            LOG_ERROR(sLogger,
                      ("pid", dataPtr->id.pid)("ktime", dataPtr->id.time)(
                          "nowKtime", TimeKeeper::GetInstance()->KtimeNs())("error", "ktime is a future time"));
        }
        std::lock_guard<std::mutex> lk(mDataMapMutex);
        auto res = mDataMap.find(dataPtr->id);
        if (res != mDataMap.end()) {
            auto& prevData = res->second;
            LOG_DEBUG(sLogger,
                      ("already have data, pid", dataPtr->id.pid)("ktime", dataPtr->id.time)("prevData", prevData)(
                          "data", std::string(dataPtr->arg, size)));
            prevData.append(dataPtr->arg, size);
        } else {
            // restrict memory usage in abnormal conditions
            // if there is some unused old data, clear it
            // if we cannot clear old data, just clear all
            if (mDataMap.size() >= mMaxSize
                && mLastClearTime
                    < std::chrono::system_clock::now() - std::chrono::nanoseconds(kMaxCacheExpiredTimeoutNs)) {
                LOG_WARNING(sLogger, ("data map size exceed limit", mMaxSize)("size", mDataMap.size()));
                clearExpiredData(std::min(time_t(dataPtr->id.time), ktimeUpperBound));
                mLastClearTime = std::chrono::system_clock::now();
            }
            if (mDataMap.size() >= mMaxSize) {
                LOG_WARNING(sLogger, ("data map size exceed limit", mMaxSize)("size", mDataMap.size()));
                mDataMap.clear();
            }
            LOG_DEBUG(sLogger,
                      ("no prev data, pid",
                       dataPtr->id.pid)("ktime", dataPtr->id.time)("data", std::string(dataPtr->arg, size)));
            mDataMap[dataPtr->id] = std::string(dataPtr->arg, size);
        }
    } else {
        LOG_ERROR(sLogger, ("pid", dataPtr->id.pid)("ktime", dataPtr->id.time)("size limit exceeded", size));
    }
}

void ProcessDataMap::Clear() {
    std::lock_guard<std::mutex> lk(mDataMapMutex);
    mDataMap.clear();
}

std::string ProcessDataMap::DataGetAndRemove(const data_event_desc* desc) {
    std::string data;
    {
        std::lock_guard<std::mutex> lk(mDataMapMutex);
        auto res = mDataMap.find(desc->id);
        if (res == mDataMap.end()) {
            return data;
        }
        data.swap(res->second);
        mDataMap.erase(res);
    }
    if (data.size() != desc->size - desc->leftover) {
        LOG_WARNING(sLogger,
                    ("size bad! data size", data.size())("expect", desc->size - desc->leftover)("pid", desc->id.pid)(
                        "desc->size", desc->size)("desc->leftover", desc->leftover));
        return "";
    }
    return data;
}

void ProcessDataMap::clearExpiredData(time_t ktime) {
    ktime -= kMaxCacheExpiredTimeoutNs;
    for (auto it = mDataMap.begin(); it != mDataMap.end();) {
        if (time_t(it->first.time) < ktime) {
            it = mDataMap.erase(it);
        } else {
            ++it;
        }
    }
}
void ProcessDataMap::PrintDebugInfo() {
    for (const auto& [key, value] : mDataMap) {
        LOG_ERROR(sLogger, ("[DUMP DATA] pid", key.pid)("ktime", key.time));
    }
}

} // namespace logtail::ebpf
