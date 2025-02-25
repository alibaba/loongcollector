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
#include "unittest/host_monitor/MockCollector.h"

using namespace std;

namespace logtail {

class HostMonitorInputRunnerUnittest : public testing::Test {
public:
    void TestUpdateAndRemoveCollector() const;
    void TestScheduleOnce() const;

private:
    static void SetUpTestCase() {
        auto collector = std::make_unique<MockCollector>();
        HostMonitorInputRunner::GetInstance()->mRegisteredCollectorMap.emplace(MockCollector::sName,
                                                                               CollectorInstance(std::move(collector)));
    }

    void TearDown() override {
        HostMonitorInputRunner::GetInstance()->Stop();
        Timer::GetInstance()->Clear();
    }
};

void HostMonitorInputRunnerUnittest::TestUpdateAndRemoveCollector() const {
    auto runner = HostMonitorInputRunner::GetInstance();
    runner->Init();
    runner->UpdateCollector({MockCollector::sName}, {60}, QueueKey{}, 0);
    APSARA_TEST_TRUE_FATAL(runner->IsCollectTaskValid(std::chrono::steady_clock::now(), MockCollector::sName));
    APSARA_TEST_FALSE_FATAL(
        runner->IsCollectTaskValid(std::chrono::steady_clock::now() - std::chrono::seconds(60), MockCollector::sName));
    APSARA_TEST_TRUE_FATAL(runner->HasRegisteredPlugins());
    APSARA_TEST_EQUAL_FATAL(1, Timer::GetInstance()->mQueue.size());
    runner->RemoveCollector();
    APSARA_TEST_FALSE_FATAL(runner->IsCollectTaskValid(std::chrono::steady_clock::now(), MockCollector::sName));
    APSARA_TEST_FALSE_FATAL(runner->HasRegisteredPlugins());
    runner->Stop();
}

void HostMonitorInputRunnerUnittest::TestScheduleOnce() const {
    auto runner = HostMonitorInputRunner::GetInstance();
    runner->Init();
    runner->mThreadPool->Start();
    runner->mRegisteredCollectorMap.find(MockCollector::sName)->second.Enable();
    std::string configName = "test";
    auto queueKey = QueueKeyManager::GetInstance()->GetKey(configName);
    auto ctx = CollectionPipelineContext();
    ctx.SetConfigName(configName);
    ProcessQueueManager::GetInstance()->CreateOrUpdateBoundedQueue(queueKey, 0, ctx);

    HostMonitorTimerEvent::CollectConfig collectConfig(MockCollector::sName, queueKey, 0, std::chrono::seconds(60));
    std::chrono::time_point now = std::chrono::steady_clock::now();
    runner->ScheduleOnce(now, collectConfig);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    APSARA_TEST_EQUAL_FATAL(1, Timer::GetInstance()->mQueue.size());
    APSARA_TEST_EQUAL_FATAL((now + std::chrono::seconds(60)).time_since_epoch().count(),
                            Timer::GetInstance()->mQueue.top()->GetExecTime().time_since_epoch().count());
    auto item = std::unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::make_shared<SourceBuffer>(), 0));
    ProcessQueueManager::GetInstance()->EnablePop(configName);
    APSARA_TEST_TRUE_FATAL(ProcessQueueManager::GetInstance()->PopItem(0, item, configName));
    APSARA_TEST_EQUAL_FATAL("test", configName);

    runner->mThreadPool->Stop();
    runner->Stop();
}

UNIT_TEST_CASE(HostMonitorInputRunnerUnittest, TestUpdateAndRemoveCollector);
UNIT_TEST_CASE(HostMonitorInputRunnerUnittest, TestScheduleOnce);

} // namespace logtail

UNIT_TEST_MAIN
