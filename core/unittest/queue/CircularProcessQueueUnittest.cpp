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

#include <memory>

#include "models/PipelineEventGroup.h"
#include "pipeline/PipelineManager.h"
#include "pipeline/queue/CircularProcessQueue.h"
#include "pipeline/queue/SenderQueue.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {
class CircularProcessQueueUnittest : public testing::Test {
public:
    void TestPush();
    void TestPop();
    void TestReset();
    void TestMetric();
    void TestSetPipeline();

protected:
    static void SetUpTestCase() { sCtx.SetConfigName("test_config"); }

    void SetUp() override {
        mQueue.reset(new CircularProcessQueue(sCap, sKey, 1, sCtx));

        mSenderQueue1.reset(new SenderQueue(10, 0, 10, 0, "", sCtx));
        mSenderQueue2.reset(new SenderQueue(10, 0, 10, 0, "", sCtx));
        mQueue->SetDownStreamQueues(vector<BoundedSenderQueueInterface*>{mSenderQueue1.get(), mSenderQueue2.get()});
        mQueue->EnablePop();
    }

private:
    static PipelineContext sCtx;
    static const QueueKey sKey = 0;
    static const size_t sCap = 2;

    unique_ptr<ProcessQueueItem> GenerateItem(size_t cnt) {
        PipelineEventGroup eventGroup(make_shared<SourceBuffer>());
        for (size_t i = 0; i < cnt; ++i) {
            eventGroup.AddLogEvent();
        }
        return make_unique<ProcessQueueItem>(std::move(eventGroup), 0);
    }

    unique_ptr<CircularProcessQueue> mQueue;
    unique_ptr<BoundedSenderQueueInterface> mSenderQueue1;
    unique_ptr<BoundedSenderQueueInterface> mSenderQueue2;
};

PipelineContext CircularProcessQueueUnittest::sCtx;

void CircularProcessQueueUnittest::TestPush() {
    unique_ptr<ProcessQueueItem> res;
    {
        auto item = GenerateItem(1);
        auto p = item.get();

        APSARA_TEST_TRUE(mQueue->Push(std::move(item)));
        APSARA_TEST_EQUAL(1U, mQueue->Size());
        mQueue->Pop(res);
        APSARA_TEST_EQUAL(p, res.get());
        APSARA_TEST_TRUE(mQueue->Empty());
    }
    {
        auto item = GenerateItem(2);
        auto p = item.get();

        APSARA_TEST_TRUE(mQueue->Push(GenerateItem(1)));
        APSARA_TEST_TRUE(mQueue->Push(std::move(item)));
        APSARA_TEST_EQUAL(2U, mQueue->Size());
        mQueue->Pop(res);
        APSARA_TEST_EQUAL(p, res.get());
        APSARA_TEST_TRUE(mQueue->Empty());
    }
    {
        APSARA_TEST_TRUE(mQueue->Push(GenerateItem(1)));
        APSARA_TEST_FALSE(mQueue->Push(GenerateItem(3)));
        APSARA_TEST_TRUE(mQueue->Empty());
    }
}

void CircularProcessQueueUnittest::TestPop() {
    unique_ptr<ProcessQueueItem> item;
    // nothing to pop
    APSARA_TEST_FALSE(mQueue->Pop(item));

    mQueue->Push(GenerateItem(1));
    // invalidate pop
    mQueue->DisablePop();
    APSARA_TEST_FALSE(mQueue->Pop(item));
    mQueue->EnablePop();

    // downstream queues are not valid to push
    mSenderQueue1->mValidToPush = false;
    APSARA_TEST_FALSE(mQueue->Pop(item));
    mSenderQueue1->mValidToPush = true;

    APSARA_TEST_TRUE(mQueue->Pop(item));
    APSARA_TEST_TRUE(mQueue->Empty());
}

void CircularProcessQueueUnittest::TestReset() {
    unique_ptr<ProcessQueueItem> res;
    {
        auto item1 = GenerateItem(1);
        auto p1 = item1.get();
        auto item2 = GenerateItem(1);
        auto p2 = item2.get();

        mQueue->Push(std::move(item1));
        mQueue->Push(std::move(item2));
        mQueue->Reset(4);
        APSARA_TEST_EQUAL(4U, mQueue->mCapacity);
        APSARA_TEST_EQUAL(2U, mQueue->Size());
        APSARA_TEST_TRUE(mQueue->mDownStreamQueues.empty());
        mQueue->Pop(res);
        APSARA_TEST_EQUAL(p1, res.get());
        mQueue->Pop(res);
        APSARA_TEST_EQUAL(p2, res.get());
        APSARA_TEST_TRUE(mQueue->Empty());
    }
    {
        auto item1 = GenerateItem(2);
        auto item2 = GenerateItem(1);
        auto p2 = item2.get();

        mQueue->Push(std::move(item1));
        mQueue->Push(std::move(item2));
        mQueue->Reset(2);
        APSARA_TEST_EQUAL(2U, mQueue->mCapacity);
        APSARA_TEST_EQUAL(1U, mQueue->Size());
        APSARA_TEST_TRUE(mQueue->mDownStreamQueues.empty());
        mQueue->Pop(res);
        APSARA_TEST_EQUAL(p2, res.get());
        APSARA_TEST_TRUE(mQueue->Empty());
    }
}

void CircularProcessQueueUnittest::TestMetric() {
    APSARA_TEST_EQUAL(5U, mQueue->mMetricsRecordRef->GetLabels()->size());
    APSARA_TEST_TRUE(mQueue->mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_PROJECT, ""));
    APSARA_TEST_TRUE(mQueue->mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_PIPELINE_NAME, "test_config"));
    APSARA_TEST_TRUE(mQueue->mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_COMPONENT_NAME, METRIC_LABEL_VALUE_COMPONENT_NAME_PROCESS_QUEUE));
    APSARA_TEST_TRUE(mQueue->mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_QUEUE_TYPE, "circular"));

    auto item = GenerateItem(2);
    auto dataSize1 = item->mEventGroup.DataSize();
    mQueue->Push(std::move(item));
    APSARA_TEST_EQUAL(1U, mQueue->mInItemsTotal->GetValue());
    APSARA_TEST_EQUAL(dataSize1, mQueue->mInItemDataSizeBytes->GetValue());
    APSARA_TEST_EQUAL(2U, mQueue->mQueueSizeTotal->GetValue());
    APSARA_TEST_EQUAL(dataSize1, mQueue->mQueueDataSizeByte->GetValue());

    item = GenerateItem(1);
    auto dataSize2 = item->mEventGroup.DataSize();
    mQueue->Push(std::move(item));
    APSARA_TEST_EQUAL(2U, mQueue->mInItemsTotal->GetValue());
    APSARA_TEST_EQUAL(dataSize1 + dataSize2, mQueue->mInItemDataSizeBytes->GetValue());
    APSARA_TEST_EQUAL(1U, mQueue->mQueueSizeTotal->GetValue());
    APSARA_TEST_EQUAL(dataSize2, mQueue->mQueueDataSizeByte->GetValue());
    APSARA_TEST_EQUAL(2U, mQueue->mDiscardedEventsTotal->GetValue());

    mQueue->Pop(item);
    APSARA_TEST_EQUAL(1U, mQueue->mOutItemsTotal->GetValue());
    APSARA_TEST_EQUAL(0U, mQueue->mQueueSizeTotal->GetValue());
    APSARA_TEST_EQUAL(0U, mQueue->mQueueDataSizeByte->GetValue());
}

void CircularProcessQueueUnittest::TestSetPipeline() {
    auto pipeline = make_shared<Pipeline>();
    PipelineManager::GetInstance()->mPipelineNameEntityMap["test_config"] = pipeline;

    auto item1 = GenerateItem(1);
    auto p1 = item1.get();
    auto pipelineTmp = make_shared<Pipeline>();
    item1->mPipeline = pipelineTmp;

    auto item2 = GenerateItem(1);
    auto p2 = item2.get();

    mQueue->Push(std::move(item1));
    mQueue->Push(std::move(item2));
    auto p = PipelineManager::GetInstance()->FindConfigByName("test_config");
    mQueue->SetPipelineForItems(p);

    APSARA_TEST_EQUAL(pipelineTmp, p1->mPipeline);
    APSARA_TEST_EQUAL(pipeline, p2->mPipeline);
}

UNIT_TEST_CASE(CircularProcessQueueUnittest, TestPush)
UNIT_TEST_CASE(CircularProcessQueueUnittest, TestPop)
UNIT_TEST_CASE(CircularProcessQueueUnittest, TestReset)
UNIT_TEST_CASE(CircularProcessQueueUnittest, TestMetric)
UNIT_TEST_CASE(CircularProcessQueueUnittest, TestSetPipeline)

} // namespace logtail

UNIT_TEST_MAIN
