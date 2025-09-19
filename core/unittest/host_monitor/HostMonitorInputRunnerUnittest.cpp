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

#include "ProcessQueueItem.h"
#include "ProcessQueueManager.h"
#include "QueueKey.h"
#include "QueueKeyManager.h"
#include "common/timer/Timer.h"
#include "host_monitor/HostMonitorContext.h"
#include "host_monitor/HostMonitorInputRunner.h"
#include "unittest/Unittest.h"
#include "unittest/host_monitor/MockCollector.h"

using namespace std;

namespace logtail {

class HostMonitorInputRunnerUnittest : public testing::Test {
public:
    void TestUpdateAndRemoveCollector() const;
    void TestScheduleOnce() const;
    void TestReset() const;

private:
    static void SetUpTestCase() {
        HostMonitorInputRunner::GetInstance()->mCollectorCreatorMap.emplace(
            MockCollector::sName,
            []() -> CollectorInstance { return CollectorInstance(std::make_unique<MockCollector>()); });
    }

    void TearDown() override {
        HostMonitorInputRunner::GetInstance()->Stop();
        Timer::GetInstance()->Clear();
    }
};

void HostMonitorInputRunnerUnittest::TestUpdateAndRemoveCollector() const {
    auto runner = HostMonitorInputRunner::GetInstance();
    runner->Init();
    std::string configName = "test";
    runner->UpdateCollector(
        configName, {{MockCollector::sName, 60, HostMonitorCollectType::kMultiValue}}, QueueKey{}, 0);
    auto startTime = runner->mRegisteredCollector.at({configName, MockCollector::sName}).startTime;
    APSARA_TEST_TRUE_FATAL(runner->IsCollectTaskValid(startTime, configName, MockCollector::sName));
    APSARA_TEST_FALSE_FATAL(
        runner->IsCollectTaskValid(startTime - std::chrono::seconds(60), configName, MockCollector::sName));
    APSARA_TEST_TRUE_FATAL(runner->HasRegisteredPlugins());
    APSARA_TEST_EQUAL_FATAL(1, Timer::GetInstance()->mQueue.size());
    runner->RemoveCollector(configName);
    APSARA_TEST_FALSE_FATAL(
        runner->IsCollectTaskValid(startTime + std::chrono::seconds(60), configName, MockCollector::sName));
    APSARA_TEST_FALSE_FATAL(runner->HasRegisteredPlugins());
    runner->Stop();
}

void HostMonitorInputRunnerUnittest::TestScheduleOnce() const {
    auto runner = HostMonitorInputRunner::GetInstance();
    runner->Init();
    runner->mThreadPool->Start();
    std::string configName = "test";
    // 先添加测试收集器配置，这会添加第一个定时器事件
    runner->UpdateCollector(
        configName, {{MockCollector::sName, 1, HostMonitorCollectType::kMultiValue}}, QueueKey{}, 0);
    // UpdateCollector会添加一个定时器事件
    APSARA_TEST_EQUAL_FATAL(1, Timer::GetInstance()->mQueue.size());
    auto queueKey = QueueKeyManager::GetInstance()->GetKey(configName);
    auto ctx = CollectionPipelineContext();
    ctx.SetConfigName(configName);
    ProcessQueueManager::GetInstance()->CreateOrUpdateBoundedQueue(queueKey, 0, ctx);

    auto mockCollector = std::make_unique<MockCollector>();
    auto collectContext = std::make_shared<HostMonitorContext>(configName,
                                                               MockCollector::sName,
                                                               queueKey,
                                                               0,
                                                               std::chrono::seconds(60),
                                                               CollectorInstance(std::move(mockCollector)));
    collectContext->SetTime(std::chrono::steady_clock::now(), 0);
    // 第一次ScheduleOnce不会立即添加TimerEvent，而是添加任务到线程池
    runner->ScheduleOnce(collectContext);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // second schedule once should be cancelled, because start time is not the same
    APSARA_TEST_EQUAL_FATAL(1, Timer::GetInstance()->mQueue.size());

    auto mockCollector2 = std::make_unique<MockCollector>();
    auto collectContext2 = std::make_shared<HostMonitorContext>(configName,
                                                                MockCollector::sName,
                                                                queueKey,
                                                                0,
                                                                std::chrono::seconds(60),
                                                                CollectorInstance(std::move(mockCollector2)));
    collectContext2->mStartTime
        = HostMonitorInputRunner::GetInstance()->mRegisteredCollector.at({configName, MockCollector::sName}).startTime;
    runner->ScheduleOnce(collectContext2);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    APSARA_TEST_EQUAL_FATAL(2, Timer::GetInstance()->mQueue.size());

    auto item = std::make_unique<ProcessQueueItem>(std::make_shared<SourceBuffer>(), 0);
    ProcessQueueManager::GetInstance()->EnablePop(configName);
    APSARA_TEST_TRUE_FATAL(ProcessQueueManager::GetInstance()->PopItem(0, item, configName));
    APSARA_TEST_EQUAL_FATAL("test", configName);

    runner->mThreadPool->Stop();
    runner->Stop();
}

void HostMonitorInputRunnerUnittest::TestReset() const {
    { // case 1: between two points
        auto mockCollector = std::make_unique<MockCollector>();
        HostMonitorContext collectContext(
            "test", "test", QueueKey{}, 0, std::chrono::seconds(15), CollectorInstance(std::move(mockCollector)));
        auto steadyClockNow = std::chrono::steady_clock::now();
        collectContext.SetTime(steadyClockNow, 1);
        collectContext.mCollectInterval = std::chrono::seconds(5);

        collectContext.CalculateFirstCollectTime(1, steadyClockNow);

        APSARA_TEST_EQUAL_FATAL(collectContext.GetMetricTime(), 15 + 5);
        APSARA_TEST_EQUAL_FATAL(collectContext.GetScheduleTime(), steadyClockNow + std::chrono::seconds(14 + 5));
    }
    { // case 2: at the edge of the collect interval
        auto mockCollector = std::make_unique<MockCollector>();
        HostMonitorContext collectContext(
            "test", "test", QueueKey{}, 0, std::chrono::seconds(15), CollectorInstance(std::move(mockCollector)));
        auto steadyClockNow = std::chrono::steady_clock::now();
        collectContext.SetTime(steadyClockNow, 5);
        collectContext.mCollectInterval = std::chrono::seconds(5);

        collectContext.CalculateFirstCollectTime(5, steadyClockNow);

        APSARA_TEST_EQUAL_FATAL(collectContext.GetMetricTime(), 15 + 5);
        APSARA_TEST_EQUAL_FATAL(collectContext.GetScheduleTime(), steadyClockNow + std::chrono::seconds(15));
    }
    { // case 3: at the edge of the report interval
        auto mockCollector = std::make_unique<MockCollector>();
        HostMonitorContext collectContext(
            "test", "test", QueueKey{}, 0, std::chrono::seconds(15), CollectorInstance(std::move(mockCollector)));
        auto steadyClockNow = std::chrono::steady_clock::now();
        collectContext.SetTime(steadyClockNow, 15);
        collectContext.mCollectInterval = std::chrono::seconds(5);

        collectContext.CalculateFirstCollectTime(15, steadyClockNow);

        APSARA_TEST_EQUAL_FATAL(collectContext.GetMetricTime(), 30 + 5);
        APSARA_TEST_EQUAL_FATAL(collectContext.GetScheduleTime(), steadyClockNow + std::chrono::seconds(15 + 5));
    }
}

UNIT_TEST_CASE(HostMonitorInputRunnerUnittest, TestUpdateAndRemoveCollector);
UNIT_TEST_CASE(HostMonitorInputRunnerUnittest, TestScheduleOnce);
UNIT_TEST_CASE(HostMonitorInputRunnerUnittest, TestReset);

} // namespace logtail

UNIT_TEST_MAIN
