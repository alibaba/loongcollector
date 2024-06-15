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


#include "batch/BatchStatus.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class EventBatchStatusUnittest : public ::testing::Test {
public:
    void TestReset();
    void TestUpdate();

protected:
    static void SetUpTestCase() {
        sSourceBuffer.reset(new SourceBuffer);
        sEventGroup.reset(new PipelineEventGroup(sSourceBuffer));
        sEvent = sEventGroup->CreateLogEvent();
    }

    void SetUp() override { mStatus.Reset(); }

private:
    static shared_ptr<SourceBuffer> sSourceBuffer;
    static unique_ptr<PipelineEventGroup> sEventGroup;
    static PipelineEventPtr sEvent;

    EventBatchStatus mStatus;
};

shared_ptr<SourceBuffer> EventBatchStatusUnittest::sSourceBuffer;
unique_ptr<PipelineEventGroup> EventBatchStatusUnittest::sEventGroup;
PipelineEventPtr EventBatchStatusUnittest::sEvent;

void EventBatchStatusUnittest::TestReset() {
    APSARA_TEST_EQUAL(0U, mStatus.GetCnt());
    APSARA_TEST_EQUAL(0U, mStatus.GetSize());
    APSARA_TEST_EQUAL(0, mStatus.GetCreateTime());
}

void EventBatchStatusUnittest::TestUpdate() {
    mStatus.Update(sEvent);
    time_t createTime = mStatus.GetCreateTime();
    APSARA_TEST_EQUAL(1U, mStatus.GetCnt());
    APSARA_TEST_EQUAL(sEvent->DataSize(), mStatus.GetSize());

    mStatus.Update(sEvent);
    APSARA_TEST_EQUAL(2U, mStatus.GetCnt());
    APSARA_TEST_EQUAL(2 * sEvent->DataSize(), mStatus.GetSize());
    APSARA_TEST_EQUAL(createTime, mStatus.GetCreateTime());
}

UNIT_TEST_CASE(EventBatchStatusUnittest, TestReset)
UNIT_TEST_CASE(EventBatchStatusUnittest, TestUpdate)

class SLSEventBatchStatusUnittest : public ::testing::Test {
public:
    void TestReset();
    void TestUpdate();

protected:
    static void SetUpTestCase() {
        sSourceBuffer.reset(new SourceBuffer);
        sEventGroup.reset(new PipelineEventGroup(sSourceBuffer));
    }

    void SetUp() override { mStatus.Reset(); }

private:
    static shared_ptr<SourceBuffer> sSourceBuffer;
    static unique_ptr<PipelineEventGroup> sEventGroup;

    SLSEventBatchStatus mStatus;
};

shared_ptr<SourceBuffer> SLSEventBatchStatusUnittest::sSourceBuffer;
unique_ptr<PipelineEventGroup> SLSEventBatchStatusUnittest::sEventGroup;

void SLSEventBatchStatusUnittest::TestReset() {
    APSARA_TEST_EQUAL(0U, mStatus.GetCnt());
    APSARA_TEST_EQUAL(0U, mStatus.GetSize());
    APSARA_TEST_EQUAL(0, mStatus.GetCreateTime());
    APSARA_TEST_EQUAL(0, mStatus.GetCreateTimeMinute());
}

void SLSEventBatchStatusUnittest::TestUpdate() {
    PipelineEventPtr e1(sEventGroup->CreateLogEvent());
    e1->SetTimestamp(1717398001);
    mStatus.Update(e1);
    time_t createTime = mStatus.GetCreateTime();
    APSARA_TEST_EQUAL(1U, mStatus.GetCnt());
    APSARA_TEST_EQUAL(e1->DataSize(), mStatus.GetSize());
    APSARA_TEST_EQUAL(1717398001 / 60, mStatus.GetCreateTimeMinute());

    PipelineEventPtr e2(sEventGroup->CreateLogEvent());
    e2->SetTimestamp(1717398030);
    mStatus.Update(e2);
    APSARA_TEST_EQUAL(2U, mStatus.GetCnt());
    APSARA_TEST_EQUAL(e1->DataSize() + e2->DataSize(), mStatus.GetSize());
    APSARA_TEST_EQUAL(createTime, mStatus.GetCreateTime());
    APSARA_TEST_EQUAL(1717398000 / 60, mStatus.GetCreateTimeMinute());
}

UNIT_TEST_CASE(SLSEventBatchStatusUnittest, TestReset)
UNIT_TEST_CASE(SLSEventBatchStatusUnittest, TestUpdate)

class GroupBatchStatusUnittest : public ::testing::Test {
public:
    void TestReset();
    void TestUpdate();

protected:
    void SetUp() override { mStatus.Reset(); }

private:
    static BatchedEvents sBatch;

    GroupBatchStatus mStatus;
};

BatchedEvents GroupBatchStatusUnittest::sBatch;

void GroupBatchStatusUnittest::TestReset() {
    APSARA_TEST_EQUAL(0U, mStatus.GetSize());
    APSARA_TEST_EQUAL(0, mStatus.GetCreateTime());
}

void GroupBatchStatusUnittest::TestUpdate() {
    mStatus.Update(sBatch);
    time_t createTime = mStatus.GetCreateTime();
    APSARA_TEST_EQUAL(sBatch.DataSize(), mStatus.GetSize());

    mStatus.Update(sBatch);
    APSARA_TEST_EQUAL(2 * sBatch.DataSize(), mStatus.GetSize());
    APSARA_TEST_EQUAL(createTime, mStatus.GetCreateTime());
}

UNIT_TEST_CASE(GroupBatchStatusUnittest, TestReset)
UNIT_TEST_CASE(GroupBatchStatusUnittest, TestUpdate)

} // namespace logtail

UNIT_TEST_MAIN
