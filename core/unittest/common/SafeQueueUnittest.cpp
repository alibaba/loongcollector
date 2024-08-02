// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "common/SafeQueue.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class SafeQueueUnittest : public ::testing::Test {
public:
    void TestPush();
    void TestPop();
    void TestWait();
};

void SafeQueueUnittest::TestPush() {
    SafeQueue<unique_ptr<int>> queue;

    queue.Push(make_unique<int>(1));
    APSARA_TEST_EQUAL(1U, queue.Size());
}

void SafeQueueUnittest::TestPop() {
    SafeQueue<unique_ptr<int>> queue;

    queue.Push(make_unique<int>(1));
    {
        unique_ptr<int> res;
        APSARA_TEST_TRUE(queue.TryPop(res));
        APSARA_TEST_EQUAL(1, *res);
    }
    {
        unique_ptr<int> res;
        APSARA_TEST_FALSE(queue.TryPop(res));
        APSARA_TEST_TRUE(queue.Empty());
    }

    queue.Push(make_unique<int>(1));
    queue.Push(make_unique<int>(2));
    queue.Push(make_unique<int>(3));
    {
        unique_ptr<int> res;
        APSARA_TEST_TRUE(queue.WaitAndPop(res, 1000));
        APSARA_TEST_EQUAL(1, *res);
    }
    {
        vector<unique_ptr<int>> res;
        APSARA_TEST_TRUE(queue.WaitAndPopAll(res, 1000));
        APSARA_TEST_EQUAL(2U, res.size());
        APSARA_TEST_EQUAL(2, *(res[0]));
        APSARA_TEST_EQUAL(3, *(res[1]));
        APSARA_TEST_TRUE(queue.Empty());
    }
    {
        unique_ptr<int> res;
        APSARA_TEST_FALSE(queue.WaitAndPop(res, 1));
    }
    {
        vector<unique_ptr<int>> res;
        APSARA_TEST_FALSE(queue.WaitAndPopAll(res, 1));
    }
}

void SafeQueueUnittest::TestWait() {
    SafeQueue<unique_ptr<int>> queue;
    {
        auto res = async(launch::async, [&queue] {
            unique_ptr<int> item;
            return queue.WaitAndPop(item, 10000);
        });
        queue.Push(make_unique<int>(1));
        APSARA_TEST_TRUE(res.get());
    }
    {
        auto res = async(launch::async, [&queue] {
            vector<unique_ptr<int>> items;
            return queue.WaitAndPopAll(items, 10000);
        });
        queue.Push(make_unique<int>(1));
        APSARA_TEST_TRUE(res.get());
    }
}

UNIT_TEST_CASE(SafeQueueUnittest, TestPush)
UNIT_TEST_CASE(SafeQueueUnittest, TestPop)
UNIT_TEST_CASE(SafeQueueUnittest, TestWait)

} // namespace logtail

UNIT_TEST_MAIN
