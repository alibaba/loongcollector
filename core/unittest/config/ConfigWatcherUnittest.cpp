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
    void SetUp() override { ConfigWatcher::GetInstance()->AddSource(configDir.string()); }

    void TearDown() override { ConfigWatcher::GetInstance()->ClearEnvironment(); }

private:
    static const filesystem::path configDir;
};

const filesystem::path ConfigWatcherUnittest::configDir = "./config";

void ConfigWatcherUnittest::InvalidConfigDirFound() const {
    ConfigDiff diff = ConfigWatcher::GetInstance()->CheckConfigDiff();
    APSARA_TEST_TRUE(diff.IsEmpty());

    { ofstream fout("config"); }
    diff = ConfigWatcher::GetInstance()->CheckConfigDiff();
    APSARA_TEST_TRUE(diff.IsEmpty());
    filesystem::remove("config");
}

void ConfigWatcherUnittest::InvalidConfigFileFound() const {
    filesystem::create_directories(configDir);

    filesystem::create_directories(configDir / "dir");
    { ofstream fout(configDir / "unsupported_extenstion.zip"); }
    { ofstream fout(configDir / "empty_file.json"); }
    {
        ofstream fout(configDir / "invalid_format.json");
        fout << "[}";
    }
    ConfigDiff diff = ConfigWatcher::GetInstance()->CheckConfigDiff();
    APSARA_TEST_TRUE(diff.IsEmpty());
    filesystem::remove_all(configDir);
}

void ConfigWatcherUnittest::DuplicateConfigs() const {
    PluginRegistry::GetInstance()->LoadPlugins();
    ConfigWatcher::GetInstance()->AddSource("dir1");
    ConfigWatcher::GetInstance()->AddSource("dir2");

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
    ConfigDiff diff = ConfigWatcher::GetInstance()->CheckConfigDiff();
    APSARA_TEST_FALSE(diff.IsEmpty());
    APSARA_TEST_EQUAL(1U, diff.mAdded.size());

    filesystem::remove_all("dir1");
    filesystem::remove_all("dir2");
    filesystem::remove_all("config");
    PluginRegistry::GetInstance()->UnloadPlugins();
}

UNIT_TEST_CASE(ConfigWatcherUnittest, InvalidConfigDirFound)
UNIT_TEST_CASE(ConfigWatcherUnittest, InvalidConfigFileFound)
UNIT_TEST_CASE(ConfigWatcherUnittest, DuplicateConfigs)

} // namespace logtail

UNIT_TEST_MAIN
