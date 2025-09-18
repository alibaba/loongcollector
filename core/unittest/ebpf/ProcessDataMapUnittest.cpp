// Copyright 2025 LoongCollector Authors
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

#include <cstddef>
#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "coolbpf/security/msg_type.h"
#include "ebpf/plugin/ProcessDataMap.h"
#include "unittest/Unittest.h"

using namespace logtail;
using namespace logtail::ebpf;

class ProcessDataMapUnittest : public ::testing::Test {
public:
    static constexpr size_t kMaxDataMapSize = 10;
    ProcessDataMapUnittest() : mProcessDataMap(kMaxDataMapSize) {}
    void SetUp() override {}

    void TearDown() override { mProcessDataMap.Clear(); }

    // ProcessCacheManager测试用例
    void TestAddDataEventNormal();
    void TestDataGetAndRemoveSizeBad();
    void TestRecordDataEventExceedLimit();

private:
    ProcessDataMap mProcessDataMap;
};


void ProcessDataMapUnittest::TestAddDataEventNormal() {
    {
        // fill msg_data
        struct msg_data msgData {};
        msgData.id.pid = 1234;
        msgData.id.time = 123546789;
        std::string filename(255, 'a');
        std::copy(filename.begin(), filename.end(), msgData.arg);
        msgData.common.op = MSG_OP_DATA;
        msgData.common.ktime = msgData.id.time;
        msgData.common.size = offsetof(struct msg_data, arg) + filename.size();

        mProcessDataMap.DataAdd(&msgData);
        APSARA_TEST_EQUAL(1UL, mProcessDataMap.Size());

        // fill data_event_desc
        data_event_desc desc{};
        desc.error = 0;
        desc.pad = 0;
        desc.size = filename.size();
        desc.leftover = 0;
        desc.id.pid = msgData.id.pid;
        desc.id.time = msgData.id.time;
        auto dataStr = mProcessDataMap.DataGetAndRemove(&desc);
        APSARA_TEST_EQUAL(dataStr, filename);
        APSARA_TEST_EQUAL(0UL, mProcessDataMap.Size());
    }
    {
        // fill msg_data
        struct msg_data msgData {};
        msgData.id.pid = 1234;
        msgData.id.time = 123546789;
        std::string filename(255, 'a');
        std::copy(filename.begin(), filename.end(), msgData.arg);
        msgData.common.op = MSG_OP_DATA;
        msgData.common.ktime = msgData.id.time;
        msgData.common.size = offsetof(struct msg_data, arg) + filename.size();

        mProcessDataMap.DataAdd(&msgData);
        mProcessDataMap.DataAdd(&msgData);
        mProcessDataMap.DataAdd(&msgData);
        mProcessDataMap.DataAdd(&msgData);
        mProcessDataMap.DataAdd(&msgData);
        APSARA_TEST_EQUAL(1UL, mProcessDataMap.Size());

        std::string fullFilename = filename + filename + filename + filename + filename;
        // fill data_event_desc
        data_event_desc desc{};
        desc.error = 0;
        desc.pad = 0;
        desc.size = fullFilename.size();
        desc.leftover = 0;
        desc.id.pid = msgData.id.pid;
        desc.id.time = msgData.id.time;
        auto dataStr = mProcessDataMap.DataGetAndRemove(&desc);
        APSARA_TEST_EQUAL(dataStr, fullFilename);
        APSARA_TEST_EQUAL(0UL, mProcessDataMap.Size());
    }
}

void ProcessDataMapUnittest::TestDataGetAndRemoveSizeBad() {
    // fill msg_data
    struct msg_data msgData {};
    msgData.id.pid = 1234;
    msgData.id.time = 123546789;
    std::string filename(255, 'a');
    std::copy(filename.begin(), filename.end(), msgData.arg);
    msgData.common.op = MSG_OP_DATA;
    msgData.common.ktime = msgData.id.time;
    msgData.common.size = offsetof(struct msg_data, arg) + filename.size();

    mProcessDataMap.DataAdd(&msgData);
    APSARA_TEST_EQUAL(1UL, mProcessDataMap.Size());

    // fill data_event_desc
    data_event_desc desc{};
    desc.error = 0;
    desc.pad = 0;
    desc.size = filename.size();
    desc.leftover = 1; // let size - leftover != filename.size()
    desc.id.pid = msgData.id.pid;
    desc.id.time = msgData.id.time;
    auto dataStr = mProcessDataMap.DataGetAndRemove(&desc);
    APSARA_TEST_EQUAL(dataStr, "");
    APSARA_TEST_EQUAL(0UL, mProcessDataMap.Size());
}

void ProcessDataMapUnittest::TestRecordDataEventExceedLimit() {
    for (size_t i = 1; i <= kMaxDataMapSize; i++) {
        struct msg_data msgData {};
        msgData.id.pid = i;
        msgData.id.time = 123546789;
        std::string filename(1, 'a');
        std::copy(filename.begin(), filename.end(), msgData.arg);
        msgData.common.op = MSG_OP_DATA;
        msgData.common.ktime = msgData.id.time;
        msgData.common.size = offsetof(struct msg_data, arg) + filename.size();
        mProcessDataMap.DataAdd(&msgData);
    }
    APSARA_TEST_EQUAL(kMaxDataMapSize, mProcessDataMap.Size());
    {
        struct msg_data msgData {};
        msgData.id.pid = 0;
        msgData.id.time = 123546789 + ProcessDataMap::kMaxCacheExpiredTimeoutNs + 1;
        std::string filename(1, 'a');
        std::copy(filename.begin(), filename.end(), msgData.arg);
        msgData.common.op = MSG_OP_DATA;
        msgData.common.ktime = msgData.id.time;
        msgData.common.size = offsetof(struct msg_data, arg) + filename.size();
        mProcessDataMap.DataAdd(&msgData);
    }
    APSARA_TEST_EQUAL(1UL, mProcessDataMap.Size()); // keep one item not expired
    for (size_t i = 1; i <= kMaxDataMapSize; i++) {
        struct msg_data msgData {};
        msgData.id.pid = i;
        msgData.id.time = 123546789 + ProcessDataMap::kMaxCacheExpiredTimeoutNs + 2;
        std::string filename(1, 'a');
        std::copy(filename.begin(), filename.end(), msgData.arg);
        msgData.common.op = MSG_OP_DATA;
        msgData.common.ktime = msgData.id.time;
        msgData.common.size = offsetof(struct msg_data, arg) + filename.size();
        mProcessDataMap.DataAdd(&msgData);
    }
    APSARA_TEST_EQUAL(1UL, mProcessDataMap.Size()); // forced clear all, only keep the last one
}

UNIT_TEST_CASE(ProcessDataMapUnittest, TestAddDataEventNormal);
UNIT_TEST_CASE(ProcessDataMapUnittest, TestDataGetAndRemoveSizeBad);
UNIT_TEST_CASE(ProcessDataMapUnittest, TestRecordDataEventExceedLimit);

UNIT_TEST_MAIN
