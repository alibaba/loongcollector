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

#include "common/JsonUtil.h"
#include "config/OnetimeConfigManager.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class OnetimeConfigManagerUnittest : public testing::Test {
public:
    void TestGetOnetimeConfigStatusFromCheckpoint() const;
    void TestUpdateConfig() const;
    void TestCheckpointFile() const;

protected:
    // static void SetUpTestCase() { sManager->mCheckpointRootPath = filesystem::path("./input_static_file"); }

    // void SetUp() override { filesystem::create_directories(sManager->mCheckpointRootPath); }

    void TearDown() override {
        // sManager->ClearUnusedCheckpoints();
        // sManager->mInputCheckpointMap.clear();
        // filesystem::remove_all(sManager->mCheckpointRootPath);
    }

private:
    static OnetimeConfigManager* sManager;
};

void OnetimeConfigManagerUnittest::TestGetOnetimeConfigStatusFromCheckpoint() const {
}

void OnetimeConfigManagerUnittest::TestUpdateConfig() const {
    filesystem::create_directories("test_config");
    { ofstream fout("test_config/test_config_1.json"); }
    { ofstream fout("test_config/test_config_2.json"); }

    // restart
    APSARA_TEST_TRUE(sManager->AddConfig(
        "test_config_1", ConfigType::Collection, filesystem::path("test_config/test_config_1.json"), 1, 1000000000));
    APSARA_TEST_TRUE(sManager->AddConfig(
        "test_config_2", ConfigType::Collection, filesystem::path("test_config/test_config_2.json"), 2, 1500000000));
    APSARA_TEST_TRUE(sManager->AddConfig(
        "test_config_3", ConfigType::Collection, filesystem::path("test_config/test_config_3.json"), 3, 4000000000));
    APSARA_TEST_EQUAL(3U, sManager->mConfigInfoMap.size());
    {
        const auto& info = sManager->mConfigInfoMap.at("test_config_1");
        APSARA_TEST_EQUAL(ConfigType::Collection, info.mType);
        APSARA_TEST_EQUAL(filesystem::path("test_config/test_config_1.json"), info.mFilepath);
        APSARA_TEST_EQUAL(1U, info.mHash);
        APSARA_TEST_EQUAL(1000000000U, info.mExpireTime);
    }
    {
        const auto& info = sManager->mConfigInfoMap.at("test_config_2");
        APSARA_TEST_EQUAL(ConfigType::Collection, info.mType);
        APSARA_TEST_EQUAL(filesystem::path("test_config/test_config_2.json"), info.mFilepath);
        APSARA_TEST_EQUAL(2U, info.mHash);
        APSARA_TEST_EQUAL(1500000000U, info.mExpireTime);
    }
    {
        const auto& info = sManager->mConfigInfoMap.at("test_config_3");
        APSARA_TEST_EQUAL(ConfigType::Collection, info.mType);
        APSARA_TEST_EQUAL(filesystem::path("test_config/test_config_3.json"), info.mFilepath);
        APSARA_TEST_EQUAL(3U, info.mHash);
        APSARA_TEST_EQUAL(4000000000U, info.mExpireTime);
    }

    // update
    APSARA_TEST_TRUE(sManager->RemoveConfig("test_config_1"));
    APSARA_TEST_TRUE(sManager->AddConfig(
        "test_config_1", ConfigType::Collection, filesystem::path("test_config/test_config_1.json"), 1, 1200000000));
    APSARA_TEST_FALSE(sManager->RemoveConfig("test_config_4"));
    APSARA_TEST_EQUAL(3U, sManager->mConfigInfoMap.size());
    {
        const auto& info = sManager->mConfigInfoMap.at("test_config_1");
        APSARA_TEST_EQUAL(ConfigType::Collection, info.mType);
        APSARA_TEST_EQUAL(filesystem::path("test_config/test_config_1.json"), info.mFilepath);
        APSARA_TEST_EQUAL(1U, info.mHash);
        APSARA_TEST_EQUAL(1200000000U, info.mExpireTime);
    }

    // delete timeout config
    filesystem::remove("test_config/test_config_1.json");
    sManager->DeleteTimeoutConfigFiles();
    APSARA_TEST_EQUAL(1U, sManager->mConfigInfoMap.size());
    APSARA_TEST_NOT_EQUAL(sManager->mConfigInfoMap.end(), sManager->mConfigInfoMap.find("test_config_3"));
    APSARA_TEST_FALSE(filesystem::exists("test_config/test_config_2.json"));

    filesystem::remove_all("test_config");
}

void OnetimeConfigManagerUnittest::TestCheckpointFile() const {
}

OnetimeConfigManager* OnetimeConfigManagerUnittest::sManager = OnetimeConfigManager::GetInstance();

UNIT_TEST_CASE(OnetimeConfigManagerUnittest, TestGetOnetimeConfigStatusFromCheckpoint)
UNIT_TEST_CASE(OnetimeConfigManagerUnittest, TestUpdateConfig)
UNIT_TEST_CASE(OnetimeConfigManagerUnittest, TestCheckpointFile)

} // namespace logtail

UNIT_TEST_MAIN
