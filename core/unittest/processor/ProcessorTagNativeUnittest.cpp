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

#include "AppConfig.h"
#include "TagConstants.h"
#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "common/JsonUtil.h"
#include "config/CollectionConfig.h"
#include "constants/Constants.h"
#include "file_server/ConfigManager.h"
#include "monitor/Monitor.h"
#include "plugin/processor/inner/ProcessorTagNative.h"
#include "sls_logs.pb.h"
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
        LoongCollectorMonitor::GetInstance();
        sls_logs::LogTag logTag;
        logTag.set_key("test_env_tag_key");
        logTag.set_value("test_env_tag_value");
        AppConfig::GetInstance()->mEnvTags.push_back(logTag);
#ifdef __ENTERPRISE__
        EnterpriseConfigProvider::GetInstance()->SetUserDefinedIdSet(std::vector<std::string>{"machine_group"});
#endif
    }

private:
};

void ProcessorTagNativeUnittest::TestInit() {
    // make config
    Json::Value config;
    CollectionPipeline pipeline;
    CollectionPipelineContext context;
    context.SetConfigName("project##config_0");
    context.SetPipeline(pipeline);
    context.GetPipeline().mGoPipelineWithoutInput = Json::Value("test");

    {
        ProcessorTagNative processor;
        processor.SetContext(context);
        APSARA_TEST_TRUE_FATAL(processor.Init(config));
    }
}

void ProcessorTagNativeUnittest::TestProcess() {
    { // native branch default
        Json::Value config;
        std::string configStr, errorMsg;
        configStr = R"(
            {
                "PipelineMetaTagKey": {}
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
        auto sourceBuffer = std::make_shared<logtail::SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string resolvedFilePath = "/run/var/log/message";
        eventGroup.SetMetadataNoCopy(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED, resolvedFilePath);
        CollectionPipeline pipeline;
        CollectionPipelineContext context;
        context.SetConfigName("project##config_0");
        context.SetPipeline(pipeline);
        Json::Value extendedParams;

        ProcessorTagNative processor;
        processor.SetContext(context);
        APSARA_TEST_TRUE_FATAL(processor.Init(config));

        processor.Process(eventGroup);

        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(GetDefaultTagKeyString(TagKey::HOST_NAME_TAG_KEY)));
        APSARA_TEST_EQUAL_FATAL(LoongCollectorMonitor::GetInstance()->mHostname,
                                eventGroup.GetTag(GetDefaultTagKeyString(TagKey::HOST_NAME_TAG_KEY)));
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag("test_env_tag_key"));
        APSARA_TEST_EQUAL_FATAL("test_env_tag_value", eventGroup.GetTag("test_env_tag_key"));
#ifdef __ENTERPRISE__
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(GetDefaultTagKeyString(TagKey::AGENT_TAG_TAG_KEY)));
        APSARA_TEST_EQUAL_FATAL(EnterpriseConfigProvider::GetInstance()->GetUserDefinedIdSet(),
                                eventGroup.GetTag(GetDefaultTagKeyString(TagKey::AGENT_TAG_TAG_KEY)));
#else
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(GetDefaultTagKeyString(TagKey::HOST_IP_TAG_KEY)));
        APSARA_TEST_EQUAL_FATAL(LoongCollectorMonitor::GetInstance()->mIpAddr,
                                eventGroup.GetTag(GetDefaultTagKeyString(TagKey::HOST_IP_TAG_KEY)));
#endif
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(LOG_RESERVED_KEY_SOURCE));
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(LOG_RESERVED_KEY_MACHINE_UUID));
    }
    { // native branch default
        Json::Value config;
        std::string configStr, errorMsg;
#ifdef __ENTERPRISE__
        configStr = R"(
            {
                "PipelineMetaTagKey": {
                    "HOST_NAME": "__default__",
                    "AGENT_TAG": "__default__"
                }
            }
        )";
#else
        configStr = R"(
            {
                "PipelineMetaTagKey": {
                    "HOST_NAME": "__default__",
                    "HOST_IP": "__default__"
                }
            }
        )";
#endif
        APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
        auto sourceBuffer = std::make_shared<logtail::SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string resolvedFilePath = "/run/var/log/message";
        eventGroup.SetMetadataNoCopy(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED, resolvedFilePath);
        CollectionPipeline pipeline;
        CollectionPipelineContext context;
        context.SetConfigName("project##config_0");
        context.SetPipeline(pipeline);
        Json::Value extendedParams;

        ProcessorTagNative processor;
        processor.SetContext(context);
        APSARA_TEST_TRUE_FATAL(processor.Init(config));

        processor.Process(eventGroup);
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(GetDefaultTagKeyString(TagKey::HOST_NAME_TAG_KEY)));
        APSARA_TEST_EQUAL_FATAL(LoongCollectorMonitor::GetInstance()->mHostname,
                                eventGroup.GetTag(GetDefaultTagKeyString(TagKey::HOST_NAME_TAG_KEY)));
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag("test_env_tag_key"));
        APSARA_TEST_EQUAL_FATAL("test_env_tag_value", eventGroup.GetTag("test_env_tag_key"));
#ifdef __ENTERPRISE__
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(GetDefaultTagKeyString(TagKey::AGENT_TAG_TAG_KEY)));
        APSARA_TEST_EQUAL_FATAL(EnterpriseConfigProvider::GetInstance()->GetUserDefinedIdSet(),
                                eventGroup.GetTag(GetDefaultTagKeyString(TagKey::AGENT_TAG_TAG_KEY)));
#else
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(GetDefaultTagKeyString(TagKey::HOST_IP_TAG_KEY)));
        APSARA_TEST_EQUAL_FATAL(LoongCollectorMonitor::GetInstance()->mIpAddr,
                                eventGroup.GetTag(GetDefaultTagKeyString(TagKey::HOST_IP_TAG_KEY)));
#endif
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(LOG_RESERVED_KEY_SOURCE));
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(LOG_RESERVED_KEY_MACHINE_UUID));
    }
    { // native branch rename
        Json::Value config;
        std::string configStr, errorMsg;
#ifdef __ENTERPRISE__
        configStr = R"(
            {
                "PipelineMetaTagKey": {
                    "HOST_NAME": "test_host_name",
                    "AGENT_TAG": "test_agent_tag"
                },
                "AgentEnvMetaTagKey": {
                    "test_env_tag_key": "test_env_tag_key_2"
                }
            }
        )";
#else
        configStr = R"(
            {
                "PipelineMetaTagKey": {
                    "HOST_NAME": "test_host_name",
                    "HOST_IP": "test_host_ip"
                },
                "AgentEnvMetaTagKey": {
                    "test_env_tag_key": "test_env_tag_key_2"
                }
            }
        )";
#endif
        APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
        auto sourceBuffer = std::make_shared<logtail::SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string resolvedFilePath = "/run/var/log/message";
        eventGroup.SetMetadataNoCopy(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED, resolvedFilePath);
        CollectionPipeline pipeline;
        CollectionPipelineContext context;
        context.SetConfigName("project##config_0");
        context.SetPipeline(pipeline);

        ProcessorTagNative processor;
        processor.SetContext(context);
        APSARA_TEST_TRUE_FATAL(processor.Init(config));

        processor.Process(eventGroup);
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag("test_host_name"));
        APSARA_TEST_EQUAL_FATAL(LoongCollectorMonitor::GetInstance()->mHostname, eventGroup.GetTag("test_host_name"));
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag("test_env_tag_key_2"));
        APSARA_TEST_EQUAL_FATAL("test_env_tag_value", eventGroup.GetTag("test_env_tag_key_2"));
#ifdef __ENTERPRISE__
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag("test_agent_tag"));
        APSARA_TEST_EQUAL_FATAL(EnterpriseConfigProvider::GetInstance()->GetUserDefinedIdSet(),
                                eventGroup.GetTag("test_agent_tag"));
#else
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag("test_host_ip"));
        APSARA_TEST_EQUAL_FATAL(LoongCollectorMonitor::GetInstance()->mIpAddr, eventGroup.GetTag("test_host_ip"));
#endif
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(LOG_RESERVED_KEY_SOURCE));
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(LOG_RESERVED_KEY_MACHINE_UUID));
    }
    { // native branch delete
        Json::Value config;
        std::string configStr, errorMsg;
#ifdef __ENTERPRISE__
        configStr = R"(
            {
                "PipelineMetaTagKey": {
                    "HOST_NAME": "",
                    "AGENT_TAG": ""
                },
                "AgentEnvMetaTagKey": {}
            }
        )";
#else
        configStr = R"(
            {
                "PipelineMetaTagKey": {
                    "HOST_NAME": "",
                    "HOST_IP": ""
                },
                "AgentEnvMetaTagKey": {}
            }
        )";
#endif
        APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
        auto sourceBuffer = std::make_shared<logtail::SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string resolvedFilePath = "/run/var/log/message";
        eventGroup.SetMetadataNoCopy(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED, resolvedFilePath);
        CollectionPipeline pipeline;
        CollectionPipelineContext context;
        context.SetConfigName("project##config_0");
        context.SetPipeline(pipeline);
        Json::Value extendedParams;
        context.InitGlobalConfig(config, extendedParams);

        ProcessorTagNative processor;
        processor.SetContext(context);
        APSARA_TEST_TRUE_FATAL(processor.Init(config));

        processor.Process(eventGroup);
        APSARA_TEST_FALSE_FATAL(eventGroup.HasTag(GetDefaultTagKeyString(TagKey::HOST_NAME_TAG_KEY)));
        APSARA_TEST_FALSE_FATAL(eventGroup.HasTag("test_env_tag_key"));
#ifdef __ENTERPRISE__
        APSARA_TEST_FALSE_FATAL(eventGroup.HasTag(GetDefaultTagKeyString(TagKey::AGENT_TAG_TAG_KEY)));
#else
        APSARA_TEST_FALSE_FATAL(eventGroup.HasTag(GetDefaultTagKeyString(TagKey::HOST_IP_TAG_KEY)));
#endif
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(LOG_RESERVED_KEY_SOURCE));
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(LOG_RESERVED_KEY_MACHINE_UUID));
    }
}

UNIT_TEST_CASE(ProcessorTagNativeUnittest, TestInit)
UNIT_TEST_CASE(ProcessorTagNativeUnittest, TestProcess)

} // namespace logtail

UNIT_TEST_MAIN
