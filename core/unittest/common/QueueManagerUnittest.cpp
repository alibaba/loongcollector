// Copyright 2022 iLogtail Authors
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

#include "unittest/Unittest.h"
#include "common/LogstoreFeedbackKey.h"
#include "common/Flags.h"
#include "processor/LogProcess.h"
#include "sender/Sender.h"

DECLARE_FLAG_INT32(logtail_queue_check_gc_interval_sec);
DECLARE_FLAG_INT32(logtail_queue_gc_threshold_sec);

namespace logtail {

const std::string kProject = "project";
const std::string kLogstore = "logstore";
static decltype(QueueManager::GetInstance()) sQueueM = nullptr;

class QueueManagerUnittest : public ::testing::Test {
    static decltype(LogProcess::GetInstance()->GetQueue().mLogstoreQueueMap)* sProcessQueueMap;
    static decltype(Sender::Instance()->GetQueue().mLogstoreSenderQueueMap)* sSenderQueueMap;

public:
    static void SetUpTestCase() {
        INT32_FLAG(logtail_queue_check_gc_interval_sec) = 1;
        sProcessQueueMap = &(LogProcess::GetInstance()->GetQueue().mLogstoreQueueMap);
        sSenderQueueMap = &(Sender::Instance()->GetQueue().mLogstoreSenderQueueMap);
        sQueueM = QueueManager::GetInstance();
    }

    void SetUp() {
        sQueueM->clear();
        sProcessQueueMap->clear();
        sSenderQueueMap->clear();
    }

    void TestInitializeExactlyOnceQueues();

    void TestMarkGC();
};

UNIT_TEST_CASE(QueueManagerUnittest, TestInitializeExactlyOnceQueues);
UNIT_TEST_CASE(QueueManagerUnittest, TestMarkGC);

decltype(QueueManagerUnittest::sProcessQueueMap) QueueManagerUnittest::sProcessQueueMap = nullptr;
decltype(QueueManagerUnittest::sSenderQueueMap) QueueManagerUnittest::sSenderQueueMap = nullptr;

void QueueManagerUnittest::TestInitializeExactlyOnceQueues() {
    std::vector<RangeCheckpointPtr> checkpoints(2);
    for (size_t idx = 0; idx < checkpoints.size(); ++idx) {
        auto& cpt = checkpoints[idx];
        cpt = std::make_shared<RangeCheckpoint>();
        cpt->index = idx;
        cpt->key = "cpt" + std::to_string(idx);
        cpt->data.set_hash_key("");
        cpt->data.set_sequence_id(1);
        cpt->data.set_read_offset(0);
        cpt->data.set_read_length(10);
    }

    auto fb1 = sQueueM->InitializeExactlyOnceQueues(kProject, kLogstore, checkpoints);
    EXPECT_EQ(sProcessQueueMap->size(), 1);
    EXPECT_EQ(sSenderQueueMap->size(), 1);
    {
        auto iter = sProcessQueueMap->find(fb1);
        EXPECT_TRUE(iter != sProcessQueueMap->end());
        EXPECT_EQ(QueueType::ExactlyOnce, iter->second.GetQueueType());
    }
    {
        auto iter = sSenderQueueMap->find(fb1);
        EXPECT_TRUE(iter != sSenderQueueMap->end());
        EXPECT_EQ(QueueType::ExactlyOnce, iter->second.GetQueueType());
        EXPECT_EQ(checkpoints.size(), iter->second.SIZE);
        EXPECT_EQ(checkpoints.size(), iter->second.HIGH_SIZE);
        EXPECT_EQ(0, iter->second.LOW_SIZE);
    }
    EXPECT_EQ(fb1, sQueueM->GenerateFeedBackKey(kProject, kLogstore, QueueType::ExactlyOnce));
    EXPECT_EQ(sProcessQueueMap->size(), 1);
    EXPECT_EQ(sSenderQueueMap->size(), 1);

    auto fb2 = sQueueM->InitializeExactlyOnceQueues(kProject, kLogstore + "2", checkpoints);
    EXPECT_NE(fb1, fb2);
    EXPECT_EQ(sProcessQueueMap->size(), 2);
    EXPECT_EQ(sSenderQueueMap->size(), 2);
    EXPECT_TRUE(sProcessQueueMap->find(fb2) != sProcessQueueMap->end());
    EXPECT_TRUE(sSenderQueueMap->find(fb2) != sSenderQueueMap->end());

    // Normal type will not create queue.
    auto fb3 = sQueueM->GenerateFeedBackKey(kProject, kLogstore + "3");
    EXPECT_NE(fb2, fb3);
    EXPECT_NE(fb1, fb3);
    EXPECT_EQ(sProcessQueueMap->size(), 2);
    EXPECT_EQ(sSenderQueueMap->size(), 2);
}

void QueueManagerUnittest::TestMarkGC() {
    auto bakThreshold = INT32_FLAG(logtail_queue_gc_threshold_sec);
    INT32_FLAG(logtail_queue_gc_threshold_sec) = 1;

    std::vector<RangeCheckpointPtr> checkpoints(2);
    for (size_t idx = 0; idx < checkpoints.size(); ++idx) {
        auto& cpt = checkpoints[idx];
        cpt = std::make_shared<RangeCheckpoint>();
        cpt->index = idx;
        cpt->key = "cpt" + std::to_string(idx);
        cpt->data.set_hash_key("");
        cpt->data.set_sequence_id(1);
        cpt->data.set_read_offset(0);
        cpt->data.set_read_length(10);
    }

    // Exactly once queue.
    auto fb1 = sQueueM->InitializeExactlyOnceQueues(kProject, kLogstore, checkpoints);
    EXPECT_EQ(sProcessQueueMap->size(), 1);
    EXPECT_EQ(sSenderQueueMap->size(), 1);
    sQueueM->MarkGC(kProject, kLogstore);
    {
        std::lock_guard<std::mutex> lock(sQueueM->mMutex);
        EXPECT_EQ(sQueueM->mGCItems.size(), 1);
        EXPECT_EQ(sQueueM->mQueueInfos.size(), 1);
    }
    sleep(1);
    {
        std::lock_guard<std::mutex> lock(sQueueM->mMutex);
        EXPECT_EQ(sQueueM->mGCItems.size(), 0);
        EXPECT_EQ(sQueueM->mQueueInfos.size(), 0);
    }
    EXPECT_EQ(sProcessQueueMap->size(), 0);
    EXPECT_EQ(sSenderQueueMap->size(), 0);

    // Normal queue will always alive.
    const std::string kLogstore2 = kLogstore + "2";
    sQueueM->GenerateFeedBackKey(kProject, kLogstore2);
    sQueueM->MarkGC(kProject, kLogstore2);
    {
        std::lock_guard<std::mutex> lock(sQueueM->mMutex);
        EXPECT_EQ(sQueueM->mGCItems.size(), 0);
        EXPECT_EQ(sQueueM->mQueueInfos.size(), 1);
    }

    // Will keep alive because process queue is not empty.
    {
        auto fb2 = sQueueM->InitializeExactlyOnceQueues(kProject, kLogstore, checkpoints);
        EXPECT_NE(fb1, fb2);
        auto iter = sProcessQueueMap->find(fb2);
        EXPECT_TRUE(iter != sProcessQueueMap->end());
        static auto& sProcessQueue = LogProcess::GetInstance()->GetQueue();
        sProcessQueue.PushItem(fb2, nullptr);
        sQueueM->MarkGC(kProject, kLogstore);
        sleep(2);
        {
            std::lock_guard<std::mutex> lock(sQueueM->mMutex);
            EXPECT_EQ(sQueueM->mGCItems.size(), 1);
            EXPECT_EQ(sQueueM->mQueueInfos.size(), 2);
        }
        {
            LogBuffer* data = nullptr;
            EXPECT_TRUE(sProcessQueue.PopItem(fb2, data));
        }
        sleep(2);
        {
            std::lock_guard<std::mutex> lock(sQueueM->mMutex);
            EXPECT_EQ(sQueueM->mGCItems.size(), 0);
            EXPECT_EQ(sQueueM->mQueueInfos.size(), 1);
        }
        EXPECT_EQ(sProcessQueueMap->size(), 0);
        EXPECT_EQ(sSenderQueueMap->size(), 0);
    }

    // Will keep alive because sender queue is not empty.
    {
        auto fb3 = sQueueM->InitializeExactlyOnceQueues(kProject, kLogstore, checkpoints);
        EXPECT_NE(fb1, fb3);
        auto iter = sSenderQueueMap->find(fb3);
        EXPECT_TRUE(iter != sSenderQueueMap->end());
        iter->second.mSize = 1;
        sQueueM->MarkGC(kProject, kLogstore);
        sleep(2);
        {
            std::lock_guard<std::mutex> lock(sQueueM->mMutex);
            EXPECT_EQ(sQueueM->mGCItems.size(), 1);
            EXPECT_EQ(sQueueM->mQueueInfos.size(), 2);
        }
        iter->second.mSize = 0;
        sleep(2);
        {
            std::lock_guard<std::mutex> lock(sQueueM->mMutex);
            EXPECT_EQ(sQueueM->mGCItems.size(), 0);
            EXPECT_EQ(sQueueM->mQueueInfos.size(), 1);
        }
        EXPECT_EQ(sProcessQueueMap->size(), 0);
        EXPECT_EQ(sSenderQueueMap->size(), 0);
    }

    INT32_FLAG(logtail_queue_gc_threshold_sec) = bakThreshold;
}

} // namespace logtail

UNIT_TEST_MAIN