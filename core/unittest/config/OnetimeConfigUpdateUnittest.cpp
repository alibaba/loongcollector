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

// #include "common/FileSystemUtil.h"
#include "collection_pipeline/CollectionPipelineManager.h"
#include "common/JsonUtil.h"
#include "config/OnetimeConfigManager.h"
#include "config/watcher/PipelineConfigWatcher.h"
#include "unittest/Unittest.h"
#include "unittest/plugin/PluginMock.h"

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
    static OnetimeConfigManager* sConfigManager;

    filesystem::path mConfigDir = "./continuous_pipeline_config";
};

OnetimeConfigManager* OnetimeConfigUpdateUnittest::sConfigManager = OnetimeConfigManager::GetInstance();

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
            ofstream fout(mConfigDir / filenames[i]);
            fout << configDetails[i];
        }

        // compute config hash
        for (size_t i = 0; i < configDetails.size(); ++i) {
            Json::Value root;
            string errorMsg;
            ParseJsonTable(configDetails[i], root, errorMsg);
            configHash[filenames[i]] = Hash(root);
        }

        // prepare checkpoint file
        {
            ofstream fout(sConfigManager->mCheckpointFilePath);
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
        }
        sConfigManager->LoadCheckpointFile();

        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_FALSE(diff.first.IsEmpty());
        CollectionPipelineManager::GetInstance()->UpdatePipelines(diff.first);
        sConfigManager->DumpCheckpointFile();

        APSARA_TEST_EQUAL(3U, sConfigManager->mConfigInfoMap.size());
        {
            const auto& item = sConfigManager->mConfigInfoMap.at("new_config");
            APSARA_TEST_EQUAL(time(nullptr) + 3600U, item.mExpireTime);
            APSARA_TEST_EQUAL(configHash["new_config.json"], item.mHash);
            APSARA_TEST_EQUAL(ConfigType::Collection, item.mType);
            APSARA_TEST_EQUAL(mConfigDir / filenames[0], item.mFilepath);
        }
        {
            const auto& item = sConfigManager->mConfigInfoMap.at("changed_config");
            APSARA_TEST_EQUAL(time(nullptr) + 7200U, item.mExpireTime);
            APSARA_TEST_EQUAL(configHash["changed_config.json"], item.mHash);
            APSARA_TEST_EQUAL(ConfigType::Collection, item.mType);
            APSARA_TEST_EQUAL(mConfigDir / filenames[1], item.mFilepath);
        }
        {
            const auto& item = sConfigManager->mConfigInfoMap.at("old_config");
            APSARA_TEST_EQUAL(2500000000U, item.mExpireTime);
            APSARA_TEST_EQUAL(configHash["old_config.json"], item.mHash);
            APSARA_TEST_EQUAL(ConfigType::Collection, item.mType);
            APSARA_TEST_EQUAL(mConfigDir / filenames[2], item.mFilepath);
        }
        APSARA_TEST_EQUAL(1U, sConfigManager->mConfigExpireTimeCheckpoint.size());
        APSARA_TEST_NOT_EQUAL(sConfigManager->mConfigExpireTimeCheckpoint.end(),
                              sConfigManager->mConfigExpireTimeCheckpoint.find("unused_config"));
    }

    // on update
    {
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
            ofstream fout(mConfigDir / filenames[i]);
            fout << configDetails[i];
        }
        {
            ofstream fout(mConfigDir / "unused_config.json");
            fout << unusedConfigDetail;
        }
        filesystem::remove(mConfigDir / "changed_config.json");

        // compute config hash
        for (size_t i = 0; i < configDetails.size(); ++i) {
            Json::Value root;
            string errorMsg;
            ParseJsonTable(configDetails[i], root, errorMsg);
            configHash[filenames[i]] = Hash(root);
        }

        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_FALSE(diff.first.IsEmpty());
        CollectionPipelineManager::GetInstance()->UpdatePipelines(diff.first);
        sConfigManager->DumpCheckpointFile();

        APSARA_TEST_EQUAL(3U, sConfigManager->mConfigInfoMap.size());
        {
            const auto& item = sConfigManager->mConfigInfoMap.at("new_config");
            APSARA_TEST_EQUAL(time(nullptr) + 1000U, item.mExpireTime);
            APSARA_TEST_EQUAL(configHash["new_config.json"], item.mHash);
            APSARA_TEST_EQUAL(ConfigType::Collection, item.mType);
            APSARA_TEST_EQUAL(mConfigDir / filenames[0], item.mFilepath);
        }
        {
            const auto& item = sConfigManager->mConfigInfoMap.at("old_config");
            APSARA_TEST_EQUAL(time(nullptr) + 1200U, item.mExpireTime);
            APSARA_TEST_EQUAL(configHash["old_config.json"], item.mHash);
            APSARA_TEST_EQUAL(ConfigType::Collection, item.mType);
            APSARA_TEST_EQUAL(mConfigDir / filenames[1], item.mFilepath);
        }
        {
            const auto& item = sConfigManager->mConfigInfoMap.at("unused_config");
            APSARA_TEST_EQUAL(2200000000U, item.mExpireTime);
            APSARA_TEST_EQUAL(configHash["unused_config.json"], item.mHash);
            APSARA_TEST_EQUAL(ConfigType::Collection, item.mType);
            APSARA_TEST_EQUAL(mConfigDir / "unused_config.json", item.mFilepath);
        }
        APSARA_TEST_EQUAL(0U, sConfigManager->mConfigExpireTimeCheckpoint.size());
    }
}

UNIT_TEST_CASE(OnetimeConfigUpdateUnittest, OnCollectionConfigUpdate)

} // namespace logtail

UNIT_TEST_MAIN
