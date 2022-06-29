/*
 * Copyright 2022 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <unordered_map>
#include <thread>
#include <mutex>
#include <memory>
#include "LogstoreFeedbackKey.h"
#include "checkpoint/RangeCheckpoint.h"

namespace logtail {

enum class QueueType { Normal, ExactlyOnce };

class QueueManager {
public:
    static QueueManager* GetInstance() {
        static auto singleton = new QueueManager;
        return singleton;
    }

    LogstoreFeedBackKey InitializeExactlyOnceQueues(const std::string& project,
                                                    const std::string& logStore,
                                                    const std::vector<RangeCheckpointPtr>& checkpoints);

    // It generates a unique feedback (queue) key for specified logstore.
    //
    // If the queue type is ExactlyOnce, corresponding queues will be reset.
    LogstoreFeedBackKey GenerateFeedBackKey(const std::string& project,
                                            const std::string& logStore,
                                            const QueueType queueType = QueueType::Normal);

    // Mark corresponding queue with GC flag.
    //
    // Called by ~LogFileReader to remove useless queues from process/sender queue map,
    //  which can accelerate queue iteration.
    // GenerateFeedBackKey will bring queue back from GC list.
    void MarkGC(const std::string& project, const std::string& logStore);

private:
    QueueManager();
    ~QueueManager();

    static std::string makeQueueKey(const std::string& project, const std::string& logStore) {
        std::string key;
        key.reserve(project.length() + 1 + logStore.length());
        key.append(project).append("-").append(logStore);
        return key;
    }

    void runGCLoop();

private:
    struct QueueInfo {
        LogstoreFeedBackKey key;
        QueueType type;
    };

    struct GCItem {
        const QueueInfo* info;
        time_t createTime;
    };

    std::mutex mMutex;
    LogstoreFeedBackKey mNextKey = 0;
    std::unordered_map<std::string, QueueInfo> mQueueInfos;

    volatile bool mStopGCThread = false;
    std::unordered_map<std::string, GCItem> mGCItems;
    std::unique_ptr<std::thread> mGCThreadPtr;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ExactlyOnceReaderUnittest;
    friend class SenderUnittest;
    friend class QueueManagerUnittest;

    void clear() {
        std::lock_guard<std::mutex> lock(mMutex);
        mGCItems.clear();
        mQueueInfos.clear();
    }
#endif
};

} // namespace logtail