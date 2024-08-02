/*
 * Copyright 2024 iLogtail Authors
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

#include <cstdint>
#include <ctime>

namespace logtail {

class RateLimiter {
public:
    RateLimiter(uint32_t maxRate) : mMaxSendBytesPerSecond(maxRate) {}

    bool IsValidToPop();
    void PostPop(size_t size);

    uint32_t mMaxSendBytesPerSecond = 0;

    // TODO: temporarily used, should use rate limiter instead
    static void FlowControl(int32_t dataSize, int64_t& lastSendTime, int32_t& lastSendByte, bool isRealTime);

private:
    time_t mLastSendTimeSecond = 0;
    uint32_t mLastSecondTotalBytes = 0;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class SenderQueueUnittest;
    friend class ExactlyOnceSenderQueueUnittest;
    friend class ExactlyOnceQueueManagerUnittest;
#endif
};

} // namespace logtail
