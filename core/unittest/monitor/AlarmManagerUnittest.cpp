// Copyright 2025 iLogtail Authors
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

#include <atomic>
#include <fstream>
#include <list>
#include <thread>

#include "json/json.h"

#include "AlarmManager.h"
#include "unittest/Unittest.h"

namespace logtail {

static std::atomic_bool running(true);

class AlarmManagerUnittest : public ::testing::Test {
public:
    void SetUp() {}

    void TearDown() {}

    void TestSendAlarm();
    void TestFlushAllRegionAlarm();
};

APSARA_UNIT_TEST_CASE(AlarmManagerUnittest, TestSendAlarm, 0);
APSARA_UNIT_TEST_CASE(AlarmManagerUnittest, TestFlushAllRegionAlarm, 1);

void AlarmManagerUnittest::TestSendAlarm() {
    {
        std::string message = "Test Alarm Message";
        std::string region = "TestRegion";
        std::string projectName = "TestProject";
        std::string config = "TestConfig";
        std::string category = "TestCategory";
        AlarmType alarmType = USER_CONFIG_ALARM; // Assuming USER_CONFIG_ALARM is valid

        AlarmManager::GetInstance()->SendAlarm(alarmType, message, region, projectName, config, category);
        // Assuming we have a method to retrieve alarms for testing
        AlarmManager::AlarmVector& alarmBufferVec
            = *AlarmManager::GetInstance()->MakesureLogtailAlarmMapVecUnlocked(region);

        std::string key = projectName + "_" + category + "_" + config;
        APSARA_TEST_EQUAL(1U, alarmBufferVec[alarmType].size());
        APSARA_TEST_EQUAL(true, alarmBufferVec[alarmType].find(key) != alarmBufferVec[alarmType].end());
        APSARA_TEST_EQUAL(category, alarmBufferVec[alarmType][key]->mCategory);
        APSARA_TEST_EQUAL(1, alarmBufferVec[alarmType][key]->mCount);
        APSARA_TEST_EQUAL(message, alarmBufferVec[alarmType][key]->mMessage);
        APSARA_TEST_EQUAL(AlarmManager::GetInstance()->mMessageType[alarmType],
                          alarmBufferVec[alarmType][key]->mMessageType);
        APSARA_TEST_EQUAL(projectName, alarmBufferVec[alarmType][key]->mProjectName);
        APSARA_TEST_EQUAL(config, alarmBufferVec[alarmType][key]->mConfig);
    }
}

void AlarmManagerUnittest::TestFlushAllRegionAlarm() {
    AlarmManager::GetInstance()->mAllAlarmMap.clear();
    // Simulate adding some alarms
    AlarmManager::GetInstance()->SendAlarm(USER_CONFIG_ALARM, "Test1", "Region1", "Project1", "Config1", "Cat1");
    AlarmManager::GetInstance()->SendAlarm(GLOBAL_CONFIG_ALARM, "Test2", "Region2", "Project2", "Config2", "Cat2");

    std::vector<PipelineEventGroup> pipelineEventGroupList;
    AlarmManager::GetInstance()->FlushAllRegionAlarm(pipelineEventGroupList);

    // Assuming each alarm results in a PipelineEventGroup
    APSARA_TEST_EQUAL(2U, pipelineEventGroupList.size());
}

} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
