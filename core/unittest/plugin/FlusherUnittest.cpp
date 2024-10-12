// Copyright 2023 iLogtail Authors
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

#include "pipeline/PipelineManager.h"
#include "pipeline/plugin/interface/Flusher.h"
#include "pipeline/queue/QueueKeyManager.h"
#include "unittest/Unittest.h"
#include "unittest/plugin/PluginMock.h"

using namespace std;

namespace logtail {

class FlusherUnittest : public testing::Test {
public:
    void TestStop() const;

protected:
    void TearDown() override { QueueKeyManager::GetInstance()->Clear(); }
};

void FlusherUnittest::TestStop() const {
    auto pipeline = make_shared<Pipeline>();
    PipelineManager::GetInstance()->mPipelineNameEntityMap["test_config"] = pipeline;

    auto ctx = PipelineContext();
    ctx.SetConfigName("test_config");

    FlusherMock* mock = new FlusherMock();
    mock->SetContext(ctx);
    Json::Value tmp;
    mock->Init(Json::Value(), tmp);

    auto q = SenderQueueManager::GetInstance()->GetQueue(mock->GetQueueKey());
    // push items to queue
    for (size_t i = 0; i < q->mCapacity; ++i) {
        auto item = make_unique<SenderQueueItem>("content", 0, nullptr, mock->GetQueueKey());
        q->Push(std::move(item));
    }
    // push items to extra buffer
    for (size_t i = 0; i < 10; ++i) {
        auto item = make_unique<SenderQueueItem>("content", 0, nullptr, mock->GetQueueKey());
        q->Push(std::move(item));
    }

    std::vector<SenderQueueItem*> items1;
    q->GetAvailableItems(items1, -1);
    for (auto item : items1) {
        APSARA_TEST_EQUAL(item->mPipeline, nullptr);
    }
    for (size_t i = 0; i < q->mExtraBuffer.size(); ++i) {
        auto item = q->mExtraBuffer[i].get();
        APSARA_TEST_EQUAL(item->mPipeline, nullptr);
    }

    mock->Stop(false);

    std::vector<SenderQueueItem*> items2;
    q->GetAvailableItems(items2, -1);
    for (auto item : items2) {
        APSARA_TEST_EQUAL(item->mPipeline, pipeline);
    }
    for (size_t i = 0; i < q->mExtraBuffer.size(); ++i) {
        auto item = q->mExtraBuffer[i].get();
        APSARA_TEST_EQUAL(item->mPipeline, pipeline);
    }
}

UNIT_TEST_CASE(FlusherUnittest, TestStop)

} // namespace logtail

UNIT_TEST_MAIN
