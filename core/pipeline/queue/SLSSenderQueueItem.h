/*
 * Copyright 2024 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "checkpoint/RangeCheckpoint.h"
#include "pipeline/queue/SenderQueueItem.h"

namespace logtail {

class Flusher;
class Pipeline;

struct SLSSenderQueueItem : public SenderQueueItem {
    std::string mShardHashKey;
    // it normally equals to flusher_sls.Logstore, except for the following situations:
    // 1. when route is enabled in Go pipeline, it is designated explicitly
    // 2. self telemetry data from C++ pipelines
    std::string mLogstore;
    RangeCheckpointPtr mExactlyOnceCheckpoint;

    std::string mCurrentHost;
    bool mRealIpFlag = false;
    int32_t mLastLogWarningTime = 0; // temporaily used

    SLSSenderQueueItem(std::string&& data,
                       size_t rawSize,
                       Flusher* flusher,
                       QueueKey key,
                       const std::string& logstore,
                       RawDataType type = RawDataType::EVENT_GROUP,
                       const std::string& shardHashKey = "",
                       RangeCheckpointPtr&& exactlyOnceCheckpoint = RangeCheckpointPtr(),
                       bool bufferOrNot = true)
        : SenderQueueItem(std::move(data), rawSize, flusher, key, type, bufferOrNot),
          mShardHashKey(shardHashKey),
          mLogstore(logstore),
          mExactlyOnceCheckpoint(std::move(exactlyOnceCheckpoint)) {}

    SenderQueueItem* Clone() override { return new SLSSenderQueueItem(*this); }
};

} // namespace logtail
