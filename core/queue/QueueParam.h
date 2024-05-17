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

struct ProcessQueueParam {
    ProcessQueueParam(const ProcessQueueParam&) = delete;
    ProcessQueueParam& operator=(const ProcessQueueParam&) = delete;

    static ProcessQueueParam* GetInstance() {
        static ProcessQueueParam instance;
        return &instance;
    }

    void SetParam(size_t cap, size_t lowSize, size_t highSize) {
        mCapacity = cap;
        mLowWatermark = lowSize;
        mHighWatermark = highSize;
    }

    size_t mCapacity = 20;
    size_t mLowWatermark = 10;
    size_t mHighWatermark = 15;

private:
    ProcessQueueParam() = default;
    ~ProcessQueueParam() = default;
};

} // namespace logtail
