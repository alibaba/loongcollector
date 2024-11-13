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

#include <json/json.h>

#include <future>
#include <memory>
#include <string>
#include <thread>

#include "app_config/AppConfig.h"
#include "common/JsonUtil.h"
#include "config/PipelineConfig.h"
#include "pipeline/Pipeline.h"
#include "pipeline/batch/TimeoutFlushManager.h"
#include "pipeline/plugin/PluginRegistry.h"
#include "pipeline/queue/BoundedProcessQueue.h"
#include "pipeline/queue/ProcessQueueManager.h"
#include "pipeline/queue/QueueKeyManager.h"
#include "plugin/input/InputFeedbackInterfaceRegistry.h"
#include "plugin/processor/inner/ProcessorSplitLogStringNative.h"
#include "plugin/processor/inner/ProcessorSplitMultilineLogStringNative.h"
#include "unittest/Unittest.h"
#include "unittest/plugin/PluginMock.h"

using namespace std;

namespace logtail {

class PipelineUnittest : public ::testing::Test {
public:
    void OnSuccessfulInit() const;
    void OnFailedInit() const;
    void OnInitVariousTopology() const;
    void TestProcessQueue() const;
    void OnInputFileWithJsonMultiline() const;
    void OnInputFileWithContainerDiscovery() const;
    void TestProcess() const;
    void TestSend() const;
    void TestFlushBatch() const;
    void TestInProcessingCount() const;
    void TestWaitAllItemsInProcessFinished() const;

protected:
    static void SetUpTestCase() {
        PluginRegistry::GetInstance()->LoadPlugins();
        LoadPluginMock();
        InputFeedbackInterfaceRegistry::GetInstance()->LoadFeedbackInterfaces();
        AppConfig::GetInstance()->mPurageContainerMode = true;
    }

    static void TearDownTestCase() { PluginRegistry::GetInstance()->UnloadPlugins(); }

    void TearDown() override {
        TimeoutFlushManager::GetInstance()->mTimeoutRecords.clear();
        QueueKeyManager::GetInstance()->Clear();
        ProcessQueueManager::GetInstance()->Clear();
    }

    unique_ptr<ProcessQueueItem> GenerateProcessItem(shared_ptr<Pipeline> pipeline) const {
        PipelineEventGroup eventGroup(make_shared<SourceBuffer>());
        auto item = make_unique<ProcessQueueItem>(std::move(eventGroup), 0);
        item->mPipeline = pipeline;
        return item;
    }

private:
    const string configName = "test_config";
};

void PipelineUnittest::OnSuccessfulInit() const {
    unique_ptr<Json::Value> configJson;
    Json::Value goPipelineWithInput, goPipelineWithoutInput;
    string configStr, goPipelineWithInputStr, goPipelineWithoutInputStr, errorMsg;
    unique_ptr<PipelineConfig> config;
    unique_ptr<Pipeline> pipeline;

    // with sls flusher
    configStr = R"(
        {
            "createTime": 123456789,
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(configName, pipeline->Name());
    APSARA_TEST_EQUAL(configName, pipeline->GetContext().GetConfigName());
    APSARA_TEST_EQUAL(123456789U, pipeline->GetContext().GetCreateTime());
    APSARA_TEST_EQUAL("test_project", pipeline->GetContext().GetProjectName());
    APSARA_TEST_EQUAL("test_logstore", pipeline->GetContext().GetLogstoreName());
    APSARA_TEST_EQUAL("test_region", pipeline->GetContext().GetRegion());
    APSARA_TEST_EQUAL(QueueKeyManager::GetInstance()->GetKey("test_config-flusher_sls-test_project#test_logstore"),
                      pipeline->GetContext().GetLogstoreKey());
    APSARA_TEST_EQUAL(0, pipeline->mInProcessCnt.load());
    APSARA_TEST_EQUAL(2U, pipeline->mMetricsRecordRef->GetLabels()->size());
    APSARA_TEST_TRUE(pipeline->mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_PIPELINE_NAME, configName));
    APSARA_TEST_TRUE(pipeline->mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_PROJECT, "test_project"));

    // without sls flusher
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(configName, pipeline->Name());
    APSARA_TEST_EQUAL(configName, pipeline->GetContext().GetConfigName());
    APSARA_TEST_EQUAL(0U, pipeline->GetContext().GetCreateTime());
    APSARA_TEST_EQUAL("", pipeline->GetContext().GetProjectName());
    APSARA_TEST_EQUAL("", pipeline->GetContext().GetLogstoreName());
    APSARA_TEST_EQUAL("", pipeline->GetContext().GetRegion());
    APSARA_TEST_EQUAL(0, pipeline->mInProcessCnt.load());
#ifndef __ENTERPRISE__
    APSARA_TEST_EQUAL(QueueKeyManager::GetInstance()->GetKey("test_config-flusher_sls-"),
                      pipeline->GetContext().GetLogstoreKey());
#endif

    // extensions and extended global param
    configStr = R"(
        {
            "global": {
                "DefaultLogGroupQueueSize": 3,
                "DefaultLogQueueSize": 5
            },
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ],
                    "EnableContainerDiscovery": true,
                    "CollectingContainersMeta": true
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_kafka_v2"
                }
            ],
            "extensions": [
                {
                    "Type": "ext_basicauth"
                }
            ]
        }
    )";
    goPipelineWithInputStr = R"(
        {
            "global" : {
                "AlwaysOnline": true,
                "EnableTimestampNanosecond": false,
                "UsingOldContentTag": false,
                "DefaultLogQueueSize" : 5,
                "DefaultLogGroupQueueSize": 3
            },
            "inputs": [
                {
                    "type": "metric_container_info/2",
                    "detail": {
                        "CollectingContainersMeta": true,
                        "LogPath": "/home",
                        "MaxDepth": 0,
                        "FilePattern": "test.log"
                    }
                }
            ],
            "extensions": [
                {
                    "type": "ext_basicauth/7",
                    "detail": {}
                }
            ]
        }
    )";
    goPipelineWithoutInputStr = R"(
        {
            "global" : {
                "EnableTimestampNanosecond": false,
                "UsingOldContentTag": false,
                "DefaultLogQueueSize" : 10,
                "DefaultLogGroupQueueSize": 3
            },
            "aggregators": [
                {
                    "type": "aggregator_default/5",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_kafka_v2/6",
                    "detail": {}
                }
            ],
            "extensions": [
                {
                    "type": "ext_basicauth/7",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithInputStr, goPipelineWithInput, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithoutInputStr, goPipelineWithoutInput, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(goPipelineWithInput.toStyledString(), pipeline->mGoPipelineWithInput.toStyledString());
    APSARA_TEST_EQUAL(goPipelineWithoutInput.toStyledString(), pipeline->mGoPipelineWithoutInput.toStyledString());
    APSARA_TEST_EQUAL(0, pipeline->mInProcessCnt.load());
    goPipelineWithInput.clear();
    goPipelineWithoutInput.clear();

    // router
    configStr = R"(
        {
            "createTime": 123456789,
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore_1",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint"
                },
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore_2",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "Match": {
                        "Type": "event_type",
                        "Value": "log"
                    }
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1U, pipeline->mRouter.mConditions.size());
    APSARA_TEST_EQUAL(1U, pipeline->mRouter.mAlwaysMatchedFlusherIdx.size());
    APSARA_TEST_EQUAL(0, pipeline->mInProcessCnt.load());
}

void PipelineUnittest::OnFailedInit() const {
    unique_ptr<Json::Value> configJson;
    string configStr, errorMsg;
    unique_ptr<PipelineConfig> config;
    unique_ptr<Pipeline> pipeline;

    // invalid input
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_FALSE(pipeline->Init(std::move(*config)));

    // invalid processor
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_FALSE(pipeline->Init(std::move(*config)));

    // invalid flusher
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_FALSE(pipeline->Init(std::move(*config)));

    // invalid router
    configStr = R"(
        {
            "createTime": 123456789,
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore_1",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "Match": "unknown"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_FALSE(pipeline->Init(std::move(*config)));

    // invalid inputs ack support
    configStr = R"(
        {
            "createTime": 123456789,
            "inputs": [
                {
                    "Type": "input_mock"
                },
                {
                    "Type": "input_mock",
                    "SupportAck": false
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore_1",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "Match": "unknown"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_FALSE(pipeline->Init(std::move(*config)));
}

void PipelineUnittest::OnInitVariousTopology() const {
    unique_ptr<Json::Value> configJson;
    Json::Value goPipelineWithInput, goPipelineWithoutInput;
    string configStr, goPipelineWithInputStr, goPipelineWithoutInputStr, errorMsg;
    unique_ptr<PipelineConfig> config;
    unique_ptr<Pipeline> pipeline;

    // topology 1: native -> native -> native
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native",
                    "SourceKey": "content",
                    "Regex": ".*",
                    "Keys": ["key"]
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1U, pipeline->mInputs.size());
    APSARA_TEST_EQUAL(1U, pipeline->mProcessorLine.size());
    APSARA_TEST_EQUAL(1U, pipeline->GetFlushers().size());
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithInput.isNull());
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithoutInput.isNull());

    // topology 2: extended -> native -> native
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native",
                    "SourceKey": "content",
                    "Regex": ".*",
                    "Keys": ["key"]
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // topology 3: (native, extended) -> native -> native
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                },
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native",
                    "SourceKey": "content",
                    "Regex": ".*",
                    "Keys": ["key"]
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // topology 4: native -> extended -> native
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                }
            ],
            "processors": [
                {
                    "Type": "processor_regex"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                }
            ]
        }
    )";
    goPipelineWithoutInputStr = R"(
        {
            "global" : {
                "EnableTimestampNanosecond": false,
                "UsingOldContentTag": false,
                "DefaultLogQueueSize" : 10
            },
            "processors": [
                {
                    "type": "processor_regex/4",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_default/5",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_sls/6",
                    "detail": {
                        "EnableShardHash": false
                    }
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithoutInputStr, goPipelineWithoutInput, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1U, pipeline->mInputs.size());
    APSARA_TEST_EQUAL(0U, pipeline->mProcessorLine.size());
    APSARA_TEST_EQUAL(1U, pipeline->GetFlushers().size());
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithInput.isNull());
    APSARA_TEST_EQUAL(goPipelineWithoutInput.toStyledString(), pipeline->mGoPipelineWithoutInput.toStyledString());
    goPipelineWithoutInput.clear();

    // topology 5: extended -> extended -> native
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_regex"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                }
            ]
        }
    )";
    goPipelineWithInputStr = R"(
        {
            "global": {
                "EnableTimestampNanosecond": false,
                "UsingOldContentTag": false
            },
            "inputs": [
                {
                    "type": "service_docker_stdout/1",
                    "detail": {}
                }
            ],
            "processors": [
                {
                    "type": "processor_regex/2",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_default/3",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_sls/4",
                    "detail": {
                        "EnableShardHash": false
                    }
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithInputStr, goPipelineWithInput, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(0U, pipeline->mInputs.size());
    APSARA_TEST_EQUAL(0U, pipeline->mProcessorLine.size());
    APSARA_TEST_EQUAL(1U, pipeline->GetFlushers().size());
    APSARA_TEST_EQUAL(goPipelineWithInput.toStyledString(), pipeline->mGoPipelineWithInput.toStyledString());
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithoutInput.isNull());
    goPipelineWithInput.clear();

    // topology 6: (native, extended) -> extended -> native
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                },
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_regex"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // topology 7: native -> (native -> extended) -> native
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native",
                    "SourceKey": "content",
                    "Regex": ".*",
                    "Keys": ["key"]
                },
                {
                    "Type": "processor_regex"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                }
            ]
        }
    )";
    goPipelineWithoutInputStr = R"(
        {
            "global" : {
                "EnableTimestampNanosecond": false,
                "UsingOldContentTag": false,
                "DefaultLogQueueSize" : 10
            },
            "processors": [
                {
                    "type": "processor_regex/5",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_default/6",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_sls/7",
                    "detail": {
                        "EnableShardHash": false
                    }
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithoutInputStr, goPipelineWithoutInput, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1U, pipeline->mInputs.size());
    APSARA_TEST_EQUAL(1U, pipeline->mProcessorLine.size());
    APSARA_TEST_EQUAL(1U, pipeline->GetFlushers().size());
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithInput.isNull());
    APSARA_TEST_EQUAL(goPipelineWithoutInput.toStyledString(), pipeline->mGoPipelineWithoutInput.toStyledString());
    goPipelineWithoutInput.clear();

    // topology 8: extended -> (native -> extended) -> native
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native",
                    "SourceKey": "content",
                    "Regex": ".*",
                    "Keys": ["key"]
                },
                {
                    "Type": "processor_regex"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // topology 9: (native, extended) -> (native -> extended) -> native
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                },
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native",
                    "SourceKey": "content",
                    "Regex": ".*",
                    "Keys": ["key"]
                },
                {
                    "Type": "processor_regex"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // topology 10: native -> none -> native
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1U, pipeline->mInputs.size());
    APSARA_TEST_EQUAL(0U, pipeline->mProcessorLine.size());
    APSARA_TEST_EQUAL(1U, pipeline->GetFlushers().size());
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithInput.isNull());
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithoutInput.isNull());

    // topology 11: extended -> none -> native (future changes maybe applied)
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                }
            ]
        }
    )";
    goPipelineWithInputStr = R"(
        {
            "global": {
                "EnableTimestampNanosecond": false,
                "UsingOldContentTag": false
            },
            "inputs": [
                {
                    "type": "service_docker_stdout/1",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_default/2",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_sls/3",
                    "detail": {
                        "EnableShardHash": false
                    }
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithInputStr, goPipelineWithInput, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(0U, pipeline->mInputs.size());
    APSARA_TEST_EQUAL(0U, pipeline->mProcessorLine.size());
    APSARA_TEST_EQUAL(1U, pipeline->GetFlushers().size());
    APSARA_TEST_EQUAL(goPipelineWithInput.toStyledString(), pipeline->mGoPipelineWithInput.toStyledString());
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithoutInput.isNull());
    goPipelineWithInput.clear();

    // topology 12: (native, extended) -> none -> native
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                },
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // topology 13: native -> native -> extended
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native",
                    "SourceKey": "content",
                    "Regex": ".*",
                    "Keys": ["key"]
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    goPipelineWithoutInputStr = R"(
        {
            "global" : {
                "EnableTimestampNanosecond": false,
                "UsingOldContentTag": false,
                "DefaultLogQueueSize" : 10
            },
            "aggregators": [
                {
                    "type": "aggregator_default/5",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_kafka_v2/6",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithoutInputStr, goPipelineWithoutInput, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1U, pipeline->mInputs.size());
    APSARA_TEST_EQUAL(1U, pipeline->mProcessorLine.size());
    APSARA_TEST_EQUAL(0U, pipeline->GetFlushers().size());
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithInput.isNull());
    APSARA_TEST_EQUAL(goPipelineWithoutInput.toStyledString(), pipeline->mGoPipelineWithoutInput.toStyledString());
    goPipelineWithoutInput.clear();

    // topology 14: extended -> native -> extended
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native",
                    "SourceKey": "content",
                    "Regex": ".*",
                    "Keys": ["key"]
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // topology 15: (native, extended) -> native -> extended
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                },
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native",
                    "SourceKey": "content",
                    "Regex": ".*",
                    "Keys": ["key"]
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // topology 16: native -> extended -> extended
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                }
            ],
            "processors": [
                {
                    "Type": "processor_regex"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    goPipelineWithoutInputStr = R"(
        {
            "global" : {
                "EnableTimestampNanosecond": false,
                "UsingOldContentTag": false,
                "DefaultLogQueueSize" : 10
            },
            "processors": [
                {
                    "type": "processor_regex/4",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_default/5",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_kafka_v2/6",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithoutInputStr, goPipelineWithoutInput, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1U, pipeline->mInputs.size());
    APSARA_TEST_EQUAL(0U, pipeline->mProcessorLine.size());
    APSARA_TEST_EQUAL(0U, pipeline->GetFlushers().size());
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithInput.isNull());
    APSARA_TEST_EQUAL(goPipelineWithoutInput.toStyledString(), pipeline->mGoPipelineWithoutInput.toStyledString());
    goPipelineWithoutInput.clear();

    // topology 17: extended -> extended -> extended
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_regex"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    goPipelineWithInputStr = R"(
        {
            "global": {
                "EnableTimestampNanosecond": false,
                "UsingOldContentTag": false
            },
            "inputs": [
                {
                    "type": "service_docker_stdout/1",
                    "detail": {}
                }
            ],
            "processors": [
                {
                    "type": "processor_regex/2",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_default/3",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_kafka_v2/4",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithInputStr, goPipelineWithInput, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(0U, pipeline->mInputs.size());
    APSARA_TEST_EQUAL(0U, pipeline->mProcessorLine.size());
    APSARA_TEST_EQUAL(0U, pipeline->GetFlushers().size());
    APSARA_TEST_EQUAL(goPipelineWithInput.toStyledString(), pipeline->mGoPipelineWithInput.toStyledString());
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithoutInput.isNull());
    goPipelineWithInput.clear();

    // topology 18: (native, extended) -> extended -> extended
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                },
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_regex"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // topology 19: native -> (native -> extended) -> extended
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native",
                    "SourceKey": "content",
                    "Regex": ".*",
                    "Keys": ["key"]
                },
                {
                    "Type": "processor_regex"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    goPipelineWithoutInputStr = R"(
        {
            "global" : {
                "EnableTimestampNanosecond": false,
                "UsingOldContentTag": false,
                "DefaultLogQueueSize" : 10
            },
            "processors": [
                {
                    "type": "processor_regex/5",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_default/6",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_kafka_v2/7",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithoutInputStr, goPipelineWithoutInput, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1U, pipeline->mInputs.size());
    APSARA_TEST_EQUAL(1U, pipeline->mProcessorLine.size());
    APSARA_TEST_EQUAL(0U, pipeline->GetFlushers().size());
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithInput.isNull());
    APSARA_TEST_EQUAL(goPipelineWithoutInput.toStyledString(), pipeline->mGoPipelineWithoutInput.toStyledString());
    goPipelineWithoutInput.clear();

    // topology 20: extended -> (native -> extended) -> extended
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native",
                    "SourceKey": "content",
                    "Regex": ".*",
                    "Keys": ["key"]
                },
                {
                    "Type": "processor_regex"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // topology 21: (native, extended) -> (native -> extended) -> extended
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                },
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native",
                    "SourceKey": "content",
                    "Regex": ".*",
                    "Keys": ["key"]
                },
                {
                    "Type": "processor_regex"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // topology 22: native -> none -> extended
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    goPipelineWithoutInputStr = R"(
        {
            "global" : {
                "EnableTimestampNanosecond": false,
                "UsingOldContentTag": false,
                "DefaultLogQueueSize" : 10
            },
            "aggregators": [
                {
                    "type": "aggregator_default/4",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_kafka_v2/5",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithoutInputStr, goPipelineWithoutInput, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1U, pipeline->mInputs.size());
    APSARA_TEST_EQUAL(0U, pipeline->mProcessorLine.size());
    APSARA_TEST_EQUAL(0U, pipeline->GetFlushers().size());
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithInput.isNull());
    APSARA_TEST_EQUAL(goPipelineWithoutInput.toStyledString(), pipeline->mGoPipelineWithoutInput.toStyledString());
    goPipelineWithoutInput.clear();

    // topology 23: extended -> none -> extended
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    goPipelineWithInputStr = R"(
        {
            "global": {
                "EnableTimestampNanosecond": false,
                "UsingOldContentTag": false
            },
            "inputs": [
                {
                    "type": "service_docker_stdout/1",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_default/2",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_kafka_v2/3",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithInputStr, goPipelineWithInput, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(0U, pipeline->mInputs.size());
    APSARA_TEST_EQUAL(0U, pipeline->mProcessorLine.size());
    APSARA_TEST_EQUAL(0U, pipeline->GetFlushers().size());
    APSARA_TEST_EQUAL(goPipelineWithInput.toStyledString(), pipeline->mGoPipelineWithInput.toStyledString());
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithoutInput.isNull());
    goPipelineWithInput.clear();

    // topology 24: (native, extended) -> none -> extended
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                },
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // topology 25: native -> native -> (native, extended) (future changes maybe applied)
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native",
                    "SourceKey": "content",
                    "Regex": ".*",
                    "Keys": ["key"]
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                },
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    goPipelineWithoutInputStr = R"(
        {
            "global" : {
                "EnableTimestampNanosecond": false,
                "UsingOldContentTag": false,
                "DefaultLogQueueSize" : 10
            },
            "aggregators": [
                {
                    "type": "aggregator_default/5",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_sls/6",
                    "detail": {
                        "EnableShardHash": false
                    }
                },
                {
                    "type": "flusher_kafka_v2/7",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithoutInputStr, goPipelineWithoutInput, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1U, pipeline->mInputs.size());
    APSARA_TEST_EQUAL(1U, pipeline->mProcessorLine.size());
    APSARA_TEST_EQUAL(1U, pipeline->GetFlushers().size());
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithInput.isNull());
    APSARA_TEST_EQUAL(goPipelineWithoutInput.toStyledString(), pipeline->mGoPipelineWithoutInput.toStyledString());
    goPipelineWithoutInput.clear();

    // topology 26: extended -> native -> (native, extended)
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native",
                    "SourceKey": "content",
                    "Regex": ".*",
                    "Keys": ["key"]
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                },
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // topology 27: (native, extended) -> native -> (native, extended)
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                },
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native",
                    "SourceKey": "content",
                    "Regex": ".*",
                    "Keys": ["key"]
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                },
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // topology 28: native -> extended -> (native, extended)
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                }
            ],
            "processors": [
                {
                    "Type": "processor_regex"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                },
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    goPipelineWithoutInputStr = R"(
        {
            "global" : {
                "EnableTimestampNanosecond": false,
                "UsingOldContentTag": false,
                "DefaultLogQueueSize" : 10
            },
            "processors": [
                {
                    "type": "processor_regex/4",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_default/5",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_sls/6",
                    "detail": {
                        "EnableShardHash": false
                    }
                },
                {
                    "type": "flusher_kafka_v2/7",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithoutInputStr, goPipelineWithoutInput, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1U, pipeline->mInputs.size());
    APSARA_TEST_EQUAL(0U, pipeline->mProcessorLine.size());
    APSARA_TEST_EQUAL(1U, pipeline->GetFlushers().size());
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithInput.isNull());
    APSARA_TEST_EQUAL(goPipelineWithoutInput.toStyledString(), pipeline->mGoPipelineWithoutInput.toStyledString());
    goPipelineWithoutInput.clear();

    // topology 29: extended -> extended -> (native, extended)
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_regex"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                },
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    goPipelineWithInputStr = R"(
        {
            "global": {
                "EnableTimestampNanosecond": false,
                "UsingOldContentTag": false
            },
            "inputs": [
                {
                    "type": "service_docker_stdout/1",
                    "detail": {}
                }
            ],
            "processors": [
                {
                    "type": "processor_regex/2",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_default/3",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_sls/4",
                    "detail": {
                        "EnableShardHash": false
                    }
                },
                {
                    "type": "flusher_kafka_v2/5",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithInputStr, goPipelineWithInput, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(0U, pipeline->mInputs.size());
    APSARA_TEST_EQUAL(0U, pipeline->mProcessorLine.size());
    APSARA_TEST_EQUAL(1U, pipeline->GetFlushers().size());
    APSARA_TEST_EQUAL(goPipelineWithInput.toStyledString(), pipeline->mGoPipelineWithInput.toStyledString());
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithoutInput.isNull());
    goPipelineWithInput.clear();

    // topology 30: (native, extended) -> extended -> (native, extended)
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                },
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_regex"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                },
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // topology 31: native -> (native -> extended) -> (native, extended)
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native",
                    "SourceKey": "content",
                    "Regex": ".*",
                    "Keys": ["key"]
                },
                {
                    "Type": "processor_regex"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                },
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    goPipelineWithoutInputStr = R"(
        {
            "global" : {
                "EnableTimestampNanosecond": false,
                "UsingOldContentTag": false,
                "DefaultLogQueueSize" : 10
            },
            "processors": [
                {
                    "type": "processor_regex/5",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_default/6",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_sls/7",
                    "detail": {
                        "EnableShardHash": false
                    }
                },
                {
                    "type": "flusher_kafka_v2/8",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithoutInputStr, goPipelineWithoutInput, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1U, pipeline->mInputs.size());
    APSARA_TEST_EQUAL(1U, pipeline->mProcessorLine.size());
    APSARA_TEST_EQUAL(1U, pipeline->GetFlushers().size());
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithInput.isNull());
    APSARA_TEST_EQUAL(goPipelineWithoutInput.toStyledString(), pipeline->mGoPipelineWithoutInput.toStyledString());
    goPipelineWithoutInput.clear();

    // topology 32: extended -> (native -> extended) -> (native, extended)
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native",
                    "SourceKey": "content",
                    "Regex": ".*",
                    "Keys": ["key"]
                },
                {
                    "Type": "processor_regex"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                },
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // topology 33: (native, extended) -> (native -> extended) -> (native, extended)
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                },
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native",
                    "SourceKey": "content",
                    "Regex": ".*",
                    "Keys": ["key"]
                },
                {
                    "Type": "processor_regex"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                },
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // topology 34: native -> none -> (native, extended) (future changes maybe applied)
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                },
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    goPipelineWithoutInputStr = R"(
        {
            "global" : {
                "EnableTimestampNanosecond": false,
                "UsingOldContentTag": false,
                "DefaultLogQueueSize" : 10
            },
            "aggregators": [
                {
                    "type": "aggregator_default/4",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_sls/5",
                    "detail": {
                        "EnableShardHash": false
                    }
                },
                {
                    "type": "flusher_kafka_v2/6",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithoutInputStr, goPipelineWithoutInput, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1U, pipeline->mInputs.size());
    APSARA_TEST_EQUAL(0U, pipeline->mProcessorLine.size());
    APSARA_TEST_EQUAL(1U, pipeline->GetFlushers().size());
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithInput.isNull());
    APSARA_TEST_EQUAL(goPipelineWithoutInput.toStyledString(), pipeline->mGoPipelineWithoutInput.toStyledString());
    goPipelineWithoutInput.clear();

    // topology 35: extended -> none -> (native, extended)
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                },
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    goPipelineWithInputStr = R"(
        {
            "global": {
                "EnableTimestampNanosecond": false,
                "UsingOldContentTag": false
            },
            "inputs": [
                {
                    "type": "service_docker_stdout/1",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_default/2",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_sls/3",
                    "detail": {
                        "EnableShardHash": false
                    }
                },
                {
                    "type": "flusher_kafka_v2/4",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithInputStr, goPipelineWithInput, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(0U, pipeline->mInputs.size());
    APSARA_TEST_EQUAL(0U, pipeline->mProcessorLine.size());
    APSARA_TEST_EQUAL(1U, pipeline->GetFlushers().size());
    APSARA_TEST_EQUAL(goPipelineWithInput.toStyledString(), pipeline->mGoPipelineWithInput.toStyledString());
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithoutInput.isNull());
    goPipelineWithInput.clear();

    // topology 36: (native, extended) -> none -> (native, extended)
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                },
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                },
                {
                    "Type": "flusher_kafka_v2"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());
}

void PipelineUnittest::TestProcessQueue() const {
    unique_ptr<Json::Value> configJson;
    string configStr, errorMsg;
    unique_ptr<PipelineConfig> config;
    unique_ptr<Pipeline> pipeline;
    QueueKey key;
    ProcessQueueManager::ProcessQueueIterator que;

    // new pipeline
    configStr = R"(
        {
            "global": {
                "ProcessPriority": 1
            },
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));

    key = QueueKeyManager::GetInstance()->GetKey(configName);
    que = ProcessQueueManager::GetInstance()->mQueues[key].first;
    APSARA_TEST_EQUAL(ProcessQueueManager::QueueType::BOUNDED, ProcessQueueManager::GetInstance()->mQueues[key].second);
    // queue level
    APSARA_TEST_EQUAL(configName, (*que)->GetConfigName());
    APSARA_TEST_EQUAL(key, (*que)->GetKey());
    APSARA_TEST_EQUAL(0U, (*que)->GetPriority());
    APSARA_TEST_EQUAL(1U, static_cast<BoundedProcessQueue*>(que->get())->mUpStreamFeedbacks.size());
    APSARA_TEST_EQUAL(InputFeedbackInterfaceRegistry::GetInstance()->GetFeedbackInterface("input_file"),
                      static_cast<BoundedProcessQueue*>(que->get())->mUpStreamFeedbacks[0]);
    APSARA_TEST_EQUAL(1U, (*que)->mDownStreamQueues.size());
    // pipeline level
    APSARA_TEST_EQUAL(key, pipeline->GetContext().GetProcessQueueKey());
    // manager level
    APSARA_TEST_EQUAL(1U, ProcessQueueManager::GetInstance()->mQueues.size());
    APSARA_TEST_EQUAL(1U, ProcessQueueManager::GetInstance()->mPriorityQueue[0].size());
    APSARA_TEST_TRUE(ProcessQueueManager::GetInstance()->mPriorityQueue[0].begin()
                     == ProcessQueueManager::GetInstance()->mQueues[key].first);

    // update pipeline with different priority
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_mock"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));

    key = QueueKeyManager::GetInstance()->GetKey(configName);
    que = ProcessQueueManager::GetInstance()->mQueues[key].first;
    APSARA_TEST_EQUAL(ProcessQueueManager::QueueType::BOUNDED, ProcessQueueManager::GetInstance()->mQueues[key].second);
    // queue level
    APSARA_TEST_EQUAL(configName, (*que)->GetConfigName());
    APSARA_TEST_EQUAL(key, (*que)->GetKey());
    APSARA_TEST_EQUAL(3U, (*que)->GetPriority());
    APSARA_TEST_EQUAL(1U, (*que)->mDownStreamQueues.size());
    // pipeline level
    APSARA_TEST_EQUAL(key, pipeline->GetContext().GetProcessQueueKey());
    // manager level
    APSARA_TEST_EQUAL(1U, ProcessQueueManager::GetInstance()->mQueues.size());
    APSARA_TEST_EQUAL(1U, ProcessQueueManager::GetInstance()->mPriorityQueue[3].size());
    APSARA_TEST_TRUE(ProcessQueueManager::GetInstance()->mPriorityQueue[3].begin()
                     == ProcessQueueManager::GetInstance()->mQueues[key].first);

    // update pipeline with different type
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_mock",
                    "SupportAck": false
                },
                {
                    "Type": "input_mock",
                    "SupportAck": false
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));

    key = QueueKeyManager::GetInstance()->GetKey(configName);
    que = ProcessQueueManager::GetInstance()->mQueues[key].first;
    APSARA_TEST_EQUAL(ProcessQueueManager::QueueType::CIRCULAR,
                      ProcessQueueManager::GetInstance()->mQueues[key].second);
    // queue level
    APSARA_TEST_EQUAL(configName, (*que)->GetConfigName());
    APSARA_TEST_EQUAL(key, (*que)->GetKey());
    APSARA_TEST_EQUAL(3U, (*que)->GetPriority());
    APSARA_TEST_EQUAL(1U, (*que)->mDownStreamQueues.size());
    // pipeline level
    APSARA_TEST_EQUAL(key, pipeline->GetContext().GetProcessQueueKey());
    // manager level
    APSARA_TEST_EQUAL(1U, ProcessQueueManager::GetInstance()->mQueues.size());
    APSARA_TEST_EQUAL(1U, ProcessQueueManager::GetInstance()->mPriorityQueue[3].size());
    APSARA_TEST_TRUE(ProcessQueueManager::GetInstance()->mPriorityQueue[3].begin()
                     == ProcessQueueManager::GetInstance()->mQueues[key].first);

    // delete pipeline
    pipeline->RemoveProcessQueue();
    pipeline.reset();
    APSARA_TEST_EQUAL(0U, ProcessQueueManager::GetInstance()->mQueues.size());
    APSARA_TEST_EQUAL("", QueueKeyManager::GetInstance()->GetName(key));
}

void PipelineUnittest::OnInputFileWithJsonMultiline() const {
    unique_ptr<Json::Value> configJson;
    string configStr, errorMsg;
    unique_ptr<PipelineConfig> config;
    unique_ptr<Pipeline> pipeline;

    // first processor is native json parser
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_json_native",
                    "SourceKey": "content"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_TRUE(pipeline->GetContext().RequiringJsonReader());
    APSARA_TEST_EQUAL(ProcessorSplitLogStringNative::sName, pipeline->mInputs[0]->GetInnerProcessors()[0]->Name());

    // first processor is extended json parser
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ]
                }
            ],
            "processors": [
                {
                    "Type": "processor_json"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_TRUE(pipeline->GetContext().RequiringJsonReader());
    APSARA_TEST_EQUAL(ProcessorSplitLogStringNative::sName, pipeline->mInputs[0]->GetInnerProcessors()[0]->Name());
}

void PipelineUnittest::OnInputFileWithContainerDiscovery() const {
    unique_ptr<Json::Value> configJson;
    Json::Value goPipelineWithInput, goPipelineWithoutInput;
    string configStr, goPipelineWithoutInputStr, goPipelineWithInputStr, errorMsg;
    unique_ptr<PipelineConfig> config;
    unique_ptr<Pipeline> pipeline;

    // native processing
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ],
                    "EnableContainerDiscovery": true,
                    "CollectingContainersMeta": true
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                }
            ]
        }
    )";
    goPipelineWithInputStr = R"(
        {
            "global" : {
                "AlwaysOnline": true,
                "EnableTimestampNanosecond": false,
                "UsingOldContentTag": false,
                "DefaultLogQueueSize" : 10
            },
            "inputs": [
                {
                    "type": "metric_container_info/2",
                    "detail": {
                        "CollectingContainersMeta": true,
                        "LogPath": "/home",
                        "MaxDepth": 0,
                        "FilePattern": "test.log"
                    }
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithInputStr, goPipelineWithInput, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(goPipelineWithInput.toStyledString(), pipeline->mGoPipelineWithInput.toStyledString());
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithoutInput.isNull());
    goPipelineWithInput.clear();

    // mixed processing
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ],
                    "EnableContainerDiscovery": true,
                    "CollectingContainersMeta": true
                }
            ],
            "processors": [
                {
                    "Type": "processor_regex"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
                    "Project": "test_project",
                    "Logstore": "test_logstore",
                    "Region": "test_region",
                    "Endpoint": "test_endpoint",
                    "EnableShardHash": false
                }
            ]
        }
    )";
    goPipelineWithInputStr = R"(
        {
            "global" : {
                "AlwaysOnline": true,
                "EnableTimestampNanosecond": false,
                "UsingOldContentTag": false,
                "DefaultLogQueueSize" : 10
            },
            "inputs": [
                {
                    "type": "metric_container_info/2",
                    "detail": {
                        "CollectingContainersMeta": true,
                        "LogPath": "/home",
                        "MaxDepth": 0,
                        "FilePattern": "test.log"
                    }
                }
            ]
        }
    )";
    goPipelineWithoutInputStr = R"(
        {
            "global" : {
                "EnableTimestampNanosecond": false,
                "UsingOldContentTag": false,
                "DefaultLogQueueSize" : 10
            },
            "processors": [
                {
                    "type": "processor_regex/5",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_default/6",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_sls/7",
                    "detail": {
                        "EnableShardHash": false
                    }
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithInputStr, goPipelineWithInput, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithoutInputStr, goPipelineWithoutInput, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(goPipelineWithInput.toStyledString(), pipeline->mGoPipelineWithInput.toStyledString());
    APSARA_TEST_EQUAL(goPipelineWithoutInput.toStyledString(), pipeline->mGoPipelineWithoutInput.toStyledString());
    goPipelineWithInput.clear();
    goPipelineWithoutInput.clear();
}

void PipelineUnittest::TestProcess() const {
    Pipeline pipeline;
    pipeline.mPluginID.store(0);
    PipelineContext ctx;
    ctx.SetPipeline(pipeline);
    Json::Value tmp;

    auto input = PluginRegistry::GetInstance()->CreateInput(InputMock::sName, pipeline.GenNextPluginMeta(false));
    input->Init(Json::Value(), ctx, 0, tmp);
    pipeline.mInputs.emplace_back(std::move(input));
    auto processor
        = PluginRegistry::GetInstance()->CreateProcessor(ProcessorMock::sName, pipeline.GenNextPluginMeta(false));
    processor->Init(Json::Value(), ctx);
    pipeline.mProcessorLine.emplace_back(std::move(processor));

    WriteMetrics::GetInstance()->PrepareMetricsRecordRef(pipeline.mMetricsRecordRef, MetricCategory::METRIC_CATEGORY_UNKNOWN, {});
    pipeline.mProcessorsInEventsTotal
        = pipeline.mMetricsRecordRef.CreateCounter(METRIC_PIPELINE_PROCESSORS_IN_EVENTS_TOTAL);
    pipeline.mProcessorsInGroupsTotal
        = pipeline.mMetricsRecordRef.CreateCounter(METRIC_PIPELINE_PROCESSORS_IN_EVENT_GROUPS_TOTAL);
    pipeline.mProcessorsInSizeBytes
        = pipeline.mMetricsRecordRef.CreateCounter(METRIC_PIPELINE_PROCESSORS_IN_SIZE_BYTES);
    pipeline.mProcessorsTotalProcessTimeMs
        = pipeline.mMetricsRecordRef.CreateTimeCounter(METRIC_PIPELINE_PROCESSORS_TOTAL_PROCESS_TIME_MS);

    vector<PipelineEventGroup> groups;
    groups.emplace_back(make_shared<SourceBuffer>());
    groups.back().AddLogEvent();
    auto size = groups.back().DataSize();
    pipeline.Process(groups, 0);
    APSARA_TEST_EQUAL(
        1U, static_cast<const ProcessorInnerMock*>(pipeline.mInputs[0]->GetInnerProcessors()[0]->mPlugin.get())->mCnt);
    APSARA_TEST_EQUAL(1U, static_cast<const ProcessorMock*>(pipeline.mProcessorLine[0]->mPlugin.get())->mCnt);
    APSARA_TEST_EQUAL(1U, pipeline.mProcessorsInEventsTotal->GetValue());
    APSARA_TEST_EQUAL(1U, pipeline.mProcessorsInGroupsTotal->GetValue());
    APSARA_TEST_EQUAL(size, pipeline.mProcessorsInSizeBytes->GetValue());
}

void PipelineUnittest::TestSend() const {
    {
        // no route
        Pipeline pipeline;
        pipeline.mPluginID.store(0);
        PipelineContext ctx;
        ctx.SetPipeline(pipeline);
        Json::Value tmp;
        {
            auto flusher
                = PluginRegistry::GetInstance()->CreateFlusher(FlusherMock::sName, pipeline.GenNextPluginMeta(false));
            flusher->Init(Json::Value(), ctx, tmp);
            pipeline.mFlushers.emplace_back(std::move(flusher));
        }
        {
            auto flusher
                = PluginRegistry::GetInstance()->CreateFlusher(FlusherMock::sName, pipeline.GenNextPluginMeta(false));
            flusher->Init(Json::Value(), ctx, tmp);
            pipeline.mFlushers.emplace_back(std::move(flusher));
        }
        vector<pair<size_t, const Json::Value*>> configs;
        configs.emplace_back(0, nullptr);
        configs.emplace_back(1, nullptr);
        pipeline.mRouter.Init(configs, ctx);

        WriteMetrics::GetInstance()->PrepareMetricsRecordRef(pipeline.mMetricsRecordRef, MetricCategory::METRIC_CATEGORY_UNKNOWN, {});
        pipeline.mFlushersInGroupsTotal
            = pipeline.mMetricsRecordRef.CreateCounter(METRIC_PIPELINE_FLUSHERS_IN_EVENT_GROUPS_TOTAL);
        pipeline.mFlushersInEventsTotal
            = pipeline.mMetricsRecordRef.CreateCounter(METRIC_PIPELINE_FLUSHERS_IN_EVENTS_TOTAL);
        pipeline.mFlushersInSizeBytes
            = pipeline.mMetricsRecordRef.CreateCounter(METRIC_PIPELINE_FLUSHERS_IN_SIZE_BYTES);
        pipeline.mFlushersTotalPackageTimeMs
            = pipeline.mMetricsRecordRef.CreateTimeCounter(METRIC_PIPELINE_FLUSHERS_TOTAL_PACKAGE_TIME_MS);
        {
            // all valid
            vector<PipelineEventGroup> group;
            group.emplace_back(make_shared<SourceBuffer>());
            group.back().AddLogEvent();
            APSARA_TEST_TRUE(pipeline.Send(std::move(group)));
        }
        {
            // some flusher not valid
            const_cast<FlusherMock*>(static_cast<const FlusherMock*>(pipeline.mFlushers[0]->GetPlugin()))->mIsValid
                = false;
            vector<PipelineEventGroup> group;
            group.emplace_back(make_shared<SourceBuffer>());
            group.back().AddLogEvent();
            APSARA_TEST_FALSE(pipeline.Send(std::move(group)));
            const_cast<FlusherMock*>(static_cast<const FlusherMock*>(pipeline.mFlushers[0]->GetPlugin()))->mIsValid
                = true;
        }
    }
    {
        // with route
        Pipeline pipeline;
        pipeline.mPluginID.store(0);
        PipelineContext ctx;
        ctx.SetPipeline(pipeline);
        Json::Value tmp;
        {
            auto flusher
                = PluginRegistry::GetInstance()->CreateFlusher(FlusherMock::sName, pipeline.GenNextPluginMeta(false));
            flusher->Init(Json::Value(), ctx, tmp);
            pipeline.mFlushers.emplace_back(std::move(flusher));
        }
        {
            auto flusher
                = PluginRegistry::GetInstance()->CreateFlusher(FlusherMock::sName, pipeline.GenNextPluginMeta(false));
            flusher->Init(Json::Value(), ctx, tmp);
            pipeline.mFlushers.emplace_back(std::move(flusher));
        }

        Json::Value configJson;
        string errorMsg;
        string configStr = R"(
            [
                {
                    "Type": "event_type",
                    "Value": "log"
                }
            ]
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        vector<pair<size_t, const Json::Value*>> configs;
        for (Json::Value::ArrayIndex i = 0; i < configJson.size(); ++i) {
            configs.emplace_back(i, &configJson[i]);
        }
        configs.emplace_back(configJson.size(), nullptr);
        pipeline.mRouter.Init(configs, ctx);

        WriteMetrics::GetInstance()->PrepareMetricsRecordRef(pipeline.mMetricsRecordRef, MetricCategory::METRIC_CATEGORY_UNKNOWN, {});
        pipeline.mFlushersInGroupsTotal
            = pipeline.mMetricsRecordRef.CreateCounter(METRIC_PIPELINE_FLUSHERS_IN_EVENT_GROUPS_TOTAL);
        pipeline.mFlushersInEventsTotal
            = pipeline.mMetricsRecordRef.CreateCounter(METRIC_PIPELINE_FLUSHERS_IN_EVENTS_TOTAL);
        pipeline.mFlushersInSizeBytes
            = pipeline.mMetricsRecordRef.CreateCounter(METRIC_PIPELINE_FLUSHERS_IN_SIZE_BYTES);
        pipeline.mFlushersTotalPackageTimeMs
            = pipeline.mMetricsRecordRef.CreateTimeCounter(METRIC_PIPELINE_FLUSHERS_TOTAL_PACKAGE_TIME_MS);

        {
            vector<PipelineEventGroup> group;
            group.emplace_back(make_shared<SourceBuffer>());
            group[0].AddLogEvent();
            APSARA_TEST_TRUE(pipeline.Send(std::move(group)));
        }
        {
            const_cast<FlusherMock*>(static_cast<const FlusherMock*>(pipeline.mFlushers[0]->GetPlugin()))->mIsValid
                = false;
            vector<PipelineEventGroup> group;
            group.emplace_back(make_shared<SourceBuffer>());
            group[0].AddMetricEvent();
            APSARA_TEST_TRUE(pipeline.Send(std::move(group)));
            const_cast<FlusherMock*>(static_cast<const FlusherMock*>(pipeline.mFlushers[0]->GetPlugin()))->mIsValid
                = true;
        }
    }
}

void PipelineUnittest::TestFlushBatch() const {
    Pipeline pipeline;
    pipeline.mName = configName;
    pipeline.mPluginID.store(0);
    PipelineContext ctx;
    ctx.SetPipeline(pipeline);
    Json::Value tmp;
    {
        auto flusher
            = PluginRegistry::GetInstance()->CreateFlusher(FlusherMock::sName, pipeline.GenNextPluginMeta(false));
        flusher->Init(Json::Value(), ctx, tmp);
        pipeline.mFlushers.emplace_back(std::move(flusher));
    }
    {
        auto flusher
            = PluginRegistry::GetInstance()->CreateFlusher(FlusherMock::sName, pipeline.GenNextPluginMeta(false));
        flusher->Init(Json::Value(), ctx, tmp);
        pipeline.mFlushers.emplace_back(std::move(flusher));
    }
    {
        // all successful
        TimeoutFlushManager::GetInstance()->UpdateRecord(configName, 0, 1, 3, nullptr);
        TimeoutFlushManager::GetInstance()->UpdateRecord(configName, 1, 1, 3, nullptr);
        APSARA_TEST_TRUE(pipeline.FlushBatch());
        APSARA_TEST_TRUE(TimeoutFlushManager::GetInstance()->mTimeoutRecords.empty());
    }
    {
        // some failed
        const_cast<FlusherMock*>(static_cast<const FlusherMock*>(pipeline.mFlushers[0]->GetPlugin()))->mIsValid = false;
        TimeoutFlushManager::GetInstance()->UpdateRecord(configName, 0, 1, 3, nullptr);
        TimeoutFlushManager::GetInstance()->UpdateRecord(configName, 1, 1, 3, nullptr);
        APSARA_TEST_FALSE(pipeline.FlushBatch());
        APSARA_TEST_TRUE(TimeoutFlushManager::GetInstance()->mTimeoutRecords.empty());
    }
}

void PipelineUnittest::TestInProcessingCount() const {
    auto pipeline = make_shared<Pipeline>();
    pipeline->mPluginID.store(0);
    pipeline->mInProcessCnt.store(0);

    PipelineContext ctx;
    unique_ptr<BoundedProcessQueue> processQueue;
    processQueue.reset(new BoundedProcessQueue(2, 2, 3, 0, 1, ctx));

    vector<PipelineEventGroup> group;
    group.emplace_back(make_shared<SourceBuffer>());

    processQueue->EnablePop();
    processQueue->Push(GenerateProcessItem(pipeline));
    APSARA_TEST_EQUAL(0, pipeline->mInProcessCnt.load());
    unique_ptr<ProcessQueueItem> item;
    APSARA_TEST_TRUE(processQueue->Pop(item));
    APSARA_TEST_EQUAL(1, pipeline->mInProcessCnt.load());

    pipeline->SubInProcessCnt();
    APSARA_TEST_EQUAL(0, pipeline->mInProcessCnt.load());
}

void PipelineUnittest::TestWaitAllItemsInProcessFinished() const {
    auto pipeline = make_shared<Pipeline>();
    pipeline->mPluginID.store(0);
    pipeline->mInProcessCnt.store(0);

    pipeline->mInProcessCnt.store(1);
    std::future<void> future = std::async(std::launch::async, &Pipeline::WaitAllItemsInProcessFinished, pipeline.get());

    // block
    APSARA_TEST_NOT_EQUAL(std::future_status::ready, future.wait_for(std::chrono::seconds(0)));
    pipeline->mInProcessCnt.store(0);
    // recover
    usleep(3000);
    APSARA_TEST_EQUAL(std::future_status::ready, future.wait_for(std::chrono::seconds(0)));
}

UNIT_TEST_CASE(PipelineUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(PipelineUnittest, OnFailedInit)
UNIT_TEST_CASE(PipelineUnittest, TestProcessQueue)
UNIT_TEST_CASE(PipelineUnittest, OnInitVariousTopology)
UNIT_TEST_CASE(PipelineUnittest, OnInputFileWithJsonMultiline)
UNIT_TEST_CASE(PipelineUnittest, OnInputFileWithContainerDiscovery)
UNIT_TEST_CASE(PipelineUnittest, TestProcess)
UNIT_TEST_CASE(PipelineUnittest, TestSend)
UNIT_TEST_CASE(PipelineUnittest, TestFlushBatch)
UNIT_TEST_CASE(PipelineUnittest, TestInProcessingCount)
UNIT_TEST_CASE(PipelineUnittest, TestWaitAllItemsInProcessFinished)

} // namespace logtail

UNIT_TEST_MAIN
