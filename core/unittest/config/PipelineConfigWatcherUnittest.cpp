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
#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#endif
#include "config/watcher/PipelineConfigWatcher.h"
#include "plugin/PluginRegistry.h"
#include "unittest/Unittest.h"
#include "unittest/config/PipelineManagerMock.h"

using namespace std;

namespace logtail {

class PipelineConfigWatcherUnittest : public testing::Test {
public:
    void TestLoadAddedSingletonConfig();
    void TestLoadModifiedSingletonConfig();
    void TestLoadRemovedSingletonConfig();
    void TestLoadUnchangedSingletonConfig();

protected:
    static void SetUpTestCase() {
        PluginRegistry::GetInstance()->LoadPlugins();
        PipelineConfigWatcher::GetInstance()->SetPipelineManager(PipelineManagerMock::GetInstance());
    }
    static void TearDownTestCase() { PluginRegistry::GetInstance()->UnloadPlugins(); }

private:
    void PrepareConfig() {
        filesystem::create_directories(configDir1);
        PipelineConfigWatcher::GetInstance()->AddSource(configDir1.string());
        filesystem::create_directories(configDir2);
        PipelineConfigWatcher::GetInstance()->AddSource(configDir2.string());
#ifdef __ENTERPRISE__
        builtinPipelineCnt = EnterpriseConfigProvider::GetInstance()->GetAllBuiltInPipelineConfigs().size();
#endif
    }

    void ClearConfig() {
        PipelineManagerMock::GetInstance()->ClearEnvironment();
        PipelineConfigWatcher::GetInstance()->ClearEnvironment();
        filesystem::remove_all(configDir1);
        filesystem::remove_all(configDir2);
    }

    size_t builtinPipelineCnt = 0;
    filesystem::path configDir1 = "./continuous_pipeline_config1";
    filesystem::path configDir2 = "./continuous_pipeline_config2";

    const std::string greaterPriorityConfig = R"(
        {
            "createTime": 1,
            "valid": true,
            "inputs": [
                {
                    "Type": "input_network_observer"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls"
                }
            ]
        }
    )";

    const std::string lessPriorityConfig = R"(
        {
            "createTime": 2,
            "valid": true,
            "inputs": [
                {
                    "Type": "input_network_observer"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls"
                }
            ]
        }
    )";

    const std::string modifiedGreaterPriorityConfig = R"(
        {
            "createTime": 1,
            "valid": true,
            "inputs": [
                {
                    "Type": "input_network_observer"
                }
            ],
            "processors": [],
            "flushers": [
                {
                    "Type": "flusher_sls"
                }
            ]
        }
    )";

    const std::string modifiedLessPriorityConfig = R"(
        {
            "createTime": 2,
            "valid": true,
            "inputs": [
                {
                    "Type": "input_network_observer"
                }
            ],
            "processors": [],
            "flushers": [
                {
                    "Type": "flusher_sls"
                }
            ]
        }
    )";

    const std::string otherConfig = R"(
        {
            "createTime": 3,
            "valid": true,
            "inputs": [
                {
                    "Type": "input_process_security"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_sls"
                }
            ]
        }
    )";

    const std::string modifiedOtherConfig = R"(
        {
            "createTime": 3,
            "valid": true,
            "inputs": [
                {
                    "Type": "input_process_security"
                }
            ],
            "processors": [],
            "flushers": [
                {
                    "Type": "flusher_sls"
                }
            ]
        }
    )";
};

// there are 4 kinds of a config: added, modified, removed, unchanged
// there are 4 kinds of priority relationship: first > second, first < second,
// first > second -> first < second, first < second -> first > second
// total case:  4 (first kind) * 4(second kind) * 4(priority) = 64
void PipelineConfigWatcherUnittest::TestLoadAddedSingletonConfig() {
    { // case: added -> added, first > second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test1", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: added -> added, first < second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test2", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: added -> added, first > second -> first < second
      // should not happen
    }
    { // case: added -> added, first < second -> first > second
      // should not happen
    }
    { // case: added -> modified, first > second
        PrepareConfig();
        ofstream fout(configDir2 / "test2.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        fout.open(configDir1 / "test1.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << modifiedLessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << modifiedOtherConfig;
        fout.close();
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test1", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: added -> modified, first < second
        PrepareConfig();
        ofstream fout(configDir2 / "test2.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        fout.open(configDir1 / "test1.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << modifiedGreaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << modifiedOtherConfig;
        fout.close();
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(2U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test2", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: added -> modified, first > second -> first < second
      // should not happen
    }
    { // case: added -> modified, first < second -> first > second
      // should not happen
    }
    { // case: added -> removed, first > second
        PrepareConfig();
        ofstream fout(configDir2 / "test2.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());

        fout.open(configDir1 / "test1.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        filesystem::remove(configDir2 / "test2.json");
        filesystem::remove(configDir2 / "test-other.json");
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(2U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(1U + builtinPipelineCnt, allConfigNames.size());
        APSARA_TEST_EQUAL_FATAL("test1", allConfigNames[0]);
        ClearConfig();
    }
    { // case: added -> removed, first < second
        PrepareConfig();
        ofstream fout(configDir2 / "test2.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());

        fout.open(configDir1 / "test1.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        filesystem::remove(configDir2 / "test2.json");
        filesystem::remove(configDir2 / "test-other.json");
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(2U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(1U + builtinPipelineCnt, allConfigNames.size());
        APSARA_TEST_EQUAL_FATAL("test1", allConfigNames[0]);
        ClearConfig();
    }
    { // case: added -> removed, first > second -> first < second
      // should not happen
    }
    { // case: added -> removed, first < second -> first > second
      // should not happen
    }
    { // case: added -> unchanged, first > second
        PrepareConfig();
        ofstream fout(configDir2 / "test2.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());

        fout.open(configDir1 / "test1.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test1", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: added -> unchanged, first < second
        PrepareConfig();
        ofstream fout(configDir2 / "test2.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());

        fout.open(configDir1 / "test1.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test2", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: added -> unchanged, first > second -> first < second
      // should not happen
    }
    { // case: added -> unchanged, first < second -> first > second
      // should not happen
    }
}

void PipelineConfigWatcherUnittest::TestLoadModifiedSingletonConfig() {
    { // case: modified -> added, first > second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(1U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        fout.open(configDir1 / "test1.json", ios::trunc);
        fout << modifiedGreaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test1", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: modified -> added, first < second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(1U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        fout.open(configDir1 / "test1.json", ios::trunc);
        fout << modifiedLessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(2U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test2", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: modified -> added, first > second -> first < second
      // should not happen
    }
    { // case: modified -> added, first < second -> first > second
      // should not happen
    }
    { // case: modified -> modified, first > second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        fout.open(configDir1 / "test1.json", ios::trunc);
        fout << modifiedGreaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << modifiedLessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << modifiedOtherConfig;
        fout.close();
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(2U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test1", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: modified -> modified, first < second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        fout.open(configDir1 / "test1.json", ios::trunc);
        fout << modifiedLessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << modifiedGreaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << modifiedOtherConfig;
        fout.close();
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(2U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test2", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: modified -> modified, first > second -> first < second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        fout.open(configDir1 / "test1.json", ios::trunc);
        fout << modifiedLessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << modifiedGreaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << modifiedOtherConfig;
        fout.close();
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test2", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: modified -> modified, first < second -> first > second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        fout.open(configDir1 / "test1.json", ios::trunc);
        fout << modifiedGreaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << modifiedLessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << modifiedOtherConfig;
        fout.close();
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test1", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: modified -> removed, first > second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        fout.open(configDir1 / "test1.json", ios::trunc);
        fout << modifiedGreaterPriorityConfig;
        fout.close();
        filesystem::remove(configDir2 / "test2.json");
        filesystem::remove(configDir2 / "test-other.json");
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(1U + builtinPipelineCnt, allConfigNames.size());
        APSARA_TEST_EQUAL_FATAL("test1", allConfigNames[builtinPipelineCnt]);
        ClearConfig();
    }
    { // case: modified -> removed, first < second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        fout.open(configDir1 / "test1.json", ios::trunc);
        fout << modifiedLessPriorityConfig;
        fout.close();
        filesystem::remove(configDir2 / "test2.json");
        filesystem::remove(configDir2 / "test-other.json");
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(2U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(1U + builtinPipelineCnt, allConfigNames.size());
        APSARA_TEST_EQUAL_FATAL("test1", allConfigNames[0]);
        ClearConfig();
    }
    { // case: modified -> removed, first > second -> first < second
      // should not happen
    }
    { // case: modified -> removed, first < second -> first > second
      // should not happen
    }
    { // case: modified -> unchanged, first > second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        fout.open(configDir1 / "test1.json", ios::trunc);
        fout << modifiedGreaterPriorityConfig;
        fout.close();
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test1", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: modified -> unchanged, first < second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        fout.open(configDir1 / "test1.json", ios::trunc);
        fout << modifiedLessPriorityConfig;
        fout.close();
        filesystem::remove(configDir2 / "test2.json");
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test1", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: modified -> unchanged, first > second -> first < second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        fout.open(configDir1 / "test1.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test2", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: modified -> unchanged, first < second -> first > second
        PrepareConfig();
        ofstream fout(configDir1 / "test3.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        fout.open(configDir1 / "test3.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test3", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
}

void PipelineConfigWatcherUnittest::TestLoadRemovedSingletonConfig() {
    { // case: removed -> added, first > second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        size_t builtinPipelineCnt = 0;
#ifdef __ENTERPRISE__
        builtinPipelineCnt += EnterpriseConfigProvider::GetInstance()->GetAllBuiltInPipelineConfigs().size();
#endif
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(1U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());

        filesystem::remove(configDir1 / "test1.json");
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(2U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test2", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: removed -> added, first < second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(1U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());

        filesystem::remove(configDir1 / "test1.json");
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(2U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test2", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: removed -> added, first > second -> first < second
      // should not happen
    }
    { // case: removed -> added, first < second -> first > second
      // should not happen
    }
    { // case: removed -> modified, first > second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
        this_thread::sleep_for(chrono::milliseconds(1));

        filesystem::remove(configDir1 / "test1.json");
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << modifiedLessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << modifiedOtherConfig;
        fout.close();
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test2", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: removed -> modified, first < second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
        this_thread::sleep_for(chrono::milliseconds(1));

        filesystem::remove(configDir1 / "test1.json");
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << modifiedGreaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << modifiedOtherConfig;
        fout.close();
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(2U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test2", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: removed -> modified, first > second -> first < second
      // should not happen
    }
    { // case: removed -> modified, first < second -> first > second
      // should not happen
    }
    { // case: removed -> removed, first > second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());

        filesystem::remove(configDir1 / "test1.json");
        filesystem::remove(configDir2 / "test2.json");
        filesystem::remove(configDir2 / "test-other.json");
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(2U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(0U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
        ClearConfig();
    }
    { // case: removed -> removed, first < second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());

        filesystem::remove(configDir1 / "test1.json");
        filesystem::remove(configDir2 / "test2.json");
        filesystem::remove(configDir2 / "test-other.json");
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(2U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(0U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
        ClearConfig();
    }
    { // case: removed -> removed, first > second -> first < second
      // should not happen
    }
    { // case: removed -> removed, first < second -> first > second
      // should not happen
    }
    { // case: removed -> unchanged, first > second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());

        filesystem::remove(configDir1 / "test1.json");
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test2", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: removed -> unchanged, first < second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());

        filesystem::remove(configDir1 / "test1.json");
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test2", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: removed -> unchanged, first > second -> first < second
      // should not happen
    }
    { // case: removed -> unchanged, first < second -> first > second
      // should not happen
    }
}

void PipelineConfigWatcherUnittest::TestLoadUnchangedSingletonConfig() {
    { // case: unchanged -> added, first > second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        size_t builtinPipelineCnt = 0;
#ifdef __ENTERPRISE__
        builtinPipelineCnt += EnterpriseConfigProvider::GetInstance()->GetAllBuiltInPipelineConfigs().size();
#endif
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(1U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());

        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test1", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: unchanged -> added, first < second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(1U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());

        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(2U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test2", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: unchanged -> added, first > second -> first < second
      // should not happen
    }
    { // case: unchanged -> added, first < second -> first > second
      // should not happen
    }
    { // case: unchanged -> modified, first > second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());

        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << modifiedLessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << modifiedOtherConfig;
        fout.close();
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test1", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: unchanged -> modified, first < second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());

        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << modifiedGreaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << modifiedOtherConfig;
        fout.close();
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(2U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test2", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: unchanged -> modified, first > second -> first < second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());

        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << modifiedOtherConfig;
        fout.close();
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test2", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: unchanged -> modified, first < second -> first > second
        PrepareConfig();
        ofstream fout(configDir1 / "test3.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());

        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << modifiedLessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << modifiedOtherConfig;
        fout.close();
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test3", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: unchanged -> removed, first > second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());

        filesystem::remove(configDir2 / "test2.json");
        filesystem::remove(configDir2 / "test-other.json");
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(1U + builtinPipelineCnt, allConfigNames.size());
        APSARA_TEST_EQUAL_FATAL("test1", allConfigNames[builtinPipelineCnt]);
        ClearConfig();
    }
    { // case: unchanged -> removed, first < second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());

        filesystem::remove(configDir2 / "test2.json");
        filesystem::remove(configDir2 / "test-other.json");
        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(1U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(2U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(1U + builtinPipelineCnt, allConfigNames.size());
        APSARA_TEST_EQUAL_FATAL("test1", allConfigNames[0]);
        ClearConfig();
    }
    { // case: unchanged -> removed, first > second -> first < second
      // should not happen
    }
    { // case: unchanged -> removed, first < second -> first > second
      // should not happen
    }
    { // case: unchanged -> unchanged, first > second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());

        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test1", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: unchanged -> unchanged, first < second
        PrepareConfig();
        ofstream fout(configDir1 / "test1.json", ios::trunc);
        fout << lessPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test2.json", ios::trunc);
        fout << greaterPriorityConfig;
        fout.close();
        fout.open(configDir2 / "test-other.json", ios::trunc);
        fout << otherConfig;
        fout.close();
        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt,
                                PipelineManagerMock::GetInstance()->GetAllConfigNames().size());

        diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mAdded.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mModified.size());
        APSARA_TEST_EQUAL_FATAL(0U, diff.first.mRemoved.size());

        PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
        auto allConfigNames = PipelineManagerMock::GetInstance()->GetAllConfigNames();
        APSARA_TEST_EQUAL_FATAL(2U + builtinPipelineCnt, allConfigNames.size());
        sort(allConfigNames.begin(), allConfigNames.end());
        APSARA_TEST_EQUAL_FATAL("test-other", allConfigNames[builtinPipelineCnt]);
        APSARA_TEST_EQUAL_FATAL("test2", allConfigNames[builtinPipelineCnt + 1]);
        ClearConfig();
    }
    { // case: unchanged -> unchanged, first > second -> first < second
      // should not happen
    }
    { // case: unchanged -> unchanged, first < second -> first > second
      // should not happen
    }
}

UNIT_TEST_CASE(PipelineConfigWatcherUnittest, TestLoadAddedSingletonConfig)
UNIT_TEST_CASE(PipelineConfigWatcherUnittest, TestLoadModifiedSingletonConfig)
UNIT_TEST_CASE(PipelineConfigWatcherUnittest, TestLoadRemovedSingletonConfig)
UNIT_TEST_CASE(PipelineConfigWatcherUnittest, TestLoadUnchangedSingletonConfig)

} // namespace logtail

UNIT_TEST_MAIN
