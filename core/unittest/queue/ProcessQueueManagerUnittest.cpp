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
#include "queue/QueueParam.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class ProcessQueueManagerUnittest : public testing::Test {
public:
    void TestUpdateSameTypeQueue();
    void TestDeleteQueue();
    void TestSetQueueUpstreamAndDownStream();
    void TestPushQueue();
    void TestPopItem();
    void TestIsAllQueueEmpty();
    void OnPipelineUpdate();

protected:
    static void SetUpTestCase() {
        sEventGroup.reset(new PipelineEventGroup(make_shared<SourceBuffer>()));
        sProcessQueueManager = ProcessQueueManager::GetInstance();
    }

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
ProcessQueueManager* ProcessQueueManagerUnittest::sProcessQueueManager;

void ProcessQueueManagerUnittest::TestUpdateSameTypeQueue() {
    // create queue
    //   and current index is invalid before creation
    QueueKey key = QueueKeyManager::GetInstance()->GetKey("test_config_1");
    APSARA_TEST_TRUE(sProcessQueueManager->CreateOrUpdateQueue(key, 0, ProcessQueueManager::QueueType::BOUNDED));
    APSARA_TEST_EQUAL(1U, sProcessQueueManager->mQueues.size());
    APSARA_TEST_EQUAL(1U, sProcessQueueManager->mPriorityQueue[0].size());
    auto iter = sProcessQueueManager->mQueues[key].first;
    APSARA_TEST_TRUE(iter == prev(sProcessQueueManager->mPriorityQueue[0].end()));
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == iter);
    APSARA_TEST_EQUAL(ProcessQueueParam::GetInstance()->mCapacity, (*iter)->mCapacity);
    APSARA_TEST_EQUAL(ProcessQueueParam::GetInstance()->mLowWatermark,
                      static_cast<BoundedProcessQueue*>(iter->get())->mLowWatermark);
    APSARA_TEST_EQUAL(ProcessQueueParam::GetInstance()->mHighWatermark,
                      static_cast<BoundedProcessQueue*>(iter->get())->mHighWatermark);
    APSARA_TEST_EQUAL("test_config_1", (*iter)->GetConfigName());

    // create queue
    //   and current index is valid before creation
    APSARA_TEST_TRUE(sProcessQueueManager->CreateOrUpdateQueue(1, 0, ProcessQueueManager::QueueType::BOUNDED));
    APSARA_TEST_EQUAL(2U, sProcessQueueManager->mQueues.size());
    APSARA_TEST_EQUAL(2U, sProcessQueueManager->mPriorityQueue[0].size());
    APSARA_TEST_TRUE(sProcessQueueManager->mQueues[1].first == prev(sProcessQueueManager->mPriorityQueue[0].end()));
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[0].first);

    // add more queue
    APSARA_TEST_TRUE(sProcessQueueManager->CreateOrUpdateQueue(2, 0, ProcessQueueManager::QueueType::BOUNDED));
    APSARA_TEST_TRUE(sProcessQueueManager->CreateOrUpdateQueue(3, 0, ProcessQueueManager::QueueType::BOUNDED));
    sProcessQueueManager->mCurrentQueueIndex.second = sProcessQueueManager->mQueues[2].first;

    // update queue with same priority
    APSARA_TEST_FALSE(sProcessQueueManager->CreateOrUpdateQueue(0, 0, ProcessQueueManager::QueueType::BOUNDED));

    // update queue with different priority
    //   and current index not equal to the updated queue
    APSARA_TEST_TRUE(sProcessQueueManager->CreateOrUpdateQueue(0, 1, ProcessQueueManager::QueueType::BOUNDED));
    APSARA_TEST_EQUAL(4U, sProcessQueueManager->mQueues.size());
    APSARA_TEST_EQUAL(3U, sProcessQueueManager->mPriorityQueue[0].size());
    APSARA_TEST_EQUAL(1U, sProcessQueueManager->mPriorityQueue[1].size());
    APSARA_TEST_TRUE(sProcessQueueManager->mQueues[0].first == prev(sProcessQueueManager->mPriorityQueue[1].end()));
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[2].first);

    // update queue with different priority
    //   and current index equals to the updated queue
    //     and the updated queue is not the last in the list
    APSARA_TEST_TRUE(sProcessQueueManager->CreateOrUpdateQueue(2, 1, ProcessQueueManager::QueueType::BOUNDED));
    APSARA_TEST_EQUAL(4U, sProcessQueueManager->mQueues.size());
    APSARA_TEST_EQUAL(2U, sProcessQueueManager->mPriorityQueue[0].size());
    APSARA_TEST_EQUAL(2U, sProcessQueueManager->mPriorityQueue[1].size());
    APSARA_TEST_TRUE(sProcessQueueManager->mQueues[2].first == prev(sProcessQueueManager->mPriorityQueue[1].end()));
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[3].first);

    // update queue with different priority
    //   and current index equals to the updated queue
    //     and the updated queue is the last in the list
    //       and more queues exist
    APSARA_TEST_TRUE(sProcessQueueManager->CreateOrUpdateQueue(3, 1, ProcessQueueManager::QueueType::BOUNDED));
    APSARA_TEST_EQUAL(4U, sProcessQueueManager->mQueues.size());
    APSARA_TEST_EQUAL(1U, sProcessQueueManager->mPriorityQueue[0].size());
    APSARA_TEST_EQUAL(3U, sProcessQueueManager->mPriorityQueue[1].size());
    APSARA_TEST_TRUE(sProcessQueueManager->mQueues[3].first == prev(sProcessQueueManager->mPriorityQueue[1].end()));
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[1].first);

    // update queue with different priority
    //   and current index equals to the updated queue
    //     and the updated queue is the last in the list
    //       and no more queue exists
    APSARA_TEST_TRUE(sProcessQueueManager->CreateOrUpdateQueue(1, 1, ProcessQueueManager::QueueType::BOUNDED));
    APSARA_TEST_EQUAL(4U, sProcessQueueManager->mQueues.size());
    APSARA_TEST_EQUAL(0U, sProcessQueueManager->mPriorityQueue[0].size());
    APSARA_TEST_EQUAL(4U, sProcessQueueManager->mPriorityQueue[1].size());
    APSARA_TEST_TRUE(sProcessQueueManager->mQueues[1].first == prev(sProcessQueueManager->mPriorityQueue[1].end()));
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mPriorityQueue[0].end());

    // update queue with different priority
    //   and current index is invalid before update
    APSARA_TEST_TRUE(sProcessQueueManager->CreateOrUpdateQueue(0, 0, ProcessQueueManager::QueueType::BOUNDED));
    APSARA_TEST_EQUAL(4U, sProcessQueueManager->mQueues.size());
    APSARA_TEST_EQUAL(1U, sProcessQueueManager->mPriorityQueue[0].size());
    APSARA_TEST_EQUAL(3U, sProcessQueueManager->mPriorityQueue[1].size());
    APSARA_TEST_TRUE(sProcessQueueManager->mQueues[0].first == prev(sProcessQueueManager->mPriorityQueue[0].end()));
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[0].first);
}

void ProcessQueueManagerUnittest::TestDeleteQueue() {
    QueueKey key1 = QueueKeyManager::GetInstance()->GetKey("test_config_1");
    QueueKey key2 = QueueKeyManager::GetInstance()->GetKey("test_config_2");
    QueueKey key3 = QueueKeyManager::GetInstance()->GetKey("test_config_3");
    QueueKey key4 = QueueKeyManager::GetInstance()->GetKey("test_config_4");
    sProcessQueueManager->CreateOrUpdateQueue(key1, 0, ProcessQueueManager::QueueType::BOUNDED);
    sProcessQueueManager->CreateOrUpdateQueue(key2, 0, ProcessQueueManager::QueueType::BOUNDED);
    sProcessQueueManager->CreateOrUpdateQueue(key3, 0, ProcessQueueManager::QueueType::BOUNDED);
    sProcessQueueManager->CreateOrUpdateQueue(key4, 0, ProcessQueueManager::QueueType::BOUNDED);
    sProcessQueueManager->mCurrentQueueIndex.second = sProcessQueueManager->mQueues[key3].first;

    // current index not equal to the deleted queue
    APSARA_TEST_TRUE(sProcessQueueManager->DeleteQueue(key1));
    APSARA_TEST_EQUAL(3U, sProcessQueueManager->mQueues.size());
    APSARA_TEST_EQUAL(3U, sProcessQueueManager->mPriorityQueue[0].size());
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[key3].first);
    APSARA_TEST_EQUAL("", QueueKeyManager::GetInstance()->GetName(key1));

    // current index equals to the deleted queue
    //   and the deleted queue is not the last in the list
    APSARA_TEST_TRUE(sProcessQueueManager->DeleteQueue(key3));
    APSARA_TEST_EQUAL(2U, sProcessQueueManager->mQueues.size());
    APSARA_TEST_EQUAL(2U, sProcessQueueManager->mPriorityQueue[0].size());
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[key4].first);
    APSARA_TEST_EQUAL("", QueueKeyManager::GetInstance()->GetName(key3));

    // current index equals to the deleted queue
    //   and the deleted queue is the last in the list
    //     and more queues exist
    APSARA_TEST_TRUE(sProcessQueueManager->DeleteQueue(key4));
    APSARA_TEST_EQUAL(1U, sProcessQueueManager->mQueues.size());
    APSARA_TEST_EQUAL(1U, sProcessQueueManager->mPriorityQueue[0].size());
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[key2].first);
    APSARA_TEST_EQUAL("", QueueKeyManager::GetInstance()->GetName(key4));

    // current index equals to the deleted queue
    //   and the deleted queue is the last in the list
    //     and no more queue exists
    APSARA_TEST_TRUE(sProcessQueueManager->DeleteQueue(key2));
    APSARA_TEST_EQUAL(0U, sProcessQueueManager->mQueues.size());
    APSARA_TEST_EQUAL(0U, sProcessQueueManager->mPriorityQueue[0].size());
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mPriorityQueue[0].end());
    APSARA_TEST_EQUAL("", QueueKeyManager::GetInstance()->GetName(key2));

    // queue not exist
    APSARA_TEST_FALSE(sProcessQueueManager->DeleteQueue(key1));
}

void ProcessQueueManagerUnittest::TestSetQueueUpstreamAndDownStream() {
    sProcessQueueManager->CreateOrUpdateQueue(0, 0, ProcessQueueManager::QueueType::BOUNDED);

    // queue exists
    APSARA_TEST_TRUE(sProcessQueueManager->SetDownStreamQueues(0, vector<BoundedSenderQueueInterface*>()));
    APSARA_TEST_TRUE(sProcessQueueManager->SetFeedbackInterface(0, vector<FeedbackInterface*>()));
    // queue not exists
    APSARA_TEST_FALSE(sProcessQueueManager->SetDownStreamQueues(1, vector<BoundedSenderQueueInterface*>()));
    APSARA_TEST_FALSE(sProcessQueueManager->SetFeedbackInterface(1, vector<FeedbackInterface*>()));
}

void ProcessQueueManagerUnittest::TestPushQueue() {
    sProcessQueueManager->CreateOrUpdateQueue(0, 0, ProcessQueueManager::QueueType::BOUNDED);
    ExactlyOnceQueueManager::GetInstance()->CreateOrUpdateQueue(1, 0, "test_config", vector<RangeCheckpointPtr>(5));

    // queue belongs to normal process queue
    APSARA_TEST_TRUE(sProcessQueueManager->IsValidToPush(0));
    APSARA_TEST_EQUAL(0, sProcessQueueManager->PushQueue(0, make_unique<ProcessQueueItem>(std::move(*sEventGroup), 0)));

    // queue belongs to exactly once process queue
    APSARA_TEST_TRUE(sProcessQueueManager->IsValidToPush(1));
    APSARA_TEST_EQUAL(0, sProcessQueueManager->PushQueue(1, make_unique<ProcessQueueItem>(std::move(*sEventGroup), 0)));

    // no queue exists
    APSARA_TEST_FALSE(sProcessQueueManager->IsValidToPush(2));
    APSARA_TEST_EQUAL(2, sProcessQueueManager->PushQueue(2, make_unique<ProcessQueueItem>(std::move(*sEventGroup), 0)));

    // invalid to push
    static_cast<BoundedProcessQueue*>(sProcessQueueManager->mQueues[0].first->get())->mValidToPush = false;
    APSARA_TEST_FALSE(sProcessQueueManager->IsValidToPush(0));
    APSARA_TEST_EQUAL(1, sProcessQueueManager->PushQueue(0, make_unique<ProcessQueueItem>(std::move(*sEventGroup), 0)));

    ExactlyOnceQueueManager::GetInstance()->mProcessQueues[1]->mValidToPush = false;
    APSARA_TEST_FALSE(sProcessQueueManager->IsValidToPush(1));
    APSARA_TEST_EQUAL(1, sProcessQueueManager->PushQueue(1, make_unique<ProcessQueueItem>(std::move(*sEventGroup), 0)));
}

void ProcessQueueManagerUnittest::TestPopItem() {
    unique_ptr<ProcessQueueItem> item;
    string configName;

    QueueKey key1 = QueueKeyManager::GetInstance()->GetKey("test_config_1");
    QueueKey key2 = QueueKeyManager::GetInstance()->GetKey("test_config_2");
    QueueKey key3 = QueueKeyManager::GetInstance()->GetKey("test_config_3");
    QueueKey key4 = QueueKeyManager::GetInstance()->GetKey("test_config_4");
    sProcessQueueManager->CreateOrUpdateQueue(key1, 0, ProcessQueueManager::QueueType::BOUNDED);
    sProcessQueueManager->CreateOrUpdateQueue(key2, 1, ProcessQueueManager::QueueType::BOUNDED);
    sProcessQueueManager->CreateOrUpdateQueue(key3, 1, ProcessQueueManager::QueueType::BOUNDED);
    sProcessQueueManager->CreateOrUpdateQueue(key4, 1, ProcessQueueManager::QueueType::BOUNDED);
    ExactlyOnceQueueManager::GetInstance()->CreateOrUpdateQueue(5, 0, "test_config_5", vector<RangeCheckpointPtr>(5));

    sProcessQueueManager->PushQueue(key2, make_unique<ProcessQueueItem>(std::move(*sEventGroup), 0));
    sProcessQueueManager->PushQueue(key3, make_unique<ProcessQueueItem>(std::move(*sEventGroup), 0));
    sProcessQueueManager->mCurrentQueueIndex = {1, prev(prev(sProcessQueueManager->mPriorityQueue[1].end()))};

    // the item comes from the queue between current index and queue list end
    APSARA_TEST_TRUE(sProcessQueueManager->PopItem(0, item, configName));
    APSARA_TEST_EQUAL("test_config_3", configName);
    APSARA_TEST_EQUAL(1U, sProcessQueueManager->mCurrentQueueIndex.first);
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[key4].first);

    // the item comes from the queue between queue list and current index
    APSARA_TEST_TRUE(sProcessQueueManager->PopItem(0, item, configName));
    APSARA_TEST_EQUAL("test_config_2", configName);
    APSARA_TEST_EQUAL(1U, sProcessQueueManager->mCurrentQueueIndex.first);
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[key3].first);

    sProcessQueueManager->PushQueue(key1, make_unique<ProcessQueueItem>(std::move(*sEventGroup), 0));
    // the item comes from queue list other than the one pointed by current index
    APSARA_TEST_TRUE(sProcessQueueManager->PopItem(0, item, configName));
    APSARA_TEST_EQUAL("test_config_1", configName);
    APSARA_TEST_EQUAL(0U, sProcessQueueManager->mCurrentQueueIndex.first);
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[key1].first);

    sProcessQueueManager->mCurrentQueueIndex = {1, prev(sProcessQueueManager->mPriorityQueue[1].end())};
    sProcessQueueManager->PushQueue(5, make_unique<ProcessQueueItem>(std::move(*sEventGroup), 0));
    // the item comes from exactly once queue
    APSARA_TEST_TRUE(sProcessQueueManager->PopItem(0, item, configName));
    APSARA_TEST_EQUAL("test_config_5", configName);
    APSARA_TEST_EQUAL(0U, sProcessQueueManager->mCurrentQueueIndex.first);
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[key1].first);

    sProcessQueueManager->mCurrentQueueIndex = {1, prev(sProcessQueueManager->mPriorityQueue[1].end())};
    // no item
    APSARA_TEST_FALSE(sProcessQueueManager->PopItem(0, item, configName));
    APSARA_TEST_EQUAL(0U, sProcessQueueManager->mCurrentQueueIndex.first);
    APSARA_TEST_TRUE(sProcessQueueManager->mCurrentQueueIndex.second == sProcessQueueManager->mQueues[key1].first);
}

void ProcessQueueManagerUnittest::TestIsAllQueueEmpty() {
    sProcessQueueManager->CreateOrUpdateQueue(0, 0, ProcessQueueManager::QueueType::BOUNDED);
    sProcessQueueManager->CreateOrUpdateQueue(1, 1, ProcessQueueManager::QueueType::BOUNDED);
    ExactlyOnceQueueManager::GetInstance()->CreateOrUpdateQueue(2, 0, "test_config_1", vector<RangeCheckpointPtr>(5));
    ExactlyOnceQueueManager::GetInstance()->CreateOrUpdateQueue(3, 2, "test_config_2", vector<RangeCheckpointPtr>(5));
    APSARA_TEST_TRUE(sProcessQueueManager->IsAllQueueEmpty());

    // non empty normal process queue
    sProcessQueueManager->PushQueue(0, make_unique<ProcessQueueItem>(std::move(*sEventGroup), 0));
    APSARA_TEST_FALSE(sProcessQueueManager->IsAllQueueEmpty());

    unique_ptr<ProcessQueueItem> item;
    string configName;
    sProcessQueueManager->PopItem(0, item, configName);
    APSARA_TEST_TRUE(sProcessQueueManager->IsAllQueueEmpty());

    // non empty exactly once process queue
    sProcessQueueManager->PushQueue(2, make_unique<ProcessQueueItem>(std::move(*sEventGroup), 0));
    APSARA_TEST_FALSE(sProcessQueueManager->IsAllQueueEmpty());

    sProcessQueueManager->PopItem(0, item, configName);
    APSARA_TEST_TRUE(sProcessQueueManager->IsAllQueueEmpty());
}

void ProcessQueueManagerUnittest::OnPipelineUpdate() {
    QueueKey key = QueueKeyManager::GetInstance()->GetKey("test_config_1");
    sProcessQueueManager->CreateOrUpdateQueue(key, 0, ProcessQueueManager::QueueType::BOUNDED);
    ExactlyOnceQueueManager::GetInstance()->CreateOrUpdateQueue(1, 0, "test_config_2", vector<RangeCheckpointPtr>(5));
    ExactlyOnceQueueManager::GetInstance()->CreateOrUpdateQueue(2, 0, "test_config_2", vector<RangeCheckpointPtr>(5));

    sProcessQueueManager->InvalidatePop("test_config_1");
    APSARA_TEST_FALSE((*sProcessQueueManager->mQueues[key].first)->mValidToPop);

    sProcessQueueManager->InvalidatePop("test_config_2");
    APSARA_TEST_FALSE(ExactlyOnceQueueManager::GetInstance()->mProcessQueues[1]->mValidToPop);
    APSARA_TEST_FALSE(ExactlyOnceQueueManager::GetInstance()->mProcessQueues[2]->mValidToPop);

    sProcessQueueManager->ValidatePop("test_config_1");
    APSARA_TEST_TRUE((*sProcessQueueManager->mQueues[key].first)->mValidToPop);

    sProcessQueueManager->ValidatePop("test_config_2");
    APSARA_TEST_TRUE(ExactlyOnceQueueManager::GetInstance()->mProcessQueues[1]->mValidToPop);
    APSARA_TEST_TRUE(ExactlyOnceQueueManager::GetInstance()->mProcessQueues[2]->mValidToPop);
}

UNIT_TEST_CASE(ProcessQueueManagerUnittest, TestUpdateSameTypeQueue)
UNIT_TEST_CASE(ProcessQueueManagerUnittest, TestDeleteQueue)
UNIT_TEST_CASE(ProcessQueueManagerUnittest, TestSetQueueUpstreamAndDownStream)
UNIT_TEST_CASE(ProcessQueueManagerUnittest, TestPushQueue)
UNIT_TEST_CASE(ProcessQueueManagerUnittest, TestPopItem)
UNIT_TEST_CASE(ProcessQueueManagerUnittest, TestIsAllQueueEmpty)
UNIT_TEST_CASE(ProcessQueueManagerUnittest, OnPipelineUpdate)

} // namespace logtail

UNIT_TEST_MAIN
