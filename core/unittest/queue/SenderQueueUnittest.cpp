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
    void TestGetAvailableItems();
    void TestMetric();

protected:
    static void SetUpTestCase() {
        sConcurrencyLimiter = make_shared<ConcurrencyLimiter>("", 80);
        sCtx.SetConfigName("test_config");
    }

    void SetUp() override {
        mQueue.reset(new SenderQueue(sCap, sLowWatermark, sHighWatermark, sKey, sFlusherId, sCtx));
        mQueue->SetConcurrencyLimiters({{"region", sConcurrencyLimiter}});
        mQueue->mRateLimiter = RateLimiter(100);
        mQueue->SetFeedback(&sFeedback);
    }

    void TearDown() override {
        sFeedback.Clear();
        sConcurrencyLimiter = make_shared<ConcurrencyLimiter>("", 80);
    }

private:
    static PipelineContext sCtx;
    static const QueueKey sKey = 0;
    static const string sFlusherId;
    static const size_t sCap = 2;
    static const size_t sLowWatermark = 1;
    static const size_t sHighWatermark = 2;

    static const size_t sDataSize = 10;

    static FeedbackInterfaceMock sFeedback;
    static shared_ptr<ConcurrencyLimiter> sConcurrencyLimiter;

    unique_ptr<SenderQueueItem> GenerateItem();

    unique_ptr<SenderQueue> mQueue;
};

PipelineContext SenderQueueUnittest::sCtx;
const QueueKey SenderQueueUnittest::sKey;
const string SenderQueueUnittest::sFlusherId = "1";
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

void SenderQueueUnittest::TestGetAvailableItems() {
    vector<SenderQueueItem*> items;
    for (size_t i = 0; i <= sCap; ++i) {
        auto item = GenerateItem();
        items.emplace_back(item.get());
        mQueue->Push(std::move(item));
    }
    {
        // no limits
        vector<SenderQueueItem*> items;
        mQueue->GetAvailableItems(items, -1);
        APSARA_TEST_EQUAL(2U, items.size());
        for (auto& item : items) {
            item->mStatus = SendingStatus::IDLE;
        }
    }
    {
        // with limits, limited by concurrency limiter
        mQueue->mRateLimiter->mMaxSendBytesPerSecond = 100;
        sConcurrencyLimiter->SetCurrentLimit(1);
        sConcurrencyLimiter->SetInSendingCount(0);
        vector<SenderQueueItem*> items;
        mQueue->GetAvailableItems(items, 80);
        APSARA_TEST_EQUAL(1U, items.size());
        APSARA_TEST_EQUAL(sDataSize, mQueue->mRateLimiter->mLastSecondTotalBytes);
        APSARA_TEST_EQUAL(1U, sConcurrencyLimiter->GetInSendingCount());
        for (auto& item : items) {
            item->mStatus = SendingStatus::IDLE;
        }
        mQueue->mRateLimiter->mLastSecondTotalBytes = 0;
    }
    {
        // with limits, limited by rate limiter
        mQueue->mRateLimiter->mMaxSendBytesPerSecond = 5;
        sConcurrencyLimiter->SetCurrentLimit(3);
        sConcurrencyLimiter->SetInSendingCount(0);
        vector<SenderQueueItem*> items;
        mQueue->GetAvailableItems(items, 80);
        APSARA_TEST_EQUAL(1U, items.size());
        APSARA_TEST_EQUAL(sDataSize, mQueue->mRateLimiter->mLastSecondTotalBytes);
        APSARA_TEST_EQUAL(1U, sConcurrencyLimiter->GetInSendingCount());
        mQueue->mRateLimiter->mLastSecondTotalBytes = 0;
    }
    {
        // with limits, does not work
        mQueue->mRateLimiter->mMaxSendBytesPerSecond = 100;
        sConcurrencyLimiter->SetCurrentLimit(3);
        sConcurrencyLimiter->SetInSendingCount(0);
        vector<SenderQueueItem*> items;
        mQueue->GetAvailableItems(items, 80);
        APSARA_TEST_EQUAL(1U, items.size());
        APSARA_TEST_EQUAL(sDataSize, mQueue->mRateLimiter->mLastSecondTotalBytes);
        APSARA_TEST_EQUAL(1U, sConcurrencyLimiter->GetInSendingCount());
    }
}

void SenderQueueUnittest::TestMetric() {
    APSARA_TEST_EQUAL(5U, mQueue->mMetricsRecordRef->GetLabels()->size());
    APSARA_TEST_TRUE(mQueue->mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_PROJECT, ""));
    APSARA_TEST_TRUE(mQueue->mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_PIPELINE_NAME, "test_config"));
    APSARA_TEST_TRUE(mQueue->mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_COMPONENT_NAME, METRIC_LABEL_VALUE_COMPONENT_NAME_SENDER_QUEUE));
    APSARA_TEST_TRUE(mQueue->mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_QUEUE_TYPE, "bounded"));
    APSARA_TEST_TRUE(mQueue->mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_FLUSHER_PLUGIN_ID, sFlusherId));

    auto item1 = GenerateItem();
    auto dataSize = item1->mData.size();
    auto ptr1 = item1.get();
    mQueue->Push(std::move(item1));

    APSARA_TEST_EQUAL(1U, mQueue->mInItemsTotal->GetValue());
    APSARA_TEST_EQUAL(dataSize, mQueue->mInItemDataSizeBytes->GetValue());
    APSARA_TEST_EQUAL(1U, mQueue->mQueueSizeTotal->GetValue());
    APSARA_TEST_EQUAL(dataSize, mQueue->mQueueDataSizeByte->GetValue());
    APSARA_TEST_EQUAL(1U, mQueue->mValidToPushFlag->GetValue());

    auto item2 = GenerateItem();
    auto ptr2 = item2.get();
    mQueue->Push(std::move(item2));

    mQueue->Push(GenerateItem());

    APSARA_TEST_EQUAL(3U, mQueue->mInItemsTotal->GetValue());
    APSARA_TEST_EQUAL(dataSize * 3, mQueue->mInItemDataSizeBytes->GetValue());
    APSARA_TEST_EQUAL(2U, mQueue->mQueueSizeTotal->GetValue());
    APSARA_TEST_EQUAL(dataSize * 2, mQueue->mQueueDataSizeByte->GetValue());
    APSARA_TEST_EQUAL(0U, mQueue->mValidToPushFlag->GetValue());
    APSARA_TEST_EQUAL(1U, mQueue->mExtraBufferSize->GetValue());
    APSARA_TEST_EQUAL(dataSize, mQueue->mExtraBufferDataSizeBytes->GetValue());

    mQueue->Remove(ptr1);
    APSARA_TEST_EQUAL(1U, mQueue->mOutItemsTotal->GetValue());
    APSARA_TEST_EQUAL(2U, mQueue->mQueueSizeTotal->GetValue());
    APSARA_TEST_EQUAL(dataSize * 2, mQueue->mQueueDataSizeByte->GetValue());
    APSARA_TEST_EQUAL(0U, mQueue->mValidToPushFlag->GetValue());
    APSARA_TEST_EQUAL(0U, mQueue->mExtraBufferSize->GetValue());
    APSARA_TEST_EQUAL(0U, mQueue->mExtraBufferDataSizeBytes->GetValue());

    mQueue->Remove(ptr2);
    APSARA_TEST_EQUAL(2U, mQueue->mOutItemsTotal->GetValue());
    APSARA_TEST_EQUAL(1U, mQueue->mQueueSizeTotal->GetValue());
    APSARA_TEST_EQUAL(dataSize, mQueue->mQueueDataSizeByte->GetValue());
    APSARA_TEST_EQUAL(1U, mQueue->mValidToPushFlag->GetValue());
}

unique_ptr<SenderQueueItem> SenderQueueUnittest::GenerateItem() {
    return make_unique<SenderQueueItem>("content", sDataSize, nullptr, sKey);
}

UNIT_TEST_CASE(SenderQueueUnittest, TestPush)
UNIT_TEST_CASE(SenderQueueUnittest, TestRemove)
UNIT_TEST_CASE(SenderQueueUnittest, TestGetAvailableItems)
UNIT_TEST_CASE(SenderQueueUnittest, TestMetric)

} // namespace logtail

UNIT_TEST_MAIN
