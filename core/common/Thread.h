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
#include <thread>
#include <memory>
#include <future>
#include <utility>
#include <functional>

namespace logtail {

class Thread {
    std::thread thread_;

public:
    template <class Function, class... Args>
    explicit Thread(Function&& f, Args&&... args) : thread_(std::forward<Function>(f), std::forward<Args>(args)...) {}

    ~Thread() { GetValue(1000 * 100); }

    void GetValue(unsigned long long microseconds) {
        // TODO: Add timeout.
        if (thread_.joinable())
            thread_.join();
    }

    void Wait(unsigned long long microseconds) { GetValue(microseconds); }

    int GetState() {
        // TODO:
        return 0;
    }
};

using ThreadPtr = std::shared_ptr<Thread>;

template <class Function, class... Args>
ThreadPtr CreateThread(Function&& f, Args&&... args) {
    return ThreadPtr(new Thread(std::forward<Function>(f), std::forward<Args>(args)...));
}

} // namespace logtail
