// Copyright 2025 iLogtail Authors
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

#include "collection_pipeline/CollectionPipelineManager.h"
#include "common/JsonUtil.h"
#include "config/OnetimeConfigInfoManager.h"
#include "config/watcher/PipelineConfigWatcher.h"
#include "unittest/Unittest.h"
#include "unittest/plugin/PluginMock.h"
#include <thread>
#include <chrono>

using namespace std;

namespace logtail {

class OnetimeConfigUpdateUnittest : public testing::Test {
public:
    void OnCollectionConfigUpdate() const;

protected:
    static void SetUpTestCase() {
        PluginRegistry::GetInstance()->LoadPlugins();
        LoadPluginMock();
        // PipelineConfigWatcher::GetInstance()->SetPipelineManager(PipelineManagerMock::GetInstance());
    }

    static void TearDownTestCase() { PluginRegistry::GetInstance()->UnloadPlugins(); }

    void SetUp() override {
        filesystem::create_directories(mConfigDir);
        PipelineConfigWatcher::GetInstance()->AddSource(mConfigDir.string());
    }

    void TearDown() override {
        // CollectionPipelineManager::GetInstance()->Clear();
        // PipelineManagerMock::GetInstance()->ClearEnvironment();
        PipelineConfigWatcher::GetInstance()->ClearEnvironment();
        sConfigManager->Clear();
        filesystem::remove_all(mConfigDir);
        error_code ec;
        filesystem::remove(sConfigManager->mCheckpointFilePath, ec);
    }

private:
    static OnetimeConfigInfoManager* sConfigManager;

    filesystem::path mConfigDir = "continuous_pipeline_config";
};

OnetimeConfigInfoManager* OnetimeConfigUpdateUnittest::sConfigManager = OnetimeConfigInfoManager::GetInstance();

void OnetimeConfigUpdateUnittest::OnCollectionConfigUpdate() const {
    map<string, uint64_t> configHash;
    string unusedConfigDetail = R"({
        "global": {
            "ExcutionTimeout": 1400
        },
        "inputs": [
            {
                "Type": "input_mock"
            }
        ],
        "flushers": [
            {
                "Type": "flusher_mock"
            }
        ]
    })";
    Json::Value root;
    string errorMsg;
    ParseJsonTable(unusedConfigDetail, root, errorMsg);
    configHash["unused_config.json"] = Hash(root);

    // on restart
    {
        cout << "=== ON RESTART PHASE START ===" << endl;
        cout << "Current time: " << time(nullptr) << endl;
        
        // prepare config files
        vector<string> configDetails = {
            R"({
            "global": {
                "ExcutionTimeout": 3600
            },
            "inputs": [
                {
                    "Type": "input_mock"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_mock"
                }
            ]
        })",
            R"({
            "global": {
                "ExcutionTimeout": 7200
            },
            "inputs": [
                {
                    "Type": "input_mock"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_mock"
                }
            ]
        })",
            R"({
            "global": {
                "ExcutionTimeout": 1800
            },
            "inputs": [
                {
                    "Type": "input_mock"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_mock"
                }
            ]
        })",
            R"({
            "global": {
                "ExcutionTimeout": 600
            },
            "inputs": [
                {
                    "Type": "input_mock"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_mock"
                }
            ]
        })"};
        vector<string> filenames
            = {"new_config.json", "changed_config.json", "old_config.json", "obsolete_config.json"};
        for (size_t i = 0; i < configDetails.size(); ++i) {
            ofstream fout(mConfigDir / filenames[i], ios::binary);
            fout << configDetails[i];
            cout << "Created file: " << (mConfigDir / filenames[i]).string() << " with content hash: " << Hash(Json::Value(configDetails[i])) << endl;
        }

        // compute config hash
        for (size_t i = 0; i < configDetails.size(); ++i) {
            Json::Value root;
            string errorMsg;
            ParseJsonTable(configDetails[i], root, errorMsg);
            configHash[filenames[i]] = Hash(root);
            cout << "Computed hash for " << filenames[i] << ": " << configHash[filenames[i]] << endl;
        }

        // prepare checkpoint file
        {
            ofstream fout(sConfigManager->mCheckpointFilePath, ios::binary);
            fout << R"({
            "changed_config": {
                "config_hash": 8279028812201817660,
                "expire_time": 2000000000
            },
            "old_config": {
                "config_hash": )"
                    + ToString(configHash["old_config.json"]) + R"(,
                "expire_time": 2500000000
            },
            "obsolete_config": {
                "config_hash": )"
                    + ToString(configHash["obsolete_config.json"]) + R"(,
                "expire_time": 1000000000
            },
            "unused_config": {
                "config_hash": )"
                    + ToString(configHash["unused_config.json"]) + R"(,
                "expire_time": 2200000000
            }
        })";
            cout << "Created checkpoint file: " << sConfigManager->mCheckpointFilePath.string() << endl;
        }
        sConfigManager->LoadCheckpointFile();
        cout << "Loaded checkpoint file" << endl;

        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        cout << "CheckConfigDiff result:" << endl;
        cout << "  Added: " << diff.first.mAdded.size() << endl;
        cout << "  Modified: " << diff.first.mModified.size() << endl;
        cout << "  Removed: " << diff.first.mRemoved.size() << endl;
        
        for (const auto& config : diff.first.mAdded) {
            cout << "  Added config: " << config.mName << endl;
        }
        for (const auto& config : diff.first.mModified) {
            cout << "  Modified config: " << config.mName << endl;
        }
        for (const auto& name : diff.first.mRemoved) {
            cout << "  Removed config: " << name << endl;
        }
        
        APSARA_TEST_FALSE(diff.first.IsEmpty());
        CollectionPipelineManager::GetInstance()->UpdatePipelines(diff.first);
        sConfigManager->DumpCheckpointFile();

        cout << "Current config manager state after UpdatePipelines:" << endl;
        cout << "  ConfigInfoMap size: " << sConfigManager->mConfigInfoMap.size() << endl;
        for (const auto& item : sConfigManager->mConfigInfoMap) {
            cout << "    " << item.first << " -> hash: " << item.second.mHash << ", expire_time: " << item.second.mExpireTime << endl;
        }
        cout << "  ConfigExpireTimeCheckpoint size: " << sConfigManager->mConfigExpireTimeCheckpoint.size() << endl;
        for (const auto& item : sConfigManager->mConfigExpireTimeCheckpoint) {
            cout << "    " << item.first << " -> hash: " << item.second.first << ", expire_time: " << item.second.second << endl;
        }

        APSARA_TEST_EQUAL(3U, sConfigManager->mConfigInfoMap.size());
        {
            const auto& item = sConfigManager->mConfigInfoMap.at("new_config");
            APSARA_TEST_EQUAL(time(nullptr) + 3600U, item.mExpireTime);
            cout << "new_config expire time: " << item.mExpireTime << endl;
            APSARA_TEST_EQUAL(configHash["new_config.json"], item.mHash);
            cout << "new_config hash: " << item.mHash << endl;
            APSARA_TEST_EQUAL(ConfigType::Collection, item.mType);
            APSARA_TEST_EQUAL(mConfigDir / filenames[0], item.mFilepath);
        }
        {
            const auto& item = sConfigManager->mConfigInfoMap.at("changed_config");
            APSARA_TEST_EQUAL(time(nullptr) + 7200U, item.mExpireTime);
            cout << "changed_config expire time: " << item.mExpireTime << endl;
            APSARA_TEST_EQUAL(configHash["changed_config.json"], item.mHash);
            cout << "changed_config hash: " << item.mHash << endl;
            APSARA_TEST_EQUAL(ConfigType::Collection, item.mType);
            APSARA_TEST_EQUAL(mConfigDir / filenames[1], item.mFilepath);
        }
        {
            const auto& item = sConfigManager->mConfigInfoMap.at("old_config");
            APSARA_TEST_EQUAL(2500000000U, item.mExpireTime);
            cout << "old_config expire time: " << item.mExpireTime << endl;
            APSARA_TEST_EQUAL(configHash["old_config.json"], item.mHash);
            cout << "old_config hash: " << item.mHash << endl;
            APSARA_TEST_EQUAL(ConfigType::Collection, item.mType);
            APSARA_TEST_EQUAL(mConfigDir / filenames[2], item.mFilepath);
        }
        APSARA_TEST_EQUAL(1U, sConfigManager->mConfigExpireTimeCheckpoint.size());
        APSARA_TEST_NOT_EQUAL(sConfigManager->mConfigExpireTimeCheckpoint.end(),
                              sConfigManager->mConfigExpireTimeCheckpoint.find("unused_config"));
        cout << "=== ON RESTART PHASE END ===" << endl;
    }

    // on update
    {
        cout << "=== ON UPDATE PHASE START ===" << endl;
        cout << "Current time: " << time(nullptr) << endl;
        
        // prepare config files
        vector<string> configDetails = {
            R"({
            "global": {
                "ExcutionTimeout": 1000
            },
            "inputs": [
                {
                    "Type": "input_mock"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_mock"
                }
            ]
        })",
            R"({
            "global": {
                "ExcutionTimeout": 1200
            },
            "inputs": [
                {
                    "Type": "input_mock"
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_mock"
                }
            ]
        })"};
        vector<string> filenames = {"new_config.json", "old_config.json"};
        for (size_t i = 0; i < configDetails.size(); ++i) {
            // 先读取原始文件信息（如果存在）
            filesystem::path filePath = mConfigDir / filenames[i];
            if (filesystem::exists(filePath)) {
                error_code ec;
                auto originalSize = filesystem::file_size(filePath, ec);
                auto originalTime = filesystem::last_write_time(filePath, ec);
                cout << "Original file info for " << filenames[i] << ":" << endl;
                cout << "  Size: " << originalSize << endl;
                cout << "  Time: " << originalTime.time_since_epoch().count() << endl;
            }
            
            // 更新文件内容
            ofstream fout(filePath, ios::binary);
            fout << configDetails[i];
            fout.close(); // 确保文件写入完成
            
            // 强制更新文件修改时间
            filesystem::file_time_type newTime = filesystem::file_time_type::clock::now();
            filesystem::last_write_time(filePath, newTime);
            
            // 验证文件更新
            error_code ec;
            auto newSize = filesystem::file_size(filePath, ec);
            auto actualTime = filesystem::last_write_time(filePath, ec);
            
            cout << "Updated file: " << filePath.string() << endl;
            cout << "  Content hash: " << Hash(Json::Value(configDetails[i])) << endl;
            cout << "  New size: " << newSize << endl;
            cout << "  New time: " << actualTime.time_since_epoch().count() << endl;
            cout << "  Time difference: " << (actualTime.time_since_epoch().count() - newTime.time_since_epoch().count()) << endl;
            
            // 添加一个小延迟确保文件系统更新
            this_thread::sleep_for(chrono::milliseconds(10));
        }
        {
            ofstream fout(mConfigDir / "unused_config.json", ios::binary);
            fout << unusedConfigDetail;
            fout.close();
            cout << "Created file: " << (mConfigDir / "unused_config.json").string() << endl;
        }
        filesystem::remove(mConfigDir / "changed_config.json");
        cout << "Removed file: " << (mConfigDir / "changed_config.json").string() << endl;

        // compute config hash
        for (size_t i = 0; i < configDetails.size(); ++i) {
            Json::Value root;
            string errorMsg;
            ParseJsonTable(configDetails[i], root, errorMsg);
            configHash[filenames[i]] = Hash(root);
            cout << "Computed hash for " << filenames[i] << ": " << configHash[filenames[i]] << endl;
        }

        cout << "Current config manager state before CheckConfigDiff:" << endl;
        cout << "  ConfigInfoMap size: " << sConfigManager->mConfigInfoMap.size() << endl;
        for (const auto& item : sConfigManager->mConfigInfoMap) {
            cout << "    " << item.first << " -> hash: " << item.second.mHash << ", expire_time: " << item.second.mExpireTime << endl;
        }
        cout << "  ConfigExpireTimeCheckpoint size: " << sConfigManager->mConfigExpireTimeCheckpoint.size() << endl;
        for (const auto& item : sConfigManager->mConfigExpireTimeCheckpoint) {
            cout << "    " << item.first << " -> hash: " << item.second.first << ", expire_time: " << item.second.second << endl;
        }

        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        cout << "CheckConfigDiff result:" << endl;
        cout << "  Added: " << diff.first.mAdded.size() << endl;
        cout << "  Modified: " << diff.first.mModified.size() << endl;
        cout << "  Removed: " << diff.first.mRemoved.size() << endl;
        
        for (const auto& config : diff.first.mAdded) {
            cout << "  Added config: " << config.mName << endl;
        }
        for (const auto& config : diff.first.mModified) {
            cout << "  Modified config: " << config.mName << endl;
        }
        for (const auto& name : diff.first.mRemoved) {
            cout << "  Removed config: " << name << endl;
        }
        
        APSARA_TEST_FALSE(diff.first.IsEmpty());
        CollectionPipelineManager::GetInstance()->UpdatePipelines(diff.first);
        sConfigManager->DumpCheckpointFile();

        cout << "Current config manager state after UpdatePipelines:" << endl;
        cout << "  ConfigInfoMap size: " << sConfigManager->mConfigInfoMap.size() << endl;
        for (const auto& item : sConfigManager->mConfigInfoMap) {
            cout << "    " << item.first << " -> hash: " << item.second.mHash << ", expire_time: " << item.second.mExpireTime << endl;
        }
        cout << "  ConfigExpireTimeCheckpoint size: " << sConfigManager->mConfigExpireTimeCheckpoint.size() << endl;
        for (const auto& item : sConfigManager->mConfigExpireTimeCheckpoint) {
            cout << "    " << item.first << " -> hash: " << item.second.first << ", expire_time: " << item.second.second << endl;
        }

        APSARA_TEST_EQUAL(3U, sConfigManager->mConfigInfoMap.size());
        {
            const auto& item = sConfigManager->mConfigInfoMap.at("new_config");
            APSARA_TEST_EQUAL(time(nullptr) + 1000U, item.mExpireTime);
            cout << "new_config expire time: " << item.mExpireTime << endl;
            APSARA_TEST_EQUAL(configHash["new_config.json"], item.mHash);
            cout << "new_config hash: " << item.mHash << endl;
            APSARA_TEST_EQUAL(ConfigType::Collection, item.mType);
            APSARA_TEST_EQUAL(mConfigDir / filenames[0], item.mFilepath);
        }
        {
            const auto& item = sConfigManager->mConfigInfoMap.at("old_config");
            APSARA_TEST_EQUAL(time(nullptr) + 1200U, item.mExpireTime);
            cout << "old_config expire time: " << item.mExpireTime << endl;
            APSARA_TEST_EQUAL(configHash["old_config.json"], item.mHash);
            cout << "old_config hash: " << item.mHash << endl;
            APSARA_TEST_EQUAL(ConfigType::Collection, item.mType);
            APSARA_TEST_EQUAL(mConfigDir / filenames[1], item.mFilepath);
        }
        {
            const auto& item = sConfigManager->mConfigInfoMap.at("unused_config");
            APSARA_TEST_EQUAL(2200000000U, item.mExpireTime);
            cout << "unused_config expire time: " << item.mExpireTime << endl;
            APSARA_TEST_EQUAL(configHash["unused_config.json"], item.mHash);
            cout << "unused_config hash: " << item.mHash << endl;
            APSARA_TEST_EQUAL(ConfigType::Collection, item.mType);
            APSARA_TEST_EQUAL(mConfigDir / "unused_config.json", item.mFilepath);
        }
        APSARA_TEST_EQUAL(0U, sConfigManager->mConfigExpireTimeCheckpoint.size());
        cout << "=== ON UPDATE PHASE END ===" << endl;
    }
}

UNIT_TEST_CASE(OnetimeConfigUpdateUnittest, OnCollectionConfigUpdate)

} // namespace logtail

UNIT_TEST_MAIN
