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

#include <curl/multi.h>

#include <atomic>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>

#include "common/SafeQueue.h"
#include "common/http/HttpRequest.h"

namespace logtail {

class AsynCurlRunner {
public:
    AsynCurlRunner(const AsynCurlRunner&) = delete;
    AsynCurlRunner& operator=(const AsynCurlRunner&) = delete;

    static AsynCurlRunner* GetInstance() {
        static AsynCurlRunner instance;
        return &instance;
    }

    bool Init();
    void Stop();
    bool AddRequest(std::unique_ptr<AsynHttpRequest>&& request);

private:
    AsynCurlRunner() = default;
    ~AsynCurlRunner() = default;

    void Run();
    void DoRun();

    CURLM* mClient = nullptr;
    SafeQueue<std::unique_ptr<AsynHttpRequest>> mQueue;

    std::future<void> mThreadRes;
    std::atomic_bool mIsFlush = false;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class HttpRequestTimerEventUnittest;
#endif
};

} // namespace logtail
