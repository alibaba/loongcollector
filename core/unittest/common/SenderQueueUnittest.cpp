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
#include "common/LogstoreSenderQueue.h"
#include "common/FileSystemUtil.h"
#include "sender/SenderQueueParam.h"
#include "aggregator/Aggregator.h"
#include "app_config/AppConfig.h"

namespace logtail {

std::string kTestRootDir;

class SenderQueueUnittest : public ::testing::Test {
public:
    static void SetUpTestCase() {
        kTestRootDir = (bfs::path(GetProcessExecutionDir()) / "SenderQueueUnittest").string();
        if (bfs::exists(kTestRootDir)) {
            bfs::remove_all(kTestRootDir);
        }
        bfs::create_directories(kTestRootDir);
        AppConfig::GetInstance()->SetLogtailSysConfDir(kTestRootDir + PATH_SEPARATOR);
    }

    void TestExactlyOnceQueue();
};

UNIT_TEST_CASE(SenderQueueUnittest, TestExactlyOnceQueue);

void SenderQueueUnittest::TestExactlyOnceQueue() {
    {
        LogstoreSenderQueue<SenderQueueParam> senderQueue;
        std::vector<RangeCheckpointPtr> checkpoints(1);
        auto& cpt = checkpoints[0];
        cpt = std::make_shared<RangeCheckpoint>();
        cpt->index = 0;
        cpt->key = "cpt1";
        cpt->data.set_hash_key("");
        cpt->data.set_sequence_id(1);
        cpt->data.set_read_offset(0);
        cpt->data.set_read_length(10);

        const LogstoreFeedBackKey kFbKey = 0;
        senderQueue.ConvertToExactlyOnceQueue(kFbKey, checkpoints);
        EXPECT_TRUE(senderQueue.IsEmpty(kFbKey));
        EXPECT_TRUE(senderQueue.IsValidToPush(kFbKey));
        auto data = new LoggroupTimeValue();
        data->mLogstoreKey = kFbKey;
        data->mLogGroupContext.mExactlyOnceCheckpoint = cpt;
        EXPECT_TRUE(senderQueue.PushItem(kFbKey, data));
        EXPECT_FALSE(senderQueue.IsValidToPush(kFbKey));
        EXPECT_FALSE(senderQueue.IsEmpty(kFbKey));
        senderQueue.OnLoggroupSendDone(data, LogstoreSenderInfo::SendResult_OK);
        EXPECT_TRUE(senderQueue.IsValidToPush(kFbKey));
        EXPECT_TRUE(senderQueue.IsEmpty(kFbKey));
    }

    {
        LogstoreSenderQueue<SenderQueueParam> senderQueue;
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

        const LogstoreFeedBackKey kFbKey = 0;
        senderQueue.ConvertToExactlyOnceQueue(kFbKey, checkpoints);
        EXPECT_TRUE(senderQueue.IsEmpty(kFbKey));
        EXPECT_TRUE(senderQueue.IsValidToPush(kFbKey));
        auto data = new LoggroupTimeValue();
        data->mLogstoreKey = kFbKey;
        data->mLogGroupContext.mExactlyOnceCheckpoint = checkpoints[0];
        EXPECT_TRUE(senderQueue.PushItem(kFbKey, data));
        EXPECT_TRUE(senderQueue.IsValidToPush(kFbKey));
        EXPECT_FALSE(senderQueue.IsEmpty(kFbKey));
        auto data2 = new LoggroupTimeValue();
        data2->mLogstoreKey = kFbKey;
        data2->mLogGroupContext.mExactlyOnceCheckpoint = checkpoints[1];
        EXPECT_TRUE(senderQueue.PushItem(kFbKey, data2));
        EXPECT_FALSE(senderQueue.IsValidToPush(kFbKey));
        EXPECT_FALSE(senderQueue.IsEmpty(kFbKey));
        senderQueue.OnLoggroupSendDone(data, LogstoreSenderInfo::SendResult_OK);
        EXPECT_TRUE(senderQueue.IsValidToPush(kFbKey));
        EXPECT_FALSE(senderQueue.IsEmpty(kFbKey));
    }
}

} // namespace logtail

UNIT_TEST_MAIN