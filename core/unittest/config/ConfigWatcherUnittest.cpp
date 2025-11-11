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

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "collection_pipeline/CollectionPipelineManager.h"
#include "config/ConfigDiff.h"
#include "config/watcher/InstanceConfigWatcher.h"
#include "config/watcher/PipelineConfigWatcher.h"
#include "unittest/Unittest.h"
#include "unittest/plugin/PluginMock.h"
#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#endif

using namespace std;

namespace logtail {

class ConfigWatcherUnittest : public testing::Test {
public:
    void InvalidConfigDirFound() const;
    void InvalidConfigFileFound() const;
    void DuplicateConfigs() const;
    void IgnoreNewLowerPrioritySingletonConfig() const;
    void IgnoreModifiedLowerPrioritySingletonConfig() const;
    void HigherPriorityOverrideLowerPrioritySingletonConfig() const;

protected:
    static void SetUpTestCase() {
        PluginRegistry::GetInstance()->LoadPlugins();
        LoadPluginMock();
    }

    static void TearDownTestCase() { PluginRegistry::GetInstance()->UnloadPlugins(); }

    void SetUp() override {
        PipelineConfigWatcher::GetInstance()->AddSource(configDir.string());
        InstanceConfigWatcher::GetInstance()->AddSource(instanceConfigDir.string());
    }

    void TearDown() override {
        PipelineConfigWatcher::GetInstance()->ClearEnvironment();
        CollectionPipelineManager::GetInstance()->ClearAllPipelines();
    }

private:
    static const filesystem::path configDir;
    static const filesystem::path instanceConfigDir;
};

const filesystem::path ConfigWatcherUnittest::configDir = "./continuous_pipeline_config";
const filesystem::path ConfigWatcherUnittest::instanceConfigDir = "./instance_config";

void ConfigWatcherUnittest::InvalidConfigDirFound() const {
    {
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        size_t builtinPipelineCnt = 0;
#ifdef __ENTERPRISE__
        builtinPipelineCnt += EnterpriseConfigProvider::GetInstance()->GetAllBuiltInPipelineConfigs().size();
#endif
        APSARA_TEST_EQUAL(0U + builtinPipelineCnt, diff.first.mAdded.size());
        APSARA_TEST_FALSE(diff.second.HasDiff());

        { ofstream fout("continuous_pipeline_config"); }
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_FALSE(diff.first.HasDiff());
        APSARA_TEST_FALSE(diff.second.HasDiff());
        filesystem::remove_all("continuous_pipeline_config");
    }
    {
        InstanceConfigDiff diff = InstanceConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_FALSE(diff.HasDiff());

        { ofstream fout("instance_config"); }
        diff = InstanceConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_FALSE(diff.HasDiff());
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
        size_t builtinPipelineCnt = 0;
#ifdef __ENTERPRISE__
        builtinPipelineCnt += EnterpriseConfigProvider::GetInstance()->GetAllBuiltInPipelineConfigs().size();
#endif
        APSARA_TEST_EQUAL(0U + builtinPipelineCnt, diff.first.mAdded.size());
        APSARA_TEST_FALSE(diff.second.HasDiff());
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
        APSARA_TEST_FALSE(diff.HasDiff());
        filesystem::remove_all(instanceConfigDir);
    }
}

void ConfigWatcherUnittest::DuplicateConfigs() const {
    {
        PipelineConfigWatcher::GetInstance()->AddSource("dir1");
        PipelineConfigWatcher::GetInstance()->AddSource("dir2");

        filesystem::create_directories("continuous_pipeline_config");
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
        size_t builtinPipelineCnt = 0;
#ifdef __ENTERPRISE__
        builtinPipelineCnt += EnterpriseConfigProvider::GetInstance()->GetAllBuiltInPipelineConfigs().size();
#endif
        APSARA_TEST_TRUE(diff.first.HasDiff());
        APSARA_TEST_EQUAL(1U + builtinPipelineCnt, diff.first.mAdded.size());

        filesystem::remove_all("dir1");
        filesystem::remove_all("dir2");
        filesystem::remove_all("continuous_pipeline_config");
    }
    {
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
        APSARA_TEST_TRUE(diff.HasDiff());
        APSARA_TEST_EQUAL(1U, diff.mAdded.size());

        filesystem::remove_all("dir1");
        filesystem::remove_all("dir2");
        filesystem::remove_all("instance_config");
    }
}

void ConfigWatcherUnittest::IgnoreNewLowerPrioritySingletonConfig() const {
    filesystem::create_directories("continuous_pipeline_config");
    size_t builtinPipelineCnt = 0;
#ifdef __ENTERPRISE__
    builtinPipelineCnt += EnterpriseConfigProvider::GetInstance()->GetAllBuiltInPipelineConfigs().size();
#endif
    // Step 1: add high priority config 'a' (createTime smaller => higher priority)
    {
        ofstream fout("continuous_pipeline_config/a.json");
        fout << R"(
            {
                "createTime": 1,
                "inputs": [
                    { "Type": "input_singleton_mock_1" }
                ],
                "flushers": [
                    { "Type": "flusher_mock" }
                ]
            }
        )";
    }

    auto diff1 = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
    // a should be added, and not ignored/removed; no modified expected
    APSARA_TEST_EQUAL(1U + builtinPipelineCnt, diff1.first.mAdded.size());
    APSARA_TEST_EQUAL(0U, diff1.first.mModified.size());
    APSARA_TEST_EQUAL(0U, diff1.first.mRemoved.size());
    APSARA_TEST_EQUAL(0U, diff1.first.mIgnored.size());
    APSARA_TEST_TRUE(std::find_if(diff1.first.mAdded.begin(),
                                  diff1.first.mAdded.end(),
                                  [](const CollectionConfig& c) { return c.mName == "a"; })
                     != diff1.first.mAdded.end());

    // Apply the config to manager so it becomes a running pipeline
    CollectionPipelineManager::GetInstance()->UpdatePipelines(diff1.first);

    // Step 2: add lower priority config 'b'
    {
        ofstream fout("continuous_pipeline_config/b.json");
        fout << R"(
            {
                "createTime": 2,
                "inputs": [
                    { "Type": "input_singleton_mock_1" }
                ],
                "flushers": [
                    { "Type": "flusher_mock" }
                ]
            }
        )";
    }

    auto diff2 = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
    // 'b' should be ignored (same singleton type, lower priority)
    APSARA_TEST_EQUAL(0U, diff2.first.mAdded.size());
    APSARA_TEST_EQUAL(0U, diff2.first.mModified.size());
    APSARA_TEST_EQUAL(0U, diff2.first.mRemoved.size());
    APSARA_TEST_EQUAL(1U, diff2.first.mIgnored.size());
    APSARA_TEST_TRUE(std::find(diff2.first.mIgnored.begin(), diff2.first.mIgnored.end(), std::string("b"))
                     != diff2.first.mIgnored.end());

    filesystem::remove_all("continuous_pipeline_config");
}

void ConfigWatcherUnittest::IgnoreModifiedLowerPrioritySingletonConfig() const {
    filesystem::create_directories("continuous_pipeline_config");
    size_t builtinPipelineCnt = 0;
#ifdef __ENTERPRISE__
    builtinPipelineCnt += EnterpriseConfigProvider::GetInstance()->GetAllBuiltInPipelineConfigs().size();
#endif
    // Step 1: add 'a' (higher priority) and 'b' (lower priority)
    {
        ofstream fa("continuous_pipeline_config/a.json");
        fa << R"(
            {
                "createTime": 1,
                "inputs": [
                    { "Type": "input_singleton_mock_1" }
                ],
                "flushers": [
                    { "Type": "flusher_mock" }
                ]
            }
        )";
        ofstream fb("continuous_pipeline_config/b.json");
        fb << R"(
            {
                "createTime": 2,
                "inputs": [
                    { "Type": "input_singleton_mock_1" }
                ],
                "flushers": [
                    { "Type": "flusher_mock" }
                ]
            }
        )";
    }

    auto diff1 = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
    // 'a' selected, 'b' ignored in first scan
    APSARA_TEST_EQUAL(1U + builtinPipelineCnt, diff1.first.mAdded.size());
    APSARA_TEST_EQUAL(0U, diff1.first.mModified.size());
    APSARA_TEST_EQUAL(0U, diff1.first.mRemoved.size());
    APSARA_TEST_EQUAL(1U, diff1.first.mIgnored.size());
    APSARA_TEST_TRUE(std::find_if(diff1.first.mAdded.begin(),
                                  diff1.first.mAdded.end(),
                                  [](const CollectionConfig& c) { return c.mName == "a"; })
                     != diff1.first.mAdded.end());
    APSARA_TEST_TRUE(std::find(diff1.first.mIgnored.begin(), diff1.first.mIgnored.end(), std::string("b"))
                     != diff1.first.mIgnored.end());

    // Apply the config to manager so 'a' becomes a running pipeline
    CollectionPipelineManager::GetInstance()->UpdatePipelines(diff1.first);

    // Step 2: modify 'b' content (size changed) => should still be ignored (IgnoredModified path)
    {
        ofstream fb("continuous_pipeline_config/b.json");
        fb << R"(
            {
                "createTime": 2,
                "inputs": [
                    { "Type": "input_singleton_mock_1" }
                ],
                "processors": [
                    { "Type": "processor_inner_mock" }
                ],
                "flushers": [
                    { "Type": "flusher_mock" }
                ]
            }
        )";
    }

    auto diff2 = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
    APSARA_TEST_EQUAL(0U, diff2.first.mAdded.size());
    APSARA_TEST_EQUAL(0U, diff2.first.mModified.size());
    APSARA_TEST_EQUAL(0U, diff2.first.mRemoved.size());
    APSARA_TEST_EQUAL(1U, diff2.first.mIgnored.size());
    APSARA_TEST_TRUE(std::find(diff2.first.mIgnored.begin(), diff2.first.mIgnored.end(), std::string("b"))
                     != diff2.first.mIgnored.end());

    filesystem::remove_all("continuous_pipeline_config");
}

void ConfigWatcherUnittest::HigherPriorityOverrideLowerPrioritySingletonConfig() const {
    filesystem::create_directories("continuous_pipeline_config");
    size_t builtinPipelineCnt = 0;
#ifdef __ENTERPRISE__
    builtinPipelineCnt += EnterpriseConfigProvider::GetInstance()->GetAllBuiltInPipelineConfigs().size();
#endif
    // Step 1: start with running 'b' (mock1) and 'c' (mock2)
    {
        ofstream fb("continuous_pipeline_config/b.json");
        fb << R"(
            {
                "createTime": 2,
                "inputs": [
                    { "Type": "input_singleton_mock_1" }
                ],
                "flushers": [
                    { "Type": "flusher_mock" }
                ]
            }
        )";
        ofstream fc("continuous_pipeline_config/c.json");
        fc << R"(
            {
                "createTime": 2,
                "inputs": [
                    { "Type": "input_singleton_mock_2" }
                ],
                "flushers": [
                    { "Type": "flusher_mock" }
                ]
            }
        )";
    }

    auto diff1 = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
    // both types should be added initially
    APSARA_TEST_EQUAL(2U + builtinPipelineCnt, diff1.first.mAdded.size());
    APSARA_TEST_EQUAL(0U, diff1.first.mModified.size());
    APSARA_TEST_EQUAL(0U, diff1.first.mRemoved.size());
    APSARA_TEST_EQUAL(0U, diff1.first.mIgnored.size());
    APSARA_TEST_TRUE(std::find_if(diff1.first.mAdded.begin(),
                                  diff1.first.mAdded.end(),
                                  [](const CollectionConfig& c) { return c.mName == "b"; })
                     != diff1.first.mAdded.end());
    APSARA_TEST_TRUE(std::find_if(diff1.first.mAdded.begin(),
                                  diff1.first.mAdded.end(),
                                  [](const CollectionConfig& c) { return c.mName == "c"; })
                     != diff1.first.mAdded.end());

    // Apply the configs to manager so they become running pipelines
    CollectionPipelineManager::GetInstance()->UpdatePipelines(diff1.first);

    // Step 2: add higher priority 'a' (mock1) => 'b' should be removed+ignored; 'c' unaffected
    {
        ofstream fa("continuous_pipeline_config/a.json");
        fa << R"(
            {
                "createTime": 1,
                "inputs": [
                    { "Type": "input_singleton_mock_1" }
                ],
                "flushers": [
                    { "Type": "flusher_mock" }
                ]
            }
        )";
    }

    auto diff2 = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
    APSARA_TEST_EQUAL(1U, diff2.first.mAdded.size());
    APSARA_TEST_EQUAL(0U, diff2.first.mModified.size());
    APSARA_TEST_EQUAL(1U, diff2.first.mRemoved.size());
    APSARA_TEST_EQUAL(1U, diff2.first.mIgnored.size());
    // 'b' becomes low-priority running config => should be removed and ignored
    APSARA_TEST_TRUE(std::find(diff2.first.mRemoved.begin(), diff2.first.mRemoved.end(), std::string("b"))
                     != diff2.first.mRemoved.end());
    APSARA_TEST_TRUE(std::find(diff2.first.mIgnored.begin(), diff2.first.mIgnored.end(), std::string("b"))
                     != diff2.first.mIgnored.end());
    // 'a' (new higher priority) should be added
    APSARA_TEST_TRUE(std::find_if(diff2.first.mAdded.begin(),
                                  diff2.first.mAdded.end(),
                                  [](const CollectionConfig& c) { return c.mName == "a"; })
                     != diff2.first.mAdded.end());

    filesystem::remove_all("continuous_pipeline_config");
}

UNIT_TEST_CASE(ConfigWatcherUnittest, InvalidConfigDirFound)
UNIT_TEST_CASE(ConfigWatcherUnittest, InvalidConfigFileFound)
UNIT_TEST_CASE(ConfigWatcherUnittest, DuplicateConfigs)
UNIT_TEST_CASE(ConfigWatcherUnittest, IgnoreNewLowerPrioritySingletonConfig)
UNIT_TEST_CASE(ConfigWatcherUnittest, IgnoreModifiedLowerPrioritySingletonConfig)
UNIT_TEST_CASE(ConfigWatcherUnittest, HigherPriorityOverrideLowerPrioritySingletonConfig)

} // namespace logtail

UNIT_TEST_MAIN
