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


namespace logtail {

class SenderQueueParam {
public:
    SenderQueueParam() : SIZE(100), LOW_SIZE(10), HIGH_SIZE(20) {}

    static SenderQueueParam* GetInstance() {
        static auto sQueueParam = new SenderQueueParam;
        return sQueueParam;
    }

    size_t GetLowSize() { return LOW_SIZE; }

    size_t GetHighSize() { return HIGH_SIZE; }

    size_t GetMaxSize() { return SIZE; }

    void SetLowSize(size_t lowSize) { LOW_SIZE = lowSize; }

    void SetHighSize(size_t highSize) { HIGH_SIZE = highSize; }

    void SetMaxSize(size_t maxSize) { SIZE = maxSize; }

    size_t SIZE;
    size_t LOW_SIZE;
    size_t HIGH_SIZE;
};

} // namespace logtail
