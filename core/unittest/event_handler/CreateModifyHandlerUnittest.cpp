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
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string>
#include <memory>
#include "common/Flags.h"
#include "common/util.h"
#include "config_manager/ConfigManager.h"
#include "event/Event.h"
#include "event_handler/EventHandler.h"
using namespace std;

DECLARE_FLAG_STRING(ilogtail_config);
DECLARE_FLAG_STRING(user_log_config);
DECLARE_FLAG_INT32(batch_send_interval);

namespace logtail {
class MockModifyHandler : public ModifyHandler {
public:
    MockModifyHandler() : ModifyHandler("", nullptr) {}
    virtual void Handle(const Event& event) { ++handle_count; }
    virtual void HandleTimeOut() { ++handle_timeout_count; }
    virtual bool DumpReaderMeta(bool checkConfigFlag = false) { return true; }
    void Reset() {
        handle_count = 0;
        handle_timeout_count = 0;
    }
    int handle_count = 0;
    int handle_timeout_count = 0;
};

class CreateModifyHandlerUnittest : public ::testing::Test {
protected:
    void SetUp() override {
        // mock config, log_path is related to the unittest
        std::string configStr = R"""({
    "log_path" : "/root/log",
    "file_pattern" : "test-0.log",
    "advanced" :
    {
        "force_multiconfig" : false,
        "k8s" : {},
        "tail_size_kb" : 1024
    },
    "aliuid" : "123456789",
    "category" : "logstore-0",
    "create_time" : 1647230190,
    "defaultEndpoint" : "cn-huhehaote-intranet.log.aliyuncs.com",
    "delay_alarm_bytes" : 0,
    "delay_skip_bytes" : 0,
    "discard_none_utf8" : false,
    "discard_unmatch" : false,
    "docker_exclude_env" : {},
    "docker_exclude_label" : {},
    "docker_file" : true,
    "docker_include_env" : {},
    "docker_include_label" : {},
    "enable" : true,
    "enable_tag" : false,
    "file_encoding" : "utf8",
    "filter_keys" : [],
    "filter_regs" : [],
    "group_topic" : "",
    "keys" :
    [
            "content"
    ],
    "local_storage" : true,
    "log_begin_reg" : ".*",
    "log_type" : "common_reg_log",
    "log_tz" : "",
    "max_depth" : 10,
    "max_send_rate" : -1,
    "merge_type" : "topic",
    "preserve" : true,
    "preserve_depth" : 1,
    "priority" : 0,
    "project_name" : "project-0",
    "raw_log" : false,
    "regex" :
    [
            "(.*)"
    ],
    "region" : "cn-huhehaote",
    "send_rate_expire" : 0,
    "sensitive_keys" : [],
    "shard_hash_key" : [],
    "tail_existed" : false,
    "timeformat" : "",
    "topic_format" : "none",
    "tz_adjust" : false,
    "version" : 1
})""";
        Json::Value userConfig;
        Json::Reader reader;
        APSARA_TEST_TRUE_FATAL(reader.parse(configStr, userConfig));
        ConfigManager::GetInstance()->LoadSingleUserConfig(mConfigName, userConfig);
    }

    void TearDown() override {}
    std::string mConfigName = "##1.0##project-0$config-0";
    CreateHandler mCreateHandler;

public:
    void TestHandleContainerStoppedEvent() {
        LOG_INFO(sLogger, ("TestFindAllSubDirAndHandler() begin", time(NULL)));
        CreateModifyHandler createModifyHandler(&mCreateHandler);
        MockModifyHandler* pHanlder = new MockModifyHandler(); //released by ~CreateModifyHandler
        createModifyHandler.mModifyHandlerPtrMap.insert(std::make_pair(mConfigName, pHanlder));

        Event event1("/not_exist", "", EVENT_ISDIR | EVENT_CONTAINER_STOPPED, 0);
        createModifyHandler.Handle(event1);
        APSARA_TEST_EQUAL_FATAL(pHanlder->handle_count, 0);

        Event event2("/root/log", "", EVENT_ISDIR | EVENT_CONTAINER_STOPPED, 0);
        createModifyHandler.Handle(event2);
        APSARA_TEST_EQUAL_FATAL(pHanlder->handle_count, 1);
    }
};

APSARA_UNIT_TEST_CASE(CreateModifyHandlerUnittest, TestHandleContainerStoppedEvent, 0);
} // end of namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
