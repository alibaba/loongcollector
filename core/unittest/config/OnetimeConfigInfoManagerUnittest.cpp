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

#include "common/FileSystemUtil.h"
#include "common/JsonUtil.h"
#include "config/OnetimeConfigInfoManager.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class OnetimeConfigInfoManagerUnittest : public testing::Test {
public:
    void TestLoadCheckpointFile() const;
    void TestGetOnetimeConfigStatusFromCheckpoint() const;
    void TestUpdateConfig() const;
    void TestDumpCheckpointFile() const;

protected:
    static void SetUpTestCase() { sManager->mCheckpointFilePath = filesystem::path(".") / "onetime_config_info.json"; }

    void TearDown() override {
        sManager->Clear();
        error_code ec;
        filesystem::remove(sManager->mCheckpointFilePath, ec);
    }

private:
    static OnetimeConfigInfoManager* sManager;
};

OnetimeConfigInfoManager* OnetimeConfigInfoManagerUnittest::sManager = OnetimeConfigInfoManager::GetInstance();

void OnetimeConfigInfoManagerUnittest::TestLoadCheckpointFile() const {
    {
        // non-existing checkpoint file
        APSARA_TEST_FALSE(sManager->LoadCheckpointFile());
    }
    {
        // invalid checkpoint file
        filesystem::create_directories("onetime_config_info.json");
        APSARA_TEST_FALSE(sManager->LoadCheckpointFile());
        filesystem::remove_all("onetime_config_info.json");
    }
    {
        // empty content
        ofstream fout(sManager->mCheckpointFilePath);
        APSARA_TEST_FALSE(sManager->LoadCheckpointFile());
    }
    {
        // invalid checkpoint file format
        {
            ofstream fout(sManager->mCheckpointFilePath);
            fout << "[}";
        }
        APSARA_TEST_FALSE(sManager->LoadCheckpointFile());
    }
    {
        // checkpoint file is not object
        {
            ofstream fout(sManager->mCheckpointFilePath);
            fout << "[]";
        }
        APSARA_TEST_FALSE(sManager->LoadCheckpointFile());
    }
    {
        // all kinds of item format
        {
            ofstream fout(sManager->mCheckpointFilePath);
            fout << R"({
  "test_config_1": {
    "config_hash": 1111111111111111,
    "expire_time": 1234567890
  },
  "test_config_2": {
    "expire_time": 1234567891
  },
  "test_config_3": {
    "config_hash": 1111111111111112
  },
  "test_config_4": []
})";
        }
        APSARA_TEST_TRUE(sManager->LoadCheckpointFile());
        APSARA_TEST_EQUAL(1U, sManager->mConfigExpireTimeCheckpoint.size());
        const auto& item = sManager->mConfigExpireTimeCheckpoint.at("test_config_1");
        APSARA_TEST_EQUAL(1111111111111111U, std::get<0>(item));
        APSARA_TEST_EQUAL(1234567890U, std::get<1>(item));
    }
}

void OnetimeConfigInfoManagerUnittest::TestGetOnetimeConfigStatusFromCheckpoint() const {
    {
        ofstream fout(sManager->mCheckpointFilePath);
        fout << R"({
    "test_config_1": {
        "config_hash": 1,
        "expire_time": 2000000000,
        "inputs_hash": 0,
        "excution_timeout": 3600
    },
    "test_config_2": {
        "config_hash": 2,
        "expire_time": 1000000000,
        "inputs_hash": 0,
        "excution_timeout": 1800
    },
    "test_config_3": {
        "config_hash": 3,
        "expire_time": 2500000000,
        "inputs_hash": 0,
        "excution_timeout": 7200
    },
    "test_config_4": {
        "config_hash": 4,
        "expire_time": 1800000000,
        "inputs_hash": 100,
        "excution_timeout": 600
    }
})";
    }
    sManager->LoadCheckpointFile();
    APSARA_TEST_EQUAL(4U, sManager->mConfigExpireTimeCheckpoint.size());

    uint32_t expireTime = 0;
    // test_config_1: hash and inputsHash both match, should return OLD
    APSARA_TEST_EQUAL(
        OnetimeConfigStatus::OLD,
        sManager->GetOnetimeConfigStatusFromCheckpoint("test_config_1", 1U, false, 0U, 3600U, &expireTime));
    APSARA_TEST_EQUAL(2000000000U, expireTime);
    // test_config_2: hash matches but expired, should return OBSOLETE
    APSARA_TEST_EQUAL(
        OnetimeConfigStatus::OBSOLETE,
        sManager->GetOnetimeConfigStatusFromCheckpoint("test_config_2", 2U, false, 0U, 1800U, &expireTime));
    APSARA_TEST_EQUAL(1000000000U, expireTime);
    // test_config_3: config_hash changed from 3 to 4, but inputsHash and excutionTimeout unchanged (0 and 7200)
    // forceRerunWhenUpdate is false, so should return UPDATED instead of NEW
    APSARA_TEST_EQUAL(
        OnetimeConfigStatus::UPDATED,
        sManager->GetOnetimeConfigStatusFromCheckpoint("test_config_3", 4U, false, 0U, 7200U, &expireTime));
    APSARA_TEST_EQUAL(2500000000U, expireTime);
    // test_config_4: config_hash changed from 4 to 5, and inputsHash also changed (100 != 0)
    // forceRerunWhenUpdate is false, but inputsHash mismatch, so should return NEW
    APSARA_TEST_EQUAL(
        OnetimeConfigStatus::NEW,
        sManager->GetOnetimeConfigStatusFromCheckpoint("test_config_4", 5U, false, 0U, 600U, &expireTime));
    // test_config_5: not in checkpoint, should return NEW
    APSARA_TEST_EQUAL(OnetimeConfigStatus::NEW,
                      sManager->GetOnetimeConfigStatusFromCheckpoint("test_config_5", 10U, false, 0U, 0U, &expireTime));
    APSARA_TEST_EQUAL(0U, sManager->mConfigExpireTimeCheckpoint.size());

    INT32_FLAG(unused_checkpoints_clear_interval_sec) = 0;
    sManager->ClearUnusedCheckpoints();
    APSARA_TEST_TRUE(sManager->mConfigExpireTimeCheckpoint.empty());
    sManager->ClearUnusedCheckpoints();
    INT32_FLAG(unused_checkpoints_clear_interval_sec) = 600;
}

void OnetimeConfigInfoManagerUnittest::TestUpdateConfig() const {
    filesystem::create_directories("test_config");
    { ofstream fout("test_config/test_config_1.json"); }
    { ofstream fout("test_config/test_config_2.json"); }

    // restart
    APSARA_TEST_TRUE(sManager->UpdateConfig("test_config_1",
                                            ConfigType::Collection,
                                            filesystem::path("test_config/test_config_1.json"),
                                            1,
                                            1000000000,
                                            0,
                                            3600));
    APSARA_TEST_TRUE(sManager->UpdateConfig("test_config_2",
                                            ConfigType::Collection,
                                            filesystem::path("test_config/test_config_2.json"),
                                            2,
                                            1500000000,
                                            0,
                                            1800));
    APSARA_TEST_TRUE(sManager->UpdateConfig("test_config_3",
                                            ConfigType::Collection,
                                            filesystem::path("test_config/test_config_3.json"),
                                            3,
                                            4000000000,
                                            0,
                                            7200));
    APSARA_TEST_EQUAL(3U, sManager->mConfigInfoMap.size());
    {
        const auto& info = sManager->mConfigInfoMap.at("test_config_1");
        APSARA_TEST_EQUAL(ConfigType::Collection, info.mType);
        APSARA_TEST_EQUAL(filesystem::path("test_config/test_config_1.json"), info.mFilepath);
        APSARA_TEST_EQUAL(1U, info.mHash);
        APSARA_TEST_EQUAL(1000000000U, info.mExpireTime);
        APSARA_TEST_EQUAL(0U, info.mInputsHash);
        APSARA_TEST_EQUAL(3600U, info.mExcutionTimeout);
    }
    {
        const auto& info = sManager->mConfigInfoMap.at("test_config_2");
        APSARA_TEST_EQUAL(ConfigType::Collection, info.mType);
        APSARA_TEST_EQUAL(filesystem::path("test_config/test_config_2.json"), info.mFilepath);
        APSARA_TEST_EQUAL(2U, info.mHash);
        APSARA_TEST_EQUAL(1500000000U, info.mExpireTime);
        APSARA_TEST_EQUAL(0U, info.mInputsHash);
        APSARA_TEST_EQUAL(1800U, info.mExcutionTimeout);
    }
    {
        const auto& info = sManager->mConfigInfoMap.at("test_config_3");
        APSARA_TEST_EQUAL(ConfigType::Collection, info.mType);
        APSARA_TEST_EQUAL(filesystem::path("test_config/test_config_3.json"), info.mFilepath);
        APSARA_TEST_EQUAL(3U, info.mHash);
        APSARA_TEST_EQUAL(4000000000U, info.mExpireTime);
        APSARA_TEST_EQUAL(0U, info.mInputsHash);
        APSARA_TEST_EQUAL(7200U, info.mExcutionTimeout);
    }

    // update
    APSARA_TEST_TRUE(sManager->UpdateConfig("test_config_1",
                                            ConfigType::Collection,
                                            filesystem::path("test_config/test_config_1.json"),
                                            1,
                                            1200000000,
                                            0,
                                            3600));
    APSARA_TEST_FALSE(sManager->RemoveConfig("test_config_4"));
    APSARA_TEST_EQUAL(3U, sManager->mConfigInfoMap.size());
    {
        const auto& info = sManager->mConfigInfoMap.at("test_config_1");
        APSARA_TEST_EQUAL(ConfigType::Collection, info.mType);
        APSARA_TEST_EQUAL(filesystem::path("test_config/test_config_1.json"), info.mFilepath);
        APSARA_TEST_EQUAL(1U, info.mHash);
        APSARA_TEST_EQUAL(1200000000U, info.mExpireTime);
        APSARA_TEST_EQUAL(0U, info.mInputsHash);
        APSARA_TEST_EQUAL(3600U, info.mExcutionTimeout);
    }

    // delete timeout config
    filesystem::remove("test_config/test_config_1.json");
    sManager->DeleteTimeoutConfigFiles();
    APSARA_TEST_EQUAL(1U, sManager->mConfigInfoMap.size());
    APSARA_TEST_NOT_EQUAL(sManager->mConfigInfoMap.end(), sManager->mConfigInfoMap.find("test_config_3"));
    APSARA_TEST_FALSE(filesystem::exists("test_config/test_config_2.json"));

    filesystem::remove_all("test_config");
}

void OnetimeConfigInfoManagerUnittest::TestDumpCheckpointFile() const {
    {
        ofstream fout(sManager->mCheckpointFilePath);
        fout << R"({
    "test_config_1": {
        "config_hash": 1,
        "expire_time": 2000000000,
        "inputs_hash": 0,
        "excution_timeout": 3600
    },
    "test_config_2": {
        "config_hash": 2,
        "expire_time": 1000000000,
        "inputs_hash": 0,
        "excution_timeout": 1800
    },
    "test_config_3": {
        "config_hash": 3,
        "expire_time": 2500000000,
        "inputs_hash": 0,
        "excution_timeout": 7200
    },
    "test_config_4": {
        "config_hash": 4,
        "expire_time": 1800000000,
        "inputs_hash": 0,
        "excution_timeout": 600
    }
})";
    }
    sManager->LoadCheckpointFile();
    APSARA_TEST_EQUAL(4U, sManager->mConfigExpireTimeCheckpoint.size());

    uint32_t expireTime = 0;

    sManager->GetOnetimeConfigStatusFromCheckpoint("test_config_1", 1U, false, 0U, 3600U, &expireTime);
    sManager->UpdateConfig("test_config_1",
                           ConfigType::Collection,
                           filesystem::path("test_config/test_config_1.json"),
                           1,
                           2000000000,
                           0,
                           3600);
    sManager->GetOnetimeConfigStatusFromCheckpoint("test_config_2", 2U, false, 0U, 1800U, &expireTime);
    sManager->GetOnetimeConfigStatusFromCheckpoint("test_config_3", 4U, false, 0U, 7200U, &expireTime);
    sManager->UpdateConfig("test_config_3",
                           ConfigType::Collection,
                           filesystem::path("test_config/test_config_3.json"),
                           4,
                           2200000000,
                           0,
                           7200);
    sManager->UpdateConfig("test_config_5",
                           ConfigType::Collection,
                           filesystem::path("test_config/test_config_5.json"),
                           5,
                           2100000000,
                           0,
                           600);

    sManager->DumpCheckpointFile();

    string content, errorMsg;
    ReadFile(sManager->mCheckpointFilePath.string(), content);
    Json::Value res;
    APSARA_TEST_TRUE(ParseJsonTable(content, res, errorMsg));
    APSARA_TEST_EQUAL(4U, res.size());
    APSARA_TEST_EQUAL(1U, res["test_config_1"]["config_hash"].asUInt64());
    APSARA_TEST_EQUAL(2000000000U, res["test_config_1"]["expire_time"].asUInt());
    APSARA_TEST_EQUAL(0U, res["test_config_1"]["inputs_hash"].asUInt64());
    APSARA_TEST_EQUAL(3600U, res["test_config_1"]["excution_timeout"].asUInt());
    APSARA_TEST_EQUAL(4U, res["test_config_3"]["config_hash"].asUInt64());
    APSARA_TEST_EQUAL(2200000000U, res["test_config_3"]["expire_time"].asUInt());
    APSARA_TEST_EQUAL(0U, res["test_config_3"]["inputs_hash"].asUInt64());
    APSARA_TEST_EQUAL(7200U, res["test_config_3"]["excution_timeout"].asUInt());
    APSARA_TEST_EQUAL(4U, res["test_config_4"]["config_hash"].asUInt64());
    APSARA_TEST_EQUAL(1800000000U, res["test_config_4"]["expire_time"].asUInt());
    APSARA_TEST_EQUAL(0U, res["test_config_4"]["inputs_hash"].asUInt64());
    APSARA_TEST_EQUAL(600U, res["test_config_4"]["excution_timeout"].asUInt());
    APSARA_TEST_EQUAL(5U, res["test_config_5"]["config_hash"].asUInt64());
    APSARA_TEST_EQUAL(2100000000U, res["test_config_5"]["expire_time"].asUInt());
    APSARA_TEST_EQUAL(0U, res["test_config_5"]["inputs_hash"].asUInt64());
    APSARA_TEST_EQUAL(600U, res["test_config_5"]["excution_timeout"].asUInt());
}

UNIT_TEST_CASE(OnetimeConfigInfoManagerUnittest, TestLoadCheckpointFile)
UNIT_TEST_CASE(OnetimeConfigInfoManagerUnittest, TestGetOnetimeConfigStatusFromCheckpoint)
UNIT_TEST_CASE(OnetimeConfigInfoManagerUnittest, TestUpdateConfig)
UNIT_TEST_CASE(OnetimeConfigInfoManagerUnittest, TestDumpCheckpointFile)

} // namespace logtail

UNIT_TEST_MAIN
