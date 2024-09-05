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

#include "pipeline/queue/SenderQueue.h"
#include "unittest/Unittest.h"
#include "unittest/queue/FeedbackInterfaceMock.h"

using namespace std;

namespace logtail {

class SenderQueueUnittest : public testing::Test {
public:
    void TestPush();
    void TestRemove();
    void TestGetAllAvailableItems();

protected:
    static void SetUpTestCase() { sConcurrencyLimiter = make_shared<ConcurrencyLimiter>(); }

    void SetUp() override {
        mQueue.reset(new SenderQueue(sCap, sLowWatermark, sHighWatermark, sKey));
        mQueue->SetConcurrencyLimiters(vector<shared_ptr<ConcurrencyLimiter>>{sConcurrencyLimiter});
        mQueue->mRateLimiter = RateLimiter(100);
        mQueue->SetFeedback(&sFeedback);
    }

    void TearDown() override {
        sFeedback.Clear();
        sConcurrencyLimiter->Reset();
    }

private:
    static const QueueKey sKey = 0;
    static const size_t sCap = 2;
    static const size_t sLowWatermark = 1;
    static const size_t sHighWatermark = 2;

    static const size_t sDataSize = 10;

    static FeedbackInterfaceMock sFeedback;
    static shared_ptr<ConcurrencyLimiter> sConcurrencyLimiter;

    unique_ptr<SenderQueueItem> GenerateItem();

    unique_ptr<SenderQueue> mQueue;
};

const QueueKey SenderQueueUnittest::sKey;
const size_t SenderQueueUnittest::sDataSize;
shared_ptr<ConcurrencyLimiter> SenderQueueUnittest::sConcurrencyLimiter;
FeedbackInterfaceMock SenderQueueUnittest::sFeedback;

void SenderQueueUnittest::TestPush() {
    // normal push
    APSARA_TEST_TRUE(mQueue->Push(GenerateItem()));
    APSARA_TEST_EQUAL(1U, mQueue->Size());
    APSARA_TEST_FALSE(mQueue->Full());
    APSARA_TEST_TRUE(mQueue->IsValidToPush());

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

void SenderQueueUnittest::TestRemove() {
    vector<SenderQueueItem*> items;
    for (size_t i = 0; i <= sCap; ++i) {
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

    // drop to low water mark
    APSARA_TEST_TRUE(mQueue->Remove(items[1]));
    APSARA_TEST_EQUAL(1U, mQueue->Size());
    APSARA_TEST_FALSE(mQueue->Full());
    APSARA_TEST_TRUE(mQueue->IsValidToPush());
    APSARA_TEST_TRUE(sFeedback.HasFeedback(0));

    // not existed
    APSARA_TEST_FALSE(mQueue->Remove(items[0]));
}

void SenderQueueUnittest::TestGetAllAvailableItems() {
    vector<SenderQueueItem*> items;
    for (size_t i = 0; i <= sCap; ++i) {
        auto item = GenerateItem();
        items.emplace_back(item.get());
        mQueue->Push(std::move(item));
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
        sConcurrencyLimiter->SetLimit(1);
        vector<SenderQueueItem*> items;
        mQueue->GetAllAvailableItems(items);
        APSARA_TEST_EQUAL(1U, items.size());
        APSARA_TEST_EQUAL(sDataSize, mQueue->mRateLimiter->mLastSecondTotalBytes);
        APSARA_TEST_EQUAL(0, sConcurrencyLimiter->GetLimit());
        for (auto& item : items) {
            item->mStatus = SendingStatus::IDLE;
        }
        mQueue->mRateLimiter->mLastSecondTotalBytes = 0;
    }
    {
        // with limits, limited by rate limiter
        mQueue->mRateLimiter->mMaxSendBytesPerSecond = 5;
        sConcurrencyLimiter->SetLimit(3);
        vector<SenderQueueItem*> items;
        mQueue->GetAllAvailableItems(items);
        APSARA_TEST_EQUAL(1U, items.size());
        APSARA_TEST_EQUAL(sDataSize, mQueue->mRateLimiter->mLastSecondTotalBytes);
        APSARA_TEST_EQUAL(2, sConcurrencyLimiter->GetLimit());
        mQueue->mRateLimiter->mLastSecondTotalBytes = 0;
    }
    {
        // with limits, does not work
        mQueue->mRateLimiter->mMaxSendBytesPerSecond = 100;
        sConcurrencyLimiter->SetLimit(3);
        vector<SenderQueueItem*> items;
        mQueue->GetAllAvailableItems(items);
        APSARA_TEST_EQUAL(1U, items.size());
        APSARA_TEST_EQUAL(sDataSize, mQueue->mRateLimiter->mLastSecondTotalBytes);
        APSARA_TEST_EQUAL(2, sConcurrencyLimiter->GetLimit());
    }
}

unique_ptr<SenderQueueItem> SenderQueueUnittest::GenerateItem() {
    return make_unique<SenderQueueItem>("content", sDataSize, nullptr, sKey);
}

UNIT_TEST_CASE(SenderQueueUnittest, TestPush)
UNIT_TEST_CASE(SenderQueueUnittest, TestRemove)
UNIT_TEST_CASE(SenderQueueUnittest, TestGetAllAvailableItems)

} // namespace logtail

UNIT_TEST_MAIN
