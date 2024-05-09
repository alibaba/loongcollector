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

#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <memory>
#include <string>

#include "app_config/AppConfig.h"
#include "common/Flags.h"
#include "config_manager/ConfigManager.h"
#include "event/Event.h"
#include "file_server/FileServer.h"
#include "unittest/Unittest.h"
using namespace std;

DECLARE_FLAG_STRING(ilogtail_config);
namespace logtail {
class ConfigContainerUnittest : public ::testing::Test {
public:
    void MockContainerInfoConfig() {
        FileDiscoveryOptions config1;
        std::string jsonStr1 = R"""({
  "ID":"abcdef",
  "Path":"/logtail_host/lib/var/docker/abcdef"
})""";
        config1.SetEnableContainerDiscoveryFlag(true);
        FileServer::GetInstance()->AddFileDiscoveryConfig("test-config-1", &config1, nullptr);
        FileDiscoveryConfig config = FileServer::GetInstance()->GetFileDiscoveryConfig("test-config-1");
        APSARA_TEST_TRUE_FATAL(config1.UpdateContainerInfo(jsonStr1));

        FileDiscoveryOptions config2;
        std::string jsonStr2 = R"""({
  "ID":"000000",
  "Path":"/logtail_host/lib/var/docker/000000"
})""";
        config2.SetEnableContainerDiscoveryFlag(true);
        FileServer::GetInstance()->AddFileDiscoveryConfig("test-config-1", &config2, nullptr);
        config = FileServer::GetInstance()->GetFileDiscoveryConfig("test-config-1");
        APSARA_TEST_TRUE_FATAL(config2.UpdateContainerInfo(jsonStr2));
    }

    void TestGetContainerStoppedEvents() {
        LOG_INFO(sLogger, ("TestGetContainerStoppedEvents() begin", time(NULL)));
        std::vector<Event*> eventVec;
        MockContainerInfoConfig();
        std::string params = R"""({
  "ID":"abcdef"
})""";
        // Case: config name does not match any config, should not generate any event
        ConfigContainerInfoUpdateCmd* cmd_invalid = new ConfigContainerInfoUpdateCmd("test-config", true, params);
        ConfigManager::GetInstance()->UpdateContainerStopped(cmd_invalid);
        ConfigManager::GetInstance()->GetContainerStoppedEvents(eventVec);
        APSARA_TEST_TRUE_FATAL(ConfigManager::GetInstance()->mDockerContainerStoppedCmdVec.empty());
        APSARA_TEST_TRUE_FATAL(eventVec.empty());

        // Case: config name matches configs, should generate a event
        ConfigContainerInfoUpdateCmd* cmd_valid = new ConfigContainerInfoUpdateCmd("test-config-1", true, params);
        ConfigManager::GetInstance()->UpdateContainerStopped(cmd_valid);
        ConfigManager::GetInstance()->GetContainerStoppedEvents(eventVec);
        APSARA_TEST_EQUAL_FATAL(eventVec.size(), 1UL);
        Event* event = eventVec[0];
        ASSERT_STREQ(event->GetSource().c_str(), "/logtail_host/lib/var/docker/abcdef");
        APSARA_TEST_EQUAL_FATAL(event->GetType(), EVENT_ISDIR | EVENT_CONTAINER_STOPPED);
        delete event;
    }
};

APSARA_UNIT_TEST_CASE(ConfigContainerUnittest, TestGetContainerStoppedEvents, 0);
} // end of namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
