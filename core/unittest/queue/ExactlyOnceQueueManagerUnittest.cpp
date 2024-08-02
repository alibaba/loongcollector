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

#include "flusher/sls/FlusherSLS.h"
#include "input/InputFeedbackInterfaceRegistry.h"
#include "models/PipelineEventGroup.h"
#include "queue/ExactlyOnceQueueManager.h"
#include "queue/QueueKeyManager.h"
#include "queue/SLSSenderQueueItem.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_INT32(logtail_queue_gc_threshold_sec);

using namespace std;

namespace logtail {

class ExactlyOnceQueueManagerUnittest : public testing::Test {
public:
    void TestUpdateQueue();
    void TestDeleteQueue();
    void TestPushProcessQueue();
    void TestIsAllProcessQueueEmpty();
    void TestPushSenderQueue();
    void TestGetAllAvailableSenderQueueItems();
    void TestRemoveSenderItem();
    void TestIsAllSenderQueueEmpty();
    void OnPipelineUpdate();

protected:
    static void SetUpTestCase() {
        InputFeedbackInterfaceRegistry::GetInstance()->LoadFeedbackInterfaces();
        sEventGroup.reset(new PipelineEventGroup(make_shared<SourceBuffer>()));
        for (size_t i = 0; i < 5; ++i) {
            auto cpt = make_shared<RangeCheckpoint>();
            cpt->index = i;
            cpt->data.set_hash_key("key");
            cpt->data.set_sequence_id(0);
            sCheckpoints.emplace_back(cpt);
        }
        sManager = ExactlyOnceQueueManager::GetInstance();
    }

    void SetUp() override {
        mFlusher.mMaxSendRate = 100;
        mFlusher.mRegion = "region";
        mFlusher.mProject = "project";
    }

    void TearDown() override {
        sManager->Clear();
        QueueKeyManager::GetInstance()->Clear();
    }

private:
    static const size_t sDataSize = 10;

    static unique_ptr<PipelineEventGroup> sEventGroup;
    static ExactlyOnceQueueManager* sManager;
    static vector<RangeCheckpointPtr> sCheckpoints;

    unique_ptr<ProcessQueueItem> GenerateProcessItem();
    unique_ptr<SenderQueueItem> GenerateSenderItem();

    // cannot be static member, because its constructor relies on logger, which is initiallized after main starts
    FlusherSLS mFlusher;
};

const size_t ExactlyOnceQueueManagerUnittest::sDataSize;
unique_ptr<PipelineEventGroup> ExactlyOnceQueueManagerUnittest::sEventGroup;
ExactlyOnceQueueManager* ExactlyOnceQueueManagerUnittest::sManager;
vector<RangeCheckpointPtr> ExactlyOnceQueueManagerUnittest::sCheckpoints;

void ExactlyOnceQueueManagerUnittest::TestUpdateQueue() {
    QueueKey key = 0;
    {
        // create queue
        size_t queueSize = 5;
        APSARA_TEST_TRUE(sManager->CreateOrUpdateQueue(key, 0, "test_config", vector<RangeCheckpointPtr>(queueSize)));
        APSARA_TEST_EQUAL(1U, sManager->mSenderQueues.size());
        auto& senderQue = sManager->mSenderQueues.at(key);
        APSARA_TEST_EQUAL(queueSize, senderQue.mCapacity);
        APSARA_TEST_EQUAL(queueSize - 1, senderQue.mLowWatermark);
        APSARA_TEST_EQUAL(queueSize, senderQue.mHighWatermark);
        APSARA_TEST_EQUAL(1U, sManager->mProcessQueues.size());
        APSARA_TEST_EQUAL(1U, sManager->mProcessPriorityQueue[0].size());
        auto& processQue = sManager->mProcessQueues.at(key);
        APSARA_TEST_EQUAL(processQue, prev(sManager->mProcessPriorityQueue[0].end()));
        APSARA_TEST_EQUAL(1U, processQue->mUpStreamFeedbacks.size());
        APSARA_TEST_EQUAL(1U, processQue->mDownStreamQueues.size());
        APSARA_TEST_EQUAL(&senderQue, processQue->mDownStreamQueues[0]);
    }
    {
        // update queue with the same priority
        size_t queueSize = 8;
        APSARA_TEST_TRUE(sManager->CreateOrUpdateQueue(key, 0, "test_config", vector<RangeCheckpointPtr>(queueSize)));
        APSARA_TEST_EQUAL(1U, sManager->mSenderQueues.size());
        auto& senderQue = sManager->mSenderQueues.at(key);
        APSARA_TEST_EQUAL(queueSize, senderQue.mCapacity);
        APSARA_TEST_EQUAL(queueSize - 1, senderQue.mLowWatermark);
        APSARA_TEST_EQUAL(queueSize, senderQue.mHighWatermark);
        APSARA_TEST_EQUAL(1U, sManager->mProcessQueues.size());
        APSARA_TEST_EQUAL(1U, sManager->mProcessPriorityQueue[0].size());
        auto& processQue = sManager->mProcessQueues.at(key);
        APSARA_TEST_TRUE(processQue == prev(sManager->mProcessPriorityQueue[0].end()));
        APSARA_TEST_EQUAL(1U, processQue->mDownStreamQueues.size());
        APSARA_TEST_EQUAL(1U, processQue->mUpStreamFeedbacks.size());
    }
    {
        // update queue with different priority
        size_t queueSize = 6;
        APSARA_TEST_TRUE(sManager->CreateOrUpdateQueue(key, 1, "test_config", vector<RangeCheckpointPtr>(queueSize)));
        APSARA_TEST_EQUAL(1U, sManager->mProcessQueues.size());
        APSARA_TEST_EQUAL(0U, sManager->mProcessPriorityQueue[0].size());
        APSARA_TEST_EQUAL(1U, sManager->mProcessPriorityQueue[1].size());
        auto& processQue = sManager->mProcessQueues.at(key);
        APSARA_TEST_TRUE(processQue == prev(sManager->mProcessPriorityQueue[1].end()));
        APSARA_TEST_EQUAL(1U, processQue->mDownStreamQueues.size());
        APSARA_TEST_EQUAL(1U, processQue->mUpStreamFeedbacks.size());
    }
}

void ExactlyOnceQueueManagerUnittest::TestDeleteQueue() {
    INT32_FLAG(logtail_queue_gc_threshold_sec) = 0;

    // queue not exists
    APSARA_TEST_FALSE(sManager->DeleteQueue(0));

    QueueKey key1 = QueueKeyManager::GetInstance()->GetKey("name_1");
    QueueKey key2 = QueueKeyManager::GetInstance()->GetKey("name_2");
    sManager->CreateOrUpdateQueue(key1, 1, "test_config_1", sCheckpoints);
    sManager->CreateOrUpdateQueue(key2, 1, "test_config_2", sCheckpoints);
    sManager->PushProcessQueue(key2, GenerateProcessItem());

    // queue exists and not marked deleted
    APSARA_TEST_TRUE(sManager->DeleteQueue(key1));
    APSARA_TEST_TRUE(sManager->DeleteQueue(key2));
    APSARA_TEST_EQUAL(2U, sManager->mQueueDeletionTimeMap.size());

    // queue exists but marked deleted
    APSARA_TEST_FALSE(sManager->DeleteQueue(key1));

    // queue key1 is deleted, but not queue key2
    sManager->ClearTimeoutQueues();
    APSARA_TEST_EQUAL(1U, sManager->mProcessQueues.size());
    APSARA_TEST_EQUAL(1U, sManager->mProcessPriorityQueue[1].size());
    APSARA_TEST_EQUAL(1U, sManager->mSenderQueues.size());
    APSARA_TEST_EQUAL(1U, sManager->mQueueDeletionTimeMap.size());
    APSARA_TEST_EQUAL("", QueueKeyManager::GetInstance()->GetName(key1));

    // update queue will remove the queue from gc queue
    sManager->CreateOrUpdateQueue(key2, 0, "test_config_2", sCheckpoints);
    APSARA_TEST_EQUAL(0U, sManager->mQueueDeletionTimeMap.size());
}

void ExactlyOnceQueueManagerUnittest::TestPushProcessQueue() {
    sManager->CreateOrUpdateQueue(0, 0, "test_config", sCheckpoints);

    // queue exists
    APSARA_TEST_TRUE(sManager->IsValidToPushProcessQueue(0));
    APSARA_TEST_EQUAL(0, sManager->PushProcessQueue(0, GenerateProcessItem()));

    // no queue exists
    APSARA_TEST_FALSE(sManager->IsValidToPushProcessQueue(1));
    APSARA_TEST_EQUAL(2, sManager->PushProcessQueue(1, GenerateProcessItem()));

    // invalid to push
    sManager->mProcessQueues[0]->mValidToPush = false;
    APSARA_TEST_FALSE(sManager->IsValidToPushProcessQueue(0));
    APSARA_TEST_EQUAL(1, sManager->PushProcessQueue(0, GenerateProcessItem()));
}

void ExactlyOnceQueueManagerUnittest::TestIsAllProcessQueueEmpty() {
    sManager->CreateOrUpdateQueue(0, 0, "test_config_1", sCheckpoints);
    sManager->CreateOrUpdateQueue(1, 2, "test_config_2", sCheckpoints);
    APSARA_TEST_TRUE(sManager->IsAllProcessQueueEmpty());

    sManager->PushProcessQueue(0, GenerateProcessItem());
    APSARA_TEST_FALSE(sManager->IsAllProcessQueueEmpty());
}

void ExactlyOnceQueueManagerUnittest::TestPushSenderQueue() {
    sManager->CreateOrUpdateQueue(0, 0, "test_config", sCheckpoints);

    // queue exists
    APSARA_TEST_EQUAL(0, sManager->PushSenderQueue(0, GenerateSenderItem()));

    // no queue exists
    APSARA_TEST_EQUAL(2, sManager->PushSenderQueue(1, GenerateSenderItem()));
}

void ExactlyOnceQueueManagerUnittest::TestGetAllAvailableSenderQueueItems() {
    vector<RangeCheckpointPtr> checkpoints1;
    for (size_t i = 0; i < 2; ++i) {
        auto cpt = make_shared<RangeCheckpoint>();
        cpt->index = i;
        cpt->data.set_hash_key("key");
        cpt->data.set_sequence_id(0);
        checkpoints1.emplace_back(cpt);
    }
    sManager->CreateOrUpdateQueue(0, 0, "test_config_1", checkpoints1);

    vector<RangeCheckpointPtr> checkpoints2;
    for (size_t i = 0; i < 2; ++i) {
        auto cpt = make_shared<RangeCheckpoint>();
        cpt->index = i;
        cpt->data.set_hash_key("key");
        cpt->data.set_sequence_id(0);
        checkpoints2.emplace_back(cpt);
    }
    sManager->CreateOrUpdateQueue(1, 2, "test_config_2", checkpoints2);

    for (size_t i = 0; i <= 2; ++i) {
        sManager->PushSenderQueue(0, GenerateSenderItem());
    }
    for (size_t i = 0; i <= 2; ++i) {
        sManager->PushSenderQueue(1, GenerateSenderItem());
    }
    {
        // no limits
        vector<SenderQueueItem*> items;
        sManager->GetAllAvailableSenderQueueItems(items, false);
        APSARA_TEST_EQUAL(4U, items.size());
        for (auto& item : items) {
            item->mStatus = SendingStatus::IDLE;
        }
    }
    auto& regionConcurrencyLimiter = sManager->mSenderQueues.at(0).mConcurrencyLimiters[0];
    {
        // with limits, limited by concurrency limiter
        regionConcurrencyLimiter->SetLimit(3);
        vector<SenderQueueItem*> items;
        sManager->GetAllAvailableSenderQueueItems(items);
        APSARA_TEST_EQUAL(3U, items.size());
        APSARA_TEST_EQUAL(0, regionConcurrencyLimiter->GetLimit());
    }
}

void ExactlyOnceQueueManagerUnittest::TestRemoveSenderItem() {
    sManager->CreateOrUpdateQueue(1, 0, "test_config", sCheckpoints);
    {
        // queue exists
        auto item = GenerateSenderItem();
        auto ptr = item.get();
        sManager->PushSenderQueue(1, std::move(item));
        APSARA_TEST_TRUE(sManager->RemoveSenderQueueItem(1, ptr));
        APSARA_TEST_FALSE(sManager->RemoveSenderQueueItem(1, nullptr));
    }
    {
        // queue not found
        auto item = GenerateSenderItem();
        auto ptr = item.get();
        APSARA_TEST_FALSE(sManager->RemoveSenderQueueItem(2, ptr));
    }
}

void ExactlyOnceQueueManagerUnittest::TestIsAllSenderQueueEmpty() {
    sManager->CreateOrUpdateQueue(0, 0, "test_config_1", sCheckpoints);
    sManager->CreateOrUpdateQueue(1, 2, "test_config_2", sCheckpoints);
    APSARA_TEST_TRUE(sManager->IsAllSenderQueueEmpty());

    sManager->PushSenderQueue(0, GenerateSenderItem());
    APSARA_TEST_FALSE(sManager->IsAllSenderQueueEmpty());
}

void ExactlyOnceQueueManagerUnittest::OnPipelineUpdate() {
    sManager->CreateOrUpdateQueue(0, 0, "test_config", sCheckpoints);
    sManager->CreateOrUpdateQueue(1, 0, "test_config", sCheckpoints);

    sManager->InvalidatePopProcessQueue("test_config");
    APSARA_TEST_FALSE(sManager->mProcessQueues[0]->mValidToPop);
    APSARA_TEST_FALSE(sManager->mProcessQueues[1]->mValidToPop);

    sManager->ValidatePopProcessQueue("test_config");
    APSARA_TEST_TRUE(sManager->mProcessQueues[0]->mValidToPop);
    APSARA_TEST_TRUE(sManager->mProcessQueues[1]->mValidToPop);
}

unique_ptr<ProcessQueueItem> ExactlyOnceQueueManagerUnittest::GenerateProcessItem() {
    return make_unique<ProcessQueueItem>(std::move(*sEventGroup), 0);
}

unique_ptr<SenderQueueItem> ExactlyOnceQueueManagerUnittest::GenerateSenderItem() {
    auto cpt = make_shared<RangeCheckpoint>();
    return make_unique<SLSSenderQueueItem>(
        "content", sDataSize, &mFlusher, cpt->fbKey, "", RawDataType::EVENT_GROUP, "", std::move(cpt), false);
}

UNIT_TEST_CASE(ExactlyOnceQueueManagerUnittest, TestUpdateQueue)
UNIT_TEST_CASE(ExactlyOnceQueueManagerUnittest, TestDeleteQueue)
UNIT_TEST_CASE(ExactlyOnceQueueManagerUnittest, TestPushProcessQueue)
UNIT_TEST_CASE(ExactlyOnceQueueManagerUnittest, TestIsAllProcessQueueEmpty)
UNIT_TEST_CASE(ExactlyOnceQueueManagerUnittest, TestPushSenderQueue)
UNIT_TEST_CASE(ExactlyOnceQueueManagerUnittest, TestGetAllAvailableSenderQueueItems)
UNIT_TEST_CASE(ExactlyOnceQueueManagerUnittest, TestRemoveSenderItem)
UNIT_TEST_CASE(ExactlyOnceQueueManagerUnittest, TestIsAllSenderQueueEmpty)
UNIT_TEST_CASE(ExactlyOnceQueueManagerUnittest, OnPipelineUpdate)

} // namespace logtail

UNIT_TEST_MAIN
