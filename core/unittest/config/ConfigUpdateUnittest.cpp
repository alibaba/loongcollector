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
#include <memory>
#include <string>
#include <vector>

#include "config/PipelineConfig.h"
#include "config/common_provider/CommonConfigProvider.h"
#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#endif
#include "config/watcher/PipelineConfigWatcher.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PipelineManager.h"
#include "pipeline/plugin/PluginRegistry.h"
#include "task_pipeline/TaskPipelineManager.h"
#include "unittest/Unittest.h"
#include "unittest/config/PipelineManagerMock.h"
#include "unittest/plugin/PluginMock.h"

using namespace std;

namespace logtail {

class ConfigUpdateUnittest : public testing::Test {
public:
    void OnStartUp() const;
    void OnConfigDelete() const;
    void OnConfigToInvalidFormat() const;
    void OnConfigToInvalidDetail() const;
    void OnConfigToEnabledValid() const;
    void OnConfigToDisabledValid() const;
    void OnConfigUnchanged() const;
    void OnConfigAdded() const;

protected:
    static void SetUpTestCase() {
        PluginRegistry::GetInstance()->LoadPlugins();
        LoadTaskMock();
        PipelineConfigWatcher::GetInstance()->SetPipelineManager(PipelineManagerMock::GetInstance());
    }

    static void TearDownTestCase() {
        PluginRegistry::GetInstance()->UnloadPlugins();
        TaskRegistry::GetInstance()->UnloadPlugins();
    }

    void SetUp() override {
        filesystem::create_directories(configDir);
        PipelineConfigWatcher::GetInstance()->AddSource(configDir.string());
#ifdef __ENTERPRISE__
        builtinPipelineCnt = EnterpriseConfigProvider::GetInstance()->GetAllBuiltInPipelineConfigs().size();
#endif
    }

    void TearDown() override {
        PipelineManagerMock::GetInstance()->ClearEnvironment();
        TaskPipelineManager::GetInstance()->ClearEnvironment();
        PipelineConfigWatcher::GetInstance()->ClearEnvironment();
        filesystem::remove_all(configDir);
    }

private:
    void PrepareInitialSettings() const;
    void GenerateInitialConfigs() const;

    size_t builtinPipelineCnt = 0;
    filesystem::path configDir = "./continuous_pipeline_config";
    vector<filesystem::path> pipelineConfigPaths = {configDir / "pipeline_invalid_format.json",
                                                    configDir / "pipeline_invalid_detail.json",
                                                    configDir / "pipeline_enabled_valid.json",
                                                    configDir / "pipeline_disabled_valid.json"};
    vector<filesystem::path> taskConfigPaths = {configDir / "task_invalid_format.json",
                                                configDir / "task_invalid_detail.json",
                                                configDir / "task_enabled_valid.json",
                                                configDir / "task_disabled_valid.json"};
    const string invalidPipelineConfigWithInvalidFormat = R"({"inputs":{}})";
    const string invalidPipelineConfigWithInvalidDetail = R"(
{
    "valid": false,
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
    const string enabledValidPipelineConfig = R"(
{
    "valid": true,
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
    const string disabledValidPipelineConfig = R"(
{
    "valid": true,
    "enable": false,
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

    const string newInvalidPipelineConfigWithInvalidFormat = R"({"flushers":{}})";
    const string newInvalidPipelineConfigWithInvalidDetail = R"(
{
    "valid": false,
    "inputs": [
        {
            "Type": "input_container_stdio"
        }
    ],
    "flushers": [
        {
            "Type": "flusher_sls"
        }
    ]
}
    )";
    const string newEnabledValidPipelineConfig = R"(
{
    "valid": true,
    "inputs": [
        {
            "Type": "input_container_stdio"
        }
    ],
    "flushers": [
        {
            "Type": "flusher_sls"
        }
    ]
}
    )";
    const string newDisabledValidPipelineConfig = R"(
{
    "valid": true,
    "enable": false,
    "inputs": [
        {
            "Type": "input_container_stdio"
        }
    ],
    "flushers": [
        {
            "Type": "flusher_sls"
        }
    ]
}
    )";

    const string invalidTaskConfigWithInvalidFormat = R"({"task":[]})";
    const string invalidTaskConfigWithInvalidDetail = R"(
{
    "task": {
        "Type": "task_mock",
        "Valid": false
    }
}
    )";
    const string enabledValidTaskConfig = R"(
{
    "task": {
        "Type": "task_mock"
    }
}
    )";
    const string disabledValidTaskConfig = R"(
{
    "enable": false,
    "task": {
        "Type": "task_mock"
    }
}
    )";

    const string newInvalidTaskConfigWithInvalidFormat = R"({"task":""})";
    const string newInvalidTaskConfigWithInvalidDetail = R"(
{
    "task": {
        "Type": "task_mock",
        "Valid": false,
        "Other": true
    }
}
    )";
    const string newEnabledValidTaskConfig = R"(
{
    "task": {
        "Type": "task_mock",
        "Other": true
    }
}
    )";
    const string newDisabledValidTaskConfig = R"(
{
    "enable": false,
    "task": {
        "Type": "task_mock",
        "Other": true
    }
}
    )";
};

void ConfigUpdateUnittest::OnStartUp() const {
    auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
    APSARA_TEST_EQUAL(0U + builtinPipelineCnt, diff.first.mAdded.size());
    APSARA_TEST_TRUE(diff.second.IsEmpty());

    GenerateInitialConfigs();
    diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
    APSARA_TEST_FALSE(diff.first.IsEmpty());
    APSARA_TEST_EQUAL(2U, diff.first.mAdded.size());
    APSARA_TEST_TRUE(diff.first.mModified.empty());
    APSARA_TEST_TRUE(diff.first.mRemoved.empty());
    APSARA_TEST_FALSE(diff.second.IsEmpty());
    APSARA_TEST_EQUAL(2U, diff.second.mAdded.size());
    APSARA_TEST_TRUE(diff.second.mModified.empty());
    APSARA_TEST_TRUE(diff.second.mRemoved.empty());

    PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
    APSARA_TEST_EQUAL(1U + builtinPipelineCnt, PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
    TaskPipelineManager::GetInstance()->UpdatePipelines(diff.second);
    APSARA_TEST_EQUAL(1U, TaskPipelineManager::GetInstance()->GetAllPipelineNames().size());
    auto& ptr = TaskPipelineManager::GetInstance()->FindPipelineByName("task_enabled_valid");
    APSARA_TEST_NOT_EQUAL(nullptr, ptr);
    APSARA_TEST_EQUAL(TaskMock::sName, ptr->GetPlugin()->Name());
    APSARA_TEST_TRUE(static_cast<TaskMock*>(ptr->GetPlugin())->mIsRunning);
}

void ConfigUpdateUnittest::OnConfigDelete() const {
    PrepareInitialSettings();
    APSARA_TEST_EQUAL(1U + builtinPipelineCnt, PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
    APSARA_TEST_EQUAL(1U, TaskPipelineManager::GetInstance()->GetAllPipelineNames().size());

    filesystem::remove_all(configDir);
    auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
    APSARA_TEST_FALSE(diff.first.IsEmpty());
    APSARA_TEST_TRUE(diff.first.mAdded.empty());
    APSARA_TEST_TRUE(diff.first.mModified.empty());
    APSARA_TEST_EQUAL(1U, diff.first.mRemoved.size());
    APSARA_TEST_FALSE(diff.second.IsEmpty());
    APSARA_TEST_TRUE(diff.second.mAdded.empty());
    APSARA_TEST_TRUE(diff.second.mModified.empty());
    APSARA_TEST_EQUAL(1U, diff.second.mRemoved.size());

    PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
    APSARA_TEST_EQUAL(builtinPipelineCnt, PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
    TaskPipelineManager::GetInstance()->UpdatePipelines(diff.second);
    APSARA_TEST_TRUE(TaskPipelineManager::GetInstance()->GetAllPipelineNames().empty());
}

void ConfigUpdateUnittest::OnConfigToInvalidFormat() const {
    PrepareInitialSettings();
    APSARA_TEST_EQUAL(1U + builtinPipelineCnt, PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
    APSARA_TEST_EQUAL(1U, TaskPipelineManager::GetInstance()->GetAllPipelineNames().size());

    for (const auto& path : pipelineConfigPaths) {
        ofstream fout(path, ios::trunc);
        fout << newInvalidPipelineConfigWithInvalidFormat;
    }
    for (const auto& path : taskConfigPaths) {
        ofstream fout(path, ios::trunc);
        fout << newInvalidTaskConfigWithInvalidFormat;
    }
    auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
    APSARA_TEST_TRUE(diff.first.IsEmpty());
    APSARA_TEST_TRUE(diff.second.IsEmpty());
}

void ConfigUpdateUnittest::OnConfigToInvalidDetail() const {
    PrepareInitialSettings();
    APSARA_TEST_EQUAL(1U + builtinPipelineCnt, PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
    APSARA_TEST_EQUAL(1U, TaskPipelineManager::GetInstance()->GetAllPipelineNames().size());

    for (const auto& path : pipelineConfigPaths) {
        ofstream fout(path, ios::trunc);
        fout << newInvalidPipelineConfigWithInvalidDetail;
    }
    for (const auto& path : taskConfigPaths) {
        ofstream fout(path, ios::trunc);
        fout << newInvalidTaskConfigWithInvalidDetail;
    }
    auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
    APSARA_TEST_FALSE(diff.first.IsEmpty());
    APSARA_TEST_EQUAL(3U, diff.first.mAdded.size());
    APSARA_TEST_EQUAL(1U, diff.first.mModified.size());
    APSARA_TEST_TRUE(diff.first.mRemoved.empty());
    APSARA_TEST_FALSE(diff.second.IsEmpty());
    APSARA_TEST_EQUAL(3U, diff.second.mAdded.size());
    APSARA_TEST_EQUAL(1U, diff.second.mModified.size());
    APSARA_TEST_TRUE(diff.second.mRemoved.empty());

    PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
    APSARA_TEST_EQUAL(1U + builtinPipelineCnt, PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
    auto& ptr = TaskPipelineManager::GetInstance()->FindPipelineByName("task_enabled_valid");
    TaskPipelineManager::GetInstance()->UpdatePipelines(diff.second);
    APSARA_TEST_EQUAL(1U, TaskPipelineManager::GetInstance()->GetAllPipelineNames().size());
    auto& newPtr = TaskPipelineManager::GetInstance()->FindPipelineByName("task_enabled_valid");
    APSARA_TEST_EQUAL(ptr, newPtr);
    APSARA_TEST_EQUAL(TaskMock::sName, newPtr->GetPlugin()->Name());
    APSARA_TEST_TRUE(static_cast<TaskMock*>(newPtr->GetPlugin())->mIsRunning);
}

void ConfigUpdateUnittest::OnConfigToEnabledValid() const {
    PrepareInitialSettings();
    APSARA_TEST_EQUAL(1U + builtinPipelineCnt, PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
    APSARA_TEST_EQUAL(1U, TaskPipelineManager::GetInstance()->GetAllPipelineNames().size());

    for (const auto& path : pipelineConfigPaths) {
        ofstream fout(path, ios::trunc);
        fout << newEnabledValidPipelineConfig;
    }
    for (const auto& path : taskConfigPaths) {
        ofstream fout(path, ios::trunc);
        fout << newEnabledValidTaskConfig;
    }
    auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
    APSARA_TEST_FALSE(diff.first.IsEmpty());
    APSARA_TEST_EQUAL(3U, diff.first.mAdded.size());
    APSARA_TEST_EQUAL(1U, diff.first.mModified.size());
    APSARA_TEST_TRUE(diff.first.mRemoved.empty());
    APSARA_TEST_FALSE(diff.second.IsEmpty());
    APSARA_TEST_EQUAL(3U, diff.second.mAdded.size());
    APSARA_TEST_EQUAL(1U, diff.second.mModified.size());
    APSARA_TEST_TRUE(diff.second.mRemoved.empty());

    PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
    APSARA_TEST_EQUAL(4U + builtinPipelineCnt, PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
    TaskPipelineManager::GetInstance()->UpdatePipelines(diff.second);
    APSARA_TEST_EQUAL(4U, TaskPipelineManager::GetInstance()->GetAllPipelineNames().size());
    {
        auto& ptr = TaskPipelineManager::GetInstance()->FindPipelineByName("task_invalid_format");
        APSARA_TEST_NOT_EQUAL(nullptr, ptr);
        APSARA_TEST_EQUAL(TaskMock::sName, ptr->GetPlugin()->Name());
        APSARA_TEST_TRUE(static_cast<TaskMock*>(ptr->GetPlugin())->mIsRunning);
    }
    {
        auto& ptr = TaskPipelineManager::GetInstance()->FindPipelineByName("task_invalid_detail");
        APSARA_TEST_NOT_EQUAL(nullptr, ptr);
        APSARA_TEST_EQUAL(TaskMock::sName, ptr->GetPlugin()->Name());
        APSARA_TEST_TRUE(static_cast<TaskMock*>(ptr->GetPlugin())->mIsRunning);
    }
    {
        auto& ptr = TaskPipelineManager::GetInstance()->FindPipelineByName("task_enabled_valid");
        APSARA_TEST_NOT_EQUAL(nullptr, ptr);
        APSARA_TEST_EQUAL(TaskMock::sName, ptr->GetPlugin()->Name());
        APSARA_TEST_TRUE(static_cast<TaskMock*>(ptr->GetPlugin())->mIsRunning);
    }
    {
        auto& ptr = TaskPipelineManager::GetInstance()->FindPipelineByName("task_disabled_valid");
        APSARA_TEST_NOT_EQUAL(nullptr, ptr);
        APSARA_TEST_EQUAL(TaskMock::sName, ptr->GetPlugin()->Name());
        APSARA_TEST_TRUE(static_cast<TaskMock*>(ptr->GetPlugin())->mIsRunning);
    }
}

void ConfigUpdateUnittest::OnConfigToDisabledValid() const {
    PrepareInitialSettings();
    APSARA_TEST_EQUAL(1U + builtinPipelineCnt, PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
    APSARA_TEST_EQUAL(1U, TaskPipelineManager::GetInstance()->GetAllPipelineNames().size());

    for (const auto& path : pipelineConfigPaths) {
        ofstream fout(path, ios::trunc);
        fout << newDisabledValidPipelineConfig;
    }
    for (const auto& path : taskConfigPaths) {
        ofstream fout(path, ios::trunc);
        fout << newDisabledValidTaskConfig;
    }
    auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
    APSARA_TEST_FALSE(diff.first.IsEmpty());
    APSARA_TEST_TRUE(diff.first.mAdded.empty());
    APSARA_TEST_TRUE(diff.first.mModified.empty());
    APSARA_TEST_EQUAL(1U, diff.first.mRemoved.size());

    PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
    APSARA_TEST_EQUAL(builtinPipelineCnt, PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
    TaskPipelineManager::GetInstance()->UpdatePipelines(diff.second);
    APSARA_TEST_TRUE(TaskPipelineManager::GetInstance()->GetAllPipelineNames().empty());
}

void ConfigUpdateUnittest::OnConfigUnchanged() const {
    PrepareInitialSettings();
    APSARA_TEST_EQUAL(1U + builtinPipelineCnt, PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
    APSARA_TEST_EQUAL(1U, TaskPipelineManager::GetInstance()->GetAllPipelineNames().size());

    auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
    APSARA_TEST_TRUE(diff.first.IsEmpty());
    APSARA_TEST_TRUE(diff.second.IsEmpty());

    GenerateInitialConfigs();
    // mandatorily overwrite modify time in case of no update when file content remains the same.
    for (const auto& path : pipelineConfigPaths) {
        filesystem::file_time_type fTime = filesystem::last_write_time(path);
        filesystem::last_write_time(path, fTime + 1s);
    }
    for (const auto& path : taskConfigPaths) {
        filesystem::file_time_type fTime = filesystem::last_write_time(path);
        filesystem::last_write_time(path, fTime + 1s);
    }
    diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
    APSARA_TEST_FALSE(diff.first.IsEmpty());
    APSARA_TEST_EQUAL(1U, diff.first.mAdded.size());
    APSARA_TEST_TRUE(diff.first.mModified.empty());
    APSARA_TEST_TRUE(diff.first.mRemoved.empty());
    APSARA_TEST_FALSE(diff.second.IsEmpty());
    APSARA_TEST_EQUAL(1U, diff.second.mAdded.size());
    APSARA_TEST_TRUE(diff.second.mModified.empty());
    APSARA_TEST_TRUE(diff.second.mRemoved.empty());

    PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
    APSARA_TEST_EQUAL(1U + builtinPipelineCnt, PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
    auto& ptr = TaskPipelineManager::GetInstance()->FindPipelineByName("task_enabled_valid");
    TaskPipelineManager::GetInstance()->UpdatePipelines(diff.second);
    APSARA_TEST_EQUAL(1U, TaskPipelineManager::GetInstance()->GetAllPipelineNames().size());
    auto& newPtr = TaskPipelineManager::GetInstance()->FindPipelineByName("task_enabled_valid");
    APSARA_TEST_EQUAL(ptr, newPtr);
    APSARA_TEST_EQUAL(TaskMock::sName, newPtr->GetPlugin()->Name());
    APSARA_TEST_TRUE(static_cast<TaskMock*>(newPtr->GetPlugin())->mIsRunning);
}

void ConfigUpdateUnittest::OnConfigAdded() const {
    PrepareInitialSettings();
    APSARA_TEST_EQUAL(1U + builtinPipelineCnt, PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
    APSARA_TEST_EQUAL(1U, TaskPipelineManager::GetInstance()->GetAllPipelineNames().size());

    {
        ofstream fout(configDir / "add_pipeline_invalid_format.json", ios::trunc);
        fout << invalidPipelineConfigWithInvalidFormat;
    }
    {
        ofstream fout(configDir / "add_pipeline_invalid_detail.json", ios::trunc);
        fout << invalidPipelineConfigWithInvalidDetail;
    }
    {
        ofstream fout(configDir / "add_pipeline_enabled_valid.json", ios::trunc);
        fout << enabledValidPipelineConfig;
    }
    {
        ofstream fout(configDir / "add_pipeline_disabled_valid.json", ios::trunc);
        fout << disabledValidPipelineConfig;
    }
    {
        ofstream fout(configDir / "add_task_invalid_format.json", ios::trunc);
        fout << invalidTaskConfigWithInvalidFormat;
    }
    {
        ofstream fout(configDir / "add_task_invalid_detail.json", ios::trunc);
        fout << invalidTaskConfigWithInvalidDetail;
    }
    {
        ofstream fout(configDir / "add_task_enabled_valid.json", ios::trunc);
        fout << enabledValidTaskConfig;
    }
    {
        ofstream fout(configDir / "add_task_disabled_valid.json", ios::trunc);
        fout << disabledValidTaskConfig;
    }
    auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
    APSARA_TEST_FALSE(diff.first.IsEmpty());
    APSARA_TEST_EQUAL(2U, diff.first.mAdded.size());
    APSARA_TEST_TRUE(diff.first.mModified.empty());
    APSARA_TEST_TRUE(diff.first.mRemoved.empty());
    APSARA_TEST_FALSE(diff.second.IsEmpty());
    APSARA_TEST_EQUAL(2U, diff.second.mAdded.size());
    APSARA_TEST_TRUE(diff.second.mModified.empty());
    APSARA_TEST_TRUE(diff.second.mRemoved.empty());

    PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
    APSARA_TEST_EQUAL(2U + builtinPipelineCnt, PipelineManagerMock::GetInstance()->GetAllConfigNames().size());
    auto& ptr = TaskPipelineManager::GetInstance()->FindPipelineByName("task_enabled_valid");
    TaskPipelineManager::GetInstance()->UpdatePipelines(diff.second);
    APSARA_TEST_EQUAL(2U, TaskPipelineManager::GetInstance()->GetAllPipelineNames().size());
    {
        auto& newPtr = TaskPipelineManager::GetInstance()->FindPipelineByName("task_enabled_valid");
        APSARA_TEST_EQUAL(ptr, newPtr);
        APSARA_TEST_EQUAL(TaskMock::sName, newPtr->GetPlugin()->Name());
        APSARA_TEST_TRUE(static_cast<TaskMock*>(newPtr->GetPlugin())->mIsRunning);
    }
    {
        auto& newPtr = TaskPipelineManager::GetInstance()->FindPipelineByName("add_task_enabled_valid");
        APSARA_TEST_NOT_EQUAL(nullptr, newPtr);
        APSARA_TEST_EQUAL(TaskMock::sName, newPtr->GetPlugin()->Name());
        APSARA_TEST_TRUE(static_cast<TaskMock*>(newPtr->GetPlugin())->mIsRunning);
    }
}

void ConfigUpdateUnittest::PrepareInitialSettings() const {
    GenerateInitialConfigs();
    auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
    PipelineManagerMock::GetInstance()->UpdatePipelines(diff.first);
    TaskPipelineManager::GetInstance()->UpdatePipelines(diff.second);
}

void ConfigUpdateUnittest::GenerateInitialConfigs() const {
    {
        ofstream fout(pipelineConfigPaths[0], ios::trunc);
        fout << invalidPipelineConfigWithInvalidFormat;
    }
    {
        ofstream fout(pipelineConfigPaths[1], ios::trunc);
        fout << invalidPipelineConfigWithInvalidDetail;
    }
    {
        ofstream fout(pipelineConfigPaths[2], ios::trunc);
        fout << enabledValidPipelineConfig;
    }
    {
        ofstream fout(pipelineConfigPaths[3], ios::trunc);
        fout << disabledValidPipelineConfig;
    }
    {
        ofstream fout(taskConfigPaths[0], ios::trunc);
        fout << invalidTaskConfigWithInvalidFormat;
    }
    {
        ofstream fout(taskConfigPaths[1], ios::trunc);
        fout << invalidTaskConfigWithInvalidDetail;
    }
    {
        ofstream fout(taskConfigPaths[2], ios::trunc);
        fout << enabledValidTaskConfig;
    }
    {
        ofstream fout(taskConfigPaths[3], ios::trunc);
        fout << disabledValidTaskConfig;
    }
}

UNIT_TEST_CASE(ConfigUpdateUnittest, OnStartUp)
UNIT_TEST_CASE(ConfigUpdateUnittest, OnConfigDelete)
UNIT_TEST_CASE(ConfigUpdateUnittest, OnConfigToInvalidFormat)
UNIT_TEST_CASE(ConfigUpdateUnittest, OnConfigToInvalidDetail)
UNIT_TEST_CASE(ConfigUpdateUnittest, OnConfigToEnabledValid)
UNIT_TEST_CASE(ConfigUpdateUnittest, OnConfigToDisabledValid)
UNIT_TEST_CASE(ConfigUpdateUnittest, OnConfigUnchanged)
UNIT_TEST_CASE(ConfigUpdateUnittest, OnConfigAdded)

} // namespace logtail

UNIT_TEST_MAIN
