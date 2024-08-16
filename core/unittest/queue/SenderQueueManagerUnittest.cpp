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

#include "flusher/sls/FlusherSLS.h"
#include "queue/ExactlyOnceQueueManager.h"
#include "queue/QueueKeyManager.h"
#include "queue/QueueParam.h"
#include "queue/SLSSenderQueueItem.h"
#include "queue/SenderQueueManager.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_INT32(sender_queue_gc_threshold_sec);

using namespace std;

namespace logtail {

class SenderQueueManagerUnittest : public testing::Test {
public:
    void TestCreateQueue();
    void TestDeleteQueue();
    void TestGetQueue();
    void TestPushQueue();
    void TestGetAllAvailableItems();
    void TestRemoveItem();
    void TestIsAllQueueEmpty();

protected:
    static void SetUpTestCase() {
        sManager = SenderQueueManager::GetInstance();
        sConcurrencyLimiter = make_shared<ConcurrencyLimiter>();
        sManager->mQueueParam.mCapacity = 2;
        sManager->mQueueParam.mLowWatermark = 1;
        sManager->mQueueParam.mHighWatermark = 3;
        for (size_t i = 0; i < 2; ++i) {
            auto cpt = make_shared<RangeCheckpoint>();
            cpt->index = i;
            cpt->data.set_hash_key("key");
            cpt->data.set_sequence_id(0);
            sCheckpoints.emplace_back(cpt);
        }
    }

    void SetUp() override {
        mFlusher.mMaxSendRate = sMaxRate;
        mFlusher.mRegion = "region";
        mFlusher.mProject = "project";
    }

    void TearDown() override {
        sManager->Clear();
        ExactlyOnceQueueManager::GetInstance()->Clear();
        QueueKeyManager::GetInstance()->Clear();
        sConcurrencyLimiter->Reset();
    }

private:
    static const uint32_t sMaxRate = 100U;
    static const size_t sDataSize = 10;

    static SenderQueueManager* sManager;
    static shared_ptr<ConcurrencyLimiter> sConcurrencyLimiter;
    static vector<RangeCheckpointPtr> sCheckpoints;

    unique_ptr<SenderQueueItem> GenerateItem(bool isSLS = false);

    // cannot be static member, because its constructor relies on logger, which is initiallized after main starts
    FlusherSLS mFlusher;
};

const size_t SenderQueueManagerUnittest::sDataSize;
SenderQueueManager* SenderQueueManagerUnittest::sManager;
shared_ptr<ConcurrencyLimiter> SenderQueueManagerUnittest::sConcurrencyLimiter;
vector<RangeCheckpointPtr> SenderQueueManagerUnittest::sCheckpoints;

void SenderQueueManagerUnittest::TestCreateQueue() {
    {
        // new queue
        uint32_t maxRate = 100U;
        APSARA_TEST_TRUE(
            sManager->CreateQueue(0, vector<shared_ptr<ConcurrencyLimiter>>{sConcurrencyLimiter}, maxRate));
        APSARA_TEST_EQUAL(1U, sManager->mQueues.size());
        auto& queue = sManager->mQueues.at(0);
        APSARA_TEST_EQUAL(sManager->mQueueParam.GetCapacity(), queue.mCapacity);
        APSARA_TEST_EQUAL(sManager->mQueueParam.GetLowWatermark(), queue.mLowWatermark);
        APSARA_TEST_EQUAL(sManager->mQueueParam.GetHighWatermark(), queue.mHighWatermark);
        APSARA_TEST_EQUAL(1U, queue.mConcurrencyLimiters.size());
        APSARA_TEST_EQUAL(sConcurrencyLimiter, queue.mConcurrencyLimiters[0]);
        APSARA_TEST_TRUE(queue.mRateLimiter.has_value());
        APSARA_TEST_EQUAL(maxRate, queue.mRateLimiter->mMaxSendBytesPerSecond);
    }
    {
        // resued queue
        shared_ptr<ConcurrencyLimiter> newLimiter = make_shared<ConcurrencyLimiter>();
        uint32_t maxRate = 10U;
        APSARA_TEST_TRUE(sManager->CreateQueue(0, vector<shared_ptr<ConcurrencyLimiter>>{newLimiter}, maxRate));
        APSARA_TEST_EQUAL(1U, sManager->mQueues.size());
        auto& queue = sManager->mQueues.at(0);
        APSARA_TEST_EQUAL(1U, queue.mConcurrencyLimiters.size());
        APSARA_TEST_EQUAL(newLimiter, queue.mConcurrencyLimiters[0]);
        APSARA_TEST_TRUE(queue.mRateLimiter.has_value());
        APSARA_TEST_EQUAL(maxRate, queue.mRateLimiter->mMaxSendBytesPerSecond);
    }
}

void SenderQueueManagerUnittest::TestDeleteQueue() {
    INT32_FLAG(sender_queue_gc_threshold_sec) = 0;

    // queue not exists
    APSARA_TEST_FALSE(sManager->DeleteQueue(0));

    QueueKey key1 = QueueKeyManager::GetInstance()->GetKey("name_1");
    QueueKey key2 = QueueKeyManager::GetInstance()->GetKey("name_2");
    sManager->CreateQueue(key1, vector<shared_ptr<ConcurrencyLimiter>>{sConcurrencyLimiter}, sMaxRate);
    sManager->CreateQueue(key2, vector<shared_ptr<ConcurrencyLimiter>>{sConcurrencyLimiter}, sMaxRate);
    sManager->PushQueue(key2, GenerateItem());

    // queue exists and not marked deleted
    APSARA_TEST_TRUE(sManager->DeleteQueue(key1));
    APSARA_TEST_TRUE(sManager->DeleteQueue(key2));
    APSARA_TEST_EQUAL(2U, sManager->mQueueDeletionTimeMap.size());

    // queue exists but marked deleted
    APSARA_TEST_FALSE(sManager->DeleteQueue(key1));

    // queue key1 is deleted, but not queue key2
    sManager->ClearUnusedQueues();
    APSARA_TEST_EQUAL(1U, sManager->mQueues.size());
    APSARA_TEST_EQUAL(1U, sManager->mQueueDeletionTimeMap.size());
    APSARA_TEST_EQUAL("", QueueKeyManager::GetInstance()->GetName(key1));

    // update queue will remove the queue from gc queue
    sManager->CreateQueue(key2, vector<shared_ptr<ConcurrencyLimiter>>{sConcurrencyLimiter}, sMaxRate);
    APSARA_TEST_EQUAL(0U, sManager->mQueueDeletionTimeMap.size());
}

void SenderQueueManagerUnittest::TestGetQueue() {
    // queue not existed
    APSARA_TEST_EQUAL(nullptr, sManager->GetQueue(0));

    // queue existed
    sManager->CreateQueue(0, vector<shared_ptr<ConcurrencyLimiter>>{sConcurrencyLimiter}, sMaxRate);
    APSARA_TEST_NOT_EQUAL(nullptr, sManager->GetQueue(0));
}

void SenderQueueManagerUnittest::TestPushQueue() {
    sManager->CreateQueue(0, vector<shared_ptr<ConcurrencyLimiter>>{sConcurrencyLimiter}, sMaxRate);
    ExactlyOnceQueueManager::GetInstance()->CreateOrUpdateQueue(1, 0, "test_config", sCheckpoints);

    // queue belongs to normal queue
    APSARA_TEST_TRUE(sManager->IsValidToPush(0));
    APSARA_TEST_EQUAL(0, sManager->PushQueue(0, GenerateItem()));

    // queue belongs to exactly once queue
    APSARA_TEST_FALSE(sManager->IsValidToPush(1));
    APSARA_TEST_EQUAL(0, sManager->PushQueue(1, GenerateItem(true)));

    // no queue exists
    APSARA_TEST_FALSE(sManager->IsValidToPush(2));
    APSARA_TEST_EQUAL(2, sManager->PushQueue(2, GenerateItem()));

    // queue full
    APSARA_TEST_EQUAL(0, sManager->PushQueue(0, GenerateItem()));
    APSARA_TEST_EQUAL(0, sManager->PushQueue(1, GenerateItem(true)));
}

void SenderQueueManagerUnittest::TestGetAllAvailableItems() {
    // prepare nomal queue
    sManager->CreateQueue(
        0, vector<shared_ptr<ConcurrencyLimiter>>{FlusherSLS::GetRegionConcurrencyLimiter(mFlusher.mRegion)}, sMaxRate);
    for (size_t i = 0; i <= sManager->mQueueParam.GetCapacity(); ++i) {
        sManager->PushQueue(0, GenerateItem());
    }

    // prepare exactly once queue
    vector<RangeCheckpointPtr> checkpoints;
    for (size_t i = 0; i < 2; ++i) {
        auto cpt = make_shared<RangeCheckpoint>();
        cpt->index = i;
        cpt->data.set_hash_key("key");
        cpt->data.set_sequence_id(0);
        checkpoints.emplace_back(cpt);
    }
    ExactlyOnceQueueManager::GetInstance()->CreateOrUpdateQueue(1, 0, "test_config", checkpoints);
    for (size_t i = 0; i <= 2; ++i) {
        ExactlyOnceQueueManager::GetInstance()->PushSenderQueue(1, GenerateItem(true));
    }

    {
        // no limits
        vector<SenderQueueItem*> items;
        sManager->GetAllAvailableItems(items, false);
        APSARA_TEST_EQUAL(4U, items.size());
        for (auto& item : items) {
            item->mStatus = SendingStatus::IDLE;
        }
    }
    auto regionConcurrencyLimiter = FlusherSLS::GetRegionConcurrencyLimiter(mFlusher.mRegion);
    {
        // with limits, limited by concurrency limiter
        regionConcurrencyLimiter->SetLimit(3);
        vector<SenderQueueItem*> items;
        sManager->GetAllAvailableItems(items);
        APSARA_TEST_EQUAL(3U, items.size());
        APSARA_TEST_EQUAL(0, regionConcurrencyLimiter->GetLimit());
    }
}

void SenderQueueManagerUnittest::TestRemoveItem() {
    sManager->CreateQueue(0, vector<shared_ptr<ConcurrencyLimiter>>{sConcurrencyLimiter}, sMaxRate);
    ExactlyOnceQueueManager::GetInstance()->CreateOrUpdateQueue(1, 0, "test_config", sCheckpoints);
    {
        // normal queue
        auto item = GenerateItem();
        auto ptr = item.get();
        sManager->PushQueue(0, std::move(item));
        APSARA_TEST_TRUE(sManager->RemoveItem(0, ptr));
        APSARA_TEST_FALSE(sManager->RemoveItem(0, nullptr));
    }
    {
        // exactly once queue
        auto item = GenerateItem(true);
        auto ptr = item.get();
        sManager->PushQueue(1, std::move(item));
        APSARA_TEST_TRUE(sManager->RemoveItem(1, ptr));
        APSARA_TEST_FALSE(sManager->RemoveItem(1, nullptr));
    }
    {
        // queue not found
        auto item = GenerateItem();
        auto ptr = item.get();
        APSARA_TEST_FALSE(sManager->RemoveItem(2, ptr));
    }
}

void SenderQueueManagerUnittest::TestIsAllQueueEmpty() {
    sManager->CreateQueue(0, vector<shared_ptr<ConcurrencyLimiter>>{sConcurrencyLimiter}, sMaxRate);
    sManager->CreateQueue(1, vector<shared_ptr<ConcurrencyLimiter>>{sConcurrencyLimiter}, sMaxRate);
    ExactlyOnceQueueManager::GetInstance()->CreateOrUpdateQueue(2, 0, "test_config_1", sCheckpoints);
    ExactlyOnceQueueManager::GetInstance()->CreateOrUpdateQueue(3, 2, "test_config_2", sCheckpoints);
    APSARA_TEST_TRUE(sManager->IsAllQueueEmpty());
    {
        // non-empty normal queue
        auto item = GenerateItem();
        auto ptr = item.get();
        sManager->PushQueue(0, std::move(item));
        APSARA_TEST_FALSE(sManager->IsAllQueueEmpty());

        sManager->RemoveItem(0, ptr);
        APSARA_TEST_TRUE(sManager->IsAllQueueEmpty());
    }
    {
        // non-empty exactly once queue
        auto item = GenerateItem(true);
        auto ptr = item.get();
        sManager->PushQueue(2, std::move(item));
        APSARA_TEST_FALSE(sManager->IsAllQueueEmpty());

        sManager->RemoveItem(2, ptr);
        APSARA_TEST_TRUE(sManager->IsAllQueueEmpty());
    }
}

unique_ptr<SenderQueueItem> SenderQueueManagerUnittest::GenerateItem(bool isSLS) {
    if (isSLS) {
        auto cpt = make_shared<RangeCheckpoint>();
        return make_unique<SLSSenderQueueItem>(
            "content", sDataSize, &mFlusher, cpt->fbKey, "", RawDataType::EVENT_GROUP, "", std::move(cpt), false);
    }
    return make_unique<SenderQueueItem>("content", 10, nullptr, 0);
}

UNIT_TEST_CASE(SenderQueueManagerUnittest, TestCreateQueue)
UNIT_TEST_CASE(SenderQueueManagerUnittest, TestDeleteQueue)
UNIT_TEST_CASE(SenderQueueManagerUnittest, TestGetQueue)
UNIT_TEST_CASE(SenderQueueManagerUnittest, TestPushQueue)
UNIT_TEST_CASE(SenderQueueManagerUnittest, TestGetAllAvailableItems)
UNIT_TEST_CASE(SenderQueueManagerUnittest, TestRemoveItem)
UNIT_TEST_CASE(SenderQueueManagerUnittest, TestIsAllQueueEmpty)

} // namespace logtail

UNIT_TEST_MAIN
