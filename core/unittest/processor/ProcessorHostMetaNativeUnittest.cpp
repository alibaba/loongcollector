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

#include <cstdlib>

#include "LogEvent.h"
#include "constants/EntityConstants.h"
#include "pipeline/Pipeline.h"
#include "plugin/processor/inner/ProcessorHostMetaNative.h"
#include "unittest/Unittest.h"

namespace logtail {

class ProcessorHostMetaNativeUnittest : public ::testing::Test {
public:
    void TestInit();
    void TestProcess();
    void TestGetProcessEntityID();

protected:
    void SetUp() override { mContext.SetConfigName("project##config_0"); }

private:
    PipelineContext mContext;
};

void ProcessorHostMetaNativeUnittest::TestInit() {
    // make config
    Json::Value config;
    Pipeline pipeline;
    mContext.SetPipeline(pipeline);

    {
        setenv(DEFAULT_ENV_KEY_HOST_TYPE.c_str(), DEFAULT_ENV_VALUE_ECS.c_str(), 1);
        ProcessorHostMetaNative processor;
        processor.SetContext(mContext);
        APSARA_TEST_TRUE_FATAL(processor.Init(config));
        APSARA_TEST_EQUAL_FATAL(processor.mDomain, DEFAULT_CONTENT_VALUE_DOMAIN_ACS);
        APSARA_TEST_EQUAL_FATAL(processor.mEntityType,
                                DEFAULT_CONTENT_VALUE_DOMAIN_ACS + "." + DEFAULT_ENV_VALUE_ECS + "."
                                    + DEFAULT_CONTENT_VALUE_ENTITY_TYPE_PROCESS);
    }
    {
        setenv(DEFAULT_ENV_KEY_HOST_TYPE.c_str(), DEFAULT_CONTENT_VALUE_DOMAIN_INFRA.c_str(), 1);
        ProcessorHostMetaNative processor;
        processor.SetContext(mContext);
        APSARA_TEST_TRUE_FATAL(processor.Init(config));
        APSARA_TEST_EQUAL_FATAL(processor.mDomain, DEFAULT_CONTENT_VALUE_DOMAIN_INFRA);
        APSARA_TEST_EQUAL_FATAL(processor.mEntityType,
                                DEFAULT_CONTENT_VALUE_DOMAIN_INFRA + "." + DEFAULT_ENV_VALUE_HOST + "."
                                    + DEFAULT_CONTENT_VALUE_ENTITY_TYPE_PROCESS);
    }
}

void ProcessorHostMetaNativeUnittest::TestProcess() {
    // make config
    Json::Value config;
    auto sourceBuffer = std::make_shared<logtail::SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    eventGroup.SetMetadataNoCopy(EventGroupMetaKey::COLLECT_TIME, "123456");
    auto event = eventGroup.AddLogEvent();
    event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_PID, "123");
    event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_CREATE_TIME, "123456");
    event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_PPID, "123");
    event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_USER, "root");
    event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_COMM, "comm");
    event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_CWD, "cwd");
    event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_BINARY, "binary");
    event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_ARGUMENTS, "arguments");
    event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_LANGUAGE, "language");
    event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_CONTAINER_ID, "container_id");

    ProcessorHostMetaNative processor;
    processor.SetContext(mContext);
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    processor.Process(eventGroup);
    APSARA_TEST_EQUAL_FATAL(eventGroup.GetEvents().size(), 1);
    auto newEvent = eventGroup.GetEvents().front().Cast<LogEvent>();
    APSARA_TEST_EQUAL_FATAL(newEvent.GetContent(DEFAULT_CONTENT_KEY_DOMAIN), processor.mDomain);
    APSARA_TEST_EQUAL_FATAL(newEvent.GetContent(DEFAULT_CONTENT_KEY_ENTITY_TYPE), processor.mEntityType);
    APSARA_TEST_EQUAL_FATAL(newEvent.GetContent(DEFAULT_CONTENT_KEY_FIRST_OBSERVED_TIME), "123456");
    APSARA_TEST_EQUAL_FATAL(newEvent.GetContent(DEFAULT_CONTENT_KEY_KEEP_ALIVE_SECONDS), "30");
    APSARA_TEST_EQUAL_FATAL(newEvent.GetContent(DEFAULT_CONTENT_KEY_METHOD), DEFAULT_CONTENT_VALUE_METHOD_UPDATE);
    APSARA_TEST_EQUAL_FATAL(newEvent.GetContent("pid"), "123");
    APSARA_TEST_EQUAL_FATAL(newEvent.GetContent("ppid"), "123");
    APSARA_TEST_EQUAL_FATAL(newEvent.GetContent("user"), "root");
    APSARA_TEST_EQUAL_FATAL(newEvent.GetContent("comm"), "comm");
    APSARA_TEST_EQUAL_FATAL(newEvent.GetContent("create_time"), "123456");
    APSARA_TEST_EQUAL_FATAL(newEvent.GetContent("cwd"), "cwd");
    APSARA_TEST_EQUAL_FATAL(newEvent.GetContent("binary"), "binary");
    APSARA_TEST_EQUAL_FATAL(newEvent.GetContent("arguments"), "arguments");
    APSARA_TEST_EQUAL_FATAL(newEvent.GetContent("language"), "language");
    APSARA_TEST_EQUAL_FATAL(newEvent.GetContent("containerID"), "container_id");
}

void ProcessorHostMetaNativeUnittest::TestGetProcessEntityID() {
    ProcessorHostMetaNative processor;
    processor.Init(Json::Value());
    processor.mHostEntityID = "123";
    APSARA_TEST_EQUAL(processor.GetProcessEntityID("123", "123"), "f5bb0c8de146c67b44babbf4e6584cc0");
}

UNIT_TEST_CASE(ProcessorHostMetaNativeUnittest, TestInit)
UNIT_TEST_CASE(ProcessorHostMetaNativeUnittest, TestProcess)
UNIT_TEST_CASE(ProcessorHostMetaNativeUnittest, TestGetProcessEntityID)

} // namespace logtail

UNIT_TEST_MAIN
