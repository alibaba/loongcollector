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
#include "unittest/Unittest.h"
#include "config_manager/ConfigManager.h"
#include "pipeline/PipelineManager.h"
#include "plugin/PluginRegistry.h"

DECLARE_FLAG_STRING(user_log_config);
using namespace std;

namespace logtail {

class PipelineManagerUnittest : public ::testing::Test {
public:
    static void SetUpTestCase() { PluginRegistry::GetInstance()->LoadPlugins(); }

    void SetUp() override {
        std::string configName = "project##config_0";
        Config* config = new Config;
        config->mConfigName = configName;
        config->mLogType = JSON_LOG;
        ConfigManager::GetInstance()->mNameConfigMap[configName] = config;
    }

    void TearDown() override { ConfigManager::GetInstance()->RemoveAllConfigs(); }

    void TestLoadAllPipelines();
};

APSARA_UNIT_TEST_CASE(PipelineManagerUnittest, TestLoadAllPipelines, 0);

void PipelineManagerUnittest::TestLoadAllPipelines() {
    string config = "{\"filters\" : [{\"project_name\" : \"123_proj\", \"category\" : \"test\", \"keys\" : [\"key1\", "
                    "\"key2\"], \"regs\" : [\"value1\",\"value2\"]}, {\"project_name\" : \"456_proj\", \"category\" : "
                    "\"test_1\", \"keys\" : [\"key1\", \"key2\"], \"regs\" : [\"value1\",\"value2\"]}]}";
    string path = GetProcessExecutionDir() + "user_log_config.json";
    ofstream file(path.c_str());
    file << config;
    file.close();

    auto manager = PipelineManager::GetInstance();
    manager->LoadAllPipelines();
    APSARA_TEST_NOT_EQUAL_FATAL(nullptr, manager->FindPipelineByName("project##config_0").get());
    APSARA_TEST_EQUAL_FATAL(nullptr, manager->FindPipelineByName("project##config_00").get());
    manager->RemoveAllPipelines();
    APSARA_TEST_EQUAL_FATAL(nullptr, manager->FindPipelineByName("project##config_0").get());
}

} // namespace logtail

UNIT_TEST_MAIN