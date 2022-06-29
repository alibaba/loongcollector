// Copyright 2022 iLogtail Authors
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

#include "unittest/Unittest.h"
#include <assert.h>
#if defined(__linux__)
#include <unistd.h>
#include <signal.h>
#include <sys/inotify.h>
#include <fnmatch.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <string>
#include <errno.h>
#include <typeinfo>
#include <iostream>
#include <fstream>
#include <memory>
#include <thread>
#include <set>
#include <json/json.h>
#include <fstream>
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <assert.h>
#include "config_manager/ConfigYamlToJson.h"
#include "config_manager/ConfigManager.h"

namespace logtail {

class ConfigYamlToJsonUnittest : public ::testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}

    void CaseSetUp() {}
    void CaseCleanUp() {}

    void generateYamlConfig(const string pluginCategory,
                            vector<string> pluginTypeVec,
                            YAML::Node& yamlConfig,
                            bool invalidType = false) {
        YAML::Node inputsConfig;
        for (string pluginType : pluginTypeVec) {
            YAML::Node inputConfig;
            if (invalidType) {
                inputConfig["type"] = pluginType;
            } else {
                inputConfig["Type"] = pluginType;
            }

            inputsConfig.push_back(inputConfig);
        }

        yamlConfig[pluginCategory] = inputsConfig;
    }

    void TestYamlToJsonForCheckConfig() {
        LOG_INFO(sLogger, ("TestYamlToJsonForCheckConfig() begin", time(NULL)));

        WorkMode workMode;
        YAML::Node yamlConfig;
        bool ret;
        generateYamlConfig("inputs", {"file_log"}, yamlConfig);
        generateYamlConfig("flushers", {"flusher_sls"}, yamlConfig);
        ret = ConfigYamlToJson::GetInstance()->CheckPluginConfig("", yamlConfig, workMode);
        EXPECT_EQ(ret, true);
        EXPECT_EQ(workMode.mIsFileMode, true);
        EXPECT_EQ(workMode.mInputPluginType, "file_log");
        EXPECT_EQ(workMode.mHasAccelerateProcessor, false);
        EXPECT_EQ(workMode.mAccelerateProcessorPluginType, "");
        EXPECT_EQ(workMode.mLogSplitProcessorPluginType, "");
        EXPECT_EQ(workMode.mLogType, "common_reg_log");

        yamlConfig.reset();
        generateYamlConfig("inputs", {"file_log"}, yamlConfig);
        generateYamlConfig("processors", {"processor_regex_accelerate"}, yamlConfig);
        generateYamlConfig("flushers", {"flusher_sls"}, yamlConfig);
        ret = ConfigYamlToJson::GetInstance()->CheckPluginConfig("", yamlConfig, workMode);
        EXPECT_EQ(ret, true);
        EXPECT_EQ(workMode.mIsFileMode, true);
        EXPECT_EQ(workMode.mInputPluginType, "file_log");
        EXPECT_EQ(workMode.mHasAccelerateProcessor, true);
        EXPECT_EQ(workMode.mAccelerateProcessorPluginType, "processor_regex_accelerate");
        EXPECT_EQ(workMode.mLogSplitProcessorPluginType, "");
        EXPECT_EQ(workMode.mLogType, "common_reg_log");

        yamlConfig.reset();
        generateYamlConfig("inputs", {"file_log"}, yamlConfig);
        generateYamlConfig("processors", {"processor_split_log_regex", "processor_regex"}, yamlConfig);
        generateYamlConfig("flushers", {"flusher_sls"}, yamlConfig);
        ret = ConfigYamlToJson::GetInstance()->CheckPluginConfig("", yamlConfig, workMode);
        EXPECT_EQ(ret, true);
        EXPECT_EQ(workMode.mIsFileMode, true);
        EXPECT_EQ(workMode.mInputPluginType, "file_log");
        EXPECT_EQ(workMode.mHasAccelerateProcessor, false);
        EXPECT_EQ(workMode.mAccelerateProcessorPluginType, "");
        EXPECT_EQ(workMode.mLogSplitProcessorPluginType, "processor_split_log_regex");
        EXPECT_EQ(workMode.mLogType, "common_reg_log");

        yamlConfig.reset();
        generateYamlConfig("inputs", {"service_syslog", "service_syslog"}, yamlConfig);
        generateYamlConfig("flushers", {"flusher_sls"}, yamlConfig);
        ret = ConfigYamlToJson::GetInstance()->CheckPluginConfig("", yamlConfig, workMode);
        EXPECT_EQ(ret, true);
        EXPECT_EQ(workMode.mIsFileMode, false);
        EXPECT_EQ(workMode.mInputPluginType, "service_syslog");
        EXPECT_EQ(workMode.mHasAccelerateProcessor, false);
        EXPECT_EQ(workMode.mAccelerateProcessorPluginType, "");
        EXPECT_EQ(workMode.mLogSplitProcessorPluginType, "");
        EXPECT_EQ(workMode.mLogType, "plugin");

        yamlConfig.reset();
        generateYamlConfig("inputs", {"file_log"}, yamlConfig);
        generateYamlConfig("processors", {"processor_json_accelerate"}, yamlConfig);
        generateYamlConfig("flushers", {"flusher_sls"}, yamlConfig);
        ret = ConfigYamlToJson::GetInstance()->CheckPluginConfig("", yamlConfig, workMode);
        EXPECT_EQ(ret, true);
        EXPECT_EQ(workMode.mIsFileMode, true);
        EXPECT_EQ(workMode.mInputPluginType, "file_log");
        EXPECT_EQ(workMode.mHasAccelerateProcessor, true);
        EXPECT_EQ(workMode.mAccelerateProcessorPluginType, "processor_json_accelerate");
        EXPECT_EQ(workMode.mLogSplitProcessorPluginType, "");
        EXPECT_EQ(workMode.mLogType, "json_log");

        yamlConfig.reset();
        generateYamlConfig("inputs", {"file_log"}, yamlConfig);
        generateYamlConfig("processors", {"processor_delimiter_accelerate"}, yamlConfig);
        generateYamlConfig("flushers", {"flusher_sls"}, yamlConfig);
        ret = ConfigYamlToJson::GetInstance()->CheckPluginConfig("", yamlConfig, workMode);
        EXPECT_EQ(ret, true);
        EXPECT_EQ(workMode.mIsFileMode, true);
        EXPECT_EQ(workMode.mInputPluginType, "file_log");
        EXPECT_EQ(workMode.mHasAccelerateProcessor, true);
        EXPECT_EQ(workMode.mAccelerateProcessorPluginType, "processor_delimiter_accelerate");
        EXPECT_EQ(workMode.mLogSplitProcessorPluginType, "");
        EXPECT_EQ(workMode.mLogType, "delimiter_log");

        // InputPluginsInfo size is not 1.
        yamlConfig.reset();
        generateYamlConfig("inputs", {"file_log", "service_syslog"}, yamlConfig);
        generateYamlConfig("flushers", {"flusher_sls"}, yamlConfig);
        ret = ConfigYamlToJson::GetInstance()->CheckPluginConfig("", yamlConfig, workMode);
        EXPECT_EQ(ret, false);

        // // service_syslog and processor_regex_accelerate are not matched
        yamlConfig.reset();
        generateYamlConfig("inputs", {"service_syslog", "service_syslog"}, yamlConfig);
        generateYamlConfig("processors", {"processor_regex_accelerate"}, yamlConfig);
        generateYamlConfig("flushers", {"flusher_sls"}, yamlConfig);
        ret = ConfigYamlToJson::GetInstance()->CheckPluginConfig("", yamlConfig, workMode);
        EXPECT_EQ(ret, false);

        // do not config flusher
        yamlConfig.reset();
        generateYamlConfig("inputs", {"file_log"}, yamlConfig);
        generateYamlConfig("processors", {"processor_regex_accelerate"}, yamlConfig);
        ret = ConfigYamlToJson::GetInstance()->CheckPluginConfig("", yamlConfig, workMode);
        EXPECT_EQ(ret, false);

        // inputs have invalid config
        yamlConfig.reset();
        generateYamlConfig("inputs", {"file_log"}, yamlConfig, true);
        generateYamlConfig("processors", {"processor_regex_accelerate"}, yamlConfig);
        generateYamlConfig("flushers", {"flusher_sls"}, yamlConfig);
        ret = ConfigYamlToJson::GetInstance()->CheckPluginConfig("", yamlConfig, workMode);
        EXPECT_EQ(ret, false);

        // processor_split_log_regex is not the first processor
        yamlConfig.reset();
        generateYamlConfig("inputs", {"file_log"}, yamlConfig);
        generateYamlConfig("processors", {"processor_regex", "processor_split_log_regex"}, yamlConfig);
        generateYamlConfig("flushers", {"flusher_sls"}, yamlConfig);
        ret = ConfigYamlToJson::GetInstance()->CheckPluginConfig("", yamlConfig, workMode);
        EXPECT_EQ(ret, false);
    }

    // input: plugin; processor: plugin
    void TestYamlToJsonForPluginMode() {
        LOG_INFO(sLogger, ("TestYamlToJsonForPluginMode() begin", time(NULL)));

        Json::Value inputJsonConfig;
        const std::string file = "testConfigDir/plugin.yaml";
        YAML::Node yamlConfig = YAML::LoadFile(file);

        Json::Value userLocalJsonConfig;
        ConfigYamlToJson::GetInstance()->GenerateLocalJsonConfig(file, yamlConfig, userLocalJsonConfig);

        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["enable"].asBool(), true);
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["log_type"].asString(), "plugin");
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["inputs"].size(), 1);
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["inputs"][0]["type"],
                  "service_input_example");
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["inputs"][0]["detail"].size(), 0);
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["processors"].size(), 2);
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["processors"][0]["type"],
                  "processor_add_fields");
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["processors"][1]["type"],
                  "processor_drop");
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["flushers"].size(), 2);
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["flushers"][0]["type"], "flusher_stdout");
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["flushers"][1]["type"], "flusher_sls");
    }

    // input: file_reg; processor: plugin
    void TestYamlToJsonForFileRegMode() {
        LOG_INFO(sLogger, ("TestYamlToJsonForFileRegMode() begin", time(NULL)));

        Json::Value inputJsonConfig;
        const std::string file = "testConfigDir/file_reg.yaml";
        YAML::Node yamlConfig = YAML::LoadFile(file);

        Json::Value userLocalJsonConfig;
        ConfigYamlToJson::GetInstance()->GenerateLocalJsonConfig(file, yamlConfig, userLocalJsonConfig);

        ConfigManager::GetInstance()->LoadJsonConfig(userLocalJsonConfig, false);

        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["enable"].asBool(), true);
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["max_depth"].asInt(), 10);
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["keys"][0].asString(), "content");
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["regex"][0].asString(), "(.*)");
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["log_type"].asString(), "common_reg_log");
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["inputs"].size(), 0);
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["processors"].size(), 2);
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["processors"][0]["type"],
                  "processor_split_log_string");
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["processors"][1]["type"],
                  "processor_regex");
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["flushers"].size(), 2);
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["flushers"][0]["type"], "flusher_sls");
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["flushers"][1]["type"], "flusher_stdout");
    }

    // input: file_json; processor: plugin
    void TestYamlToJsonForFileJsonMode() {
        LOG_INFO(sLogger, ("TestYamlToJsonForFileJsonMode() begin", time(NULL)));

        Json::Value inputJsonConfig;
        const std::string file = "testConfigDir/file_json.yaml";
        YAML::Node yamlConfig = YAML::LoadFile(file);

        Json::Value userLocalJsonConfig;
        ConfigYamlToJson::GetInstance()->GenerateLocalJsonConfig(file, yamlConfig, userLocalJsonConfig);

        ConfigManager::GetInstance()->LoadJsonConfig(userLocalJsonConfig, false);

        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["enable"].asBool(), true);
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["max_depth"].asInt(), 10);
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["log_type"].asString(), "common_reg_log");
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["inputs"].size(), 0);
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["processors"].size(), 2);
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["processors"][0]["type"],
                  "processor_split_log_string");
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["processors"][1]["type"],
                  "processor_json");
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["flushers"].size(), 2);
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["flushers"][0]["type"], "flusher_sls");
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["flushers"][1]["type"], "flusher_stdout");
    }

    // input: file_json; processor: plugin
    void TestYamlToJsonForFileDelimiterMode() {
        LOG_INFO(sLogger, ("TestYamlToJsonForFileDelimiterMode() begin", time(NULL)));

        Json::Value inputJsonConfig;
        const std::string file = "testConfigDir/file_json.yaml";
        YAML::Node yamlConfig = YAML::LoadFile(file);

        Json::Value userLocalJsonConfig;
        ConfigYamlToJson::GetInstance()->GenerateLocalJsonConfig(file, yamlConfig, userLocalJsonConfig);

        ConfigManager::GetInstance()->LoadJsonConfig(userLocalJsonConfig, false);

        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["enable"].asBool(), true);
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["max_depth"].asInt(), 10);
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["log_type"].asString(), "common_reg_log");
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["inputs"].size(), 0);
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["processors"].size(), 2);
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["processors"][0]["type"],
                  "processor_split_log_string");
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["processors"][1]["type"],
                  "processor_json");
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["flushers"].size(), 2);
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["flushers"][0]["type"], "flusher_sls");
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["plugin"]["flushers"][1]["type"], "flusher_stdout");
    }

    // input: file; processor: accelerate
    void TestYamlToJsonForFileRegexAccelerateMode() {
        LOG_INFO(sLogger, ("TestYamlToJsonForFileRegexAccelerateMode() begin", time(NULL)));

        Json::Value inputJsonConfig;
        const std::string file = "testConfigDir/file_regex_accelerate.yaml";
        YAML::Node yamlConfig = YAML::LoadFile(file);

        Json::Value userLocalJsonConfig;
        ConfigYamlToJson::GetInstance()->GenerateLocalJsonConfig(file, yamlConfig, userLocalJsonConfig);

        ConfigManager::GetInstance()->LoadJsonConfig(userLocalJsonConfig, false);

        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["enable"].asBool(), true);
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["max_depth"].asInt(), 10);
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["keys"][0].asString(), "content");
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["regex"][0].asString(), "(.*)");
        EXPECT_EQ(userLocalJsonConfig["metrics"]["config#" + file]["log_type"].asString(), "common_reg_log");
    }
};

TEST_F(ConfigYamlToJsonUnittest, TestYamlToJsonForCheckConfig) {
    TestYamlToJsonForCheckConfig();
}

TEST_F(ConfigYamlToJsonUnittest, TestYamlToJsonForPluginMode) {
    TestYamlToJsonForPluginMode();
}

TEST_F(ConfigYamlToJsonUnittest, TestYamlToJsonForFileRegMode) {
    TestYamlToJsonForFileRegMode();
}

TEST_F(ConfigYamlToJsonUnittest, TestYamlToJsonForFileJsonMode) {
    TestYamlToJsonForFileJsonMode();
}

TEST_F(ConfigYamlToJsonUnittest, TestYamlToJsonForFileDelimiterMode) {
    TestYamlToJsonForFileDelimiterMode();
}

TEST_F(ConfigYamlToJsonUnittest, TestYamlToJsonForFileRegexAccelerateMode) {
    TestYamlToJsonForFileRegexAccelerateMode();
}

} // end of namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
