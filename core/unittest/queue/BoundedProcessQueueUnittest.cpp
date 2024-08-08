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

#include "common/FeedbackInterface.h"
#include "models/PipelineEventGroup.h"
#include "queue/BoundedProcessQueue.h"
#include "queue/SenderQueue.h"
#include "unittest/Unittest.h"
#include "unittest/queue/FeedbackInterfaceMock.h"

using namespace std;

namespace logtail {

class BoundedProcessQueueUnittest : public testing::Test {
public:
    void TestPush();
    void TestPop();
    void TestReset();

protected:
    static void SetUpTestCase() { sEventGroup.reset(new PipelineEventGroup(make_shared<SourceBuffer>())); }

    void SetUp() override {
        mQueue.reset(new BoundedProcessQueue(sCap, sLowWatermark, sHighWatermark, sKey, 1, "test_config"));

        mSenderQueue1.reset(new SenderQueue(10, 0, 10, 0));
        mSenderQueue2.reset(new SenderQueue(10, 0, 10, 0));
        mQueue->SetDownStreamQueues(vector<BoundedSenderQueueInterface*>{mSenderQueue1.get(), mSenderQueue2.get()});

        mFeedback1.reset(new FeedbackInterfaceMock);
        mFeedback2.reset(new FeedbackInterfaceMock);
        mQueue->SetUpStreamFeedbacks(vector<FeedbackInterface*>{mFeedback1.get(), mFeedback2.get()});
    }

private:
    static unique_ptr<PipelineEventGroup> sEventGroup;
    static const QueueKey sKey = 0;
    static const size_t sCap = 6;
    static const size_t sLowWatermark = 2;
    static const size_t sHighWatermark = 4;

    unique_ptr<ProcessQueueItem> GenerateItem() { return make_unique<ProcessQueueItem>(std::move(*sEventGroup), 0); }

    unique_ptr<BoundedProcessQueue> mQueue;
    unique_ptr<FeedbackInterface> mFeedback1;
    unique_ptr<FeedbackInterface> mFeedback2;
    unique_ptr<BoundedSenderQueueInterface> mSenderQueue1;
    unique_ptr<BoundedSenderQueueInterface> mSenderQueue2;
};

unique_ptr<PipelineEventGroup> BoundedProcessQueueUnittest::sEventGroup;

void BoundedProcessQueueUnittest::TestPush() {
    // push first
    APSARA_TEST_TRUE(mQueue->Push(GenerateItem()));
    APSARA_TEST_TRUE(mQueue->Push(GenerateItem()));
    APSARA_TEST_TRUE(mQueue->Push(GenerateItem()));
    APSARA_TEST_TRUE(mQueue->Push(GenerateItem()));
    // now queue size comes to high watermark, push is forbidden
    APSARA_TEST_FALSE(mQueue->Push(GenerateItem()));
    // still not valid to push when low watermark is reached
    unique_ptr<ProcessQueueItem> item;
    mQueue->Pop(item);
    APSARA_TEST_FALSE(mQueue->Push(GenerateItem()));
    mQueue->Pop(item);
    // now queue size comes to low watermark, push can be resumed
    APSARA_TEST_TRUE(mQueue->Push(GenerateItem()));
}

void BoundedProcessQueueUnittest::TestPop() {
    unique_ptr<ProcessQueueItem> item;
    // nothing to pop
    APSARA_TEST_EQUAL(0, mQueue->Pop(item));

    mQueue->Push(GenerateItem());
    // invalidate pop
    mQueue->InvalidatePop();
    APSARA_TEST_EQUAL(0, mQueue->Pop(item));
    mQueue->ValidatePop();

    // downstream queues are not valid to push
    mSenderQueue1->mValidToPush = false;
    APSARA_TEST_EQUAL(0, mQueue->Pop(item));
    mSenderQueue1->mValidToPush = true;

    // push to high watermark
    mQueue->Push(GenerateItem());
    mQueue->Push(GenerateItem());
    mQueue->Push(GenerateItem());
    mQueue->Push(GenerateItem());
    // from high watermark to low wartermark
    APSARA_TEST_TRUE(mQueue->Pop(item));
    APSARA_TEST_FALSE(static_cast<FeedbackInterfaceMock*>(mFeedback1.get())->HasFeedback(sKey));
    APSARA_TEST_FALSE(static_cast<FeedbackInterfaceMock*>(mFeedback2.get())->HasFeedback(sKey));
    APSARA_TEST_TRUE(mQueue->Pop(item));
    APSARA_TEST_TRUE(static_cast<FeedbackInterfaceMock*>(mFeedback1.get())->HasFeedback(sKey));
    APSARA_TEST_TRUE(static_cast<FeedbackInterfaceMock*>(mFeedback2.get())->HasFeedback(sKey));
}

void BoundedProcessQueueUnittest::TestReset() {
    mQueue->mHighWatermark = 1;
    mQueue->Push(GenerateItem());
    mQueue->InvalidatePop();
    mQueue->Reset(5, 4, 5);
    APSARA_TEST_TRUE(mQueue->Empty());
    APSARA_TEST_TRUE(mQueue->mDownStreamQueues.empty());
    APSARA_TEST_TRUE(mQueue->mUpStreamFeedbacks.empty());
    APSARA_TEST_TRUE(mQueue->mValidToPop);
    APSARA_TEST_TRUE(mQueue->mValidToPush);
    APSARA_TEST_EQUAL(5U, mQueue->mCapacity);
    APSARA_TEST_EQUAL(4U, mQueue->mLowWatermark);
    APSARA_TEST_EQUAL(5U, mQueue->mHighWatermark);
}

UNIT_TEST_CASE(BoundedProcessQueueUnittest, TestPush)
UNIT_TEST_CASE(BoundedProcessQueueUnittest, TestPop)
UNIT_TEST_CASE(BoundedProcessQueueUnittest, TestReset)

} // namespace logtail

UNIT_TEST_MAIN
