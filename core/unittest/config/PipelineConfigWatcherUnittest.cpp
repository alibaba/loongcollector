// Copyright 2024 iLogtail Authors
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
#include "config/watcher/PipelineConfigWatcher.h"
#include "plugin/PluginRegistry.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class PipelineConfigWatcherUnittest : public testing::Test {
public:
    void TestPreCheckConfig() const;
    void TestPreCheckConfigWithInvalidConfig() const;

protected:
    static void SetUpTestCase() { PluginRegistry::GetInstance()->LoadPlugins(); }
    static void TearDownTestCase() { PluginRegistry::GetInstance()->UnloadPlugins(); }

private:
};

void PipelineConfigWatcherUnittest::TestPreCheckConfig() const {
    unique_ptr<Json::Value> configJson;
    string configStr, errorMsg;
    std::unordered_map<std::string, ConfigWithPath> toBeDiffedConfigs;
    std::unordered_map<std::string, ConfigPriority> singletonConfigs;

    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    std::string configName1 = "test1";
    PipelineConfigWatcher::GetInstance()->PreCheckConfig(
        configName1, std::filesystem::path(), std::move(configJson), toBeDiffedConfigs, singletonConfigs);
    APSARA_TEST_EQUAL_FATAL(1, toBeDiffedConfigs.size());

    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                }
            ],
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    std::string configName2 = "test2";
    PipelineConfigWatcher::GetInstance()->PreCheckConfig(
        configName2, std::filesystem::path(), std::move(configJson), toBeDiffedConfigs, singletonConfigs);
    APSARA_TEST_EQUAL_FATAL(2, toBeDiffedConfigs.size());

    // case: singleton input
    configStr = R"(
        {
            "createTime": 123456,
            "inputs": [
                {
                    "Type": "input_network_observer"
                }
            ],
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    std::string configName3 = "test3";
    PipelineConfigWatcher::GetInstance()->PreCheckConfig(
        configName3, std::filesystem::path(), std::move(configJson), toBeDiffedConfigs, singletonConfigs);
    APSARA_TEST_EQUAL_FATAL(3, toBeDiffedConfigs.size());
    APSARA_TEST_EQUAL_FATAL(singletonConfigs["input_network_observer"].second, configName3);

    // case: compare by create time
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_network_observer"
                }
            ],
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    std::string configName4 = "test4";
    PipelineConfigWatcher::GetInstance()->PreCheckConfig(
        configName4, std::filesystem::path(), std::move(configJson), toBeDiffedConfigs, singletonConfigs);
    APSARA_TEST_EQUAL_FATAL(3, toBeDiffedConfigs.size());
    APSARA_TEST_EQUAL_FATAL(singletonConfigs["input_network_observer"].second, configName4);

    // case: compare by name
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_network_observer"
                }
            ],
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    std::string configName5 = "a-test5";
    PipelineConfigWatcher::GetInstance()->PreCheckConfig(
        configName5, std::filesystem::path(), std::move(configJson), toBeDiffedConfigs, singletonConfigs);
    APSARA_TEST_EQUAL_FATAL(3, toBeDiffedConfigs.size());
    APSARA_TEST_EQUAL_FATAL(singletonConfigs["input_network_observer"].second, configName5);

    // case: load config with singleton input and not singleton input
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_file"
                },
                {
                    "Type": "input_network_observer"
                }
            ],
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    std::string configName6 = "ab-test6";
    PipelineConfigWatcher::GetInstance()->PreCheckConfig(
        configName6, std::filesystem::path(), std::move(configJson), toBeDiffedConfigs, singletonConfigs);
    APSARA_TEST_EQUAL_FATAL(3, toBeDiffedConfigs.size());
    APSARA_TEST_EQUAL_FATAL(singletonConfigs["input_network_observer"].second, configName5);

    // case: load config with different singleton input
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_network_security"
                }
            ],
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    std::string configName7 = "test7";
    PipelineConfigWatcher::GetInstance()->PreCheckConfig(
        configName7, std::filesystem::path(), std::move(configJson), toBeDiffedConfigs, singletonConfigs);
    APSARA_TEST_EQUAL_FATAL(4, toBeDiffedConfigs.size());
    APSARA_TEST_EQUAL_FATAL(singletonConfigs["input_network_security"].second, configName7);

    // case: load config with two singleton input, one valid and one invalid
    configStr = R"(
        {
            "inputs": [
                {
                    "Type": "input_network_observer"
                },
                {
                    "Type": "input_network_security"
                }
            ],
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    std::string configName8 = "a-test8";
    PipelineConfigWatcher::GetInstance()->PreCheckConfig(
        configName8, std::filesystem::path(), std::move(configJson), toBeDiffedConfigs, singletonConfigs);
    APSARA_TEST_EQUAL_FATAL(4, toBeDiffedConfigs.size());
    APSARA_TEST_EQUAL_FATAL(singletonConfigs["input_network_observer"].second, configName5);
    APSARA_TEST_EQUAL_FATAL(singletonConfigs["input_network_security"].second, configName7);
}

void PipelineConfigWatcherUnittest::TestPreCheckConfigWithInvalidConfig() const {
    {
        unique_ptr<Json::Value> configJson;
        string configStr, errorMsg;
        std::unordered_map<std::string, ConfigWithPath> toBeDiffedConfigs;
        std::unordered_map<std::string, ConfigPriority> singletonConfigs;

        configStr = R"(
        {
        }
        )";
        configJson.reset(new Json::Value());
        APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
        std::string configName1 = "test1";
        PipelineConfigWatcher::GetInstance()->PreCheckConfig(
            configName1, std::filesystem::path(), std::move(configJson), toBeDiffedConfigs, singletonConfigs);
        APSARA_TEST_EQUAL_FATAL(1, toBeDiffedConfigs.size());
    }
    {
        unique_ptr<Json::Value> configJson;
        string configStr, errorMsg;
        std::unordered_map<std::string, ConfigWithPath> toBeDiffedConfigs;
        std::unordered_map<std::string, ConfigPriority> singletonConfigs;

        configStr = R"(
        {
            "inputs": 1
        }
        )";
        configJson.reset(new Json::Value());
        APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
        std::string configName1 = "test1";
        PipelineConfigWatcher::GetInstance()->PreCheckConfig(
            configName1, std::filesystem::path(), std::move(configJson), toBeDiffedConfigs, singletonConfigs);
        APSARA_TEST_EQUAL_FATAL(1, toBeDiffedConfigs.size());
    }
    {
        unique_ptr<Json::Value> configJson;
        string configStr, errorMsg;
        std::unordered_map<std::string, ConfigWithPath> toBeDiffedConfigs;
        std::unordered_map<std::string, ConfigPriority> singletonConfigs;

        configStr = R"(
        {
            "inputs": [],
        }
        )";
        configJson.reset(new Json::Value());
        APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
        std::string configName1 = "test1";
        PipelineConfigWatcher::GetInstance()->PreCheckConfig(
            configName1, std::filesystem::path(), std::move(configJson), toBeDiffedConfigs, singletonConfigs);
        APSARA_TEST_EQUAL_FATAL(1, toBeDiffedConfigs.size());
    }
    {
        unique_ptr<Json::Value> configJson;
        string configStr, errorMsg;
        std::unordered_map<std::string, ConfigWithPath> toBeDiffedConfigs;
        std::unordered_map<std::string, ConfigPriority> singletonConfigs;

        configStr = R"(
        {
            "inputs": [
                {
                }
            ],
        }
        )";
        configJson.reset(new Json::Value());
        APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
        std::string configName1 = "test1";
        PipelineConfigWatcher::GetInstance()->PreCheckConfig(
            configName1, std::filesystem::path(), std::move(configJson), toBeDiffedConfigs, singletonConfigs);
        APSARA_TEST_EQUAL_FATAL(1, toBeDiffedConfigs.size());
    }
    {
        unique_ptr<Json::Value> configJson;
        string configStr, errorMsg;
        std::unordered_map<std::string, ConfigWithPath> toBeDiffedConfigs;
        std::unordered_map<std::string, ConfigPriority> singletonConfigs;

        configStr = R"(
        {
            "inputs": [
                {
                    "Type": 1
                }
            ],
        }
        )";
        configJson.reset(new Json::Value());
        APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
        std::string configName1 = "test1";
        PipelineConfigWatcher::GetInstance()->PreCheckConfig(
            configName1, std::filesystem::path(), std::move(configJson), toBeDiffedConfigs, singletonConfigs);
        APSARA_TEST_EQUAL_FATAL(1, toBeDiffedConfigs.size());
    }
}

UNIT_TEST_CASE(PipelineConfigWatcherUnittest, TestPreCheckConfig)
UNIT_TEST_CASE(PipelineConfigWatcherUnittest, TestPreCheckConfigWithInvalidConfig)

} // namespace logtail

UNIT_TEST_MAIN
