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

#include "unittest/Unittest.h"
#include <thread>
#include <iostream>
#include "common/Lock.h"
#include "common/TimeUtil.h"

namespace logtail {

class LockUnittest : public ::testing::Test {};

// Write lock should have higher priority, so it can not be blocked by read lock.
TEST_F(LockUnittest, TestReadWriteLockPriority) {
    ReadWriteLock rwLock;

    // A reading thread acquires read lock regularly.
    bool exitFlag = false;
    int readLoopCount = 0;
    std::thread readThread([&]() {
        while (!exitFlag) {
            ReadLock(rwLock);
            ++readLoopCount;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    // Acquire write lock.
    for (int i = 0; i < 100; ++i) {
        auto start = GetCurrentTimeInMilliSeconds();
        rwLock.lock();
        auto end = GetCurrentTimeInMilliSeconds();
        rwLock.unlock();

        EXPECT_LE(end - start, 3);

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    EXPECT_GT(readLoopCount, 0);

    exitFlag = true;
    readThread.join();
}

} // namespace logtail