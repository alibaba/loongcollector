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

#include "pipeline/plugin/PluginRegistry.h"
#include "pipeline/queue/SenderQueueManager.h"
#include "runner/FlusherRunner.h"
#include "runner/sink/http/HttpSink.h"
#include "unittest/Unittest.h"
#include "unittest/plugin/PluginMock.h"

using namespace std;

namespace logtail {

class FlusherRunnerUnittest : public ::testing::Test {
public:
    void TestDispatch();
};

void FlusherRunnerUnittest::TestDispatch() {
    {
        // http
        auto flusher = make_unique<FlusherHttpMock>();
        Json::Value tmp;
        PipelineContext ctx;
        flusher->SetContext(ctx);
        flusher->SetMetricsRecordRef("name", "pluginId", "nodeId", "childNodeId");
        flusher->Init(Json::Value(), tmp);

        auto item = make_unique<SenderQueueItem>("content", 10, flusher.get(), flusher->GetQueueKey());
        auto realItem = item.get();
        flusher->PushToQueue(std::move(item));

        FlusherRunner::GetInstance()->Dispatch(realItem);

        unique_ptr<HttpSinkRequest> req;
        APSARA_TEST_TRUE(HttpSink::GetInstance()->mQueue.TryPop(req));
        APSARA_TEST_NOT_EQUAL(nullptr, req);
    }
    {
        // unknown
        auto flusher = make_unique<FlusherMock>();
        Json::Value tmp;
        PipelineContext ctx;
        flusher->SetContext(ctx);
        flusher->SetMetricsRecordRef("name", "pluginId", "nodeId", "childNodeId");
        flusher->Init(Json::Value(), tmp);

        auto item = make_unique<SenderQueueItem>("content", 10, flusher.get(), flusher->GetQueueKey());
        auto realItem = item.get();
        flusher->PushToQueue(std::move(item));

        FlusherRunner::GetInstance()->Dispatch(realItem);

        APSARA_TEST_TRUE(SenderQueueManager::GetInstance()->mQueues.at(flusher->GetQueueKey()).Empty());
    }
}

UNIT_TEST_CASE(FlusherRunnerUnittest, TestDispatch)

} // namespace logtail

UNIT_TEST_MAIN
