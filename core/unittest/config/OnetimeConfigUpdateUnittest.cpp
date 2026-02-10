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
        fs::create_directories(mConfigDir);
        PipelineConfigWatcher::GetInstance()->AddSource(mConfigDir.string());
    }

    void TearDown() override {
        // CollectionPipelineManager::GetInstance()->Clear();
        // PipelineManagerMock::GetInstance()->ClearEnvironment();
        PipelineConfigWatcher::GetInstance()->ClearEnvironment();
        sConfigManager->Clear();
        fs::remove_all(mConfigDir);
        error_code ec;
        fs::remove(sConfigManager->mCheckpointFilePath, ec);
    }

private:
    static OnetimeConfigInfoManager* sConfigManager;

    fs::path mConfigDir = "continuous_pipeline_config";
};

OnetimeConfigInfoManager* OnetimeConfigUpdateUnittest::sConfigManager = OnetimeConfigInfoManager::GetInstance();

void OnetimeConfigUpdateUnittest::OnCollectionConfigUpdate() const {
    map<string, uint64_t> configHash;
    map<string, uint64_t> inputsHash;
    map<string, uint32_t> excutionTimeout;

    // Helper function to calculate inputsHash (only inputs, without ExcutionTimeout)
    auto calculateInputsHash = [](const Json::Value& root) -> uint64_t {
        const char* inputsKey = "inputs";
        const auto* inputsIt = root.find(inputsKey, inputsKey + strlen(inputsKey));
        if (inputsIt != nullptr && inputsIt->isArray()) {
            Json::Value inputsHashValue(Json::objectValue);
            inputsHashValue["inputs"] = *inputsIt;
            return static_cast<uint64_t>(Hash(inputsHashValue));
        }
        return 0;
    };

    // Helper function to extract ExcutionTimeout
    auto extractExcutionTimeout = [](const Json::Value& root) -> uint32_t {
        const char* globalKey = "global";
        const char* excutionTimeoutKey = "ExcutionTimeout";
        const auto* globalIt = root.find(globalKey, globalKey + strlen(globalKey));
        if (globalIt != nullptr && globalIt->isObject()) {
            const auto* timeoutIt = globalIt->find(excutionTimeoutKey, excutionTimeoutKey + strlen(excutionTimeoutKey));
            if (timeoutIt != nullptr && timeoutIt->isUInt()) {
                return timeoutIt->asUInt();
            }
        }
        return 0;
    };

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
    inputsHash["unused_config.json"] = calculateInputsHash(root);
    excutionTimeout["unused_config.json"] = extractExcutionTimeout(root);

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
            ofstream fout(mConfigDir / filenames[i], ios::binary);
            fout << configDetails[i];
        }

        // compute config hash, inputs hash and excution timeout
        for (size_t i = 0; i < configDetails.size(); ++i) {
            Json::Value root;
            string errorMsg;
            ParseJsonTable(configDetails[i], root, errorMsg);
            configHash[filenames[i]] = Hash(root);
            inputsHash[filenames[i]] = calculateInputsHash(root);
            excutionTimeout[filenames[i]] = extractExcutionTimeout(root);
        }

        // prepare checkpoint file
        {
            ofstream fout(sConfigManager->mCheckpointFilePath, ios::binary);
            fout << R"({
            "changed_config": {
                "config_hash": 8279028812201817660,
                "expire_time": 2000000000,
                "inputs_hash": )"
                    + ToString(inputsHash["changed_config.json"]) + R"(,
                "excution_timeout": )"
                    + ToString(excutionTimeout["changed_config.json"]) + R"(
            },
            "old_config": {
                "config_hash": )"
                    + ToString(configHash["old_config.json"]) + R"(,
                "expire_time": 2500000000,
                "inputs_hash": )"
                    + ToString(inputsHash["old_config.json"]) + R"(,
                "excution_timeout": )"
                    + ToString(excutionTimeout["old_config.json"]) + R"(
            },
            "obsolete_config": {
                "config_hash": )"
                    + ToString(configHash["obsolete_config.json"]) + R"(,
                "expire_time": 1000000000,
                "inputs_hash": )"
                    + ToString(inputsHash["obsolete_config.json"]) + R"(,
                "excution_timeout": )"
                    + ToString(excutionTimeout["obsolete_config.json"]) + R"(
            },
            "unused_config": {
                "config_hash": )"
                    + ToString(configHash["unused_config.json"]) + R"(,
                "expire_time": 2200000000,
                "inputs_hash": )"
                    + ToString(inputsHash["unused_config.json"]) + R"(,
                "excution_timeout": )"
                    + ToString(excutionTimeout["unused_config.json"]) + R"(
            }
        })";
        }
        sConfigManager->LoadCheckpointFile();

        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_TRUE(diff.first.HasDiff());
        CollectionPipelineManager::GetInstance()->UpdatePipelines(diff.first);
        sConfigManager->DumpCheckpointFile();

        APSARA_TEST_EQUAL(3U, sConfigManager->mConfigInfoMap.size());
        {
            const auto& item = sConfigManager->mConfigInfoMap.at("new_config");
            APSARA_TEST_EQUAL(time(nullptr) + 3600U, item.mExpireTime);
            APSARA_TEST_EQUAL(configHash["new_config.json"], item.mConfigHash);
            APSARA_TEST_EQUAL(ConfigType::Collection, item.mType);
            APSARA_TEST_EQUAL(mConfigDir / filenames[0], item.mFilepath);
        }
        {
            const auto& item = sConfigManager->mConfigInfoMap.at("changed_config");
            APSARA_TEST_EQUAL(time(nullptr) + 7200U, item.mExpireTime);
            APSARA_TEST_EQUAL(configHash["changed_config.json"], item.mConfigHash);
            APSARA_TEST_EQUAL(ConfigType::Collection, item.mType);
            APSARA_TEST_EQUAL(mConfigDir / filenames[1], item.mFilepath);
        }
        {
            const auto& item = sConfigManager->mConfigInfoMap.at("old_config");
            APSARA_TEST_EQUAL(2500000000U, item.mExpireTime);
            APSARA_TEST_EQUAL(configHash["old_config.json"], item.mConfigHash);
            APSARA_TEST_EQUAL(ConfigType::Collection, item.mType);
            APSARA_TEST_EQUAL(mConfigDir / filenames[2], item.mFilepath);
        }
        APSARA_TEST_EQUAL(1U, sConfigManager->mConfigCheckpointMap.size());
        APSARA_TEST_NOT_EQUAL(sConfigManager->mConfigCheckpointMap.end(),
                              sConfigManager->mConfigCheckpointMap.find("unused_config"));
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
            fs::path filePath = mConfigDir / filenames[i];
            ofstream fout(filePath, ios::binary);
            fout << configDetails[i];
            fout.close();
            // 强制更新文件修改时间
            fs::file_time_type newTime = fs::file_time_type::clock::now();
            fs::last_write_time(filePath, newTime);
            // 添加一个小延迟确保文件系统更新
            this_thread::sleep_for(chrono::milliseconds(10));
        }
        {
            ofstream fout(mConfigDir / "unused_config.json", ios::binary);
            fout << unusedConfigDetail;
            fout.close();
        }
        fs::remove(mConfigDir / "changed_config.json");

        // compute config hash, inputs hash and excution timeout
        for (size_t i = 0; i < configDetails.size(); ++i) {
            Json::Value root;
            string errorMsg;
            ParseJsonTable(configDetails[i], root, errorMsg);
            configHash[filenames[i]] = Hash(root);
            inputsHash[filenames[i]] = calculateInputsHash(root);
            excutionTimeout[filenames[i]] = extractExcutionTimeout(root);
        }

        auto diff = PipelineConfigWatcher::GetInstance()->CheckConfigDiff();
        APSARA_TEST_TRUE(diff.first.HasDiff());
        CollectionPipelineManager::GetInstance()->UpdatePipelines(diff.first);
        sConfigManager->DumpCheckpointFile();

        APSARA_TEST_EQUAL(3U, sConfigManager->mConfigInfoMap.size());
        {
            const auto& item = sConfigManager->mConfigInfoMap.at("new_config");
            APSARA_TEST_EQUAL(time(nullptr) + 1000U, item.mExpireTime);
            APSARA_TEST_EQUAL(configHash["new_config.json"], item.mConfigHash);
            APSARA_TEST_EQUAL(ConfigType::Collection, item.mType);
            APSARA_TEST_EQUAL(mConfigDir / filenames[0], item.mFilepath);
        }
        {
            const auto& item = sConfigManager->mConfigInfoMap.at("old_config");
            APSARA_TEST_EQUAL(time(nullptr) + 1200U, item.mExpireTime);
            APSARA_TEST_EQUAL(configHash["old_config.json"], item.mConfigHash);
            APSARA_TEST_EQUAL(ConfigType::Collection, item.mType);
            APSARA_TEST_EQUAL(mConfigDir / filenames[1], item.mFilepath);
        }
        {
            const auto& item = sConfigManager->mConfigInfoMap.at("unused_config");
            APSARA_TEST_EQUAL(2200000000U, item.mExpireTime);
            APSARA_TEST_EQUAL(configHash["unused_config.json"], item.mConfigHash);
            APSARA_TEST_EQUAL(ConfigType::Collection, item.mType);
            APSARA_TEST_EQUAL(mConfigDir / "unused_config.json", item.mFilepath);
        }
        APSARA_TEST_EQUAL(0U, sConfigManager->mConfigCheckpointMap.size());
    }
}

UNIT_TEST_CASE(OnetimeConfigUpdateUnittest, OnCollectionConfigUpdate)

} // namespace logtail

UNIT_TEST_MAIN
