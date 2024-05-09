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

#include <memory>
#include <string>

#include "app_config/AppConfig.h"
#include "common/JsonUtil.h"
#include "common/LogstoreFeedbackKey.h"
#include "config/Config.h"
#include "pipeline/Pipeline.h"
#include "plugin/PluginRegistry.h"
#include "processor/inner/ProcessorSplitLogStringNative.h"
#include "processor/inner/ProcessorSplitMultilineLogStringNative.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class PipelineUnittest : public ::testing::Test {
public:
    void OnSuccessfulInit() const;
    void OnFailedInit() const;
    void OnInitVariousTopology() const;
    void OnInputFileWithMultiline() const;
    void OnInputFileWithContainerDiscovery() const;

protected:
    static void SetUpTestCase() {
        PluginRegistry::GetInstance()->LoadPlugins();
        AppConfig::GetInstance()->mPurageContainerMode = true;
    }

    static void TearDownTestCase() { PluginRegistry::GetInstance()->UnloadPlugins(); }

private:
    const string configName = "test_config";
};

void PipelineUnittest::OnSuccessfulInit() const {
    unique_ptr<Json::Value> configJson;
    Json::Value goPipelineWithInput, goPipelineWithoutInput;
    string configStr, goPipelineWithInputStr, goPipelineWithoutInputStr, errorMsg;
    unique_ptr<Config> config;
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
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(configName, pipeline->Name());
    APSARA_TEST_EQUAL(configName, pipeline->GetContext().GetConfigName());
    APSARA_TEST_EQUAL(123456789, int(pipeline->GetContext().GetCreateTime()));
    APSARA_TEST_EQUAL("test_project", pipeline->GetContext().GetProjectName());
    APSARA_TEST_EQUAL("test_logstore", pipeline->GetContext().GetLogstoreName());
    APSARA_TEST_EQUAL("test_region", pipeline->GetContext().GetRegion());
    APSARA_TEST_EQUAL(GenerateLogstoreFeedBackKey("test_project", "test_logstore"),
                      pipeline->GetContext().GetLogstoreKey());

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
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(configName, pipeline->Name());
    APSARA_TEST_EQUAL(configName, pipeline->GetContext().GetConfigName());
    APSARA_TEST_EQUAL(0, int(pipeline->GetContext().GetCreateTime()));
    APSARA_TEST_EQUAL("", pipeline->GetContext().GetProjectName());
    APSARA_TEST_EQUAL("", pipeline->GetContext().GetLogstoreName());
    APSARA_TEST_EQUAL("", pipeline->GetContext().GetRegion());
#ifndef __ENTERPRISE__
    APSARA_TEST_EQUAL(GenerateLogstoreFeedBackKey("", ""), pipeline->GetContext().GetLogstoreKey());
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
                    "type": "metric_container_info",
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
                    "type": "ext_basicauth",
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
            "flushers": [
                {
                    "type": "flusher_kafka_v2",
                    "detail": {}
                }
            ],
            "extensions": [
                {
                    "type": "ext_basicauth",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithInputStr, goPipelineWithInput, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithoutInputStr, goPipelineWithoutInput, errorMsg));
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(goPipelineWithInput.toStyledString(), pipeline->mGoPipelineWithInput.toStyledString());
    APSARA_TEST_EQUAL(goPipelineWithoutInput.toStyledString(), pipeline->mGoPipelineWithoutInput.toStyledString());
    goPipelineWithInput.clear();
    goPipelineWithoutInput.clear();
}

void PipelineUnittest::OnFailedInit() const {
    unique_ptr<Json::Value> configJson;
    string configStr, errorMsg;
    unique_ptr<Config> config;
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
    config.reset(new Config(configName, std::move(configJson)));
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
    config.reset(new Config(configName, std::move(configJson)));
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
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_FALSE(pipeline->Init(std::move(*config)));
}

void PipelineUnittest::OnInitVariousTopology() const {
    unique_ptr<Json::Value> configJson;
    Json::Value goPipelineWithInput, goPipelineWithoutInput;
    string configStr, goPipelineWithInputStr, goPipelineWithoutInputStr, errorMsg;
    unique_ptr<Config> config;
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
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1, int(pipeline->mInputs.size()));
    APSARA_TEST_EQUAL(3, int(pipeline->mProcessorLine.size()));
    APSARA_TEST_EQUAL(1, int(pipeline->GetFlushers().size()));
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
    config.reset(new Config(configName, std::move(configJson)));
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
    config.reset(new Config(configName, std::move(configJson)));
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
                    "type": "processor_regex",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_context",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_sls",
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
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1, int(pipeline->mInputs.size()));
    APSARA_TEST_EQUAL(2, int(pipeline->mProcessorLine.size()));
    APSARA_TEST_EQUAL(1, int(pipeline->GetFlushers().size()));
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithInput.isNull());
    APSARA_TEST_TRUE(goPipelineWithoutInput == pipeline->mGoPipelineWithoutInput);
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
                    "type": "service_docker_stdout",
                    "detail": {}
                }
            ],
            "processors": [
                {
                    "type": "processor_regex",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_context",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_sls",
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
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(0, int(pipeline->mInputs.size()));
    APSARA_TEST_EQUAL(0, int(pipeline->mProcessorLine.size()));
    APSARA_TEST_EQUAL(1, int(pipeline->GetFlushers().size()));
    APSARA_TEST_TRUE(goPipelineWithInput == pipeline->mGoPipelineWithInput);
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
    config.reset(new Config(configName, std::move(configJson)));
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
                    "type": "processor_regex",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_context",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_sls",
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
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1, int(pipeline->mInputs.size()));
    APSARA_TEST_EQUAL(3, int(pipeline->mProcessorLine.size()));
    APSARA_TEST_EQUAL(1, int(pipeline->GetFlushers().size()));
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithInput.isNull());
    APSARA_TEST_TRUE(goPipelineWithoutInput == pipeline->mGoPipelineWithoutInput);
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
    config.reset(new Config(configName, std::move(configJson)));
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
    config.reset(new Config(configName, std::move(configJson)));
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
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1, int(pipeline->mInputs.size()));
    APSARA_TEST_EQUAL(2, int(pipeline->mProcessorLine.size()));
    APSARA_TEST_EQUAL(1, int(pipeline->GetFlushers().size()));
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
                    "type": "service_docker_stdout",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_context",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_sls",
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
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(0, int(pipeline->mInputs.size()));
    APSARA_TEST_EQUAL(0, int(pipeline->mProcessorLine.size()));
    APSARA_TEST_EQUAL(1, int(pipeline->GetFlushers().size()));
    APSARA_TEST_TRUE(goPipelineWithInput == pipeline->mGoPipelineWithInput);
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
    config.reset(new Config(configName, std::move(configJson)));
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
                    "type": "aggregator_context",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_kafka_v2",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithoutInputStr, goPipelineWithoutInput, errorMsg));
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1, int(pipeline->mInputs.size()));
    APSARA_TEST_EQUAL(3, int(pipeline->mProcessorLine.size()));
    APSARA_TEST_EQUAL(0, int(pipeline->GetFlushers().size()));
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithInput.isNull());
    APSARA_TEST_TRUE(goPipelineWithoutInput == pipeline->mGoPipelineWithoutInput);
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
    config.reset(new Config(configName, std::move(configJson)));
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
    config.reset(new Config(configName, std::move(configJson)));
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
                    "type": "processor_regex",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_context",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_kafka_v2",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithoutInputStr, goPipelineWithoutInput, errorMsg));
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1, int(pipeline->mInputs.size()));
    APSARA_TEST_EQUAL(2, int(pipeline->mProcessorLine.size()));
    APSARA_TEST_EQUAL(0, int(pipeline->GetFlushers().size()));
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithInput.isNull());
    APSARA_TEST_TRUE(goPipelineWithoutInput == pipeline->mGoPipelineWithoutInput);
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
                    "type": "service_docker_stdout",
                    "detail": {}
                }
            ],
            "processors": [
                {
                    "type": "processor_regex",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_context",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_kafka_v2",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithInputStr, goPipelineWithInput, errorMsg));
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(0, int(pipeline->mInputs.size()));
    APSARA_TEST_EQUAL(0, int(pipeline->mProcessorLine.size()));
    APSARA_TEST_EQUAL(0, int(pipeline->GetFlushers().size()));
    APSARA_TEST_TRUE(goPipelineWithInput == pipeline->mGoPipelineWithInput);
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
    config.reset(new Config(configName, std::move(configJson)));
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
                    "type": "processor_regex",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_context",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_kafka_v2",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithoutInputStr, goPipelineWithoutInput, errorMsg));
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1, int(pipeline->mInputs.size()));
    APSARA_TEST_EQUAL(3, int(pipeline->mProcessorLine.size()));
    APSARA_TEST_EQUAL(0, int(pipeline->GetFlushers().size()));
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithInput.isNull());
    APSARA_TEST_TRUE(goPipelineWithoutInput == pipeline->mGoPipelineWithoutInput);
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
    config.reset(new Config(configName, std::move(configJson)));
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
    config.reset(new Config(configName, std::move(configJson)));
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
                    "type": "aggregator_context",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_kafka_v2",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithoutInputStr, goPipelineWithoutInput, errorMsg));
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1, int(pipeline->mInputs.size()));
    APSARA_TEST_EQUAL(2, int(pipeline->mProcessorLine.size()));
    APSARA_TEST_EQUAL(0, int(pipeline->GetFlushers().size()));
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithInput.isNull());
    APSARA_TEST_TRUE(goPipelineWithoutInput == pipeline->mGoPipelineWithoutInput);
    goPipelineWithoutInput.clear();

    // topology 23: extended -> none -> extended
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
                    "type": "service_docker_stdout",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_context",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_kafka_v2",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithInputStr, goPipelineWithInput, errorMsg));
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(0, int(pipeline->mInputs.size()));
    APSARA_TEST_EQUAL(0, int(pipeline->mProcessorLine.size()));
    APSARA_TEST_EQUAL(0, int(pipeline->GetFlushers().size()));
    APSARA_TEST_TRUE(goPipelineWithInput == pipeline->mGoPipelineWithInput);
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
    config.reset(new Config(configName, std::move(configJson)));
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
                    "type": "aggregator_context",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_sls",
                    "detail": {
                        "EnableShardHash": false
                    }
                },
                {
                    "type": "flusher_kafka_v2",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithoutInputStr, goPipelineWithoutInput, errorMsg));
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1, int(pipeline->mInputs.size()));
    APSARA_TEST_EQUAL(3, int(pipeline->mProcessorLine.size()));
    APSARA_TEST_EQUAL(1, int(pipeline->GetFlushers().size()));
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithInput.isNull());
    APSARA_TEST_TRUE(goPipelineWithoutInput == pipeline->mGoPipelineWithoutInput);
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
    config.reset(new Config(configName, std::move(configJson)));
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
    config.reset(new Config(configName, std::move(configJson)));
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
                    "type": "processor_regex",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_context",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_sls",
                    "detail": {
                        "EnableShardHash": false
                    }
                },
                {
                    "type": "flusher_kafka_v2",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithoutInputStr, goPipelineWithoutInput, errorMsg));
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1, int(pipeline->mInputs.size()));
    APSARA_TEST_EQUAL(2, int(pipeline->mProcessorLine.size()));
    APSARA_TEST_EQUAL(1, int(pipeline->GetFlushers().size()));
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithInput.isNull());
    APSARA_TEST_TRUE(goPipelineWithoutInput == pipeline->mGoPipelineWithoutInput);
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
                    "type": "service_docker_stdout",
                    "detail": {}
                }
            ],
            "processors": [
                {
                    "type": "processor_regex",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_context",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_sls",
                    "detail": {
                        "EnableShardHash": false
                    }
                },
                {
                    "type": "flusher_kafka_v2",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithInputStr, goPipelineWithInput, errorMsg));
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(0, int(pipeline->mInputs.size()));
    APSARA_TEST_EQUAL(0, int(pipeline->mProcessorLine.size()));
    APSARA_TEST_EQUAL(1, int(pipeline->GetFlushers().size()));
    APSARA_TEST_TRUE(goPipelineWithInput == pipeline->mGoPipelineWithInput);
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
    config.reset(new Config(configName, std::move(configJson)));
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
                    "type": "processor_regex",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_context",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_sls",
                    "detail": {
                        "EnableShardHash": false
                    }
                },
                {
                    "type": "flusher_kafka_v2",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithoutInputStr, goPipelineWithoutInput, errorMsg));
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1, int(pipeline->mInputs.size()));
    APSARA_TEST_EQUAL(3, int(pipeline->mProcessorLine.size()));
    APSARA_TEST_EQUAL(1, int(pipeline->GetFlushers().size()));
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithInput.isNull());
    APSARA_TEST_TRUE(goPipelineWithoutInput == pipeline->mGoPipelineWithoutInput);
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
    config.reset(new Config(configName, std::move(configJson)));
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
    config.reset(new Config(configName, std::move(configJson)));
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
                    "type": "aggregator_context",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_sls",
                    "detail": {
                        "EnableShardHash": false
                    }
                },
                {
                    "type": "flusher_kafka_v2",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithoutInputStr, goPipelineWithoutInput, errorMsg));
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(1, int(pipeline->mInputs.size()));
    APSARA_TEST_EQUAL(2, int(pipeline->mProcessorLine.size()));
    APSARA_TEST_EQUAL(1, int(pipeline->GetFlushers().size()));
    APSARA_TEST_TRUE(pipeline->mGoPipelineWithInput.isNull());
    APSARA_TEST_TRUE(goPipelineWithoutInput == pipeline->mGoPipelineWithoutInput);
    goPipelineWithoutInput.clear();

    // topology 35: extended -> none -> (native, extended)
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
                    "type": "service_docker_stdout",
                    "detail": {}
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_context",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_sls",
                    "detail": {
                        "EnableShardHash": false
                    }
                },
                {
                    "type": "flusher_kafka_v2",
                    "detail": {}
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(goPipelineWithInputStr, goPipelineWithInput, errorMsg));
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(0, int(pipeline->mInputs.size()));
    APSARA_TEST_EQUAL(0, int(pipeline->mProcessorLine.size()));
    APSARA_TEST_EQUAL(1, int(pipeline->GetFlushers().size()));
    APSARA_TEST_TRUE(goPipelineWithInput == pipeline->mGoPipelineWithInput);
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
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());
}

void PipelineUnittest::OnInputFileWithMultiline() const {
    unique_ptr<Json::Value> configJson;
    string configStr, errorMsg;
    unique_ptr<Config> config;
    unique_ptr<Pipeline> pipeline;

    // normal multiline
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ],
                    "Multiline": {
                        "StartPattern": "\\d+",
                        "EndPattern": "end"
                    }
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
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(ProcessorSplitMultilineLogStringNative::sName, pipeline->mProcessorLine[1]->Name());

    // json multiline
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
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_TRUE(pipeline->GetContext().RequiringJsonReader());
    APSARA_TEST_EQUAL(ProcessorSplitLogStringNative::sName, pipeline->mProcessorLine[1]->Name());

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
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_TRUE(pipeline->GetContext().RequiringJsonReader());
    APSARA_TEST_EQUAL(ProcessorSplitLogStringNative::sName, pipeline->mProcessorLine[1]->Name());

    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/home/test.log"
                    ],
                    "Multiline": {
                        "Mode": "JSON"
                    }
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
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_TRUE(pipeline->GetContext().RequiringJsonReader());
    APSARA_TEST_EQUAL(ProcessorSplitLogStringNative::sName, pipeline->mProcessorLine[1]->Name());
}

void PipelineUnittest::OnInputFileWithContainerDiscovery() const {
    unique_ptr<Json::Value> configJson;
    Json::Value goPipelineWithInput, goPipelineWithoutInput;
    string configStr, goPipelineWithoutInputStr, goPipelineWithInputStr, errorMsg;
    unique_ptr<Config> config;
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
                    "type": "metric_container_info",
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
    config.reset(new Config(configName, std::move(configJson)));
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
                    "type": "metric_container_info",
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
                    "type": "processor_regex",
                    "detail": {}
                }
            ],
            "flushers": [
                {
                    "type": "flusher_sls",
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
    config.reset(new Config(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    pipeline.reset(new Pipeline());
    APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
    APSARA_TEST_EQUAL(goPipelineWithInput.toStyledString(), pipeline->mGoPipelineWithInput.toStyledString());
    APSARA_TEST_TRUE(goPipelineWithoutInput == pipeline->mGoPipelineWithoutInput);
    goPipelineWithInput.clear();
    goPipelineWithoutInput.clear();
}

UNIT_TEST_CASE(PipelineUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(PipelineUnittest, OnFailedInit)
UNIT_TEST_CASE(PipelineUnittest, OnInputFileWithMultiline)
UNIT_TEST_CASE(PipelineUnittest, OnInputFileWithContainerDiscovery)
UNIT_TEST_CASE(PipelineUnittest, OnInitVariousTopology)

} // namespace logtail

UNIT_TEST_MAIN
