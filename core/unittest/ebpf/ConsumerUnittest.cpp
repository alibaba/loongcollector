// Copyright 2025 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <json/json.h>

#include <algorithm>
#include <iostream>
#include <random>

#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/util/Consumer.h"
#include "logger/Logger.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_BOOL(logtail_mode);

namespace logtail::ebpf {

class ConsumerUnittest : public testing::Test {
public:
    ConsumerUnittest()
        : queue_(moodycamel::BlockingConcurrentQueue<int>(1024)),
          consumer_(queue_, std::chrono::milliseconds(100), [&](const std::vector<int>& items, size_t count) {
              LOG_INFO(sLogger, ("count", count));
              for (size_t i = 0; i < count; ++i) {
                  processed_items_.push_back(items[i]);
              }
              return true;
          }) {}

    void BasicFunctionality();
    void SuspendAndResume();

protected:
    void SetUp() override { consumer_.start(); }
    void TearDown() override { consumer_.stop(); }

private:
    moodycamel::BlockingConcurrentQueue<int> queue_;
    Consumer<int> consumer_;
    std::vector<int> processed_items_;
};

void ConsumerUnittest::BasicFunctionality() {
    queue_.enqueue(1);
    queue_.enqueue(2);
    queue_.enqueue(3);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    // consumer_.suspend();
    APSARA_TEST_EQUAL(processed_items_.size(), 3UL);
    APSARA_TEST_EQUAL(processed_items_[0], 1);
    APSARA_TEST_EQUAL(processed_items_[1], 2);
    APSARA_TEST_EQUAL(processed_items_[2], 3);
}

void ConsumerUnittest::SuspendAndResume() {
    processed_items_.clear();
    // consumer_.resume();
    APSARA_TEST_EQUAL(processed_items_.size(), 0UL);
    queue_.enqueue(4);
    queue_.enqueue(5);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    LOG_INFO(sLogger, ("begin to suspend", ""));
    consumer_.suspend();
    LOG_INFO(sLogger, ("after suspend", ""));
    APSARA_TEST_EQUAL(processed_items_.size(), 2UL);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    queue_.enqueue(6);
    APSARA_TEST_EQUAL(processed_items_.size(), 2UL);

    LOG_INFO(sLogger, ("begin to resume", ""));
    consumer_.resume();
    LOG_INFO(sLogger, ("after resume", ""));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    APSARA_TEST_EQUAL(processed_items_.size(), 3UL);
    APSARA_TEST_EQUAL(processed_items_[0], 4);
    APSARA_TEST_EQUAL(processed_items_[1], 5);
    APSARA_TEST_EQUAL(processed_items_[2], 6);
}

UNIT_TEST_CASE(ConsumerUnittest, BasicFunctionality);
UNIT_TEST_CASE(ConsumerUnittest, SuspendAndResume);

} // namespace logtail::ebpf


UNIT_TEST_MAIN
