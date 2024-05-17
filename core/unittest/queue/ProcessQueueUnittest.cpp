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
#include <unordered_set>

#include "common/FeedbackInterface.h"
#include "models/PipelineEventGroup.h"
#include "queue/ProcessQueue.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {
class FeedbackInterfaceMock : public FeedbackInterface {
public:
    void Feedback(QueueKey key) override { mFeedbackedKeys.insert(key); };

    size_t HasFeedback(QueueKey key) const { return mFeedbackedKeys.find(key) != mFeedbackedKeys.end(); }

private:
    unordered_set<QueueKey> mFeedbackedKeys;
};

class ProcessQueueUnittest : public testing::Test {
public:
    void TestPush();
    void TestPop();

protected:
    static void SetUpTestCase() { sEventGroup.reset(new PipelineEventGroup(make_shared<SourceBuffer>())); }

    void SetUp() override {
        mQueue.reset(new ProcessQueue(sCap, sLowWatermark, sHighWatermark, sKey, 1, "test_config"));

        mSenderQueue1.reset(new SingleLogstoreSenderManager<SenderQueueParam>);
        mSenderQueue2.reset(new SingleLogstoreSenderManager<SenderQueueParam>);
        vector<SingleLogstoreSenderManager<SenderQueueParam>*> queues{mSenderQueue1.get(), mSenderQueue2.get()};
        mQueue->SetDownStreamQueues(queues);

        mFeedback1.reset(new FeedbackInterfaceMock);
        mFeedback2.reset(new FeedbackInterfaceMock);
        vector<FeedbackInterface*> feedbacks{mFeedback1.get(), mFeedback2.get()};
        mQueue->SetUpStreamFeedbacks(feedbacks);
    }

private:
    static unique_ptr<PipelineEventGroup> sEventGroup;
    static const QueueKey sKey = 0;
    static const size_t sCap = 6;
    static const size_t sLowWatermark = 2;
    static const size_t sHighWatermark = 4;

    unique_ptr<ProcessQueue> mQueue;
    unique_ptr<FeedbackInterface> mFeedback1;
    unique_ptr<FeedbackInterface> mFeedback2;
    unique_ptr<SingleLogstoreSenderManager<SenderQueueParam>> mSenderQueue1;
    unique_ptr<SingleLogstoreSenderManager<SenderQueueParam>> mSenderQueue2;
};

unique_ptr<PipelineEventGroup> ProcessQueueUnittest::sEventGroup;

void ProcessQueueUnittest::TestPush() {
    // push first
    APSARA_TEST_TRUE(mQueue->Push(unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0))));
    APSARA_TEST_TRUE(mQueue->Push(unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0))));
    APSARA_TEST_TRUE(mQueue->Push(unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0))));
    APSARA_TEST_TRUE(mQueue->Push(unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0))));
    // now queue size comes to high watermark, push is forbidden
    APSARA_TEST_FALSE(mQueue->Push(unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0))));
    // still not valid to push when low watermark is reached
    unique_ptr<ProcessQueueItem> item;
    mQueue->Pop(item);
    APSARA_TEST_FALSE(mQueue->Push(unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0))));
    mQueue->Pop(item);
    // now queue size comes to low watermark, push can be resumed
    APSARA_TEST_TRUE(mQueue->Push(unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0))));
}

void ProcessQueueUnittest::TestPop() {
    unique_ptr<ProcessQueueItem> item;
    // nothing to pop
    APSARA_TEST_EQUAL(0, mQueue->Pop(item));

    mQueue->Push(unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0)));
    // invalidate pop
    mQueue->InvalidatePop();
    APSARA_TEST_EQUAL(0, mQueue->Pop(item));
    mQueue->ValidatePop();

    // downstream queues are not valid to push
    mSenderQueue1->mValid = false;
    APSARA_TEST_EQUAL(0, mQueue->Pop(item));
    mSenderQueue1->mValid = true;

    // push to high watermark
    mQueue->Push(unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0)));
    mQueue->Push(unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0)));
    mQueue->Push(unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0)));
    mQueue->Push(unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0)));
    // from high watermark to low wartermark
    APSARA_TEST_TRUE(mQueue->Pop(item));
    APSARA_TEST_FALSE(static_cast<FeedbackInterfaceMock*>(mFeedback1.get())->HasFeedback(sKey));
    APSARA_TEST_FALSE(static_cast<FeedbackInterfaceMock*>(mFeedback2.get())->HasFeedback(sKey));
    APSARA_TEST_TRUE(mQueue->Pop(item));
    APSARA_TEST_TRUE(static_cast<FeedbackInterfaceMock*>(mFeedback1.get())->HasFeedback(sKey));
    APSARA_TEST_TRUE(static_cast<FeedbackInterfaceMock*>(mFeedback2.get())->HasFeedback(sKey));
}

UNIT_TEST_CASE(ProcessQueueUnittest, TestPush)
UNIT_TEST_CASE(ProcessQueueUnittest, TestPop)

} // namespace logtail

UNIT_TEST_MAIN
