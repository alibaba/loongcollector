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

#include "config/OnetimeConfigInfoManager.h"
#include "config/PipelineConfig.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

struct ConfigMock : public PipelineConfig {
    ConfigMock(const string& name, unique_ptr<Json::Value>&& detail, const filesystem::path& filepath)
        : PipelineConfig(name, std::move(detail), filepath) {}

    bool Parse() override { return true; }
};

class PipelineConfigUnittest : public testing::Test {
public:
    void TestOnetimeConfig() const;

protected:
    void TearDown() override {
        sConfigManager->Clear();
        error_code ec;
        filesystem::remove(sConfigManager->mCheckpointFilePath, ec);
    }

private:
    static OnetimeConfigInfoManager* sConfigManager;
};

OnetimeConfigInfoManager* PipelineConfigUnittest::sConfigManager = OnetimeConfigInfoManager::GetInstance();


void PipelineConfigUnittest::TestOnetimeConfig() const {
    filesystem::path filepath("test_config.json");
    {
        // enable is removed
        auto configJson = make_unique<Json::Value>();
        (*configJson)["enable"] = true;
        (*configJson)["global"] = Json::objectValue;

        ConfigMock config("test", std::move(configJson), filepath);
        APSARA_TEST_FALSE(config.mDetail->isMember("enable"));
    }
    {
        // not onetime
        auto configJson = make_unique<Json::Value>();
        (*configJson)["global"] = Json::objectValue;

        ConfigMock config("test", std::move(configJson), filepath);
        APSARA_TEST_TRUE(config.GetExpireTimeIfOneTime((*config.mDetail)["global"]));
        APSARA_TEST_EQUAL(nullopt, config.mExpireTime);
    }
    {
        // invalid ExcutionTimeout
        auto configJson = make_unique<Json::Value>();
        (*configJson)["global"]["ExcutionTimeout"] = true;

        ConfigMock config("test", std::move(configJson), filepath);
        APSARA_TEST_TRUE(config.GetExpireTimeIfOneTime((*config.mDetail)["global"]));
        APSARA_TEST_EQUAL(time(nullptr) + 604800U, config.mExpireTime);
        APSARA_TEST_FALSE(config.mIsFromCheckpoint);
    }
    {
        // ExcutionTimeout too small
        auto configJson = make_unique<Json::Value>();
        (*configJson)["global"]["ExcutionTimeout"] = 1U;

        ConfigMock config("test", std::move(configJson), filepath);
        APSARA_TEST_TRUE(config.GetExpireTimeIfOneTime((*config.mDetail)["global"]));
        APSARA_TEST_EQUAL(time(nullptr) + 600U, config.mExpireTime);
        APSARA_TEST_FALSE(config.mIsFromCheckpoint);
    }
    {
        // ExcutionTimeout too large
        auto configJson = make_unique<Json::Value>();
        (*configJson)["global"]["ExcutionTimeout"] = 1000000U;

        ConfigMock config("test", std::move(configJson), filepath);
        APSARA_TEST_TRUE(config.GetExpireTimeIfOneTime((*config.mDetail)["global"]));
        APSARA_TEST_EQUAL(time(nullptr) + 604800U, config.mExpireTime);
        APSARA_TEST_FALSE(config.mIsFromCheckpoint);
    }

    // prepare checkpoint file
    {
        ofstream fout(sConfigManager->mCheckpointFilePath);
        fout << R"({
            "old_config": {
                "config_hash": 17947813715317768927,
                "expire_time": 2500000000
            },
            "obsolete_config_1": {
                "config_hash": 5643533951955847862,
                "expire_time": 1000000000
            },
            "obsolete_config_2": {
                "config_hash": 5643533951955847862,
                "expire_time": 1000000000
            }
        })";
    }
    sConfigManager->LoadCheckpointFile();

    filesystem::create_directories("config");
    {
        // new config
        { ofstream fout("config/new_config.json"); }
        auto configJson = make_unique<Json::Value>();
        (*configJson)["global"]["ExcutionTimeout"] = 3600U;

        ConfigMock config("new_config", std::move(configJson), filepath);
        APSARA_TEST_TRUE(config.GetExpireTimeIfOneTime((*config.mDetail)["global"]));
        APSARA_TEST_EQUAL(time(nullptr) + 3600U, config.mExpireTime);
        APSARA_TEST_FALSE(config.mIsFromCheckpoint);
        APSARA_TEST_EQUAL(sConfigManager->mConfigExpireTimeCheckpoint.end(),
                          sConfigManager->mConfigExpireTimeCheckpoint.find("new_config"));
    }
    {
        // old config
        { ofstream fout("config/old_config.json"); }
        auto configJson = make_unique<Json::Value>();
        (*configJson)["global"]["ExcutionTimeout"] = 600U;

        ConfigMock config("old_config", std::move(configJson), filepath);
        APSARA_TEST_TRUE(config.GetExpireTimeIfOneTime((*config.mDetail)["global"]));
        APSARA_TEST_EQUAL(2500000000U, config.mExpireTime);
        APSARA_TEST_TRUE(config.mIsFromCheckpoint);
        APSARA_TEST_EQUAL(sConfigManager->mConfigExpireTimeCheckpoint.end(),
                          sConfigManager->mConfigExpireTimeCheckpoint.find("old_config"));
    }
    {
        // obsolete config, config file existed
        { ofstream fout("config/obselete_config_1.json"); }
        auto configJson = make_unique<Json::Value>();
        (*configJson)["global"]["ExcutionTimeout"] = 1800U;

        ConfigMock config("obsolete_config_1", std::move(configJson), filepath);
        APSARA_TEST_FALSE(config.GetExpireTimeIfOneTime((*config.mDetail)["global"]));
        APSARA_TEST_EQUAL(sConfigManager->mConfigExpireTimeCheckpoint.end(),
                          sConfigManager->mConfigExpireTimeCheckpoint.find("obsolete_config_1"));
        APSARA_TEST_FALSE(filesystem::exists("obsolete_config_1.json"));
    }
    {
        // obsolete config, config file existed
        auto configJson = make_unique<Json::Value>();
        (*configJson)["global"]["ExcutionTimeout"] = 1800U;

        ConfigMock config("obsolete_config_2", std::move(configJson), filepath);
        APSARA_TEST_FALSE(config.GetExpireTimeIfOneTime((*config.mDetail)["global"]));
    }
    filesystem::remove_all("config");
}

UNIT_TEST_CASE(PipelineConfigUnittest, TestOnetimeConfig)

} // namespace logtail

UNIT_TEST_MAIN
