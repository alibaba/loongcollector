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

#include <chrono>
#include <memory>

#include "HostMonitorInputRunner.h"
#include "HostMonitorTimerEvent.h"
#include "ProcessQueueItem.h"
#include "ProcessQueueManager.h"
#include "QueueKey.h"
#include "QueueKeyManager.h"
#include "common/timer/Timer.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class HostMonitorInputRunnerUnittest : public testing::Test {
public:
    void TestUpdateAndRemoveCollector() const;
    void TestScheduleOnce() const;
};

void HostMonitorInputRunnerUnittest::TestUpdateAndRemoveCollector() const {
    auto runner = HostMonitorInputRunner::GetInstance();
    runner->Init();
    runner->UpdateCollector("test", {"mock"}, QueueKey{}, 0);
    APSARA_TEST_TRUE_FATAL(runner->IsCollectTaskValid("test", "mock"));
    APSARA_TEST_TRUE_FATAL(runner->HasRegisteredPlugins());
    runner->RemoveCollector("test");
    APSARA_TEST_FALSE_FATAL(runner->IsCollectTaskValid("test", "mock"));
    APSARA_TEST_FALSE_FATAL(runner->HasRegisteredPlugins());
    runner->Stop();
}

void HostMonitorInputRunnerUnittest::TestScheduleOnce() const {
    auto runner = HostMonitorInputRunner::GetInstance();
    runner->Init();
    runner->mThreadPool.Start();
    std::string configName = "test";
    auto queueKey = QueueKeyManager::GetInstance()->GetKey(configName);
    auto ctx = PipelineContext();
    ctx.SetConfigName(configName);
    ProcessQueueManager::GetInstance()->CreateOrUpdateBoundedQueue(queueKey, 0, ctx);

    auto collectConfig = std::make_unique<HostMonitorTimerEvent::CollectConfig>(
        configName, "process", queueKey, 0, std::chrono::seconds(1));
    runner->ScheduleOnce(std::move(collectConfig));
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto item = std::unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::make_shared<SourceBuffer>(), 0));
    ProcessQueueManager::GetInstance()->EnablePop(configName);
    APSARA_TEST_TRUE_FATAL(ProcessQueueManager::GetInstance()->PopItem(0, item, configName));
    APSARA_TEST_EQUAL_FATAL("test", configName);
    APSARA_TEST_TRUE_FATAL(item->mEventGroup.GetEvents().size() == 1);

    // verify schdule next
    APSARA_TEST_EQUAL_FATAL(Timer::GetInstance()->mQueue.size(), 1);
    runner->mThreadPool.Stop();
    runner->Stop();
}

UNIT_TEST_CASE(HostMonitorInputRunnerUnittest, TestUpdateAndRemoveCollector);
UNIT_TEST_CASE(HostMonitorInputRunnerUnittest, TestScheduleOnce);

} // namespace logtail

UNIT_TEST_MAIN
