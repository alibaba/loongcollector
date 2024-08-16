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

#include <cstddef>

namespace logtail {

class BoundedQueueParam {
public:
    BoundedQueueParam(size_t cap, double ratio = 2.0 / 3)
        : mCapacity(cap), mHighWatermark(cap), mLowWatermark(cap * ratio), mRatio(ratio) {
        if (cap == 0) {
            mCapacity = mHighWatermark = 1;
        }
    }

    size_t GetCapacity() const { return mCapacity; }
    size_t GetHighWatermark() const { return mHighWatermark; }
    size_t GetLowWatermark() const { return mLowWatermark; }

private:
    size_t mCapacity;
    size_t mHighWatermark;
    size_t mLowWatermark;
    double mRatio = 2.0 / 3;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class SenderQueueManagerUnittest;
#endif
};

} // namespace logtail
