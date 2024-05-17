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
#include "queue/ProcessQueueManager.h"
#include "queue/QueueKeyManager.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {
class ProcessQueueManagerUnittest : public testing::Test {
public:
    void TestUpdateQueue();
    void TestDeleteQueue();
    void TestSetQueueUpstreamAndDownStream();
    void TestPushQueue();
    void TestPopItem();
    void TestIsAllQueueEmpty();
    void OnPipelineUpdate();

protected:
    static void SetUpTestCase() { sEventGroup.reset(new PipelineEventGroup(make_shared<SourceBuffer>())); }
    void TearDown() override {
        QueueKeyManager::GetInstance()->Clear();
        sProcessQueueManager->Clear();
        ExactlyOnceQueueManager::GetInstance()->Clear();
    }

private:
    static unique_ptr<PipelineEventGroup> sEventGroup;
    static ProcessQueueManager* sProcessQueueManager;
};

unique_ptr<PipelineEventGroup> ProcessQueueManagerUnittest::sEventGroup;
ProcessQueueManager* ProcessQueueManagerUnittest::sProcessQueueManager = ProcessQueueManager::GetInstance();

void ProcessQueueManagerUnittest::TestUpdateQueue() {
    // create queue
    //   and current index is invalid before creation
    APSARA_TEST_TRUE(sProcessQueueManager->CreateOrUpdateQueue(0, 0));
    APSARA_TEST_EQUAL(1U, sProcessQueueManager->mQueues.size());
    APSARA_TEST_EQUAL(1U, sProcessQueueManager->mPriorityQueue[0].size());
    APSARA_TEST_TRUE(sProcessQueueManager->mQueues[0] == prev(sProcessQueueManager->mPriorityQueue[0].end()));
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[0]);

    // create queue
    //   and current index is valid before creation
    APSARA_TEST_TRUE(sProcessQueueManager->CreateOrUpdateQueue(1, 0));
    APSARA_TEST_EQUAL(2U, sProcessQueueManager->mQueues.size());
    APSARA_TEST_EQUAL(2U, sProcessQueueManager->mPriorityQueue[0].size());
    APSARA_TEST_TRUE(sProcessQueueManager->mQueues[1] == prev(sProcessQueueManager->mPriorityQueue[0].end()));
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[0]);

    // add more queue
    APSARA_TEST_TRUE(sProcessQueueManager->CreateOrUpdateQueue(2, 0));
    APSARA_TEST_TRUE(sProcessQueueManager->CreateOrUpdateQueue(3, 0));
    sProcessQueueManager->mCurrentQueueIndex.second = sProcessQueueManager->mQueues[2];

    // update queue with same priority
    APSARA_TEST_FALSE(sProcessQueueManager->CreateOrUpdateQueue(0, 0));

    // update queue with different priority
    //   and current index not equal to the updated queue
    APSARA_TEST_TRUE(sProcessQueueManager->CreateOrUpdateQueue(0, 1));
    APSARA_TEST_EQUAL(4U, sProcessQueueManager->mQueues.size());
    APSARA_TEST_EQUAL(3U, sProcessQueueManager->mPriorityQueue[0].size());
    APSARA_TEST_EQUAL(1U, sProcessQueueManager->mPriorityQueue[1].size());
    APSARA_TEST_TRUE(sProcessQueueManager->mQueues[0] == prev(sProcessQueueManager->mPriorityQueue[1].end()));
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[2]);

    // update queue with different priority
    //   and current index equals to the updated queue
    //     and the updated queue is not the last in the list
    APSARA_TEST_TRUE(sProcessQueueManager->CreateOrUpdateQueue(2, 1));
    APSARA_TEST_EQUAL(4U, sProcessQueueManager->mQueues.size());
    APSARA_TEST_EQUAL(2U, sProcessQueueManager->mPriorityQueue[0].size());
    APSARA_TEST_EQUAL(2U, sProcessQueueManager->mPriorityQueue[1].size());
    APSARA_TEST_TRUE(sProcessQueueManager->mQueues[2] == prev(sProcessQueueManager->mPriorityQueue[1].end()));
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[3]);

    // update queue with different priority
    //   and current index equals to the updated queue
    //     and the updated queue is the last in the list
    //       and more queues exist
    APSARA_TEST_TRUE(sProcessQueueManager->CreateOrUpdateQueue(3, 1));
    APSARA_TEST_EQUAL(4U, sProcessQueueManager->mQueues.size());
    APSARA_TEST_EQUAL(1U, sProcessQueueManager->mPriorityQueue[0].size());
    APSARA_TEST_EQUAL(3U, sProcessQueueManager->mPriorityQueue[1].size());
    APSARA_TEST_TRUE(sProcessQueueManager->mQueues[3] == prev(sProcessQueueManager->mPriorityQueue[1].end()));
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[1]);

    // update queue with different priority
    //   and current index equals to the updated queue
    //     and the updated queue is the last in the list
    //       and no more queue exists
    APSARA_TEST_TRUE(sProcessQueueManager->CreateOrUpdateQueue(1, 1));
    APSARA_TEST_EQUAL(4U, sProcessQueueManager->mQueues.size());
    APSARA_TEST_EQUAL(0U, sProcessQueueManager->mPriorityQueue[0].size());
    APSARA_TEST_EQUAL(4U, sProcessQueueManager->mPriorityQueue[1].size());
    APSARA_TEST_TRUE(sProcessQueueManager->mQueues[1] == prev(sProcessQueueManager->mPriorityQueue[1].end()));
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mPriorityQueue[0].end());

    // update queue with different priority
    //   and current index is invalid before update
    APSARA_TEST_TRUE(sProcessQueueManager->CreateOrUpdateQueue(0, 0));
    APSARA_TEST_EQUAL(4U, sProcessQueueManager->mQueues.size());
    APSARA_TEST_EQUAL(1U, sProcessQueueManager->mPriorityQueue[0].size());
    APSARA_TEST_EQUAL(3U, sProcessQueueManager->mPriorityQueue[1].size());
    APSARA_TEST_TRUE(sProcessQueueManager->mQueues[0] == prev(sProcessQueueManager->mPriorityQueue[0].end()));
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[0]);
}

void ProcessQueueManagerUnittest::TestDeleteQueue() {
    sProcessQueueManager->CreateOrUpdateQueue(0, 0);
    sProcessQueueManager->CreateOrUpdateQueue(1, 0);
    sProcessQueueManager->CreateOrUpdateQueue(2, 0);
    sProcessQueueManager->CreateOrUpdateQueue(3, 0);
    sProcessQueueManager->mCurrentQueueIndex.second = sProcessQueueManager->mQueues[2];

    // current index not equal to the deleted queue
    APSARA_TEST_TRUE(sProcessQueueManager->DeleteQueue(0));
    APSARA_TEST_EQUAL(3U, sProcessQueueManager->mQueues.size());
    APSARA_TEST_EQUAL(3U, sProcessQueueManager->mPriorityQueue[0].size());
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[2]);

    // current index equals to the deleted queue
    //   and the deleted queue is not the last in the list
    APSARA_TEST_TRUE(sProcessQueueManager->DeleteQueue(2));
    APSARA_TEST_EQUAL(2U, sProcessQueueManager->mQueues.size());
    APSARA_TEST_EQUAL(2U, sProcessQueueManager->mPriorityQueue[0].size());
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[3]);

    // current index equals to the deleted queue
    //   and the deleted queue is the last in the list
    //     and more queues exist
    APSARA_TEST_TRUE(sProcessQueueManager->DeleteQueue(3));
    APSARA_TEST_EQUAL(1U, sProcessQueueManager->mQueues.size());
    APSARA_TEST_EQUAL(1U, sProcessQueueManager->mPriorityQueue[0].size());
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[1]);

    // current index equals to the deleted queue
    //   and the deleted queue is the last in the list
    //     and no more queue exists
    APSARA_TEST_TRUE(sProcessQueueManager->DeleteQueue(1));
    APSARA_TEST_EQUAL(0U, sProcessQueueManager->mQueues.size());
    APSARA_TEST_EQUAL(0U, sProcessQueueManager->mPriorityQueue[0].size());
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mPriorityQueue[0].end());

    // queue not exist
    APSARA_TEST_FALSE(sProcessQueueManager->DeleteQueue(0));
}

void ProcessQueueManagerUnittest::TestSetQueueUpstreamAndDownStream() {
    sProcessQueueManager->CreateOrUpdateQueue(0, 0);

    vector<SingleLogstoreSenderManager<SenderQueueParam>*> queues;
    vector<FeedbackInterface*> feedbacks;
    // queue exists
    APSARA_TEST_TRUE(sProcessQueueManager->SetDownStreamQueues(0, queues));
    APSARA_TEST_TRUE(sProcessQueueManager->SetFeedbackInterface(0, feedbacks));
    // queue not exists
    APSARA_TEST_FALSE(sProcessQueueManager->SetDownStreamQueues(1, queues));
    APSARA_TEST_FALSE(sProcessQueueManager->SetFeedbackInterface(1, feedbacks));
}

void ProcessQueueManagerUnittest::TestPushQueue() {
    sProcessQueueManager->CreateOrUpdateQueue(0, 0);
    ExactlyOnceQueueManager::GetInstance()->CreateOrUpdateQueue(1, 0, "test_config", vector<RangeCheckpointPtr>(5));

    // queue belongs to normal process queue
    APSARA_TEST_TRUE(sProcessQueueManager->IsValidToPush(0));
    APSARA_TEST_EQUAL(0,
                      sProcessQueueManager->PushQueue(
                          0, unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0))));

    // queue belongs to exactly once process queue
    APSARA_TEST_TRUE(sProcessQueueManager->IsValidToPush(1));
    APSARA_TEST_EQUAL(0,
                      sProcessQueueManager->PushQueue(
                          1, unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0))));

    // no queue exists
    APSARA_TEST_FALSE(sProcessQueueManager->IsValidToPush(2));
    APSARA_TEST_EQUAL(2,
                      sProcessQueueManager->PushQueue(
                          2, unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0))));

    // invalid to push
    sProcessQueueManager->mQueues[0]->mValidToPush = false;
    APSARA_TEST_FALSE(sProcessQueueManager->IsValidToPush(0));
    APSARA_TEST_EQUAL(1,
                      sProcessQueueManager->PushQueue(
                          0, unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0))));

    ExactlyOnceQueueManager::GetInstance()->mProcessQueues[1]->mValidToPush = false;
    APSARA_TEST_FALSE(sProcessQueueManager->IsValidToPush(1));
    APSARA_TEST_EQUAL(1,
                      sProcessQueueManager->PushQueue(
                          1, unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0))));
}

void ProcessQueueManagerUnittest::TestPopItem() {
    unique_ptr<ProcessQueueItem> item;
    string configName;

    QueueKey key1 = QueueKeyManager::GetInstance()->GetKey("test_config_1");
    QueueKey key2 = QueueKeyManager::GetInstance()->GetKey("test_config_2");
    QueueKey key3 = QueueKeyManager::GetInstance()->GetKey("test_config_3");
    QueueKey key4 = QueueKeyManager::GetInstance()->GetKey("test_config_4");
    sProcessQueueManager->CreateOrUpdateQueue(key1, 0);
    sProcessQueueManager->CreateOrUpdateQueue(key2, 1);
    sProcessQueueManager->CreateOrUpdateQueue(key3, 1);
    sProcessQueueManager->CreateOrUpdateQueue(key4, 1);
    ExactlyOnceQueueManager::GetInstance()->CreateOrUpdateQueue(5, 0, "test_config_5", vector<RangeCheckpointPtr>(5));

    sProcessQueueManager->PushQueue(key2,
                                    unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0)));
    sProcessQueueManager->PushQueue(key3,
                                    unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0)));
    sProcessQueueManager->mCurrentQueueIndex = {1, prev(prev(sProcessQueueManager->mPriorityQueue[1].end()))};

    // the item comes from the queue between current index and queue list end
    APSARA_TEST_TRUE(sProcessQueueManager->PopItem(0, item, configName));
    APSARA_TEST_EQUAL("test_config_3", configName);
    APSARA_TEST_EQUAL(1U, sProcessQueueManager->mCurrentQueueIndex.first);
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[key4]);

    // the item comes from the queue between queue list and current index
    APSARA_TEST_TRUE(sProcessQueueManager->PopItem(0, item, configName));
    APSARA_TEST_EQUAL("test_config_2", configName);
    APSARA_TEST_EQUAL(1U, sProcessQueueManager->mCurrentQueueIndex.first);
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[key3]);

    sProcessQueueManager->PushQueue(key1,
                                    unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0)));
    // the item comes from queue list other than the one pointed by current index
    APSARA_TEST_TRUE(sProcessQueueManager->PopItem(0, item, configName));
    APSARA_TEST_EQUAL("test_config_1", configName);
    APSARA_TEST_EQUAL(0U, sProcessQueueManager->mCurrentQueueIndex.first);
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[key1]);

    sProcessQueueManager->mCurrentQueueIndex = {1, prev(sProcessQueueManager->mPriorityQueue[1].end())};
    sProcessQueueManager->PushQueue(5, unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0)));
    // the item comes from exactly once queue
    APSARA_TEST_TRUE(sProcessQueueManager->PopItem(0, item, configName));
    APSARA_TEST_EQUAL("test_config_5", configName);
    APSARA_TEST_EQUAL(0U, sProcessQueueManager->mCurrentQueueIndex.first);
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[key1]);

    sProcessQueueManager->mCurrentQueueIndex = {1, prev(sProcessQueueManager->mPriorityQueue[1].end())};
    // no item
    APSARA_TEST_FALSE(sProcessQueueManager->PopItem(0, item, configName));
    APSARA_TEST_EQUAL(0U, sProcessQueueManager->mCurrentQueueIndex.first);
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[key1]);
}

void ProcessQueueManagerUnittest::TestIsAllQueueEmpty() {
    sProcessQueueManager->CreateOrUpdateQueue(0, 0);
    sProcessQueueManager->CreateOrUpdateQueue(1, 1);
    ExactlyOnceQueueManager::GetInstance()->CreateOrUpdateQueue(2, 0, "test_config_1", vector<RangeCheckpointPtr>(5));
    ExactlyOnceQueueManager::GetInstance()->CreateOrUpdateQueue(3, 2, "test_config_2", vector<RangeCheckpointPtr>(5));
    APSARA_TEST_TRUE(sProcessQueueManager->IsAllQueueEmpty());

    // non empty normal process queue
    sProcessQueueManager->PushQueue(0, unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0)));
    APSARA_TEST_FALSE(sProcessQueueManager->IsAllQueueEmpty());

    unique_ptr<ProcessQueueItem> item;
    string configName;
    sProcessQueueManager->PopItem(0, item, configName);
    APSARA_TEST_TRUE(sProcessQueueManager->IsAllQueueEmpty());

    // non empty exactly once process queue
    sProcessQueueManager->PushQueue(2, unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(*sEventGroup), 0)));
    APSARA_TEST_FALSE(sProcessQueueManager->IsAllQueueEmpty());

    sProcessQueueManager->PopItem(0, item, configName);
    APSARA_TEST_TRUE(sProcessQueueManager->IsAllQueueEmpty());
}

void ProcessQueueManagerUnittest::OnPipelineUpdate() {
    QueueKey key = QueueKeyManager::GetInstance()->GetKey("test_config_1");
    sProcessQueueManager->CreateOrUpdateQueue(key, 0);
    ExactlyOnceQueueManager::GetInstance()->CreateOrUpdateQueue(1, 0, "test_config_2", vector<RangeCheckpointPtr>(5));
    ExactlyOnceQueueManager::GetInstance()->CreateOrUpdateQueue(2, 0, "test_config_2", vector<RangeCheckpointPtr>(5));

    sProcessQueueManager->InvalidatePop("test_config_1");
    APSARA_TEST_FALSE(sProcessQueueManager->mQueues[key]->mValidToPop);

    sProcessQueueManager->InvalidatePop("test_config_2");
    APSARA_TEST_FALSE(ExactlyOnceQueueManager::GetInstance()->mProcessQueues[1]->mValidToPop);
    APSARA_TEST_FALSE(ExactlyOnceQueueManager::GetInstance()->mProcessQueues[2]->mValidToPop);

    sProcessQueueManager->ValidatePop("test_config_1");
    APSARA_TEST_TRUE(sProcessQueueManager->mQueues[key]->mValidToPop);

    sProcessQueueManager->ValidatePop("test_config_2");
    APSARA_TEST_TRUE(ExactlyOnceQueueManager::GetInstance()->mProcessQueues[1]->mValidToPop);
    APSARA_TEST_TRUE(ExactlyOnceQueueManager::GetInstance()->mProcessQueues[2]->mValidToPop);
}

UNIT_TEST_CASE(ProcessQueueManagerUnittest, TestUpdateQueue)
UNIT_TEST_CASE(ProcessQueueManagerUnittest, TestDeleteQueue)
UNIT_TEST_CASE(ProcessQueueManagerUnittest, TestSetQueueUpstreamAndDownStream)
UNIT_TEST_CASE(ProcessQueueManagerUnittest, TestPushQueue)
UNIT_TEST_CASE(ProcessQueueManagerUnittest, TestPopItem)
UNIT_TEST_CASE(ProcessQueueManagerUnittest, TestIsAllQueueEmpty)
UNIT_TEST_CASE(ProcessQueueManagerUnittest, OnPipelineUpdate)

} // namespace logtail

UNIT_TEST_MAIN
