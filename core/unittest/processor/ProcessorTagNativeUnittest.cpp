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

#include "common/Constants.h"
#include "config/Config.h"
#include "config_manager/ConfigManager.h"
#include "pipeline/Pipeline.h"
#include "processor/inner/ProcessorTagNative.h"
#include "unittest/Unittest.h"

namespace logtail {

class ProcessorTagNativeUnittest : public ::testing::Test {
public:
    static void SetUpTestCase() {}

    void SetUp() override { mContext.SetConfigName("project##config_0"); }

    void TestInit();
    void TestProcess();

    PipelineContext mContext;
};

UNIT_TEST_CASE(ProcessorTagNativeUnittest, TestInit);
UNIT_TEST_CASE(ProcessorTagNativeUnittest, TestProcess);

void ProcessorTagNativeUnittest::TestInit() {
    // make config
    Json::Value config;
    Pipeline pipeline;
    mContext.SetPipeline(pipeline);
    mContext.GetPipeline().mGoPipelineWithoutInput = Json::Value("test");

    {
        ProcessorTagNative processor;
        processor.SetContext(mContext);
        std::string pluginId = "testID";
        APSARA_TEST_TRUE_FATAL(processor.Init(config));
    }
}

void ProcessorTagNativeUnittest::TestProcess() {
    // make config
    Json::Value config;
    auto sourceBuffer = std::make_shared<logtail::SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string filePath = "/var/log/message";
    eventGroup.SetMetadataNoCopy(EventGroupMetaKey::LOG_FILE_PATH, filePath);
    std::string resolvedFilePath = "/run/var/log/message";
    eventGroup.SetMetadataNoCopy(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED, resolvedFilePath);
    std::string inode = "123456";
    eventGroup.SetMetadataNoCopy(EventGroupMetaKey::LOG_FILE_INODE, inode);
    std::string userDefinedId = "my-group";
    eventGroup.SetMetadataNoCopy(EventGroupMetaKey::AGENT_TAG, userDefinedId);
    std::string ip = "127.0.0.7";
    eventGroup.SetMetadataNoCopy(EventGroupMetaKey::HOST_IP, ip);
    std::string hostname = "my-machine";
    eventGroup.SetMetadataNoCopy(EventGroupMetaKey::HOST_NAME, hostname);

    { // test plugin branch
        Pipeline pipeline;
        mContext.SetPipeline(pipeline);
        mContext.GetPipeline().mGoPipelineWithoutInput = Json::Value("test");
        ProcessorTagNative processor;
        processor.SetContext(mContext);
        std::string pluginId = "testID";
        APSARA_TEST_TRUE_FATAL(processor.Init(config));
        processor.Process(eventGroup);
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(LOG_RESERVED_KEY_PATH));
        APSARA_TEST_EQUAL_FATAL(eventGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_PATH),
                                eventGroup.GetTag(LOG_RESERVED_KEY_PATH));
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(LOG_RESERVED_KEY_USER_DEFINED_ID));
        APSARA_TEST_EQUAL_FATAL(eventGroup.GetMetadata(EventGroupMetaKey::AGENT_TAG),
                                eventGroup.GetTag(LOG_RESERVED_KEY_USER_DEFINED_ID));
        APSARA_TEST_FALSE_FATAL(eventGroup.HasTag(LOG_RESERVED_KEY_HOSTNAME));
    }

    { // test native branch
        Pipeline pipeline;
        mContext.SetPipeline(pipeline);
        ProcessorTagNative processor;
        processor.SetContext(mContext);
        std::string pluginId = "testID";
        APSARA_TEST_TRUE_FATAL(processor.Init(config));
        processor.Process(eventGroup);
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(LOG_RESERVED_KEY_PATH));
        APSARA_TEST_EQUAL_FATAL(eventGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_PATH),
                                eventGroup.GetTag(LOG_RESERVED_KEY_PATH));
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(LOG_RESERVED_KEY_USER_DEFINED_ID));
        APSARA_TEST_EQUAL_FATAL(eventGroup.GetMetadata(EventGroupMetaKey::AGENT_TAG),
                                eventGroup.GetTag(LOG_RESERVED_KEY_USER_DEFINED_ID));
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(LOG_RESERVED_KEY_HOSTNAME));
        APSARA_TEST_EQUAL_FATAL(eventGroup.GetMetadata(EventGroupMetaKey::HOST_NAME),
                                eventGroup.GetTag(LOG_RESERVED_KEY_HOSTNAME));
    }
}

} // namespace logtail

UNIT_TEST_MAIN