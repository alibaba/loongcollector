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

#include "constants/Constants.h"
#include "config/PipelineConfig.h"
#include "file_server/ConfigManager.h"
#include "pipeline/Pipeline.h"
#include "plugin/processor/inner/ProcessorTagNative.h"
#include "unittest/Unittest.h"
#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#endif

namespace logtail {

class ProcessorTagNativeUnittest : public ::testing::Test {
public:
    void TestInit();
    void TestProcess();

protected:
    void SetUp() override {
        mContext.SetConfigName("project##config_0");
        LogFileProfiler::GetInstance();
#ifdef __ENTERPRISE__
        EnterpriseConfigProvider::GetInstance()->SetUserDefinedIdSet(std::vector<std::string>{"machine_group"});
#endif
    }

private:
    PipelineContext mContext;
};

void ProcessorTagNativeUnittest::TestInit() {
    // make config
    Json::Value config;
    Pipeline pipeline;
    mContext.SetPipeline(pipeline);
    mContext.GetPipeline().mGoPipelineWithoutInput = Json::Value("test");

    {
        ProcessorTagNative processor;
        processor.SetContext(mContext);
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

    { // plugin branch
        Pipeline pipeline;
        mContext.SetPipeline(pipeline);
        mContext.GetPipeline().mGoPipelineWithoutInput = Json::Value("test");
        ProcessorTagNative processor;
        processor.SetContext(mContext);
        APSARA_TEST_TRUE_FATAL(processor.Init(config));

        processor.Process(eventGroup);
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(LOG_RESERVED_KEY_PATH));
        APSARA_TEST_EQUAL_FATAL(eventGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_PATH),
                                eventGroup.GetTag(LOG_RESERVED_KEY_PATH));
        APSARA_TEST_FALSE_FATAL(eventGroup.HasTag(LOG_RESERVED_KEY_HOSTNAME));
#ifdef __ENTERPRISE__
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(LOG_RESERVED_KEY_USER_DEFINED_ID));
        APSARA_TEST_EQUAL_FATAL(EnterpriseConfigProvider::GetInstance()->GetUserDefinedIdSet(),
                                eventGroup.GetTag(LOG_RESERVED_KEY_USER_DEFINED_ID));
#endif
    }

    { // native branch
        Pipeline pipeline;
        mContext.SetPipeline(pipeline);
        ProcessorTagNative processor;
        processor.SetContext(mContext);
        APSARA_TEST_TRUE_FATAL(processor.Init(config));

        processor.Process(eventGroup);
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(LOG_RESERVED_KEY_PATH));
        APSARA_TEST_EQUAL_FATAL(eventGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_PATH),
                                eventGroup.GetTag(LOG_RESERVED_KEY_PATH));
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(LOG_RESERVED_KEY_HOSTNAME));
        APSARA_TEST_EQUAL_FATAL(LogFileProfiler::mHostname, eventGroup.GetTag(LOG_RESERVED_KEY_HOSTNAME));
#ifdef __ENTERPRISE__
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(LOG_RESERVED_KEY_USER_DEFINED_ID));
        APSARA_TEST_EQUAL_FATAL(EnterpriseConfigProvider::GetInstance()->GetUserDefinedIdSet(),
                                eventGroup.GetTag(LOG_RESERVED_KEY_USER_DEFINED_ID));
#endif
    }
}

UNIT_TEST_CASE(ProcessorTagNativeUnittest, TestInit)
UNIT_TEST_CASE(ProcessorTagNativeUnittest, TestProcess)

} // namespace logtail

UNIT_TEST_MAIN
