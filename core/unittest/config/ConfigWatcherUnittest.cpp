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

#include <filesystem>
#include <fstream>

#include "config/ConfigDiff.h"
#include "config/watcher/InstanceConfigWatcher.h"
#include "config/watcher/PipelineConfigWatcher.h"
#include "pipeline/plugin/PluginRegistry.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class ConfigWatcherUnittest : public testing::Test {
public:
    void InvalidConfigDirFound() const;
    void InvalidConfigFileFound() const;
    void DuplicateConfigs() const;

protected:
    void SetUp() override {
        PipelineConfigWatcher::GetInstance()->AddSource(configDir.string());
        InstanceConfigWatcher::GetInstance()->AddSource(instanceConfigDir.string());
    }

    void TearDown() override { PipelineConfigWatcher::GetInstance()->ClearEnvironment(); }

private:
    static const filesystem::path configDir;
    static const filesystem::path instanceConfigDir;
};

const filesystem::path ConfigWatcherUnittest::configDir = "./continuous_pipeline_config";
const filesystem::path ConfigWatcherUnittest::instanceConfigDir = "./instance_config";

void ConfigWatcherUnittest::InvalidConfigDirFound() const {
    {
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL(1U, diff.first.mAdded.size());
        APSARA_TEST_TRUE(diff.second.IsEmpty());

        { ofstream fout("continuous_pipeline_config"); }
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_TRUE(diff.first.IsEmpty());
        APSARA_TEST_TRUE(diff.second.IsEmpty());
        filesystem::remove_all("continuous_pipeline_config");
    }
    {
        InstanceConfigDiff diff = InstanceConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_TRUE(diff.IsEmpty());

        { ofstream fout("instance_config"); }
        diff = InstanceConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_TRUE(diff.IsEmpty());
        filesystem::remove_all("instance_config");
    }
}

void ConfigWatcherUnittest::InvalidConfigFileFound() const {
    {
        filesystem::create_directories(configDir);

        filesystem::create_directories(configDir / "dir");
        { ofstream fout(configDir / "unsupported_extenstion.zip"); }
        { ofstream fout(configDir / "empty_file.json"); }
        {
            ofstream fout(configDir / "invalid_format.json");
            fout << "[}";
        }
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL(1U, diff.first.mAdded.size());
        APSARA_TEST_TRUE(diff.second.IsEmpty());
        filesystem::remove_all(configDir);
    }
    {
        filesystem::create_directories(instanceConfigDir);

        filesystem::create_directories(instanceConfigDir / "dir");
        { ofstream fout(instanceConfigDir / "unsupported_extenstion.zip"); }
        { ofstream fout(instanceConfigDir / "empty_file.json"); }
        {
            ofstream fout(instanceConfigDir / "invalid_format.json");
            fout << "[}";
        }
        InstanceConfigDiff diff = InstanceConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_TRUE(diff.IsEmpty());
        filesystem::remove_all(instanceConfigDir);
    }
}

void ConfigWatcherUnittest::DuplicateConfigs() const {
    {
        PluginRegistry::GetInstance()->LoadPlugins();
        PipelineConfigWatcher::GetInstance()->AddSource("dir1");
        PipelineConfigWatcher::GetInstance()->AddSource("dir2");

        filesystem::create_directories("config");
        filesystem::create_directories("dir1");
        filesystem::create_directories("dir2");

        {
            ofstream fout("dir1/config.json");
            fout << R"(
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
                ]
            }
        )";
        }
        { ofstream fout("dir2/config.json"); }
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_FALSE(diff.first.IsEmpty());
        APSARA_TEST_EQUAL(2U, diff.first.mAdded.size());

        filesystem::remove_all("dir1");
        filesystem::remove_all("dir2");
        filesystem::remove_all("config");
        PluginRegistry::GetInstance()->UnloadPlugins();
    }
    {
        PluginRegistry::GetInstance()->LoadPlugins();
        InstanceConfigWatcher::GetInstance()->AddSource("dir1");
        InstanceConfigWatcher::GetInstance()->AddSource("dir2");

        filesystem::create_directories("instance_config");
        filesystem::create_directories("dir1");
        filesystem::create_directories("dir2");

        {
            ofstream fout("dir1/config.json");
            fout << R"(
            {
                "enable": true,
                "max_bytes_per_sec": 1234,
                "mem_usage_limit": 456,
                "cpu_usage_limit": 2
            }
        )";
        }
        { ofstream fout("dir2/config.json"); }
        InstanceConfigDiff diff = InstanceConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_FALSE(diff.IsEmpty());
        APSARA_TEST_EQUAL(1U, diff.mAdded.size());

        filesystem::remove_all("dir1");
        filesystem::remove_all("dir2");
        filesystem::remove_all("instance_config");
        PluginRegistry::GetInstance()->UnloadPlugins();
    }
}

UNIT_TEST_CASE(ConfigWatcherUnittest, InvalidConfigDirFound)
UNIT_TEST_CASE(ConfigWatcherUnittest, InvalidConfigFileFound)
UNIT_TEST_CASE(ConfigWatcherUnittest, DuplicateConfigs)

} // namespace logtail

UNIT_TEST_MAIN
