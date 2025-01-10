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

#include "TagConstants.h"
#include "common/JsonUtil.h"
#include "config/PipelineConfig.h"
#include "constants/Constants.h"
#include "file_server/ConfigManager.h"
#include "monitor/Monitor.h"
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
        LoongCollectorMonitor::GetInstance();
#ifdef __ENTERPRISE__
        EnterpriseConfigProvider::GetInstance()->SetUserDefinedIdSet(std::vector<std::string>{"machine_group"});
#endif
    }

private:
};

void ProcessorTagNativeUnittest::TestInit() {
    // make config
    Json::Value config;
    Pipeline pipeline;
    PipelineContext context;
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
    { // plugin branch default
        Json::Value processorConfig;
        Json::Value config;
        std::string configStr, errorMsg;
#ifdef __ENTERPRISE__
        configStr = R"(
            {
                "PipelineMetaTagKey": {},
                "AgentEnvMetaTagKey": {}
            }
        )";
#else
        configStr = R"(
            {
                "PipelineMetaTagKey": {}
            }
        )";
#endif
        APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
        auto sourceBuffer = std::make_shared<logtail::SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inode = "123456";
        eventGroup.SetMetadataNoCopy(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED, inode);
        Pipeline pipeline;
        PipelineContext context;
        context.SetConfigName("project##config_0");
        context.SetPipeline(pipeline);
        context.GetPipeline().mGoPipelineWithoutInput = Json::Value("test");
        Json::Value extendedParams;
        context.InitGlobalConfig(config, extendedParams);

        ProcessorTagNative processor;
        processor.SetContext(context);
        APSARA_TEST_TRUE_FATAL(processor.Init(processorConfig));

        processor.Process(eventGroup);
#ifdef __ENTERPRISE__
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(AGENT_TAG_DEFAULT_KEY));
        APSARA_TEST_EQUAL_FATAL(EnterpriseConfigProvider::GetInstance()->GetUserDefinedIdSet(),
                                eventGroup.GetTag(AGENT_TAG_DEFAULT_KEY));
#else
#endif
    }
    { // plugin branch default
        Json::Value processorConfig;
        Json::Value config;
        std::string configStr, errorMsg;
#ifdef __ENTERPRISE__
        configStr = R"(
            {
                "PipelineMetaTagKey": {
                    "HOST_NAME": "__default__",
                    "AGENT_TAG": "__default__"
                },
                "AgentEnvMetaTagKey": {}
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
        Pipeline pipeline;
        PipelineContext context;
        context.SetConfigName("project##config_0");
        context.SetPipeline(pipeline);
        context.GetPipeline().mGoPipelineWithoutInput = Json::Value("test");
        Json::Value extendedParams;
        context.InitGlobalConfig(config, extendedParams);

        ProcessorTagNative processor;
        processor.SetContext(context);
        APSARA_TEST_TRUE_FATAL(processor.Init(processorConfig));

        processor.Process(eventGroup);
#ifdef __ENTERPRISE__
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(AGENT_TAG_DEFAULT_KEY));
        APSARA_TEST_EQUAL_FATAL(EnterpriseConfigProvider::GetInstance()->GetUserDefinedIdSet(),
                                eventGroup.GetTag(AGENT_TAG_DEFAULT_KEY));
#else
#endif
    }
    { // plugin branch rename
        Json::Value processorConfig;
        Json::Value config;
        std::string configStr, errorMsg;
#ifdef __ENTERPRISE__
        configStr = R"(
            {
                "PipelineMetaTagKey": {
                    "HOST_NAME": "test_host_name",
                    "AGENT_TAG": "test_agent_tag"
                },
                "AgentEnvMetaTagKey": {}
            }
        )";
#else
        configStr = R"(
            {
                "PipelineMetaTagKey": {
                    "HOST_NAME": "test_host_name",
                    "HOST_IP": "test_host_ip"
                }
            }
        )";
#endif
        APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
        auto sourceBuffer = std::make_shared<logtail::SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string resolvedFilePath = "/run/var/log/message";
        eventGroup.SetMetadataNoCopy(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED, resolvedFilePath);
        Pipeline pipeline;
        PipelineContext context;
        context.SetConfigName("project##config_0");
        context.SetPipeline(pipeline);
        context.GetPipeline().mGoPipelineWithoutInput = Json::Value("test");
        Json::Value extendedParams;
        context.InitGlobalConfig(config, extendedParams);

        ProcessorTagNative processor;
        processor.SetContext(context);
        APSARA_TEST_TRUE_FATAL(processor.Init(processorConfig));

        processor.Process(eventGroup);
#ifdef __ENTERPRISE__
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag("test_agent_tag"));
        APSARA_TEST_EQUAL_FATAL(EnterpriseConfigProvider::GetInstance()->GetUserDefinedIdSet(),
                                eventGroup.GetTag("test_agent_tag"));
#else
#endif
    }
    { // plugin branch delete
        Json::Value processorConfig;
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
                }
            }
        )";
#endif
        APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
        auto sourceBuffer = std::make_shared<logtail::SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string resolvedFilePath = "/run/var/log/message";
        eventGroup.SetMetadataNoCopy(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED, resolvedFilePath);
        Pipeline pipeline;
        PipelineContext context;
        context.SetConfigName("project##config_0");
        context.SetPipeline(pipeline);
        context.GetPipeline().mGoPipelineWithoutInput = Json::Value("test");
        Json::Value extendedParams;
        context.InitGlobalConfig(config, extendedParams);

        ProcessorTagNative processor;
        processor.SetContext(context);
        APSARA_TEST_TRUE_FATAL(processor.Init(processorConfig));

        processor.Process(eventGroup);
#ifdef __ENTERPRISE__
        APSARA_TEST_FALSE_FATAL(eventGroup.HasTag("test_agent_tag"));
#else
#endif
    }
    { // native branch default
        Json::Value processorConfig;
        Json::Value config;
        std::string configStr, errorMsg;
#ifdef __ENTERPRISE__
        configStr = R"(
            {
                "PipelineMetaTagKey": {},
                "AgentEnvMetaTagKey": {}
            }
        )";
#else
        configStr = R"(
            {
                "PipelineMetaTagKey": {}
            }
        )";
#endif
        APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
        auto sourceBuffer = std::make_shared<logtail::SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string resolvedFilePath = "/run/var/log/message";
        eventGroup.SetMetadataNoCopy(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED, resolvedFilePath);
        Pipeline pipeline;
        PipelineContext context;
        context.SetConfigName("project##config_0");
        context.SetPipeline(pipeline);
        Json::Value extendedParams;
        context.InitGlobalConfig(config, extendedParams);

        ProcessorTagNative processor;
        processor.SetContext(context);
        APSARA_TEST_TRUE_FATAL(processor.Init(processorConfig));

        processor.Process(eventGroup);

        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(TagKeyToString(TagKey::HOST_NAME)));
        APSARA_TEST_EQUAL_FATAL(LoongCollectorMonitor::GetInstance()->mHostname,
                                eventGroup.GetTag(TagKeyToString(TagKey::HOST_NAME)));
#ifdef __ENTERPRISE__
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(TagKeyToString(TagKey::AGENT_TAG)));
        APSARA_TEST_EQUAL_FATAL(EnterpriseConfigProvider::GetInstance()->GetUserDefinedIdSet(),
                                eventGroup.GetTag(TagKeyToString(TagKey::AGENT_TAG)));
#else
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(TagKeyToString(TagKey::HOST_IP)));
        APSARA_TEST_EQUAL_FATAL(LoongCollectorMonitor::GetInstance()->mIpAddr,
                                eventGroup.GetTag(TagKeyToString(TagKey::HOST_IP)));
#endif
    }
    { // native branch default
        Json::Value processorConfig;
        Json::Value config;
        std::string configStr, errorMsg;
#ifdef __ENTERPRISE__
        configStr = R"(
            {
                "PipelineMetaTagKey": {
                    "HOST_NAME": "__default__",
                    "AGENT_TAG": "__default__"
                },
                "AgentEnvMetaTagKey": {}
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
        Pipeline pipeline;
        PipelineContext context;
        context.SetConfigName("project##config_0");
        context.SetPipeline(pipeline);
        Json::Value extendedParams;
        context.InitGlobalConfig(config, extendedParams);

        ProcessorTagNative processor;
        processor.SetContext(context);
        APSARA_TEST_TRUE_FATAL(processor.Init(processorConfig));

        processor.Process(eventGroup);
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(TagKeyToString(TagKey::HOST_NAME)));
        APSARA_TEST_EQUAL_FATAL(LoongCollectorMonitor::GetInstance()->mHostname,
                                eventGroup.GetTag(TagKeyToString(TagKey::HOST_NAME)));
#ifdef __ENTERPRISE__
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(TagKeyToString(TagKey::AGENT_TAG)));
        APSARA_TEST_EQUAL_FATAL(EnterpriseConfigProvider::GetInstance()->GetUserDefinedIdSet(),
                                eventGroup.GetTag(TagKeyToString(TagKey::AGENT_TAG)));
#else
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag(TagKeyToString(TagKey::HOST_IP)));
        APSARA_TEST_EQUAL_FATAL(LoongCollectorMonitor::GetInstance()->mIpAddr,
                                eventGroup.GetTag(TagKeyToString(TagKey::HOST_IP)));
#endif
    }
    { // native branch rename
        Json::Value processorConfig;
        Json::Value config;
        std::string configStr, errorMsg;
#ifdef __ENTERPRISE__
        configStr = R"(
            {
                "PipelineMetaTagKey": {
                    "HOST_NAME": "test_host_name",
                    "AGENT_TAG": "test_agent_tag"
                },
                "AgentEnvMetaTagKey": {}
            }
        )";
#else
        configStr = R"(
            {
                "PipelineMetaTagKey": {
                    "HOST_NAME": "test_host_name",
                    "HOST_IP": "test_host_ip"
                }
            }
        )";
#endif
        APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
        auto sourceBuffer = std::make_shared<logtail::SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string resolvedFilePath = "/run/var/log/message";
        eventGroup.SetMetadataNoCopy(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED, resolvedFilePath);
        Pipeline pipeline;
        PipelineContext context;
        context.SetConfigName("project##config_0");
        context.SetPipeline(pipeline);
        Json::Value extendedParams;
        context.InitGlobalConfig(config, extendedParams);

        ProcessorTagNative processor;
        processor.SetContext(context);
        APSARA_TEST_TRUE_FATAL(processor.Init(processorConfig));

        processor.Process(eventGroup);
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag("test_host_name"));
        APSARA_TEST_EQUAL_FATAL(LoongCollectorMonitor::GetInstance()->mHostname, eventGroup.GetTag("test_host_name"));
#ifdef __ENTERPRISE__
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag("test_agent_tag"));
        APSARA_TEST_EQUAL_FATAL(EnterpriseConfigProvider::GetInstance()->GetUserDefinedIdSet(),
                                eventGroup.GetTag("test_agent_tag"));
#else
        APSARA_TEST_TRUE_FATAL(eventGroup.HasTag("test_host_ip"));
        APSARA_TEST_EQUAL_FATAL(LoongCollectorMonitor::GetInstance()->mIpAddr, eventGroup.GetTag("test_host_ip"));
#endif
    }
    { // native branch delete
        Json::Value processorConfig;
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
                }
            }
        )";
#endif
        APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
        auto sourceBuffer = std::make_shared<logtail::SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string resolvedFilePath = "/run/var/log/message";
        eventGroup.SetMetadataNoCopy(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED, resolvedFilePath);
        Pipeline pipeline;
        PipelineContext context;
        context.SetConfigName("project##config_0");
        context.SetPipeline(pipeline);
        Json::Value extendedParams;
        context.InitGlobalConfig(config, extendedParams);

        ProcessorTagNative processor;
        processor.SetContext(context);
        APSARA_TEST_TRUE_FATAL(processor.Init(processorConfig));

        processor.Process(eventGroup);
        APSARA_TEST_FALSE_FATAL(eventGroup.HasTag(TagKeyToString(TagKey::HOST_NAME)));
#ifdef __ENTERPRISE__
        APSARA_TEST_FALSE_FATAL(eventGroup.HasTag(TagKeyToString(TagKey::AGENT_TAG)));
#else
        APSARA_TEST_FALSE_FATAL(eventGroup.HasTag(TagKeyToString(TagKey::HOST_IP)));
#endif
    }
}

UNIT_TEST_CASE(ProcessorTagNativeUnittest, TestInit)
UNIT_TEST_CASE(ProcessorTagNativeUnittest, TestProcess)

} // namespace logtail

UNIT_TEST_MAIN
