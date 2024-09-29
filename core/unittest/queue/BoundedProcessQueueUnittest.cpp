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
#include "pipeline/PipelineManager.h"
#include "pipeline/queue/BoundedProcessQueue.h"
#include "pipeline/queue/SenderQueue.h"
#include "unittest/Unittest.h"
#include "unittest/queue/FeedbackInterfaceMock.h"

using namespace std;

namespace logtail {

class BoundedProcessQueueUnittest : public testing::Test {
public:
    void TestPush();
    void TestPop();
    void TestMetric();
    void TestSetPipeline();

protected:
    static void SetUpTestCase() { sCtx.SetConfigName("test_config"); }

    void SetUp() override {
        mQueue.reset(new BoundedProcessQueue(sCap, sLowWatermark, sHighWatermark, sKey, 1, sCtx));

        mSenderQueue1.reset(new SenderQueue(10, 0, 10, 0, "", sCtx));
        mSenderQueue2.reset(new SenderQueue(10, 0, 10, 0, "", sCtx));
        mQueue->SetDownStreamQueues(vector<BoundedSenderQueueInterface*>{mSenderQueue1.get(), mSenderQueue2.get()});

        mFeedback1.reset(new FeedbackInterfaceMock);
        mFeedback2.reset(new FeedbackInterfaceMock);
        mQueue->SetUpStreamFeedbacks(vector<FeedbackInterface*>{mFeedback1.get(), mFeedback2.get()});
        mQueue->EnablePop();
    }

private:
    static PipelineContext sCtx;
    static const QueueKey sKey = 0;
    static const size_t sCap = 6;
    static const size_t sLowWatermark = 2;
    static const size_t sHighWatermark = 4;

    unique_ptr<ProcessQueueItem> GenerateItem() {
        PipelineEventGroup g(make_shared<SourceBuffer>());
        return make_unique<ProcessQueueItem>(std::move(g), 0);
    }

    unique_ptr<BoundedProcessQueue> mQueue;
    unique_ptr<FeedbackInterface> mFeedback1;
    unique_ptr<FeedbackInterface> mFeedback2;
    unique_ptr<BoundedSenderQueueInterface> mSenderQueue1;
    unique_ptr<BoundedSenderQueueInterface> mSenderQueue2;
};

PipelineContext BoundedProcessQueueUnittest::sCtx;

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
    mQueue->DisablePop();
    APSARA_TEST_EQUAL(0, mQueue->Pop(item));
    mQueue->EnablePop();

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

void BoundedProcessQueueUnittest::TestMetric() {
    APSARA_TEST_EQUAL(4U, mQueue->mMetricsRecordRef->GetLabels()->size());
    APSARA_TEST_TRUE(mQueue->mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_PROJECT, ""));
    APSARA_TEST_TRUE(mQueue->mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_PIPELINE_NAME, "test_config"));
    APSARA_TEST_TRUE(mQueue->mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_COMPONENT_NAME, METRIC_LABEL_VALUE_COMPONENT_NAME_PROCESS_QUEUE));
    APSARA_TEST_TRUE(mQueue->mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_QUEUE_TYPE, "bounded"));

    auto item = GenerateItem();
    auto e = item->mEventGroup.AddLogEvent();
    e->SetContent(string("key"), string("value"));
    auto dataSize = item->mEventGroup.DataSize();
    mQueue->Push(std::move(item));

    APSARA_TEST_EQUAL(1U, mQueue->mInItemsTotal->GetValue());
    APSARA_TEST_EQUAL(dataSize, mQueue->mInItemDataSizeBytes->GetValue());
    APSARA_TEST_EQUAL(1U, mQueue->mQueueSizeTotal->GetValue());
    APSARA_TEST_EQUAL(dataSize, mQueue->mQueueDataSizeByte->GetValue());
    APSARA_TEST_EQUAL(1U, mQueue->mValidToPushFlag->GetValue());

    mQueue->Pop(item);
    APSARA_TEST_EQUAL(1U, mQueue->mOutItemsTotal->GetValue());
    APSARA_TEST_EQUAL(0U, mQueue->mQueueSizeTotal->GetValue());
    APSARA_TEST_EQUAL(0U, mQueue->mQueueDataSizeByte->GetValue());
    APSARA_TEST_EQUAL(1U, mQueue->mValidToPushFlag->GetValue());
}

void BoundedProcessQueueUnittest::TestSetPipeline() {
    auto pipeline = make_shared<Pipeline>();
    PipelineManager::GetInstance()->mPipelineNameEntityMap["test_config"] = pipeline;

    auto item1 = GenerateItem();
    auto p1 = item1.get();
    auto pipelineTmp = make_shared<Pipeline>();
    item1->mPipeline = pipelineTmp;

    auto item2 = GenerateItem();
    auto p2 = item2.get();

    mQueue->Push(std::move(item1));
    mQueue->Push(std::move(item2));
    mQueue->SetPipelineForItems("test_config");

    APSARA_TEST_EQUAL(pipelineTmp, p1->mPipeline);
    APSARA_TEST_EQUAL(pipeline, p2->mPipeline);
}

UNIT_TEST_CASE(BoundedProcessQueueUnittest, TestPush)
UNIT_TEST_CASE(BoundedProcessQueueUnittest, TestPop)
UNIT_TEST_CASE(BoundedProcessQueueUnittest, TestMetric)
UNIT_TEST_CASE(BoundedProcessQueueUnittest, TestSetPipeline)

} // namespace logtail

UNIT_TEST_MAIN
