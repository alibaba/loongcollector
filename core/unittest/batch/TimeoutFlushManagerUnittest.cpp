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

#include "pipeline/batch/TimeoutFlushManager.h"
#include "unittest/Unittest.h"
#include "unittest/plugin/PluginMock.h"

using namespace std;

namespace logtail {

class TimeoutFlushManagerUnittest : public ::testing::Test {
public:
    void TestUpdateRecord();
    void TestFlushTimeoutBatch();
    void TestClearRecords();

protected:
    static void SetUpTestCase() {
        sFlusher = make_unique<FlusherMock>();
        sCtx.SetConfigName("test_config");
        sFlusher->SetContext(sCtx);
        sFlusher->SetMetricsRecordRef(FlusherMock::sName, "1");
    }

    void TearDown() override { TimeoutFlushManager::GetInstance()->mTimeoutRecords.clear(); }

private:
    static unique_ptr<FlusherMock> sFlusher;
    static PipelineContext sCtx;
};

unique_ptr<FlusherMock> TimeoutFlushManagerUnittest::sFlusher;
PipelineContext TimeoutFlushManagerUnittest::sCtx;

void TimeoutFlushManagerUnittest::TestUpdateRecord() {
    // new batch queue
    TimeoutFlushManager::GetInstance()->UpdateRecord("test_config", 0, 1, 3, sFlusher.get());
    APSARA_TEST_EQUAL(1U, TimeoutFlushManager::GetInstance()->mTimeoutRecords.size());
    APSARA_TEST_EQUAL(1U, TimeoutFlushManager::GetInstance()->mTimeoutRecords["test_config"].size());
    auto& record1 = TimeoutFlushManager::GetInstance()->mTimeoutRecords["test_config"].at(make_pair(0, 1));
    APSARA_TEST_EQUAL(1U, record1.mKey);
    APSARA_TEST_EQUAL(3U, record1.mTimeoutSecs);
    APSARA_TEST_EQUAL(sFlusher.get(), record1.mFlusher);
    APSARA_TEST_GT(record1.mUpdateTime, 0);

    // existed batch queue
    time_t lastTime = record1.mUpdateTime;
    TimeoutFlushManager::GetInstance()->UpdateRecord("test_config", 0, 1, 3, sFlusher.get());
    APSARA_TEST_EQUAL(1U, TimeoutFlushManager::GetInstance()->mTimeoutRecords.size());
    APSARA_TEST_EQUAL(1U, TimeoutFlushManager::GetInstance()->mTimeoutRecords["test_config"].size());
    auto& record2 = TimeoutFlushManager::GetInstance()->mTimeoutRecords["test_config"].at(make_pair(0, 1));
    APSARA_TEST_EQUAL(1U, record2.mKey);
    APSARA_TEST_EQUAL(3U, record2.mTimeoutSecs);
    APSARA_TEST_EQUAL(sFlusher.get(), record2.mFlusher);
    APSARA_TEST_GT(record2.mUpdateTime, lastTime - 1);
}

void TimeoutFlushManagerUnittest::TestFlushTimeoutBatch() {
    TimeoutFlushManager::GetInstance()->UpdateRecord("test_config", 0, 0, 0, sFlusher.get());
    TimeoutFlushManager::GetInstance()->UpdateRecord("test_config", 0, 1, 3, sFlusher.get());
    TimeoutFlushManager::GetInstance()->UpdateRecord("test_config", 0, 2, 0, sFlusher.get());

    TimeoutFlushManager::GetInstance()->FlushTimeoutBatch();
    APSARA_TEST_EQUAL(2U, sFlusher->mFlushedQueues.size()); // key 0 && 2
    APSARA_TEST_EQUAL(1U, TimeoutFlushManager::GetInstance()->mTimeoutRecords.size());
}

void TimeoutFlushManagerUnittest::TestClearRecords() {
    TimeoutFlushManager::GetInstance()->UpdateRecord("test_config", 0, 1, 3, sFlusher.get());
    TimeoutFlushManager::GetInstance()->ClearRecords("test_config");

    APSARA_TEST_EQUAL(0U, TimeoutFlushManager::GetInstance()->mTimeoutRecords.size());
}

UNIT_TEST_CASE(TimeoutFlushManagerUnittest, TestUpdateRecord)
UNIT_TEST_CASE(TimeoutFlushManagerUnittest, TestFlushTimeoutBatch)
UNIT_TEST_CASE(TimeoutFlushManagerUnittest, TestClearRecords)

} // namespace logtail

UNIT_TEST_MAIN
