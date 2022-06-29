// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "QueueManager.h"
#include "common/Flags.h"
#include "common/TimeUtil.h"
#include "logger/Logger.h"
#include "processor/LogProcess.h"
#include "sender/Sender.h"

DEFINE_FLAG_INT32(logtail_queue_check_gc_interval_sec, "30s", 30);
DEFINE_FLAG_INT32(logtail_queue_gc_threshold_sec, "2min", 2 * 60);
DEFINE_FLAG_INT64(logtail_queue_max_used_time_per_round_in_msec, "500ms", 500);

namespace logtail {

QueueManager::QueueManager() {
    mGCThreadPtr.reset(new std::thread([&]() { runGCLoop(); }));
}

QueueManager::~QueueManager() {
    if (mGCThreadPtr) {
        mStopGCThread = true;
        mGCThreadPtr->join();
    }
}

void QueueManager::runGCLoop() {
    while (!mStopGCThread) {
        sleep(INT32_FLAG(logtail_queue_check_gc_interval_sec));

        auto const curTime = time(NULL);
        const auto startTimeInMs = GetCurrentTimeInMilliSeconds();
        std::lock_guard<std::mutex> lock(mMutex);
        auto iter = mGCItems.begin();
        while (iter != mGCItems.end()) {
            if (mStopGCThread
                || (GetCurrentTimeInMilliSeconds() - startTimeInMs
                    >= static_cast<uint64_t>(INT64_FLAG(logtail_queue_max_used_time_per_round_in_msec)))) {
                break;
            }

            auto& item = iter->second;
            if (!(curTime >= item.createTime
                  && curTime - item.createTime >= INT32_FLAG(logtail_queue_gc_threshold_sec))) {
                ++iter;
                continue;
            }

            static auto& sProcessQueue = LogProcess::GetInstance()->GetQueue();
            static auto& sSenderQueue = Sender::Instance()->GetQueue();
            auto fbKey = item.info->key;
#define LOG_PATTERN ("queue is not empty", fbKey)("key", item.info->key)
            if (!sProcessQueue.IsEmpty(fbKey)) {
                LOG_INFO(sLogger, LOG_PATTERN("type", "process"));
                ++iter;
                continue;
            }
            if (!sSenderQueue.IsEmpty(fbKey)) {
                LOG_INFO(sLogger, LOG_PATTERN("type", "sender"));
                ++iter;
                continue;
            }
#undef LOG_PATTERN

            LOG_INFO(sLogger, ("GC queue", fbKey)("key", item.info->key)("time", curTime - item.createTime));
            sProcessQueue.Delete(fbKey);
            sSenderQueue.Delete(fbKey);
            mQueueInfos.erase(iter->first);
            iter = mGCItems.erase(iter);
        }
    }
}

LogstoreFeedBackKey QueueManager::InitializeExactlyOnceQueues(const std::string& project,
                                                              const std::string& logStore,
                                                              const std::vector<RangeCheckpointPtr>& checkpoints) {
    auto fbKey = GenerateFeedBackKey(project, logStore, QueueType::ExactlyOnce);
    LogProcess::GetInstance()->GetQueue().ConvertToExactlyOnceQueue(fbKey);
    Sender::Instance()->GetQueue().ConvertToExactlyOnceQueue(fbKey, checkpoints);
    return fbKey;
}

LogstoreFeedBackKey
QueueManager::GenerateFeedBackKey(const std::string& project, const std::string& logStore, const QueueType queueType) {
    const auto key = makeQueueKey(project, logStore);

    std::lock_guard<std::mutex> lock(mMutex);
    auto iter = mQueueInfos.find(key);
    if (iter != mQueueInfos.end()) {
        auto gcIter = mGCItems.find(key);
        if (gcIter != mGCItems.end()) {
            LOG_INFO(sLogger, ("bring GC queue back", key));
            mGCItems.erase(gcIter);
        }
        return iter->second.key;
    }

    auto& info = mQueueInfos[key];
    info.key = mNextKey++;
    info.type = queueType;
    return info.key;
}

void QueueManager::MarkGC(const std::string& project, const std::string& logStore) {
    const auto key = makeQueueKey(project, logStore);

    std::lock_guard<std::mutex> lock(mMutex);
    {
        auto iter = mGCItems.find(key);
        if (iter != mGCItems.end()) {
            LOG_ERROR(sLogger,
                      ("double mark GC for queue",
                       key)("create time", iter->second.createTime)("feedback key", iter->second.info->key));
            return;
        }
    }

    auto const iter = mQueueInfos.find(key);
    if (iter == mQueueInfos.end()) {
        LOG_ERROR(sLogger, ("can not find queue to mark", key));
        return;
    }
    if (iter->second.type != QueueType::ExactlyOnce) {
        LOG_ERROR(sLogger,
                  ("can not mark queue that is not exactly once", key)("type", static_cast<int>(iter->second.type)));
        return;
    }
    auto& item = mGCItems[key];
    item.createTime = time(NULL);
    item.info = &(iter->second);
    LOG_DEBUG(sLogger, ("mark queue for GC", key)("feedback key", item.info->key));
}

} // namespace logtail