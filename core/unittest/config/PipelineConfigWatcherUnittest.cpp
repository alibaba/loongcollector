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
    void TestPushPipelineConfig() const;

protected:
    static void SetUpTestCase() { PluginRegistry::GetInstance()->LoadPlugins(); }
    static void TearDownTestCase() { PluginRegistry::GetInstance()->UnloadPlugins(); }

private:
};

void PipelineConfigWatcherUnittest::TestPushPipelineConfig() const {
    PipelineConfigDiff pDiff;
    unique_ptr<Json::Value> configJson;
    string configStr, errorMsg;
    SingletonConfigCache singletonCache;

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
    PipelineConfig config1(configName1, std::move(configJson));
    config1.Parse();
    PipelineConfigWatcher::GetInstance()->PushPipelineConfig(
        std::move(config1), ConfigDiffEnum::Added, pDiff, singletonCache);
    APSARA_TEST_EQUAL_FATAL(1, pDiff.mAdded.size());

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
    PipelineConfig config2(configName2, std::move(configJson));
    config2.Parse();
    PipelineConfigWatcher::GetInstance()->PushPipelineConfig(
        std::move(config2), ConfigDiffEnum::Added, pDiff, singletonCache);
    APSARA_TEST_EQUAL_FATAL(2, pDiff.mAdded.size());

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
    PipelineConfig config3(configName3, std::move(configJson));
    config3.Parse();
    APSARA_TEST_EQUAL_FATAL("input_network_observer", config3.mSingletonInput);
    PipelineConfigWatcher::GetInstance()->PushPipelineConfig(
        std::move(config3), ConfigDiffEnum::Added, pDiff, singletonCache);
    APSARA_TEST_EQUAL_FATAL(singletonCache["input_network_observer"]->config.mName, configName3);

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
    PipelineConfig config4(configName4, std::move(configJson));
    config4.Parse();
    APSARA_TEST_EQUAL_FATAL("input_network_observer", config4.mSingletonInput);
    PipelineConfigWatcher::GetInstance()->PushPipelineConfig(
        std::move(config4), ConfigDiffEnum::Added, pDiff, singletonCache);
    APSARA_TEST_EQUAL_FATAL(singletonCache["input_network_observer"]->config.mName, configName4);

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
    std::string configName5 = "z-test5";
    PipelineConfig config5(configName5, std::move(configJson));
    config5.Parse();
    APSARA_TEST_EQUAL_FATAL("input_network_observer", config5.mSingletonInput);
    PipelineConfigWatcher::GetInstance()->PushPipelineConfig(
        std::move(config5), ConfigDiffEnum::Added, pDiff, singletonCache);
    APSARA_TEST_EQUAL_FATAL(singletonCache["input_network_observer"]->config.mName, configName4);
}


UNIT_TEST_CASE(PipelineConfigWatcherUnittest, TestPushPipelineConfig)

} // namespace logtail

UNIT_TEST_MAIN
