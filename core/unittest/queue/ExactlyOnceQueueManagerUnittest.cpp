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

#include "models/PipelineEventGroup.h"
#include "queue/ExactlyOnceQueueManager.h"
#include "queue/QueueKeyManager.h"
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
    void OnPipelineUpdate();

protected:
    static void SetUpTestCase() { sEventGroup.reset(new PipelineEventGroup(make_shared<SourceBuffer>())); }
    void TearDown() override {
        sManager->Clear();
        QueueKeyManager::GetInstance()->Clear();
    }

private:
    static unique_ptr<PipelineEventGroup> sEventGroup;
    static ExactlyOnceQueueManager* sManager;
};

unique_ptr<PipelineEventGroup> ExactlyOnceQueueManagerUnittest::sEventGroup;
ExactlyOnceQueueManager* ExactlyOnceQueueManagerUnittest::sManager = ExactlyOnceQueueManager::GetInstance();

void ExactlyOnceQueueManagerUnittest::TestUpdateQueue() {
    // create queue
    size_t queueSize = 5;
    APSARA_TEST_TRUE(sManager->CreateOrUpdateQueue(0, 0, "test_config", vector<RangeCheckpointPtr>(queueSize)));
    APSARA_TEST_EQUAL(1U, sManager->mProcessQueues.size());
    APSARA_TEST_EQUAL(1U, sManager->mProcessPriorityQueue[0].size());
    APSARA_TEST_TRUE(sManager->mProcessQueues[0] == prev(sManager->mProcessPriorityQueue[0].end()));
    APSARA_TEST_EQUAL(1U, sManager->mProcessQueues[0]->mUpStreamFeedbacks.size());
    APSARA_TEST_EQUAL(1U, sManager->mProcessQueues[0]->mDownStreamQueues.size());
    APSARA_TEST_EQUAL(queueSize, sManager->mProcessQueues[0]->mDownStreamQueues[0]->SIZE);

    // update queue with the same priority
    queueSize = 8;
    APSARA_TEST_TRUE(sManager->CreateOrUpdateQueue(0, 0, "test_config", vector<RangeCheckpointPtr>(queueSize)));
    APSARA_TEST_EQUAL(1U, sManager->mProcessQueues.size());
    APSARA_TEST_EQUAL(1U, sManager->mProcessPriorityQueue[0].size());
    APSARA_TEST_TRUE(sManager->mProcessQueues[0] == prev(sManager->mProcessPriorityQueue[0].end()));
    APSARA_TEST_EQUAL(1U, sManager->mProcessQueues[0]->mDownStreamQueues.size());
    APSARA_TEST_EQUAL(1U, sManager->mProcessQueues[0]->mUpStreamFeedbacks.size());
    APSARA_TEST_EQUAL(queueSize, sManager->mProcessQueues[0]->mDownStreamQueues[0]->SIZE);

    // update queue with different priority
    queueSize = 6;
    APSARA_TEST_TRUE(sManager->CreateOrUpdateQueue(0, 1, "test_config", vector<RangeCheckpointPtr>(queueSize)));
    APSARA_TEST_EQUAL(1U, sManager->mProcessQueues.size());
    APSARA_TEST_EQUAL(0U, sManager->mProcessPriorityQueue[0].size());
    APSARA_TEST_EQUAL(1U, sManager->mProcessPriorityQueue[1].size());
    APSARA_TEST_TRUE(sManager->mProcessQueues[0] == prev(sManager->mProcessPriorityQueue[1].end()));
    APSARA_TEST_EQUAL(1U, sManager->mProcessQueues[0]->mDownStreamQueues.size());
    APSARA_TEST_EQUAL(1U, sManager->mProcessQueues[0]->mUpStreamFeedbacks.size());
    APSARA_TEST_EQUAL(queueSize, sManager->mProcessQueues[0]->mDownStreamQueues[0]->SIZE);
}

void ExactlyOnceQueueManagerUnittest::TestDeleteQueue() {
    INT32_FLAG(logtail_queue_gc_threshold_sec) = 0;

    // queue not exists
    APSARA_TEST_FALSE(sManager->DeleteQueue(0));

    QueueKey key1 = QueueKeyManager::GetInstance()->GetKey("name_1");
    QueueKey key2 = QueueKeyManager::GetInstance()->GetKey("name_2");
    sManager->CreateOrUpdateQueue(key1, 1, "test_config_1", vector<RangeCheckpointPtr>(5));
    sManager->CreateOrUpdateQueue(key2, 1, "test_config_2", vector<RangeCheckpointPtr>(5));
    sManager->PushProcessQueue(key2, unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0)));

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
    APSARA_TEST_EQUAL(1U, sManager->mQueueDeletionTimeMap.size());
    APSARA_TEST_EQUAL("", QueueKeyManager::GetInstance()->GetName(key1));

    // update queue will remove the queue from gc queue
    sManager->CreateOrUpdateQueue(key2, 0, "test_config_2", vector<RangeCheckpointPtr>(5));
    APSARA_TEST_EQUAL(0U, sManager->mQueueDeletionTimeMap.size());
}

void ExactlyOnceQueueManagerUnittest::TestPushProcessQueue() {
    sManager->CreateOrUpdateQueue(0, 0, "test_config", vector<RangeCheckpointPtr>(5));

    // queue exists
    APSARA_TEST_TRUE(sManager->IsValidToPushProcessQueue(0));
    APSARA_TEST_EQUAL(
        0,
        sManager->PushProcessQueue(0, unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0))));

    // no queue exists
    APSARA_TEST_FALSE(sManager->IsValidToPushProcessQueue(1));
    APSARA_TEST_EQUAL(
        2,
        sManager->PushProcessQueue(1, unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0))));

    // invalid to push
    sManager->mProcessQueues[0]->mValidToPush = false;
    APSARA_TEST_FALSE(sManager->IsValidToPushProcessQueue(0));
    APSARA_TEST_EQUAL(
        1,
        sManager->PushProcessQueue(0, unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0))));
}

void ExactlyOnceQueueManagerUnittest::TestIsAllProcessQueueEmpty() {
    sManager->CreateOrUpdateQueue(0, 0, "test_config_1", vector<RangeCheckpointPtr>(5));
    sManager->CreateOrUpdateQueue(1, 2, "test_config_2", vector<RangeCheckpointPtr>(5));
    APSARA_TEST_TRUE(sManager->IsAllProcessQueueEmpty());

    sManager->PushProcessQueue(0, unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0)));
    APSARA_TEST_FALSE(sManager->IsAllProcessQueueEmpty());
}

void ExactlyOnceQueueManagerUnittest::OnPipelineUpdate() {
    sManager->CreateOrUpdateQueue(0, 0, "test_config", vector<RangeCheckpointPtr>(5));
    sManager->CreateOrUpdateQueue(1, 0, "test_config", vector<RangeCheckpointPtr>(5));

    sManager->InvalidatePop("test_config");
    APSARA_TEST_FALSE(sManager->mProcessQueues[0]->mValidToPop);
    APSARA_TEST_FALSE(sManager->mProcessQueues[1]->mValidToPop);

    sManager->ValidatePop("test_config");
    APSARA_TEST_TRUE(sManager->mProcessQueues[0]->mValidToPop);
    APSARA_TEST_TRUE(sManager->mProcessQueues[1]->mValidToPop);
}

UNIT_TEST_CASE(ExactlyOnceQueueManagerUnittest, TestUpdateQueue)
UNIT_TEST_CASE(ExactlyOnceQueueManagerUnittest, TestDeleteQueue)
UNIT_TEST_CASE(ExactlyOnceQueueManagerUnittest, TestPushProcessQueue)
UNIT_TEST_CASE(ExactlyOnceQueueManagerUnittest, TestIsAllProcessQueueEmpty)
UNIT_TEST_CASE(ExactlyOnceQueueManagerUnittest, OnPipelineUpdate)

} // namespace logtail

UNIT_TEST_MAIN
