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


#include "pipeline/batch/BatchItem.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class EventBatchItemUnittest : public ::testing::Test {
public:
    void TestReset();
    void TestAddSourceBuffer();
    void TestAdd();
    void TestFlushEmpty();
    void TestFlushGroupBatchItem();
    void TestFlushBatchedEvensList();
    void TestFlushBatchedEvensLists();
    void TestExactlyOnce();

protected:
    static void SetUpTestCase() {
        sSourceBuffer.reset(new SourceBuffer);
        sEventGroup.reset(new PipelineEventGroup(sSourceBuffer));
        sEventGroup->SetTag(string("key"), string("val"));
        sEventGroup->SetExactlyOnceCheckpoint(RangeCheckpointPtr(new RangeCheckpoint));
    }

    void SetUp() override {
        StringBuffer b = sEventGroup->GetSourceBuffer()->CopyString(string("pack_id"));
        mItem.Reset(sEventGroup->GetSizedTags(),
                    sEventGroup->GetSourceBuffer(),
                    sEventGroup->GetExactlyOnceCheckpoint(),
                    StringView(b.data, b.size));
    }

    void TearDown() override {
        vector<PipelineEventPtr> tmp;
        sEventGroup->SwapEvents(tmp);
    }

private:
    static shared_ptr<SourceBuffer> sSourceBuffer;
    static unique_ptr<PipelineEventGroup> sEventGroup;

    EventBatchItem<EventBatchStatus> mItem;
};

shared_ptr<SourceBuffer> EventBatchItemUnittest::sSourceBuffer;
unique_ptr<PipelineEventGroup> EventBatchItemUnittest::sEventGroup;

void EventBatchItemUnittest::TestReset() {
    APSARA_TEST_EQUAL(1U, mItem.mBatch.mTags.mInner.size());
    APSARA_TEST_NOT_EQUAL(nullptr, mItem.mBatch.mExactlyOnceCheckpoint);
    APSARA_TEST_STREQ("pack_id", mItem.mBatch.mPackIdPrefix.data());
    APSARA_TEST_EQUAL(1U, mItem.mBatch.mSourceBuffers.size());
    APSARA_TEST_EQUAL(1U, mItem.mSourceBuffers.size());
}

void EventBatchItemUnittest::TestAddSourceBuffer() {
    shared_ptr<SourceBuffer> buffer1 = sEventGroup->GetSourceBuffer();
    shared_ptr<SourceBuffer> buffer2 = make_shared<SourceBuffer>();

    mItem.AddSourceBuffer(buffer1);
    APSARA_TEST_EQUAL(1U, mItem.mBatch.mSourceBuffers.size());
    APSARA_TEST_EQUAL(1U, mItem.mSourceBuffers.size());

    mItem.AddSourceBuffer(buffer2);
    APSARA_TEST_EQUAL(2U, mItem.mBatch.mSourceBuffers.size());
    APSARA_TEST_EQUAL(2U, mItem.mSourceBuffers.size());
}

void EventBatchItemUnittest::TestAdd() {
    sEventGroup->AddLogEvent();
    PipelineEventPtr& e = sEventGroup->MutableEvents().back();
    size_t size = e->DataSize();
    mItem.Add(std::move(e));

    APSARA_TEST_EQUAL(1U, mItem.mBatch.mEvents.size());
    APSARA_TEST_EQUAL(1U, mItem.GetStatus().GetCnt());
    APSARA_TEST_EQUAL(size, mItem.GetStatus().GetSize());
    // APSARA_TEST_NOT_EQUAL(0, mItem.mTotalEnqueTimeMs);
}

void EventBatchItemUnittest::TestFlushEmpty() {
    {
        GroupBatchItem res;
        mItem.Flush(res);
        APSARA_TEST_TRUE(res.mGroups.empty());
    }
    {
        BatchedEventsList res;
        mItem.Flush(res);
        APSARA_TEST_TRUE(res.empty());
    }
    {
        vector<BatchedEventsList> res;
        mItem.Flush(res);
        APSARA_TEST_TRUE(res.empty());
    }
}

void EventBatchItemUnittest::TestFlushGroupBatchItem() {
    sEventGroup->AddLogEvent();
    PipelineEventPtr& e = sEventGroup->MutableEvents().back();
    mItem.Add(std::move(e));
    auto size = mItem.DataSize();

    GroupBatchItem res;
    mItem.Flush(res);
    APSARA_TEST_EQUAL(1U, res.mGroups.size());
    APSARA_TEST_EQUAL(size, res.mGroups[0].mSizeBytes);

    APSARA_TEST_TRUE(mItem.IsEmpty());
    APSARA_TEST_TRUE(mItem.mBatch.mTags.mInner.empty());
    APSARA_TEST_EQUAL(nullptr, mItem.mBatch.mExactlyOnceCheckpoint);
    APSARA_TEST_TRUE(mItem.mBatch.mPackIdPrefix.empty());
    APSARA_TEST_TRUE(mItem.mBatch.mSourceBuffers.empty());
    APSARA_TEST_TRUE(mItem.mSourceBuffers.empty());
    APSARA_TEST_EQUAL(0U, mItem.GetStatus().GetCnt());
    APSARA_TEST_EQUAL(0U, mItem.GetStatus().GetSize());
    APSARA_TEST_EQUAL(0, mItem.GetStatus().GetCreateTime());
    // APSARA_TEST_EQUAL(0, mItem.mTotalEnqueTimeMs);
}

void EventBatchItemUnittest::TestFlushBatchedEvensList() {
    sEventGroup->AddLogEvent();
    PipelineEventPtr& e = sEventGroup->MutableEvents().back();
    mItem.Add(std::move(e));
    auto size = mItem.DataSize();

    BatchedEventsList res;
    mItem.Flush(res);
    APSARA_TEST_EQUAL(1U, res.size());
    APSARA_TEST_EQUAL(1U, res[0].mEvents.size());
    APSARA_TEST_EQUAL(1U, res[0].mTags.mInner.size());
    APSARA_TEST_NOT_EQUAL(nullptr, res[0].mExactlyOnceCheckpoint);
    APSARA_TEST_STREQ("pack_id", res[0].mPackIdPrefix.data());
    APSARA_TEST_EQUAL(1U, res[0].mSourceBuffers.size());
    APSARA_TEST_EQUAL(size, res[0].mSizeBytes);

    APSARA_TEST_TRUE(mItem.IsEmpty());
    APSARA_TEST_TRUE(mItem.mBatch.mTags.mInner.empty());
    APSARA_TEST_EQUAL(nullptr, mItem.mBatch.mExactlyOnceCheckpoint);
    APSARA_TEST_TRUE(mItem.mBatch.mPackIdPrefix.empty());
    APSARA_TEST_TRUE(mItem.mBatch.mSourceBuffers.empty());
    APSARA_TEST_TRUE(mItem.mSourceBuffers.empty());
    APSARA_TEST_EQUAL(0U, mItem.GetStatus().GetCnt());
    APSARA_TEST_EQUAL(0U, mItem.GetStatus().GetSize());
    APSARA_TEST_EQUAL(0, mItem.GetStatus().GetCreateTime());
    // APSARA_TEST_EQUAL(0, mItem.mTotalEnqueTimeMs);
}

void EventBatchItemUnittest::TestFlushBatchedEvensLists() {
    sEventGroup->AddLogEvent();
    PipelineEventPtr& e = sEventGroup->MutableEvents().back();
    mItem.Add(std::move(e));
    auto size = mItem.DataSize();

    vector<BatchedEventsList> res;
    mItem.Flush(res);
    APSARA_TEST_EQUAL(1U, res.size());
    APSARA_TEST_EQUAL(1U, res[0].size());
    APSARA_TEST_EQUAL(1U, res[0][0].mEvents.size());
    APSARA_TEST_EQUAL(1U, res[0][0].mTags.mInner.size());
    APSARA_TEST_NOT_EQUAL(nullptr, res[0][0].mExactlyOnceCheckpoint);
    APSARA_TEST_STREQ("pack_id", res[0][0].mPackIdPrefix.data());
    APSARA_TEST_EQUAL(1U, res[0][0].mSourceBuffers.size());
    APSARA_TEST_EQUAL(size, res[0][0].mSizeBytes);

    APSARA_TEST_TRUE(mItem.IsEmpty());
    APSARA_TEST_TRUE(mItem.mBatch.mTags.mInner.empty());
    APSARA_TEST_EQUAL(nullptr, mItem.mBatch.mExactlyOnceCheckpoint);
    APSARA_TEST_TRUE(mItem.mBatch.mPackIdPrefix.empty());
    APSARA_TEST_TRUE(mItem.mBatch.mSourceBuffers.empty());
    APSARA_TEST_TRUE(mItem.mSourceBuffers.empty());
    APSARA_TEST_EQUAL(0U, mItem.GetStatus().GetCnt());
    APSARA_TEST_EQUAL(0U, mItem.GetStatus().GetSize());
    APSARA_TEST_EQUAL(0, mItem.GetStatus().GetCreateTime());
    // APSARA_TEST_EQUAL(0, mItem.mTotalEnqueTimeMs);
}

void EventBatchItemUnittest::TestExactlyOnce() {
    sEventGroup->AddLogEvent()->SetPosition(1, 5);
    sEventGroup->AddLogEvent()->SetPosition(6, 10);
    for (auto& e : sEventGroup->MutableEvents()) {
        mItem.Add(std::move(e));
    }

    BatchedEventsList res;
    mItem.Flush(res);
    APSARA_TEST_EQUAL(1U, res[0].mExactlyOnceCheckpoint->data.read_offset());
    APSARA_TEST_EQUAL(15U, res[0].mExactlyOnceCheckpoint->data.read_length());
}

UNIT_TEST_CASE(EventBatchItemUnittest, TestReset)
UNIT_TEST_CASE(EventBatchItemUnittest, TestAdd)
UNIT_TEST_CASE(EventBatchItemUnittest, TestAddSourceBuffer)
UNIT_TEST_CASE(EventBatchItemUnittest, TestFlushEmpty)
UNIT_TEST_CASE(EventBatchItemUnittest, TestFlushGroupBatchItem)
UNIT_TEST_CASE(EventBatchItemUnittest, TestFlushBatchedEvensList)
UNIT_TEST_CASE(EventBatchItemUnittest, TestFlushBatchedEvensLists)
UNIT_TEST_CASE(EventBatchItemUnittest, TestExactlyOnce)

class GroupBatchItemUnittest : public ::testing::Test {
public:
    void TestAdd();
    void TestFlushEmpty();
    void TestFlushBatchedEvensList();
    void TestFlushBatchedEvensLists();

protected:
    void SetUp() override {
        shared_ptr<SourceBuffer> sourceBuffer = make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        eventGroup.SetTag(string("key"), string("val"));
        eventGroup.AddLogEvent();
        eventGroup.SetExactlyOnceCheckpoint(RangeCheckpointPtr(new RangeCheckpoint));

        mBatch.mEvents = std::move(eventGroup.MutableEvents());
        mBatch.mSourceBuffers.emplace_back(std::move(eventGroup.GetSourceBuffer()));
        mBatch.mTags = std::move(eventGroup.GetSizedTags());
        mBatch.mSizeBytes = 100;
    }

    void TearDown() override {
        mBatch.Clear();
        mItem.Clear();
    }

private:
    BatchedEvents mBatch;
    GroupBatchItem mItem;
};

void GroupBatchItemUnittest::TestAdd() {
    size_t size = mBatch.mSizeBytes;
    mItem.Add(std::move(mBatch), 1234567890000);

    APSARA_TEST_EQUAL(1U, mItem.mGroups.size());
    APSARA_TEST_EQUAL(size, mItem.GetStatus().GetSize());
    // APSARA_TEST_EQUAL(1234567890000, mItem.TotalEnqueTimeMs());
    APSARA_TEST_EQUAL(1U, mItem.EventSize());
    APSARA_TEST_EQUAL(1U, mItem.GroupSize());
    APSARA_TEST_EQUAL(100U, mItem.DataSize());
}

void GroupBatchItemUnittest::TestFlushEmpty() {
    {
        BatchedEventsList res;
        mItem.Flush(res);
        APSARA_TEST_TRUE(res.empty());
    }
    {
        vector<BatchedEventsList> res;
        mItem.Flush(res);
        APSARA_TEST_TRUE(res.empty());
    }
}

void GroupBatchItemUnittest::TestFlushBatchedEvensList() {
    mItem.Add(std::move(mBatch), 1234567890000);

    BatchedEventsList res;
    mItem.Flush(res);
    APSARA_TEST_EQUAL(1U, res.size());

    APSARA_TEST_TRUE(mItem.IsEmpty());
    APSARA_TEST_EQUAL(0U, mItem.GetStatus().GetSize());
    APSARA_TEST_EQUAL(0, mItem.GetStatus().GetCreateTime());
    APSARA_TEST_EQUAL(0, mItem.TotalEnqueTimeMs());
    APSARA_TEST_EQUAL(0U, mItem.EventSize());
    APSARA_TEST_EQUAL(0U, mItem.GroupSize());
    APSARA_TEST_EQUAL(0U, mItem.DataSize());
}

void GroupBatchItemUnittest::TestFlushBatchedEvensLists() {
    mItem.Add(std::move(mBatch), 1234567890000);

    vector<BatchedEventsList> res;
    mItem.Flush(res);
    APSARA_TEST_EQUAL(1U, res.size());
    APSARA_TEST_EQUAL(1U, res[0].size());

    APSARA_TEST_TRUE(mItem.IsEmpty());
    APSARA_TEST_EQUAL(0U, mItem.GetStatus().GetSize());
    APSARA_TEST_EQUAL(0, mItem.GetStatus().GetCreateTime());
    APSARA_TEST_EQUAL(0, mItem.TotalEnqueTimeMs());
    APSARA_TEST_EQUAL(0U, mItem.EventSize());
    APSARA_TEST_EQUAL(0U, mItem.GroupSize());
    APSARA_TEST_EQUAL(0U, mItem.DataSize());
}

UNIT_TEST_CASE(GroupBatchItemUnittest, TestAdd)
UNIT_TEST_CASE(GroupBatchItemUnittest, TestFlushEmpty)
UNIT_TEST_CASE(GroupBatchItemUnittest, TestFlushBatchedEvensList)
UNIT_TEST_CASE(GroupBatchItemUnittest, TestFlushBatchedEvensLists)

} // namespace logtail

UNIT_TEST_MAIN
