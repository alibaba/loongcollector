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

#include <cstdint>
#include <ctime>
#include <memory>
#include <string>

#include "queue/FeedbackQueueKey.h"

namespace logtail {

class Flusher;
class Pipeline;

enum class SendingStatus { IDLE, SENDING };
enum class RawDataType { EVENT_GROUP_LIST, EVENT_GROUP }; // the order must not be changed for backward compatibility

struct SenderQueueItem {
    std::string mData;
    size_t mRawSize = 0;
    RawDataType mType = RawDataType::EVENT_GROUP;
    bool mBufferOrNot = true;
    std::shared_ptr<Pipeline> mPipeline; // not null only during pipeline update
    Flusher* mFlusher = nullptr;
    QueueKey mQueueKey;

    SendingStatus mStatus = SendingStatus::IDLE;
    time_t mEnqueTime = 0;
    time_t mLastSendTime = 0;
    uint32_t mTryCnt = 1;

    SenderQueueItem(std::string&& data,
                    size_t rawSize,
                    Flusher* flusher,
                    QueueKey key,
                    RawDataType type = RawDataType::EVENT_GROUP,
                    bool bufferOrNot = true)
        : mData(std::move(data)),
          mRawSize(rawSize),
          mType(type),
          mBufferOrNot(bufferOrNot),
          mFlusher(flusher),
          mQueueKey(key) {}
    virtual ~SenderQueueItem() = default;

    virtual SenderQueueItem* Clone() { return new SenderQueueItem(*this); }
};

} // namespace logtail
