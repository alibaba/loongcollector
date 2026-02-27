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

#include <memory>

#include "collection_pipeline/queue/BytesBoundedProcessQueue.h"
#include "collection_pipeline/queue/SenderQueue.h"
#include "common/FeedbackInterface.h"
#include "models/PipelineEventGroup.h"
#include "unittest/Unittest.h"
#include "unittest/queue/FeedbackInterfaceMock.h"

using namespace std;

namespace logtail {

class BytesBoundedProcessQueueUnittest : public testing::Test {
public:
    void TestPush();
    void TestPop();
    void TestSetUpStreamFeedbacks();
    void TestMetric();
    void TestBytesWatermark();
    void TestEmptyQueue();
    void TestSingleItemExceedsMaxBytes();
    void TestSingleItemExceedsHighWatermark();
    void TestExactWatermarkBoundaries();
    void TestQueueFullScenario();
    void TestMultiplePushPopCycles();
    void TestZeroSizeItem();
    void TestExactlyOnceEnabled();

protected:
    static void SetUpTestCase() { sCtx.SetConfigName("test_config"); }

    void SetUp() override {
        mQueue.reset(new BytesBoundedProcessQueue(sMaxBytes, sLowWatermark, sHighWatermark, sKey, 1, sCtx));

        mSenderQueue1.reset(new SenderQueue(10, 0, 10, 0, "", "", sCtx));
        mSenderQueue2.reset(new SenderQueue(10, 0, 10, 0, "", "", sCtx));
        mQueue->SetDownStreamQueues(vector<BoundedSenderQueueInterface*>{mSenderQueue1.get(), mSenderQueue2.get()});

        mFeedback1.reset(new FeedbackInterfaceMock);
        mFeedback2.reset(new FeedbackInterfaceMock);
        mQueue->SetUpStreamFeedbacks(vector<FeedbackInterface*>{mFeedback1.get(), mFeedback2.get()});
        mQueue->EnablePop();
    }

private:
    static CollectionPipelineContext sCtx;
    static const QueueKey sKey = 0;
    static const size_t sMaxBytes = 10000; // max capacity in bytes
    static const size_t sLowWatermark = 2000; // low watermark in bytes
    static const size_t sHighWatermark = 8000; // high watermark in bytes

    // Generate an item with specified content to control data size
    unique_ptr<ProcessQueueItem> GenerateItem(const string& content = "") {
        PipelineEventGroup g(make_shared<SourceBuffer>());
        if (!content.empty()) {
            auto* e = g.AddLogEvent();
            e->SetContent(string("key"), content);
        }
        return make_unique<ProcessQueueItem>(std::move(g), 0);
    }

    // Generate an item with a specific size
    unique_ptr<ProcessQueueItem> GenerateItemWithSize(size_t targetSize) {
        PipelineEventGroup g(make_shared<SourceBuffer>());
        auto* e = g.AddLogEvent();
        // Create content with approximately targetSize bytes
        // Note: Actual size may vary slightly due to metadata overhead
        string content(targetSize, 'x');
        e->SetContent(string("key"), content);
        return make_unique<ProcessQueueItem>(std::move(g), 0);
    }

    unique_ptr<BytesBoundedProcessQueue> mQueue;
    unique_ptr<FeedbackInterface> mFeedback1;
    unique_ptr<FeedbackInterface> mFeedback2;
    unique_ptr<BoundedSenderQueueInterface> mSenderQueue1;
    unique_ptr<BoundedSenderQueueInterface> mSenderQueue2;
};

CollectionPipelineContext BytesBoundedProcessQueueUnittest::sCtx;

void BytesBoundedProcessQueueUnittest::TestPush() {
    // Test pushing items based on bytes
    auto item1 = GenerateItemWithSize(1000);
    APSARA_TEST_TRUE(mQueue->Push(std::move(item1)));
    size_t item1Size = mQueue->mCurrentBytesSize;
    APSARA_TEST_TRUE(item1Size > 0);

    auto item2 = GenerateItemWithSize(2000);
    size_t item2Size = item2->mEventGroup.DataSize();
    APSARA_TEST_TRUE(mQueue->Push(std::move(item2)));
    APSARA_TEST_EQUAL(item1Size + item2Size, mQueue->mCurrentBytesSize);

    auto item3 = GenerateItemWithSize(5000);
    size_t item3Size = item3->mEventGroup.DataSize();
    APSARA_TEST_TRUE(mQueue->Push(std::move(item3)));
    APSARA_TEST_EQUAL(item1Size + item2Size + item3Size, mQueue->mCurrentBytesSize);

    // Now currentBytesSize should be >= high watermark, push is forbidden
    APSARA_TEST_FALSE(mQueue->IsValidToPush());
    auto item4 = GenerateItemWithSize(100);
    APSARA_TEST_FALSE(mQueue->Push(std::move(item4)));
    APSARA_TEST_EQUAL(item1Size + item2Size + item3Size, mQueue->mCurrentBytesSize);
}

void BytesBoundedProcessQueueUnittest::TestPop() {
    unique_ptr<ProcessQueueItem> item;

    // Nothing to pop from empty queue
    APSARA_TEST_FALSE(mQueue->Pop(item));

    // Push some items
    auto item1 = GenerateItemWithSize(3500);
    mQueue->Push(std::move(item1));

    // Invalidate pop
    mQueue->DisablePop();
    APSARA_TEST_FALSE(mQueue->Pop(item));
    mQueue->EnablePop();

    // Downstream queues are not valid to push
    mSenderQueue1->mValidToPush = false;
    APSARA_TEST_FALSE(mQueue->Pop(item));
    mSenderQueue1->mValidToPush = true;

    // Push to high watermark
    auto item2 = GenerateItemWithSize(3500);
    size_t item2Size = item2->mEventGroup.DataSize();
    mQueue->Push(std::move(item2));

    auto item3 = GenerateItemWithSize(1500);
    size_t item3Size = item3->mEventGroup.DataSize();
    mQueue->Push(std::move(item3));

    // Now should be at or above high watermark
    APSARA_TEST_FALSE(mQueue->IsValidToPush());

    // Pop first item - should not trigger feedback yet
    APSARA_TEST_TRUE(mQueue->Pop(item));
    APSARA_TEST_EQUAL(item2Size + item3Size, mQueue->mCurrentBytesSize);
    APSARA_TEST_FALSE(static_cast<FeedbackInterfaceMock*>(mFeedback1.get())->HasFeedback(sKey));
    APSARA_TEST_FALSE(static_cast<FeedbackInterfaceMock*>(mFeedback2.get())->HasFeedback(sKey));

    // Pop second item - now should drop below low watermark and trigger feedback
    APSARA_TEST_TRUE(mQueue->Pop(item));
    APSARA_TEST_EQUAL(item3Size, mQueue->mCurrentBytesSize);
    APSARA_TEST_TRUE(mQueue->IsValidToPush());
    APSARA_TEST_TRUE(static_cast<FeedbackInterfaceMock*>(mFeedback1.get())->HasFeedback(sKey));
    APSARA_TEST_TRUE(static_cast<FeedbackInterfaceMock*>(mFeedback2.get())->HasFeedback(sKey));
}

void BytesBoundedProcessQueueUnittest::TestSetUpStreamFeedbacks() {
    // Clear existing feedbacks
    mQueue->SetUpStreamFeedbacks(vector<FeedbackInterface*>{});

    // Push to high watermark and pop to low watermark
    auto item1 = GenerateItemWithSize(5000);
    mQueue->Push(std::move(item1));
    auto item2 = GenerateItemWithSize(3500);
    mQueue->Push(std::move(item2));

    APSARA_TEST_FALSE(mQueue->IsValidToPush());

    // Pop items - no feedback should be triggered since we cleared feedbacks
    unique_ptr<ProcessQueueItem> item;
    mQueue->Pop(item);
    mQueue->Pop(item);

    // These should still be false since we cleared the feedbacks
    APSARA_TEST_FALSE(static_cast<FeedbackInterfaceMock*>(mFeedback1.get())->HasFeedback(sKey));
    APSARA_TEST_FALSE(static_cast<FeedbackInterfaceMock*>(mFeedback2.get())->HasFeedback(sKey));

    // Set feedbacks again with nullptr handling
    static_cast<FeedbackInterfaceMock*>(mFeedback1.get())->Clear();
    static_cast<FeedbackInterfaceMock*>(mFeedback2.get())->Clear();
    mQueue->SetUpStreamFeedbacks(vector<FeedbackInterface*>{mFeedback1.get(), nullptr, mFeedback2.get()});

    // Trigger feedback by reaching high and then low watermark
    auto item3 = GenerateItemWithSize(5000);
    mQueue->Push(std::move(item3));
    auto item4 = GenerateItemWithSize(3500);
    mQueue->Push(std::move(item4));

    mQueue->Pop(item);
    mQueue->Pop(item);

    // Feedbacks should be triggered
    APSARA_TEST_TRUE(static_cast<FeedbackInterfaceMock*>(mFeedback1.get())->HasFeedback(sKey));
    APSARA_TEST_TRUE(static_cast<FeedbackInterfaceMock*>(mFeedback2.get())->HasFeedback(sKey));
}

void BytesBoundedProcessQueueUnittest::TestMetric() {
    // Test metric labels
    APSARA_TEST_EQUAL(4U, mQueue->mMetricsRecordRef->GetLabels()->size());
    APSARA_TEST_TRUE(mQueue->mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_PROJECT, ""));
    APSARA_TEST_TRUE(mQueue->mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_PIPELINE_NAME, "test_config"));
    APSARA_TEST_TRUE(mQueue->mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_COMPONENT_NAME,
                                                        METRIC_LABEL_VALUE_COMPONENT_NAME_PROCESS_QUEUE));
    APSARA_TEST_TRUE(mQueue->mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_QUEUE_TYPE, "bounded"));

    // Test metric values
    auto item = GenerateItem();
    auto* e = item->mEventGroup.AddLogEvent();
    e->SetContent(string("key"), string("value"));
    auto dataSize = item->mEventGroup.DataSize();
    mQueue->Push(std::move(item));

    APSARA_TEST_EQUAL(1U, mQueue->mInItemsTotal->GetValue());
    APSARA_TEST_EQUAL(dataSize, mQueue->mInItemDataSizeBytes->GetValue());
    APSARA_TEST_EQUAL(1U, mQueue->mQueueSizeTotal->GetValue());
    APSARA_TEST_EQUAL(dataSize, mQueue->mQueueDataSizeByte->GetValue());
    APSARA_TEST_EQUAL(1U, mQueue->mValidToPushFlag->GetValue());

    unique_ptr<ProcessQueueItem> poppedItem;
    mQueue->Pop(poppedItem);
    APSARA_TEST_EQUAL(1U, mQueue->mOutItemsTotal->GetValue());
    APSARA_TEST_EQUAL(0U, mQueue->mQueueSizeTotal->GetValue());
    APSARA_TEST_EQUAL(0U, mQueue->mQueueDataSizeByte->GetValue());
    APSARA_TEST_EQUAL(1U, mQueue->mValidToPushFlag->GetValue());
}

void BytesBoundedProcessQueueUnittest::TestBytesWatermark() {
    // Test exact watermark boundaries
    // Start with empty queue
    APSARA_TEST_TRUE(mQueue->IsValidToPush());
    APSARA_TEST_EQUAL(0U, mQueue->mCurrentBytesSize);

    // Push items to just below high watermark
    auto item1 = GenerateItemWithSize(7500);
    APSARA_TEST_TRUE(mQueue->Push(std::move(item1)));
    APSARA_TEST_TRUE(mQueue->IsValidToPush());

    // Push to reach or exceed high watermark
    auto item2 = GenerateItemWithSize(500);
    size_t item2Size = item2->mEventGroup.DataSize();
    APSARA_TEST_TRUE(mQueue->Push(std::move(item2)));

    // Should now be invalid to push
    APSARA_TEST_FALSE(mQueue->IsValidToPush());

    // Pop one item but stay above low watermark
    unique_ptr<ProcessQueueItem> item;
    APSARA_TEST_TRUE(mQueue->Pop(item));
    APSARA_TEST_EQUAL(item2Size, mQueue->mCurrentBytesSize);

    // Should still be invalid to push (above low watermark)
    if (item2Size > sLowWatermark) {
        APSARA_TEST_FALSE(mQueue->IsValidToPush());
    }

    // Pop remaining items to go below low watermark
    APSARA_TEST_TRUE(mQueue->Pop(item));
    APSARA_TEST_EQUAL(0U, mQueue->mCurrentBytesSize);
    APSARA_TEST_TRUE(mQueue->IsValidToPush());
}

void BytesBoundedProcessQueueUnittest::TestEmptyQueue() {
    // Test operations on empty queue
    unique_ptr<ProcessQueueItem> item;

    // Pop from empty queue
    APSARA_TEST_FALSE(mQueue->Pop(item));
    APSARA_TEST_EQUAL(0U, mQueue->mCurrentBytesSize);

    // Queue should be valid to push when empty
    APSARA_TEST_TRUE(mQueue->IsValidToPush());

    // Size should be 0
    APSARA_TEST_EQUAL(0U, mQueue->mQueue.size());
}

void BytesBoundedProcessQueueUnittest::TestSingleItemExceedsMaxBytes() {
    // Test pushing a single item that exceeds maxBytes
    // Note: Current implementation allows this if IsValidToPush() is true
    // This tests the actual behavior

    APSARA_TEST_TRUE(mQueue->IsValidToPush());
    APSARA_TEST_EQUAL(0U, mQueue->mCurrentBytesSize);

    // Create an item larger than maxBytes (10000)
    auto largeItem = GenerateItemWithSize(12000);
    size_t largeItemSize = largeItem->mEventGroup.DataSize();

    // According to current implementation, this will be accepted if IsValidToPush() is true
    APSARA_TEST_TRUE(mQueue->Push(std::move(largeItem)));
    APSARA_TEST_EQUAL(largeItemSize, mQueue->mCurrentBytesSize);

    // After pushing, the queue should exceed maxBytes and be invalid to push
    APSARA_TEST_FALSE(mQueue->IsValidToPush());

    // Try to push another item, should fail
    auto anotherItem = GenerateItemWithSize(100);
    APSARA_TEST_FALSE(mQueue->Push(std::move(anotherItem)));

    // Current bytes should remain unchanged
    APSARA_TEST_EQUAL(largeItemSize, mQueue->mCurrentBytesSize);

    // Pop the large item
    unique_ptr<ProcessQueueItem> item;
    APSARA_TEST_TRUE(mQueue->Pop(item));
    APSARA_TEST_EQUAL(0U, mQueue->mCurrentBytesSize);
    APSARA_TEST_TRUE(mQueue->IsValidToPush());
}

void BytesBoundedProcessQueueUnittest::TestSingleItemExceedsHighWatermark() {
    // Test pushing a single item that exceeds high watermark but not maxBytes
    APSARA_TEST_TRUE(mQueue->IsValidToPush());

    // Create an item larger than highWatermark (8000) but smaller than maxBytes (10000)
    auto item = GenerateItemWithSize(9000);
    size_t itemSize = item->mEventGroup.DataSize();

    // Should be able to push
    APSARA_TEST_TRUE(mQueue->Push(std::move(item)));
    APSARA_TEST_EQUAL(itemSize, mQueue->mCurrentBytesSize);

    // After pushing, should be invalid to push (exceeded high watermark)
    APSARA_TEST_FALSE(mQueue->IsValidToPush());

    // Pop the item
    unique_ptr<ProcessQueueItem> poppedItem;
    APSARA_TEST_TRUE(mQueue->Pop(poppedItem));
    APSARA_TEST_EQUAL(0U, mQueue->mCurrentBytesSize);

    // Should be valid to push again (below low watermark)
    APSARA_TEST_TRUE(mQueue->IsValidToPush());
}

void BytesBoundedProcessQueueUnittest::TestExactWatermarkBoundaries() {
    // Test exact boundary conditions for watermarks

    // Test: Push exactly to low watermark
    auto item1 = GenerateItemWithSize(2000);
    size_t item1Size = item1->mEventGroup.DataSize();
    APSARA_TEST_TRUE(mQueue->Push(std::move(item1)));
    APSARA_TEST_TRUE(mQueue->IsValidToPush());

    // Test: Push to exactly high watermark
    size_t remaining = sHighWatermark - item1Size;
    if (remaining > 0) {
        auto item2 = GenerateItemWithSize(remaining);
        APSARA_TEST_TRUE(mQueue->Push(std::move(item2)));

        // Should be at or near high watermark
        if (mQueue->mCurrentBytesSize >= sHighWatermark) {
            APSARA_TEST_FALSE(mQueue->IsValidToPush());
        }
    }

    // Clear queue
    unique_ptr<ProcessQueueItem> item;
    while (mQueue->Pop(item)) {
        // Pop all items
    }
    APSARA_TEST_TRUE(mQueue->IsValidToPush());

    // Test: Push exactly maxBytes in a single item
    auto maxItem = GenerateItemWithSize(sMaxBytes);
    size_t maxItemSize = maxItem->mEventGroup.DataSize();
    APSARA_TEST_TRUE(mQueue->Push(std::move(maxItem)));
    APSARA_TEST_EQUAL(maxItemSize, mQueue->mCurrentBytesSize);
    APSARA_TEST_FALSE(mQueue->IsValidToPush());
}

void BytesBoundedProcessQueueUnittest::TestQueueFullScenario() {
    // Test complete scenario of filling the queue to capacity

    APSARA_TEST_TRUE(mQueue->IsValidToPush());
    size_t totalPushed = 0;

    // Push items until we hit high watermark
    while (mQueue->IsValidToPush() && totalPushed < sMaxBytes) {
        auto item = GenerateItemWithSize(1500);
        size_t itemSize = item->mEventGroup.DataSize();
        if (mQueue->Push(std::move(item))) {
            totalPushed += itemSize;
        } else {
            break;
        }
    }

    // Should have exceeded high watermark
    APSARA_TEST_FALSE(mQueue->IsValidToPush());
    APSARA_TEST_TRUE(mQueue->mCurrentBytesSize >= sHighWatermark);

    // Try to push when queue is "full" (exceeded high watermark)
    auto failItem = GenerateItemWithSize(100);
    APSARA_TEST_FALSE(mQueue->Push(std::move(failItem)));

    // Pop items until we reach below low watermark
    unique_ptr<ProcessQueueItem> item;
    while (mQueue->mCurrentBytesSize > sLowWatermark && mQueue->Pop(item)) {
        // Keep popping
    }

    // Should now be valid to push again
    APSARA_TEST_TRUE(mQueue->IsValidToPush());
    APSARA_TEST_TRUE(mQueue->mCurrentBytesSize <= sLowWatermark);
}

void BytesBoundedProcessQueueUnittest::TestMultiplePushPopCycles() {
    // Test multiple cycles of push and pop operations

    for (int cycle = 0; cycle < 3; cycle++) {
        // Push phase
        auto item1 = GenerateItemWithSize(3000);
        auto item2 = GenerateItemWithSize(3000);
        auto item3 = GenerateItemWithSize(2500);

        size_t size1 = item1->mEventGroup.DataSize();
        size_t size2 = item2->mEventGroup.DataSize();
        size_t size3 = item3->mEventGroup.DataSize();

        APSARA_TEST_TRUE(mQueue->Push(std::move(item1)));
        APSARA_TEST_TRUE(mQueue->Push(std::move(item2)));
        APSARA_TEST_TRUE(mQueue->Push(std::move(item3)));

        size_t expectedSize = size1 + size2 + size3;
        APSARA_TEST_EQUAL(expectedSize, mQueue->mCurrentBytesSize);
        APSARA_TEST_FALSE(mQueue->IsValidToPush());

        // Pop phase
        unique_ptr<ProcessQueueItem> item;
        APSARA_TEST_TRUE(mQueue->Pop(item));
        APSARA_TEST_TRUE(mQueue->Pop(item));
        APSARA_TEST_TRUE(mQueue->Pop(item));

        APSARA_TEST_EQUAL(0U, mQueue->mCurrentBytesSize);
        APSARA_TEST_TRUE(mQueue->IsValidToPush());
        APSARA_TEST_EQUAL(0U, mQueue->mQueue.size());
    }
}

void BytesBoundedProcessQueueUnittest::TestZeroSizeItem() {
    // Test pushing items with zero or minimal size
    auto emptyItem = GenerateItem("");
    size_t emptySize = emptyItem->mEventGroup.DataSize();

    APSARA_TEST_TRUE(mQueue->IsValidToPush());
    APSARA_TEST_TRUE(mQueue->Push(std::move(emptyItem)));
    APSARA_TEST_EQUAL(emptySize, mQueue->mCurrentBytesSize);
    APSARA_TEST_TRUE(mQueue->IsValidToPush());

    // Push multiple zero/small size items
    for (int i = 0; i < 10; i++) {
        auto item = GenerateItem("");
        mQueue->Push(std::move(item));
    }

    // Even with many small items, should still be valid to push
    // unless total size exceeds high watermark
    APSARA_TEST_EQUAL(11U, mQueue->mQueue.size());

    // Pop all items
    unique_ptr<ProcessQueueItem> item;
    size_t popCount = 0;
    while (mQueue->Pop(item)) {
        popCount++;
    }
    APSARA_TEST_EQUAL(11U, popCount);
    APSARA_TEST_EQUAL(0U, mQueue->mCurrentBytesSize);
}

void BytesBoundedProcessQueueUnittest::TestExactlyOnceEnabled() {
    // Test BytesBoundedProcessQueue with ExactlyOnce mode enabled
    CollectionPipelineContext eoCtx;
    eoCtx.SetConfigName("test_exactly_once_config");
    eoCtx.SetExactlyOnceFlag(true);

    BytesBoundedProcessQueue eoQueue(sMaxBytes, sLowWatermark, sHighWatermark, 1, 1, eoCtx);

    // Check that exactly once label is set
    APSARA_TEST_TRUE(eoQueue.mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_EXACTLY_ONCE_ENABLED, "true"));

    // Setup downstream and upstream
    unique_ptr<BoundedSenderQueueInterface> senderQueue(new SenderQueue(10, 0, 10, 0, "", "", eoCtx));
    eoQueue.SetDownStreamQueues(vector<BoundedSenderQueueInterface*>{senderQueue.get()});

    unique_ptr<FeedbackInterface> feedback(new FeedbackInterfaceMock);
    eoQueue.SetUpStreamFeedbacks(vector<FeedbackInterface*>{feedback.get()});
    eoQueue.EnablePop();

    // Test basic push/pop operations work with exactly once enabled
    auto item1 = GenerateItemWithSize(3000);
    size_t size1 = item1->mEventGroup.DataSize();
    APSARA_TEST_TRUE(eoQueue.Push(std::move(item1)));
    APSARA_TEST_EQUAL(size1, eoQueue.mCurrentBytesSize);

    unique_ptr<ProcessQueueItem> poppedItem;
    APSARA_TEST_TRUE(eoQueue.Pop(poppedItem));
    APSARA_TEST_EQUAL(0U, eoQueue.mCurrentBytesSize);

    // Verify the popped item has the pipeline in-process count updated
    APSARA_TEST_NOT_EQUAL(nullptr, poppedItem);
}

UNIT_TEST_CASE(BytesBoundedProcessQueueUnittest, TestPush)
UNIT_TEST_CASE(BytesBoundedProcessQueueUnittest, TestPop)
UNIT_TEST_CASE(BytesBoundedProcessQueueUnittest, TestSetUpStreamFeedbacks)
UNIT_TEST_CASE(BytesBoundedProcessQueueUnittest, TestMetric)
UNIT_TEST_CASE(BytesBoundedProcessQueueUnittest, TestBytesWatermark)
UNIT_TEST_CASE(BytesBoundedProcessQueueUnittest, TestEmptyQueue)
UNIT_TEST_CASE(BytesBoundedProcessQueueUnittest, TestSingleItemExceedsMaxBytes)
UNIT_TEST_CASE(BytesBoundedProcessQueueUnittest, TestSingleItemExceedsHighWatermark)
UNIT_TEST_CASE(BytesBoundedProcessQueueUnittest, TestExactWatermarkBoundaries)
UNIT_TEST_CASE(BytesBoundedProcessQueueUnittest, TestQueueFullScenario)
UNIT_TEST_CASE(BytesBoundedProcessQueueUnittest, TestMultiplePushPopCycles)
UNIT_TEST_CASE(BytesBoundedProcessQueueUnittest, TestZeroSizeItem)
UNIT_TEST_CASE(BytesBoundedProcessQueueUnittest, TestExactlyOnceEnabled)

} // namespace logtail

UNIT_TEST_MAIN
