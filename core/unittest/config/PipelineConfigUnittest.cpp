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

#include "common/JsonUtil.h"
#include "config/PipelineConfig.h"
#include "pipeline/plugin/PluginRegistry.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class PipelineConfigUnittest : public testing::Test {
public:
    void HandleValidConfig() const;
    void HandleInvalidCreateTime() const;
    void HandleInvalidGlobal() const;
    void HandleInvalidInputs() const;
    void HandleInvalidProcessors() const;
    void HandleInvalidAggregators() const;
    void HandleInvalidFlushers() const;
    void HandleInvalidExtensions() const;
    void TestReplaceEnvVarRef() const;

protected:
    static void SetUpTestCase() { PluginRegistry::GetInstance()->LoadPlugins(); }
    static void TearDownTestCase() { PluginRegistry::GetInstance()->UnloadPlugins(); }

private:
    const string configName = "test";
};

void PipelineConfigUnittest::HandleValidConfig() const {
    unique_ptr<Json::Value> configJson;
    string configStr, errorMsg;
    unique_ptr<PipelineConfig> config;

    configStr = R"(
        {
            "createTime": 123456789,
            "global": {
                "TopicType": "none"
            },
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native"
                },
                {
                    "Type": "processor_json"
                }
            ],
            "aggregators":[
                {
                    "Type": "aggregator_context"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls"
                },
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
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    APSARA_TEST_EQUAL(configName, config->mName);
    APSARA_TEST_EQUAL(123456789U, config->mCreateTime);
    APSARA_TEST_NOT_EQUAL(config->mGlobal, nullptr);
    APSARA_TEST_EQUAL(1U, config->mInputs.size());
    APSARA_TEST_EQUAL(2U, config->mProcessors.size());
    APSARA_TEST_EQUAL(1U, config->mAggregators.size());
    APSARA_TEST_EQUAL(2U, config->mFlushers.size());
    APSARA_TEST_EQUAL(1U, config->mExtensions.size());

    // topology 1: native -> native -> native
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
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
    APSARA_TEST_EQUAL(1U, config->mInputs.size());
    APSARA_TEST_EQUAL(1U, config->mProcessors.size());
    APSARA_TEST_EQUAL(1U, config->mFlushers.size());
    APSARA_TEST_EQUAL(1U, config->mRouter.size());
    APSARA_TEST_FALSE(config->ShouldNativeFlusherConnectedByGoPipeline());
    APSARA_TEST_FALSE(config->IsFlushingThroughGoPipelineExisted());
    // APSARA_TEST_TRUE(config->IsProcessRunnerInvolved());
    APSARA_TEST_FALSE(config->HasGoPlugin());

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
                    "Type": "processor_parse_regex_native"
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
    APSARA_TEST_FALSE(config->Parse());

    // topology 3: (native, extended) -> native -> native
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                },
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native"
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
    APSARA_TEST_FALSE(config->Parse());

    // topology 4: native -> extended -> native
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "processors": [
                {
                    "Type": "processor_json"
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
    APSARA_TEST_EQUAL(1U, config->mInputs.size());
    APSARA_TEST_EQUAL(1U, config->mProcessors.size());
    APSARA_TEST_EQUAL(1U, config->mFlushers.size());
    APSARA_TEST_TRUE(config->ShouldNativeFlusherConnectedByGoPipeline());
    APSARA_TEST_TRUE(config->IsFlushingThroughGoPipelineExisted());
    // APSARA_TEST_TRUE(config->IsProcessRunnerInvolved());
    APSARA_TEST_TRUE(config->HasGoPlugin());

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
                    "Type": "processor_json"
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
    APSARA_TEST_EQUAL(1U, config->mInputs.size());
    APSARA_TEST_EQUAL(1U, config->mProcessors.size());
    APSARA_TEST_EQUAL(1U, config->mFlushers.size());
    APSARA_TEST_TRUE(config->ShouldNativeFlusherConnectedByGoPipeline());
    APSARA_TEST_TRUE(config->IsFlushingThroughGoPipelineExisted());
    // APSARA_TEST_FALSE(config->IsProcessRunnerInvolved());
    APSARA_TEST_TRUE(config->HasGoPlugin());

    // topology 6: (native, extended) -> extended -> native
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
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
                    "Type": "flusher_sls"
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
                    "Type": "input_file"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native"
                },
                {
                    "Type": "processor_json"
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
    APSARA_TEST_EQUAL(1U, config->mInputs.size());
    APSARA_TEST_EQUAL(2U, config->mProcessors.size());
    APSARA_TEST_EQUAL(1U, config->mFlushers.size());
    APSARA_TEST_TRUE(config->ShouldNativeFlusherConnectedByGoPipeline());
    APSARA_TEST_TRUE(config->IsFlushingThroughGoPipelineExisted());
    // APSARA_TEST_TRUE(config->IsProcessRunnerInvolved());
    APSARA_TEST_TRUE(config->HasGoPlugin());

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
                    "Type": "processor_parse_regex_native"
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
                    "Type": "flusher_sls"
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
                    "Type": "input_file"
                },
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native"
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
                    "Type": "flusher_sls"
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
                    "Type": "input_file"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls",
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
    APSARA_TEST_EQUAL(1U, config->mInputs.size());
    APSARA_TEST_EQUAL(0U, config->mProcessors.size());
    APSARA_TEST_EQUAL(1U, config->mFlushers.size());
    APSARA_TEST_EQUAL(1U, config->mRouter.size());
    APSARA_TEST_FALSE(config->ShouldNativeFlusherConnectedByGoPipeline());
    APSARA_TEST_FALSE(config->IsFlushingThroughGoPipelineExisted());
    // APSARA_TEST_TRUE(config->IsProcessRunnerInvolved());
    APSARA_TEST_FALSE(config->HasGoPlugin());

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
                    "Type": "flusher_sls"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    APSARA_TEST_EQUAL(1U, config->mInputs.size());
    APSARA_TEST_EQUAL(0U, config->mProcessors.size());
    APSARA_TEST_EQUAL(1U, config->mFlushers.size());
    APSARA_TEST_TRUE(config->ShouldNativeFlusherConnectedByGoPipeline());
    APSARA_TEST_TRUE(config->IsFlushingThroughGoPipelineExisted());
    // APSARA_TEST_FALSE(config->IsProcessRunnerInvolved());
    APSARA_TEST_TRUE(config->HasGoPlugin());

    // topology 12: (native, extended) -> none -> native
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
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
                    "Type": "flusher_sls"
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
                    "Type": "input_file"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native"
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
    APSARA_TEST_EQUAL(1U, config->mInputs.size());
    APSARA_TEST_EQUAL(1U, config->mProcessors.size());
    APSARA_TEST_EQUAL(1U, config->mFlushers.size());
    APSARA_TEST_FALSE(config->ShouldNativeFlusherConnectedByGoPipeline());
    APSARA_TEST_TRUE(config->IsFlushingThroughGoPipelineExisted());
    // APSARA_TEST_TRUE(config->IsProcessRunnerInvolved());
    APSARA_TEST_TRUE(config->HasGoPlugin());

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
                    "Type": "processor_parse_regex_native"
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
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // topology 15: (native, extended) -> native -> extended
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                },
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native"
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
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // topology 16: native -> extended -> extended
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "processors": [
                {
                    "Type": "processor_json"
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
    APSARA_TEST_EQUAL(1U, config->mInputs.size());
    APSARA_TEST_EQUAL(1U, config->mProcessors.size());
    APSARA_TEST_EQUAL(1U, config->mFlushers.size());
    APSARA_TEST_TRUE(config->ShouldNativeFlusherConnectedByGoPipeline());
    APSARA_TEST_TRUE(config->IsFlushingThroughGoPipelineExisted());
    // APSARA_TEST_TRUE(config->IsProcessRunnerInvolved());
    APSARA_TEST_TRUE(config->HasGoPlugin());

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
                    "Type": "processor_json"
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
    APSARA_TEST_EQUAL(1U, config->mInputs.size());
    APSARA_TEST_EQUAL(1U, config->mProcessors.size());
    APSARA_TEST_EQUAL(1U, config->mFlushers.size());
    APSARA_TEST_TRUE(config->ShouldNativeFlusherConnectedByGoPipeline());
    APSARA_TEST_TRUE(config->IsFlushingThroughGoPipelineExisted());
    // APSARA_TEST_FALSE(config->IsProcessRunnerInvolved());
    APSARA_TEST_TRUE(config->HasGoPlugin());

    // topology 18: (native, extended) -> extended -> extended
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
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
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // topology 19: native -> (native -> extended) -> extended
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native"
                },
                {
                    "Type": "processor_json"
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
    APSARA_TEST_EQUAL(1U, config->mInputs.size());
    APSARA_TEST_EQUAL(2U, config->mProcessors.size());
    APSARA_TEST_EQUAL(1U, config->mFlushers.size());
    APSARA_TEST_TRUE(config->ShouldNativeFlusherConnectedByGoPipeline());
    APSARA_TEST_TRUE(config->IsFlushingThroughGoPipelineExisted());
    // APSARA_TEST_TRUE(config->IsProcessRunnerInvolved());
    APSARA_TEST_TRUE(config->HasGoPlugin());

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
                    "Type": "processor_parse_regex_native"
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
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // topology 21: (native, extended) -> (native -> extended) -> extended
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                },
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native"
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
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // topology 22: native -> none -> extended
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
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
    APSARA_TEST_EQUAL(1U, config->mInputs.size());
    APSARA_TEST_EQUAL(0U, config->mProcessors.size());
    APSARA_TEST_EQUAL(1U, config->mFlushers.size());
    APSARA_TEST_FALSE(config->ShouldNativeFlusherConnectedByGoPipeline());
    APSARA_TEST_TRUE(config->IsFlushingThroughGoPipelineExisted());
    // APSARA_TEST_TRUE(config->IsProcessRunnerInvolved());
    APSARA_TEST_TRUE(config->HasGoPlugin());

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
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    APSARA_TEST_EQUAL(1U, config->mInputs.size());
    APSARA_TEST_EQUAL(0U, config->mProcessors.size());
    APSARA_TEST_EQUAL(1U, config->mFlushers.size());
    APSARA_TEST_TRUE(config->ShouldNativeFlusherConnectedByGoPipeline());
    APSARA_TEST_TRUE(config->IsFlushingThroughGoPipelineExisted());
    // APSARA_TEST_FALSE(config->IsProcessRunnerInvolved());
    APSARA_TEST_TRUE(config->HasGoPlugin());

    // topology 24: (native, extended) -> none -> extended
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
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
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // topology 25: native -> native -> (native, extended) (future changes maybe applied)
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls"
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
    APSARA_TEST_TRUE(config->Parse());
    APSARA_TEST_EQUAL(1U, config->mInputs.size());
    APSARA_TEST_EQUAL(1U, config->mProcessors.size());
    APSARA_TEST_EQUAL(2U, config->mFlushers.size());
    APSARA_TEST_TRUE(config->ShouldNativeFlusherConnectedByGoPipeline());
    APSARA_TEST_TRUE(config->IsFlushingThroughGoPipelineExisted());
    // APSARA_TEST_TRUE(config->IsProcessRunnerInvolved());
    APSARA_TEST_TRUE(config->HasGoPlugin());

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
                    "Type": "processor_parse_regex_native"
                }
            ],
            "aggregators": [
                {
                    "Type": "aggregator_context"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls"
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
                    "Type": "input_file"
                },
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native"
                }
            ],
            "aggregators": [
                {
                    "Type": "aggregator_context"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls"
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
                    "Type": "input_file"
                }
            ],
            "processors": [
                {
                    "Type": "processor_json"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls"
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
    APSARA_TEST_TRUE(config->Parse());
    APSARA_TEST_EQUAL(1U, config->mInputs.size());
    APSARA_TEST_EQUAL(1U, config->mProcessors.size());
    APSARA_TEST_EQUAL(2U, config->mFlushers.size());
    APSARA_TEST_TRUE(config->ShouldNativeFlusherConnectedByGoPipeline());
    APSARA_TEST_TRUE(config->IsFlushingThroughGoPipelineExisted());
    // APSARA_TEST_TRUE(config->IsProcessRunnerInvolved());
    APSARA_TEST_TRUE(config->HasGoPlugin());

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
                    "Type": "processor_json"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls"
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
    APSARA_TEST_TRUE(config->Parse());
    APSARA_TEST_EQUAL(1U, config->mInputs.size());
    APSARA_TEST_EQUAL(1U, config->mProcessors.size());
    APSARA_TEST_EQUAL(2U, config->mFlushers.size());
    APSARA_TEST_TRUE(config->ShouldNativeFlusherConnectedByGoPipeline());
    APSARA_TEST_TRUE(config->IsFlushingThroughGoPipelineExisted());
    // APSARA_TEST_FALSE(config->IsProcessRunnerInvolved());
    APSARA_TEST_TRUE(config->HasGoPlugin());

    // topology 30: (native, extended) -> extended -> (native, extended)
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
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
                    "Type": "flusher_sls"
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

    // topology 31: native -> (naive -> extended) -> (native, extended)
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native"
                },
                {
                    "Type": "processor_json"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls"
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
    APSARA_TEST_TRUE(config->Parse());
    APSARA_TEST_EQUAL(1U, config->mInputs.size());
    APSARA_TEST_EQUAL(2U, config->mProcessors.size());
    APSARA_TEST_EQUAL(2U, config->mFlushers.size());
    APSARA_TEST_TRUE(config->ShouldNativeFlusherConnectedByGoPipeline());
    APSARA_TEST_TRUE(config->IsFlushingThroughGoPipelineExisted());
    // APSARA_TEST_TRUE(config->IsProcessRunnerInvolved());
    APSARA_TEST_TRUE(config->HasGoPlugin());

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
                    "Type": "processor_parse_regex_native"
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
                    "Type": "flusher_sls"
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
                    "Type": "input_file"
                },
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native"
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
                    "Type": "flusher_sls"
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
                    "Type": "input_file"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls"
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
    APSARA_TEST_TRUE(config->Parse());
    APSARA_TEST_EQUAL(1U, config->mInputs.size());
    APSARA_TEST_EQUAL(0U, config->mProcessors.size());
    APSARA_TEST_EQUAL(2U, config->mFlushers.size());
    APSARA_TEST_TRUE(config->ShouldNativeFlusherConnectedByGoPipeline());
    APSARA_TEST_TRUE(config->IsFlushingThroughGoPipelineExisted());
    // APSARA_TEST_TRUE(config->IsProcessRunnerInvolved());
    APSARA_TEST_TRUE(config->HasGoPlugin());

    // topology 35: extended -> none -> (native, extended) (future changes maybe applied)
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls"
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
    APSARA_TEST_TRUE(config->Parse());
    APSARA_TEST_EQUAL(1U, config->mInputs.size());
    APSARA_TEST_EQUAL(0U, config->mProcessors.size());
    APSARA_TEST_EQUAL(2U, config->mFlushers.size());
    APSARA_TEST_TRUE(config->ShouldNativeFlusherConnectedByGoPipeline());
    APSARA_TEST_TRUE(config->IsFlushingThroughGoPipelineExisted());
    // APSARA_TEST_FALSE(config->IsProcessRunnerInvolved());
    APSARA_TEST_TRUE(config->HasGoPlugin());

    // topology 36: (native, extended) -> none -> (native, extended)
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
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
                    "Type": "flusher_sls"
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

void PipelineConfigUnittest::HandleInvalidCreateTime() const {
    unique_ptr<Json::Value> configJson;
    string configStr, errorMsg;
    unique_ptr<PipelineConfig> config;

    configStr = R"(
        {
            "createTime": "123456789",
            "inputs": [
                {
                    "Type": "input_file"
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
    APSARA_TEST_EQUAL(0U, config->mCreateTime);
}

void PipelineConfigUnittest::HandleInvalidGlobal() const {
    unique_ptr<Json::Value> configJson;
    string configStr, errorMsg;
    unique_ptr<PipelineConfig> config;

    // global is not of type object
    configStr = R"(
        {
            "global": []
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());
}

void PipelineConfigUnittest::HandleInvalidInputs() const {
    unique_ptr<Json::Value> configJson;
    string configStr, errorMsg;
    unique_ptr<PipelineConfig> config;

    // no inputs
    configStr = R"(
        {
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
    APSARA_TEST_FALSE(config->Parse());

    // inputs is not of type array
    configStr = R"(
        {
            "inputs": {},
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
    APSARA_TEST_FALSE(config->Parse());

    // inputs is empty
    configStr = R"(
        {
            "inputs": [],
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
    APSARA_TEST_FALSE(config->Parse());

    // inputs element is not of type object
    configStr = R"(
        {
            "inputs": [
                []
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
    APSARA_TEST_FALSE(config->Parse());

    // no Type
    configStr = R"(
        {
            "inputs": [
                {
                    "type": "input_file"
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
    APSARA_TEST_FALSE(config->Parse());

    // Type is not of type string
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": true
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
    APSARA_TEST_FALSE(config->Parse());

    // unsupported input
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_unknown"
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

    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                },
                {
                    "Type": "input_unknown"
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
    APSARA_TEST_FALSE(config->Parse());

    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "service_docker_stdout"
                },
                {
                    "Type": "input_unknown"
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

    // more than 1 native input
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                },
                {
                    "Type": "input_file"
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
    APSARA_TEST_FALSE(config->Parse());

    // native and extended inputs coexist
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                },
                {
                    "Type": "service_docker_stdout"
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
    APSARA_TEST_FALSE(config->Parse());

    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "service_docker_stdoutinput_file"
                },
                {
                    "Type": "input_file"
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
    APSARA_TEST_FALSE(config->Parse());
}

void PipelineConfigUnittest::HandleInvalidProcessors() const {
    unique_ptr<Json::Value> configJson;
    string configStr, errorMsg;
    unique_ptr<PipelineConfig> config;

    // processors is not of type array
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "processors": {}
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // processors element is not of type object
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "processors": [
                []
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // no Type
    configStr = R"(
        {
            "inputs": [
                {
                    "type": "input_file"
                }
            ],
            "processors": [
                {
                    "type": "processor_parse_regex_native"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // Type is not of type string
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "processors": [
                {
                    "Type": true
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // unsupported processors
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "processors": [
                {
                    "Type": "processor_unknown"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_unknown"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native"
                },
                {
                    "Type": "processor_unknown"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // native processor plugin comes after extended processor plugin
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "processors": [
                {
                    "Type": "processor_json"
                },
                {
                    "Type": "processor_parse_regex_native"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // native processor plugins coexist with extended input plugins
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "service_docker_stdout"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // native processor plugins coexist with processor_spl
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "processors": [
                {
                    "Type": "processor_parse_regex_native"
                },
                {
                    "Type": "processor_spl"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "processors": [
                {
                    "Type": "processor_spl"
                },
                {
                    "Type": "processor_parse_regex_native"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());
}

void PipelineConfigUnittest::HandleInvalidAggregators() const {
    unique_ptr<Json::Value> configJson;
    string configStr, errorMsg;
    unique_ptr<PipelineConfig> config;

    // aggregators is not of type array
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "aggregators": {},
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
    APSARA_TEST_FALSE(config->Parse());

    // aggregators element is not of type object
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "aggregators": [
                []
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
    APSARA_TEST_FALSE(config->Parse());

    // no Type
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "aggregators": [
                {
                    "type": "aggregator_context"
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
    APSARA_TEST_FALSE(config->Parse());

    // Type is not of type string
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "aggregators": [
                {
                    "Type": true
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
    APSARA_TEST_FALSE(config->Parse());

    // unsupported aggregator
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "aggregators": [
                {
                    "Type": "aggregator_unknown"
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
    APSARA_TEST_FALSE(config->Parse());

    // more than 1 aggregator
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "aggregators": [
                {
                    "Type": "aggregator_context"
                },
                {
                    "Type": "aggregator_default"
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

    // aggregator plugins exist in native flushing mode
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "aggregators": [
                {
                    "Type": "aggregator_context"
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
    APSARA_TEST_FALSE(config->Parse());
}

void PipelineConfigUnittest::HandleInvalidFlushers() const {
    unique_ptr<Json::Value> configJson;
    string configStr, errorMsg;
    unique_ptr<PipelineConfig> config;

    // no flushers
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // flushers is not of type array
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "flushers": {}
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // flushers is empty
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "flushers": []
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // flushers element is not of type object
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "flushers": [
                []
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // no Type
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "flushers": [
                {
                    "type": "flusher_sls"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // Type is not of type string
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "flushers": [
                {
                    "type": "flusher_sls"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // unsupported flusher
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_unknown"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
}

void PipelineConfigUnittest::HandleInvalidExtensions() const {
    unique_ptr<Json::Value> configJson;
    string configStr, errorMsg;
    unique_ptr<PipelineConfig> config;

    // extensions is not of type array
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls"
                }
            ],
            "extensions": {}
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // extensions element is not of type object
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls"
                }
            ],
            "extensions": [
                []
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // no Type
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls"
                }
            ],
            "extensions": [
                {
                    "type": "ext_basicauth"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // Type is not of type string
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls"
                }
            ],
            "extensions": [
                {
                    "type": true
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // unsupported extension
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls"
                }
            ],
            "extensions": [
                {
                    "Type": "ext_unknown"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // extension plugins exist when no extended plugin is given
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls"
                }
            ],
            "extensions": [
                {
                    "Type": "ext_basicauth"
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());
}

void PipelineConfigUnittest::TestReplaceEnvVarRef() const {
#if defined(__linux__)
    setenv("__path4ut", "_home/$work", 1);
    setenv("__file4ut", "!transaction/~un-do.log", 1);
#else
    _putenv_s("__path4ut", "_home/$work");
    _putenv_s("__file4ut", "!transaction/~un-do.log");
#endif

    unique_ptr<Json::Value> configJson;
    Json::Value resJson;
    string configStr, resStr, errorMsg;
    unique_ptr<PipelineConfig> config;

    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/${__path4ut}/${__file4ut}"
                    ]
                }
            ]
        }
    )";

    resStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": [
                        "/_home/$work/!transaction/~un-do.log"
                    ]
                }
            ]
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(resStr, resJson, errorMsg));
    config.reset(new PipelineConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->ReplaceEnvVar());
    APSARA_TEST_TRUE(*config->mDetail == resJson);
}

UNIT_TEST_CASE(PipelineConfigUnittest, HandleValidConfig)
UNIT_TEST_CASE(PipelineConfigUnittest, HandleInvalidCreateTime)
UNIT_TEST_CASE(PipelineConfigUnittest, HandleInvalidGlobal)
UNIT_TEST_CASE(PipelineConfigUnittest, HandleInvalidInputs)
UNIT_TEST_CASE(PipelineConfigUnittest, HandleInvalidProcessors)
UNIT_TEST_CASE(PipelineConfigUnittest, HandleInvalidAggregators)
UNIT_TEST_CASE(PipelineConfigUnittest, HandleInvalidFlushers)
UNIT_TEST_CASE(PipelineConfigUnittest, HandleInvalidExtensions)
UNIT_TEST_CASE(PipelineConfigUnittest, TestReplaceEnvVarRef)

} // namespace logtail

UNIT_TEST_MAIN
