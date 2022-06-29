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
#include <mutex>
#include <condition_variable>

namespace logtail {

class Semaphore {
public:
    Semaphore(size_t count_ = 0) : count(count_) {}

    inline void Post() {
        std::unique_lock<std::mutex> lock(mtx);
        count++;
        // Notify the waiting thread.
        cv.notify_one();
    }

    inline void Wait() {
        std::unique_lock<std::mutex> lock(mtx);
        while (0 == count) {
            // Wait on the mutex until notify is called.
            cv.wait(lock);
        }
        count--;
    }

private:
    std::mutex mtx;
    std::condition_variable cv;
    size_t count;
};

} // namespace logtail
