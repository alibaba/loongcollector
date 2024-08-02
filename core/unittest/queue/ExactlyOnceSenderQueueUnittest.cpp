// Copyright 2024 iLogtail Authors
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

#include "flusher/sls/FlusherSLS.h"
#include "queue/ExactlyOnceSenderQueue.h"
#include "queue/SLSSenderQueueItem.h"
#include "unittest/Unittest.h"
#include "unittest/queue/FeedbackInterfaceMock.h"

using namespace std;

namespace logtail {

class ExactlyOnceSenderQueueUnittest : public testing::Test {
public:
    void TestPush();
    void TestRemove();
    void TestGetAllAvailableItems();
    void TestReset();

protected:
    static void SetUpTestCase() {
        for (size_t i = 0; i < 2; ++i) {
            auto cpt = make_shared<RangeCheckpoint>();
            cpt->index = i;
            cpt->data.set_hash_key("key");
            cpt->data.set_sequence_id(0);
            sCheckpoints.emplace_back(cpt);
        }
    }

    void SetUp() override {
        mQueue.reset(new ExactlyOnceSenderQueue(sCheckpoints, sKey));
        mQueue->SetFeedback(&sFeedback);
        mFlusher.mMaxSendRate = 100;
        mFlusher.mRegion = "region";
        mFlusher.mProject = "project";
    }

    void TearDown() override { sFeedback.Clear(); }

private:
    static const QueueKey sKey = 0;
    static const size_t sDataSize = 10;

    static FeedbackInterfaceMock sFeedback;
    static vector<RangeCheckpointPtr> sCheckpoints;

    unique_ptr<SenderQueueItem> GenerateItem(int32_t idx = -1);

    // cannot be static member, because its constructor relies on logger, which is initiallized after main starts
    FlusherSLS mFlusher;
    unique_ptr<ExactlyOnceSenderQueue> mQueue;
};

const size_t ExactlyOnceSenderQueueUnittest::sDataSize;
FeedbackInterfaceMock ExactlyOnceSenderQueueUnittest::sFeedback;
vector<RangeCheckpointPtr> ExactlyOnceSenderQueueUnittest::sCheckpoints;

void ExactlyOnceSenderQueueUnittest::TestPush() {
    // first push, replay item
    APSARA_TEST_TRUE(mQueue->Push(GenerateItem(1)));
    APSARA_TEST_EQUAL(1U, mQueue->Size());
    APSARA_TEST_FALSE(mQueue->Full());
    APSARA_TEST_TRUE(mQueue->IsValidToPush());
    APSARA_TEST_NOT_EQUAL(nullptr, mQueue->mQueue[1]);
    APSARA_TEST_TRUE(mQueue->mRateLimiter.has_value());
    APSARA_TEST_EQUAL(100U, mQueue->mRateLimiter->mMaxSendBytesPerSecond);
    APSARA_TEST_EQUAL(2U, mQueue->mConcurrencyLimiters.size());
    APSARA_TEST_EQUAL(FlusherSLS::GetRegionConcurrencyLimiter("region"), mQueue->mConcurrencyLimiters[0]);
    APSARA_TEST_EQUAL(FlusherSLS::GetProjectConcurrencyLimiter("project"), mQueue->mConcurrencyLimiters[1]);

    // reach high water mark
    APSARA_TEST_TRUE(mQueue->Push(GenerateItem()));
    APSARA_TEST_EQUAL(2U, mQueue->Size());
    APSARA_TEST_TRUE(mQueue->Full());
    APSARA_TEST_FALSE(mQueue->IsValidToPush());

    // full
    APSARA_TEST_TRUE(mQueue->Push(GenerateItem()));
    APSARA_TEST_EQUAL(2U, mQueue->Size());
    APSARA_TEST_TRUE(mQueue->Full());
    APSARA_TEST_FALSE(mQueue->IsValidToPush());
    APSARA_TEST_EQUAL(1U, mQueue->mExtraBuffer.size());
}

void ExactlyOnceSenderQueueUnittest::TestRemove() {
    vector<SenderQueueItem*> items;
    for (size_t i = 0; i <= sCheckpoints.size(); ++i) {
        auto item = GenerateItem();
        items.emplace_back(item.get());
        mQueue->Push(std::move(item));
    }

    // full
    APSARA_TEST_TRUE(mQueue->Remove(items[0]));
    APSARA_TEST_EQUAL(2U, mQueue->Size());
    APSARA_TEST_TRUE(mQueue->Full());
    APSARA_TEST_FALSE(mQueue->IsValidToPush());
    APSARA_TEST_EQUAL(0U, mQueue->mExtraBuffer.size());

    // // drop to low water mark
    APSARA_TEST_TRUE(mQueue->Remove(items[1]));
    APSARA_TEST_EQUAL(1U, mQueue->Size());
    APSARA_TEST_FALSE(mQueue->Full());
    APSARA_TEST_TRUE(mQueue->IsValidToPush());
    APSARA_TEST_TRUE(sFeedback.HasFeedback(0));
}

void ExactlyOnceSenderQueueUnittest::TestGetAllAvailableItems() {
    for (size_t i = 0; i <= sCheckpoints.size(); ++i) {
        mQueue->Push(GenerateItem());
    }
    {
        // no limits
        vector<SenderQueueItem*> items;
        mQueue->GetAllAvailableItems(items, false);
        APSARA_TEST_EQUAL(2U, items.size());
        for (auto& item : items) {
            item->mStatus = SendingStatus::IDLE;
        }
    }
    {
        // with limits, limited by concurrency limiter
        mQueue->mRateLimiter->mMaxSendBytesPerSecond = 100;
        mQueue->mConcurrencyLimiters[0]->SetLimit(1);
        vector<SenderQueueItem*> items;
        mQueue->GetAllAvailableItems(items);
        APSARA_TEST_EQUAL(1U, items.size());
        APSARA_TEST_EQUAL(sDataSize, mQueue->mRateLimiter->mLastSecondTotalBytes);
        APSARA_TEST_EQUAL(0, mQueue->mConcurrencyLimiters[0]->GetLimit());
        for (auto& item : items) {
            item->mStatus = SendingStatus::IDLE;
        }
        mQueue->mRateLimiter->mLastSecondTotalBytes = 0;
    }
    {
        // with limits, limited by rate limiter
        mQueue->mRateLimiter->mMaxSendBytesPerSecond = 5;
        mQueue->mConcurrencyLimiters[0]->SetLimit(3);
        vector<SenderQueueItem*> items;
        mQueue->GetAllAvailableItems(items);
        APSARA_TEST_EQUAL(1U, items.size());
        APSARA_TEST_EQUAL(sDataSize, mQueue->mRateLimiter->mLastSecondTotalBytes);
        APSARA_TEST_EQUAL(2, mQueue->mConcurrencyLimiters[0]->GetLimit());
        mQueue->mRateLimiter->mLastSecondTotalBytes = 0;
    }
    {
        // with limits, does not work
        mQueue->mRateLimiter->mMaxSendBytesPerSecond = 100;
        mQueue->mConcurrencyLimiters[0]->SetLimit(3);
        vector<SenderQueueItem*> items;
        mQueue->GetAllAvailableItems(items);
        APSARA_TEST_EQUAL(1U, items.size());
        APSARA_TEST_EQUAL(sDataSize, mQueue->mRateLimiter->mLastSecondTotalBytes);
        APSARA_TEST_EQUAL(2, mQueue->mConcurrencyLimiters[0]->GetLimit());
    }
}

void ExactlyOnceSenderQueueUnittest::TestReset() {
    for (size_t i = 0; i <= sCheckpoints.size(); ++i) {
        mQueue->Push(GenerateItem());
    }
    mQueue->InvalidatePop();

    vector<RangeCheckpointPtr> newCheckpoints;
    for (size_t i = 0; i < 5; ++i) {
        auto cpt = make_shared<RangeCheckpoint>();
        cpt->index = i;
        cpt->data.set_hash_key("key");
        newCheckpoints.emplace_back(cpt);
    }
    mQueue->Reset(newCheckpoints);

    APSARA_TEST_EQUAL(newCheckpoints.size(), mQueue->mCapacity);
    APSARA_TEST_EQUAL(newCheckpoints.size() - 1, mQueue->mLowWatermark);
    APSARA_TEST_EQUAL(newCheckpoints.size(), mQueue->mHighWatermark);
    APSARA_TEST_TRUE(mQueue->Empty());
    APSARA_TEST_TRUE(mQueue->mExtraBuffer.empty());
    APSARA_TEST_TRUE(mQueue->mValidToPop);
    APSARA_TEST_TRUE(mQueue->mValidToPush);
    APSARA_TEST_TRUE(mQueue->mConcurrencyLimiters.empty());
    APSARA_TEST_FALSE(mQueue->mRateLimiter.has_value());
}

unique_ptr<SenderQueueItem> ExactlyOnceSenderQueueUnittest::GenerateItem(int32_t idx) {
    auto cpt = make_shared<RangeCheckpoint>();
    if (idx != -1) {
        cpt->index = idx;
        cpt->data.set_hash_key("key");
        cpt->data.set_sequence_id(0);
        cpt->data.set_read_offset(0);
        cpt->data.set_read_length(10);
    }
    return make_unique<SLSSenderQueueItem>("content",
                                           sDataSize,
                                           &mFlusher,
                                           cpt->fbKey,
                                           "",
                                           RawDataType::EVENT_GROUP,
                                           cpt->data.hash_key(),
                                           std::move(cpt),
                                           false);
}

UNIT_TEST_CASE(ExactlyOnceSenderQueueUnittest, TestPush)
UNIT_TEST_CASE(ExactlyOnceSenderQueueUnittest, TestRemove)
UNIT_TEST_CASE(ExactlyOnceSenderQueueUnittest, TestGetAllAvailableItems)
UNIT_TEST_CASE(ExactlyOnceSenderQueueUnittest, TestReset)

} // namespace logtail

UNIT_TEST_MAIN
