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
#include "config/watcher/ConfigWatcher.h"
#include "plugin/PluginRegistry.h"
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
        ConfigWatcher::GetInstance()->AddPipelineSource(configDir.string());
        ConfigWatcher::GetInstance()->AddProcessSource(processConfigDir.string());
    }

    void TearDown() override { ConfigWatcher::GetInstance()->ClearEnvironment(); }

private:
    static const filesystem::path configDir;
    static const filesystem::path processConfigDir;
};

const filesystem::path ConfigWatcherUnittest::configDir = "./config";
const filesystem::path ConfigWatcherUnittest::processConfigDir = "./processconfig";

void ConfigWatcherUnittest::InvalidConfigDirFound() const {
    {
    PipelineConfigDiff diff = ConfigWatcher::GetInstance()->CheckPipelineConfigDiff();
    APSARA_TEST_TRUE(diff.IsEmpty());

    { ofstream fout("config"); }
    diff = ConfigWatcher::GetInstance()->CheckPipelineConfigDiff();
    APSARA_TEST_TRUE(diff.IsEmpty());
    filesystem::remove("config");
    }
    {
        ProcessConfigDiff diff = ConfigWatcher::GetInstance()->CheckProcessConfigDiff();
        APSARA_TEST_TRUE(diff.IsEmpty());

        { ofstream fout("processconfig"); }
        diff = ConfigWatcher::GetInstance()->CheckProcessConfigDiff();
        APSARA_TEST_TRUE(diff.IsEmpty());
        filesystem::remove("processconfig");
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
    PipelineConfigDiff diff = ConfigWatcher::GetInstance()->CheckPipelineConfigDiff();
    APSARA_TEST_TRUE(diff.IsEmpty());
    filesystem::remove_all(configDir);
    }
    {
        filesystem::create_directories(processConfigDir);

        filesystem::create_directories(processConfigDir / "dir");
        { ofstream fout(processConfigDir / "unsupported_extenstion.zip"); }
        { ofstream fout(processConfigDir / "empty_file.json"); }
        {
            ofstream fout(processConfigDir / "invalid_format.json");
            fout << "[}";
        }
        ProcessConfigDiff diff = ConfigWatcher::GetInstance()->CheckProcessConfigDiff();
        APSARA_TEST_TRUE(diff.IsEmpty());
        filesystem::remove_all(processConfigDir);
    }
}

void ConfigWatcherUnittest::DuplicateConfigs() const {
    {
    PluginRegistry::GetInstance()->LoadPlugins();
    ConfigWatcher::GetInstance()->AddPipelineSource("dir1");
    ConfigWatcher::GetInstance()->AddPipelineSource("dir2");

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
    PipelineConfigDiff diff = ConfigWatcher::GetInstance()->CheckPipelineConfigDiff();
    APSARA_TEST_FALSE(diff.IsEmpty());
    APSARA_TEST_EQUAL(1U, diff.mAdded.size());

    filesystem::remove_all("dir1");
    filesystem::remove_all("dir2");
    filesystem::remove_all("config");
    PluginRegistry::GetInstance()->UnloadPlugins();
    }
    {
        PluginRegistry::GetInstance()->LoadPlugins();
        ConfigWatcher::GetInstance()->AddProcessSource("dir1");
        ConfigWatcher::GetInstance()->AddProcessSource("dir2");

        filesystem::create_directories("processconfig");
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
        ProcessConfigDiff diff = ConfigWatcher::GetInstance()->CheckProcessConfigDiff();
        APSARA_TEST_FALSE(diff.IsEmpty());
        APSARA_TEST_EQUAL(1U, diff.mAdded.size());

        filesystem::remove_all("dir1");
        filesystem::remove_all("dir2");
        filesystem::remove_all("processconfig");
        PluginRegistry::GetInstance()->UnloadPlugins();
    }
}

UNIT_TEST_CASE(ConfigWatcherUnittest, InvalidConfigDirFound)
UNIT_TEST_CASE(ConfigWatcherUnittest, InvalidConfigFileFound)
UNIT_TEST_CASE(ConfigWatcherUnittest, DuplicateConfigs)

} // namespace logtail

UNIT_TEST_MAIN
