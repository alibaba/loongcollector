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


#include <json/json.h>

#include "AppConfig.h"
#include "common/FileSystemUtil.h"
#include "common/version.h"
#include "config/ConfigDiff.h"
#include "config/provider/CommonConfigProvider.h"
#include "config/watcher/ConfigWatcher.h"
#include "gmock/gmock.h"
#include "pipeline/PipelineManager.h"
#include "pipeline/ProcessConfigManager.h"
#include "unittest/Unittest.h"

using namespace testing;
using namespace std;

namespace logtail {

class MockCommonConfigProvider : public CommonConfigProvider {
public:
    MOCK_METHOD5(SendHttpRequest, bool(const std::string&, const std::string&, const std::string&, const std::string&, std::string&));
};

class CommonConfigProviderUnittest : public ::testing::Test {
public:
    std::string mRootDir;
    const std::string& PS = PATH_SEPARATOR;
    string ilogtailConfigPath;
    string subConfigPath;

    bool writeJsonToFile(const std::string& jsonString, const std::string& filePath) {
        Json::Reader reader;
        Json::Value root;

        bool parsingSuccessful = reader.parse(jsonString, root);
        if (!parsingSuccessful) {
            std::cout << "Failed to parse configuration\n" << reader.getFormattedErrorMessages();
            return false;
        }

        std::ofstream fout(filePath.c_str());
        if (!fout) {
            std::cout << "Failed to open file: " << filePath << std::endl;
            return false;
        }

        fout << root.toStyledString() << std::endl;
        fout.close();
        AppConfig::GetInstance()->LoadAppConfig(ilogtailConfigPath);
        return true;
    }

    // 在每个测试用例开始前的设置
    void SetUp() override {
        mRootDir = GetProcessExecutionDir();
        bfs::create_directories(mRootDir);
        ilogtailConfigPath = mRootDir + PS + STRING_FLAG(ilogtail_config);
        subConfigPath = mRootDir + PS + STRING_FLAG(ilogtail_config) + ".d";
        bfs::create_directories(subConfigPath);
        std::ofstream fout(ilogtailConfigPath.c_str());
        fout << "" << std::endl;
        MockCommonConfigProvider provider;
        provider.Init("common_v2");
        provider.Stop();
        bfs::remove_all(provider.mPipelineSourceDir.string());
        bfs::remove_all(provider.mProcessSourceDir.string());
    }

    // 在每个测试用例结束后的清理
    void TearDown() override {
        MockCommonConfigProvider provider;
        provider.Init("common_v2");
        provider.Stop();
        bfs::remove_all(provider.mPipelineSourceDir.string());
        bfs::remove_all(provider.mProcessSourceDir.string());
    }

    void TestInit();

    void TestGetConfigUpdateAndConfigWatcher();
};

void CommonConfigProviderUnittest::TestInit() {
    {
        string config = R"(
        {
            "config_server_list": [
                {
                    "cluster": "community",
                    "endpoint_list": [
                        "test.config.com:80"
                    ]
                }
            ],
            "ilogtail_tags": {
                "key1": "value1",
                "key2": "value2"
            }
        }
        )";
        APSARA_TEST_TRUE_FATAL(writeJsonToFile(config, ilogtailConfigPath));

        MockCommonConfigProvider provider;
        provider.Init("common_v2");
        APSARA_TEST_EQUAL(provider.mSequenceNum, 0);
        APSARA_TEST_EQUAL(provider.sName, "CommonConfigProvider");
        APSARA_TEST_EQUAL(provider.mConfigServerAvailable, true);
        APSARA_TEST_EQUAL(provider.mConfigServerAddresses.size(), 1);
        APSARA_TEST_EQUAL(provider.mConfigServerAddresses[0].host, "test.config.com");
        APSARA_TEST_EQUAL(provider.mConfigServerAddresses[0].port, 80);
        APSARA_TEST_EQUAL(provider.mConfigServerTags.size(), 2);
        APSARA_TEST_EQUAL(provider.mConfigServerTags["key1"], "value1");
        APSARA_TEST_EQUAL(provider.mConfigServerTags["key2"], "value2");
        APSARA_TEST_EQUAL(provider.GetOneConfigServerAddress(false).host, "test.config.com");
        APSARA_TEST_EQUAL(provider.GetOneConfigServerAddress(false).port, 80);
        APSARA_TEST_EQUAL(provider.mConfigServerAddressId, 0);
        APSARA_TEST_EQUAL(provider.GetOneConfigServerAddress(true).host, "test.config.com");
        APSARA_TEST_EQUAL(provider.GetOneConfigServerAddress(true).port, 80);
        APSARA_TEST_EQUAL(provider.mConfigServerAddressId, 0);
        provider.Stop();
    }
    {
        string config = R"(
        {
            "config_server_list": [
                {
                    "cluster": "community",
                    "endpoint_list": [
                        "test.config.com:80",
                        "test1.config.com:81",
                        "test1.config.com81",
                        "test1.config.com:0",
                        "test1.config.com:65536"
                    ]
                }
            ]
        }
        )";
        APSARA_TEST_TRUE_FATAL(writeJsonToFile(config, ilogtailConfigPath));

        MockCommonConfigProvider provider;
        provider.Init("common_v2");
        APSARA_TEST_EQUAL(provider.mSequenceNum, 0);
        APSARA_TEST_EQUAL(provider.sName, "CommonConfigProvider");
        APSARA_TEST_EQUAL(provider.mConfigServerAvailable, true);
        APSARA_TEST_EQUAL(provider.mConfigServerAddresses.size(), 2);
        APSARA_TEST_EQUAL(provider.mConfigServerAddresses[0].host, "test.config.com");
        APSARA_TEST_EQUAL(provider.mConfigServerAddresses[0].port, 80);
        APSARA_TEST_EQUAL(provider.mConfigServerAddresses[1].host, "test1.config.com");
        APSARA_TEST_EQUAL(provider.mConfigServerAddresses[1].port, 81);
        APSARA_TEST_EQUAL(provider.mConfigServerTags.size(), 0);
        APSARA_TEST_EQUAL(provider.GetOneConfigServerAddress(false).host,
                          provider.mConfigServerAddresses[provider.mConfigServerAddressId].host);
        APSARA_TEST_EQUAL(provider.GetOneConfigServerAddress(false).port,
                          provider.mConfigServerAddresses[provider.mConfigServerAddressId].port);
        APSARA_TEST_EQUAL(provider.mConfigServerAddressId, 0);

        CommonConfigProvider::ConfigServerAddress address = provider.GetOneConfigServerAddress(true);
        APSARA_TEST_EQUAL(address.host, provider.mConfigServerAddresses[provider.mConfigServerAddressId].host);
        APSARA_TEST_EQUAL(address.port, provider.mConfigServerAddresses[provider.mConfigServerAddressId].port);
        APSARA_TEST_EQUAL(provider.mConfigServerAddressId, 1);
        APSARA_TEST_EQUAL(provider.GetOneConfigServerAddress(false).host,
                          provider.mConfigServerAddresses[provider.mConfigServerAddressId].host);
        APSARA_TEST_EQUAL(provider.GetOneConfigServerAddress(false).port,
                          provider.mConfigServerAddresses[provider.mConfigServerAddressId].port);
        APSARA_TEST_EQUAL(provider.mConfigServerAddressId, 1);

        address = provider.GetOneConfigServerAddress(true);
        APSARA_TEST_EQUAL(address.host, provider.mConfigServerAddresses[provider.mConfigServerAddressId].host);
        APSARA_TEST_EQUAL(address.port, provider.mConfigServerAddresses[provider.mConfigServerAddressId].port);
        APSARA_TEST_EQUAL(provider.mConfigServerAddressId, 0);
        APSARA_TEST_EQUAL(provider.GetOneConfigServerAddress(false).host,
                          provider.mConfigServerAddresses[provider.mConfigServerAddressId].host);
        APSARA_TEST_EQUAL(provider.GetOneConfigServerAddress(false).port,
                          provider.mConfigServerAddresses[provider.mConfigServerAddressId].port);
        APSARA_TEST_EQUAL(provider.mConfigServerAddressId, 0);

        provider.Stop();
    }
    {
        string config = R"(
        {
            "config_server_list1": [
                {
                    "cluster": "community",
                    "endpoint_list": [
                        "test.config.com:80",
                        "test1.config.com:81"
                    ]
                }
            ],
            "config_server_list": [
                {
                    "cluster": "community",
                    "endpoint_list2": [
                        "test.config.com:80",
                        "test1.config.com:81"
                    ],
                    "endpoint_list": "test.config.com:80"
                }
            ]
        }
        )";
        APSARA_TEST_TRUE_FATAL(writeJsonToFile(config, ilogtailConfigPath));

        MockCommonConfigProvider provider;
        provider.Init("common_v2");
        APSARA_TEST_EQUAL(provider.mSequenceNum, 0);
        APSARA_TEST_EQUAL(provider.sName, "CommonConfigProvider");
        APSARA_TEST_EQUAL(provider.mConfigServerAvailable, false);
        APSARA_TEST_EQUAL(provider.mConfigServerAddresses.size(), 0);
        APSARA_TEST_EQUAL(provider.mConfigServerTags.size(), 0);
        APSARA_TEST_EQUAL(provider.GetOneConfigServerAddress(false).host, "");
        APSARA_TEST_EQUAL(provider.GetOneConfigServerAddress(false).port, -1);
        APSARA_TEST_EQUAL(provider.mConfigServerAddressId, 0);

        CommonConfigProvider::ConfigServerAddress address = provider.GetOneConfigServerAddress(true);
        APSARA_TEST_EQUAL(address.host, "");
        APSARA_TEST_EQUAL(address.port, -1);
        APSARA_TEST_EQUAL(provider.mConfigServerAddressId, 0);
        APSARA_TEST_EQUAL(provider.GetOneConfigServerAddress(false).host, "");
        APSARA_TEST_EQUAL(provider.GetOneConfigServerAddress(false).port, -1);
        APSARA_TEST_EQUAL(provider.mConfigServerAddressId, 0);

        address = provider.GetOneConfigServerAddress(true);
        APSARA_TEST_EQUAL(address.host, "");
        APSARA_TEST_EQUAL(address.port, -1);
        APSARA_TEST_EQUAL(provider.mConfigServerAddressId, 0);
        APSARA_TEST_EQUAL(provider.GetOneConfigServerAddress(false).host, "");
        APSARA_TEST_EQUAL(provider.GetOneConfigServerAddress(false).port, -1);
        APSARA_TEST_EQUAL(provider.mConfigServerAddressId, 0);

        provider.Stop();
    }
}

void CommonConfigProviderUnittest::TestGetConfigUpdateAndConfigWatcher() {
    // add config
    {
        string config = R"(
        {
            "config_server_list": [
                {
                    "cluster": "community",
                    "endpoint_list": [
                        "test.config.com:80"
                    ]
                }
            ],
            "ilogtail_tags": {
                "key1": "value1",
                "key2": "value2"
            }
        }
        )";
        APSARA_TEST_TRUE_FATAL(writeJsonToFile(config, ilogtailConfigPath));

        MockCommonConfigProvider provider;
        auto setResponse
            = [&provider](const string& operation, const string& reqBody, const string& configType, std::string& resp) {
                  static int sequence_num = 0;
                  configserver::proto::v2::HeartbeatRequest heartbeatReq;
                  heartbeatReq.ParseFromString(reqBody);
                  APSARA_TEST_EQUAL(heartbeatReq.sequence_num(), sequence_num);
                  sequence_num++;
                  APSARA_TEST_TRUE(heartbeatReq.capabilities() & configserver::proto::v2::AcceptsProcessConfig);
                  APSARA_TEST_TRUE(heartbeatReq.capabilities() & configserver::proto::v2::AcceptsPipelineConfig);
                  APSARA_TEST_EQUAL(heartbeatReq.instance_id(), provider.GetInstanceId());
                  APSARA_TEST_EQUAL(heartbeatReq.agent_type(), "LoongCollector");
                  APSARA_TEST_EQUAL(heartbeatReq.attributes().ip(), LogFileProfiler::mIpAddr);
                  APSARA_TEST_EQUAL(heartbeatReq.attributes().hostname(), LogFileProfiler::mHostname);
                  APSARA_TEST_EQUAL(heartbeatReq.attributes().version(), ILOGTAIL_VERSION);
                  auto it = heartbeatReq.tags().begin();
                  APSARA_TEST_EQUAL(it->name(), "key1");
                  APSARA_TEST_EQUAL(it->value(), "value1");
                  it++;
                  APSARA_TEST_EQUAL(it->name(), "key2");
                  APSARA_TEST_EQUAL(it->value(), "value2");

                  APSARA_TEST_EQUAL(heartbeatReq.running_status(), "running");
                  APSARA_TEST_EQUAL(heartbeatReq.startup_time(), provider.mStartTime);


                  configserver::proto::v2::HeartbeatResponse heartbeatRespPb;
                  heartbeatRespPb.set_capabilities(configserver::proto::v2::ResponseFlags::ReportFullState);
                  {
                      auto pipeline = heartbeatRespPb.mutable_pipeline_config_updates();
                      auto configDetail = pipeline->Add();
                      configDetail->set_name("config1");
                      configDetail->set_version(1);
                      configDetail->set_detail(R"(
                        {
                            "enable": true,
                            "flushers": [
                                {
                                    "OnlyStdout": true,
                                    "Type": "flusher_stdout"
                                }
                            ],
                            "inputs": [
                                {
                                    "FilePaths": [
                                        "\/workspaces\/test1.log"
                                    ],
                                    "Type": "input_file"
                                }
                            ]
                        }
                        )");
                      configDetail = pipeline->Add();
                      configDetail->set_name("config2");
                      configDetail->set_version(1);
                      configDetail->set_detail(R"(
                {
                    "enable": true,
                    "flushers": 
                        {
                            "OnlyStdout": true,
                            "Type": "flusher_stdout"
                        }
                    ],
                    "inputs": [
                        {
                            "FilePaths": [
                                "\/workspaces\/test1.log"
                            ],
                            "Type": "input_file"
                        }
                    ]
                }
                )");
                  }
                  {
                      auto processconfig = heartbeatRespPb.mutable_process_config_updates();
                      auto configDetail = processconfig->Add();
                      configDetail->set_name("processconfig1");
                      configDetail->set_version(1);
                      configDetail->set_detail(R"(
                        {
                                "enable": true,
                                "max_bytes_per_sec": 100012031023
                            }
                        )");
                      configDetail = processconfig->Add();
                      configDetail->set_name("processconfig2");
                      configDetail->set_version(1);
                      configDetail->set_detail(R"(
                        {
                                "enable": true
                                "max_bytes_per_sec": 100012031023
                            }
                        )");
                  }
                  {
                      auto commandconfig = heartbeatRespPb.mutable_custom_command_updates();
                      auto configDetail = commandconfig->Add();
                      configDetail->set_name("commandconfig1");
                      configDetail->set_type("history");
                      configDetail->set_detail(R"(
                        {
                                "enable": true,
                                "max_bytes_per_sec": 100012031023
                            }
                        )");
                  }
                  heartbeatRespPb.SerializeToString(&resp);
              };

        EXPECT_CALL(provider, SendHttpRequest(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillRepeatedly(::testing::DoAll(::testing::Invoke([setResponse](const string& operation,
                                                                             const string& reqBody,
                                                                             const string& configType,
                                                                             const std::string& requestId,
                                                                             std::string& resp) {
                                                 setResponse(operation, reqBody, configType, resp);
                                             }),
                                             ::testing::Return(true)));


        provider.Init("common_v2");
        APSARA_TEST_EQUAL(provider.sName, "CommonConfigProvider");
        APSARA_TEST_EQUAL(provider.mConfigServerAvailable, true);
        APSARA_TEST_EQUAL(provider.mConfigServerAddresses.size(), 1);
        APSARA_TEST_EQUAL(provider.mConfigServerAddresses[0].host, "test.config.com");
        APSARA_TEST_EQUAL(provider.mConfigServerAddresses[0].port, 80);
        APSARA_TEST_EQUAL(provider.mConfigServerTags.size(), 2);
        APSARA_TEST_EQUAL(provider.mConfigServerTags["key1"], "value1");
        APSARA_TEST_EQUAL(provider.mConfigServerTags["key2"], "value2");
        APSARA_TEST_EQUAL(provider.GetOneConfigServerAddress(false).host, "test.config.com");
        APSARA_TEST_EQUAL(provider.GetOneConfigServerAddress(false).port, 80);
        APSARA_TEST_EQUAL(provider.mConfigServerAddressId, 0);
        APSARA_TEST_EQUAL(provider.GetOneConfigServerAddress(true).host, "test.config.com");
        APSARA_TEST_EQUAL(provider.GetOneConfigServerAddress(true).port, 80);
        APSARA_TEST_EQUAL(provider.mConfigServerAddressId, 0);

        auto heartbeatRequest = provider.PrepareHeartbeat();
        configserver::proto::v2::HeartbeatResponse heartbeatResponse;
        provider.GetConfigUpdate();

        APSARA_TEST_EQUAL(provider.mPipelineConfigInfoMap.size(), 2);
        APSARA_TEST_EQUAL(provider.mPipelineConfigInfoMap["config1"].status, ConfigFeedbackStatus::APPLYING);
        APSARA_TEST_EQUAL(provider.mPipelineConfigInfoMap["config2"].status, ConfigFeedbackStatus::FAILED);

        // 处理 pipelineconfig
        PipelineConfigDiff pipelineConfigDiff = ConfigWatcher::GetInstance()->CheckPipelineConfigDiff();
        PipelineManager::GetInstance()->UpdatePipelines(pipelineConfigDiff);
        APSARA_TEST_TRUE(!pipelineConfigDiff.IsEmpty());
        APSARA_TEST_EQUAL(1U, pipelineConfigDiff.mAdded.size());
        APSARA_TEST_EQUAL(pipelineConfigDiff.mAdded[0].mName, "config1");
        APSARA_TEST_EQUAL(PipelineManager::GetInstance()->GetAllConfigNames().size(), 1);
        APSARA_TEST_EQUAL(PipelineManager::GetInstance()->GetAllConfigNames()[0], "config1");
        // 再次处理 pipelineconfig
        pipelineConfigDiff = ConfigWatcher::GetInstance()->CheckPipelineConfigDiff();
        PipelineManager::GetInstance()->UpdatePipelines(pipelineConfigDiff);
        APSARA_TEST_TRUE(pipelineConfigDiff.IsEmpty());
        APSARA_TEST_TRUE(pipelineConfigDiff.mAdded.empty());
        APSARA_TEST_EQUAL(PipelineManager::GetInstance()->GetAllConfigNames().size(), 1);
        APSARA_TEST_EQUAL(PipelineManager::GetInstance()->GetAllConfigNames()[0], "config1");


        APSARA_TEST_EQUAL(provider.mProcessConfigInfoMap.size(), 2);
        APSARA_TEST_EQUAL(provider.mProcessConfigInfoMap["processconfig1"].status, ConfigFeedbackStatus::APPLYING);
        APSARA_TEST_EQUAL(provider.mProcessConfigInfoMap["processconfig2"].status, ConfigFeedbackStatus::FAILED);

        // 处理 processconfig
        ProcessConfigDiff processConfigDiff = ConfigWatcher::GetInstance()->CheckProcessConfigDiff();
        ProcessConfigManager::GetInstance()->UpdateProcessConfigs(processConfigDiff);
        APSARA_TEST_TRUE(!processConfigDiff.IsEmpty());
        APSARA_TEST_EQUAL(1U, processConfigDiff.mAdded.size());
        APSARA_TEST_EQUAL(processConfigDiff.mAdded[0].mName, "processconfig1");
        APSARA_TEST_EQUAL(ProcessConfigManager::GetInstance()->GetAllConfigNames().size(), 1);
        APSARA_TEST_EQUAL(ProcessConfigManager::GetInstance()->GetAllConfigNames()[0], "processconfig1");
        // 再次处理 processconfig
        processConfigDiff = ConfigWatcher::GetInstance()->CheckProcessConfigDiff();
        ProcessConfigManager::GetInstance()->UpdateProcessConfigs(processConfigDiff);
        APSARA_TEST_TRUE(processConfigDiff.IsEmpty());
        APSARA_TEST_TRUE(processConfigDiff.mAdded.empty());
        APSARA_TEST_EQUAL(ProcessConfigManager::GetInstance()->GetAllConfigNames().size(), 1);
        APSARA_TEST_EQUAL(ProcessConfigManager::GetInstance()->GetAllConfigNames()[0], "processconfig1");

        provider.Stop();
    }
    // test LoadConfigFile
    {
        MockCommonConfigProvider provider;
        provider.Init("common_v2");
        APSARA_TEST_EQUAL(provider.mPipelineConfigInfoMap.size(), 1);
        APSARA_TEST_EQUAL(provider.mPipelineConfigInfoMap["config1"].status, ConfigFeedbackStatus::APPLYING);
        APSARA_TEST_EQUAL(provider.mProcessConfigInfoMap.size(), 1);
        APSARA_TEST_EQUAL(provider.mProcessConfigInfoMap["processconfig1"].status, ConfigFeedbackStatus::APPLYING);
        provider.Stop();
    }
    // delete config
    {
        string config = R"(
        {
            "config_server_list": [
                {
                    "cluster": "community",
                    "endpoint_list": [
                        "test.config.com:80"
                    ]
                }
            ],
            "ilogtail_tags": {
                "key1": "value1",
                "key2": "value2"
            }
        }
        )";
        APSARA_TEST_TRUE_FATAL(writeJsonToFile(config, ilogtailConfigPath));

        MockCommonConfigProvider provider;
        auto setResponse
            = [&provider](const string& operation, const string& reqBody, const string& configType, std::string& resp) {
                  static int sequence_num = 0;
                  configserver::proto::v2::HeartbeatRequest heartbeatReq;
                  heartbeatReq.ParseFromString(reqBody);
                  APSARA_TEST_EQUAL(heartbeatReq.sequence_num(), sequence_num);
                  sequence_num++;
                  APSARA_TEST_TRUE(heartbeatReq.capabilities() & configserver::proto::v2::AcceptsProcessConfig);
                  APSARA_TEST_TRUE(heartbeatReq.capabilities() & configserver::proto::v2::AcceptsPipelineConfig);
                  APSARA_TEST_EQUAL(heartbeatReq.instance_id(), provider.GetInstanceId());
                  APSARA_TEST_EQUAL(heartbeatReq.agent_type(), "LoongCollector");
                  APSARA_TEST_EQUAL(heartbeatReq.attributes().ip(), LogFileProfiler::mIpAddr);
                  APSARA_TEST_EQUAL(heartbeatReq.attributes().hostname(), LogFileProfiler::mHostname);
                  APSARA_TEST_EQUAL(heartbeatReq.attributes().version(), ILOGTAIL_VERSION);
                  auto it = heartbeatReq.tags().begin();
                  APSARA_TEST_EQUAL(it->name(), "key1");
                  APSARA_TEST_EQUAL(it->value(), "value1");
                  it++;
                  APSARA_TEST_EQUAL(it->name(), "key2");
                  APSARA_TEST_EQUAL(it->value(), "value2");

                  APSARA_TEST_EQUAL(heartbeatReq.running_status(), "running");
                  APSARA_TEST_EQUAL(heartbeatReq.startup_time(), provider.mStartTime);


                  configserver::proto::v2::HeartbeatResponse heartbeatRespPb;
                  heartbeatRespPb.set_capabilities(configserver::proto::v2::ResponseFlags::ReportFullState);
                  // pipeline
                  {
                      auto pipeline = heartbeatRespPb.mutable_pipeline_config_updates();
                      auto configDetail = pipeline->Add();
                      configDetail->set_name("config1");
                      configDetail->set_version(-1);
                      configDetail->set_detail(R"(
                        {
                            "enable": true,
                            "flushers": [
                                {
                                    "OnlyStdout": true,
                                    "Type": "flusher_stdout"
                                }
                            ],
                            "inputs": [
                                {
                                    "FilePaths": [
                                        "\/workspaces\/test1.log"
                                    ],
                                    "Type": "input_file"
                                }
                            ]
                        }
                        )");
                      configDetail = pipeline->Add();
                      configDetail->set_name("config2");
                      configDetail->set_version(-1);
                      configDetail->set_detail(R"(
                {
                    "enable": true,
                    "flushers": 
                        {
                            "OnlyStdout": true,
                            "Type": "flusher_stdout"
                        }
                    ],
                    "inputs": [
                        {
                            "FilePaths": [
                                "\/workspaces\/test1.log"
                            ],
                            "Type": "input_file"
                        }
                    ]
                }
                )");
                  }
                  // processconfig
                  {
                      auto processconfig = heartbeatRespPb.mutable_process_config_updates();
                      auto configDetail = processconfig->Add();
                      configDetail->set_name("processconfig1");
                      configDetail->set_version(-1);
                      configDetail->set_detail(R"(
                        {
                                "enable": true,
                                "max_bytes_per_sec": 100012031023
                            }
                        )");
                      configDetail = processconfig->Add();
                      configDetail->set_name("processconfig2");
                      configDetail->set_version(-1);
                      configDetail->set_detail(R"(
                        {
                                "enable": true
                                "max_bytes_per_sec": 100012031023
                            }
                        )");
                  }
                  // commandconfig
                  {
                      auto commandconfig = heartbeatRespPb.mutable_custom_command_updates();
                      auto configDetail = commandconfig->Add();
                      configDetail->set_name("commandconfig1");
                      configDetail->set_type("history");
                      configDetail->set_detail(R"(
                        {
                                "enable": true,
                                "max_bytes_per_sec": 100012031023
                            }
                        )");
                  }
                  heartbeatRespPb.SerializeToString(&resp);
              };

        EXPECT_CALL(provider, SendHttpRequest(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillRepeatedly(::testing::DoAll(::testing::Invoke([setResponse](const string& operation,
                                                                             const string& reqBody,
                                                                             const string& configType,
                                                                             const std::string& requestId,
                                                                             std::string& resp) {
                                                 setResponse(operation, reqBody, configType, resp);
                                             }),
                                             ::testing::Return(true)));


        provider.Init("common_v2");
        APSARA_TEST_EQUAL(provider.sName, "CommonConfigProvider");
        APSARA_TEST_EQUAL(provider.mConfigServerAvailable, true);
        APSARA_TEST_EQUAL(provider.mConfigServerAddresses.size(), 1);
        APSARA_TEST_EQUAL(provider.mConfigServerAddresses[0].host, "test.config.com");
        APSARA_TEST_EQUAL(provider.mConfigServerAddresses[0].port, 80);
        APSARA_TEST_EQUAL(provider.mConfigServerTags.size(), 2);
        APSARA_TEST_EQUAL(provider.mConfigServerTags["key1"], "value1");
        APSARA_TEST_EQUAL(provider.mConfigServerTags["key2"], "value2");
        APSARA_TEST_EQUAL(provider.GetOneConfigServerAddress(false).host, "test.config.com");
        APSARA_TEST_EQUAL(provider.GetOneConfigServerAddress(false).port, 80);
        APSARA_TEST_EQUAL(provider.mConfigServerAddressId, 0);
        APSARA_TEST_EQUAL(provider.GetOneConfigServerAddress(true).host, "test.config.com");
        APSARA_TEST_EQUAL(provider.GetOneConfigServerAddress(true).port, 80);
        APSARA_TEST_EQUAL(provider.mConfigServerAddressId, 0);

        auto heartbeatRequest = provider.PrepareHeartbeat();
        configserver::proto::v2::HeartbeatResponse heartbeatResponse;
        provider.GetConfigUpdate();

        APSARA_TEST_TRUE(provider.mPipelineConfigInfoMap.empty());

        // 处理pipelineConfigDiff
        PipelineConfigDiff pipelineConfigDiff = ConfigWatcher::GetInstance()->CheckPipelineConfigDiff();
        PipelineManager::GetInstance()->UpdatePipelines(pipelineConfigDiff);
        APSARA_TEST_TRUE(!pipelineConfigDiff.IsEmpty());
        APSARA_TEST_EQUAL(1U, pipelineConfigDiff.mRemoved.size());
        APSARA_TEST_EQUAL(pipelineConfigDiff.mRemoved[0], "config1");
        APSARA_TEST_TRUE(PipelineManager::GetInstance()->GetAllConfigNames().empty());
        // 再次处理pipelineConfigDiff
        pipelineConfigDiff = ConfigWatcher::GetInstance()->CheckPipelineConfigDiff();
        PipelineManager::GetInstance()->UpdatePipelines(pipelineConfigDiff);
        APSARA_TEST_TRUE(pipelineConfigDiff.IsEmpty());
        APSARA_TEST_TRUE(pipelineConfigDiff.mRemoved.empty());
        APSARA_TEST_TRUE(PipelineManager::GetInstance()->GetAllConfigNames().empty());

        APSARA_TEST_TRUE(provider.mProcessConfigInfoMap.empty());
        // 处理processConfigDiff
        ProcessConfigDiff processConfigDiff = ConfigWatcher::GetInstance()->CheckProcessConfigDiff();
        ProcessConfigManager::GetInstance()->UpdateProcessConfigs(processConfigDiff);
        APSARA_TEST_TRUE(ProcessConfigManager::GetInstance()->GetAllConfigNames().empty());
        APSARA_TEST_EQUAL(1U, processConfigDiff.mRemoved.size());
        APSARA_TEST_EQUAL(processConfigDiff.mRemoved[0], "processconfig1");

        // 再次处理processConfigDiff
        processConfigDiff = ConfigWatcher::GetInstance()->CheckProcessConfigDiff();
        ProcessConfigManager::GetInstance()->UpdateProcessConfigs(processConfigDiff);
        APSARA_TEST_TRUE(ProcessConfigManager::GetInstance()->GetAllConfigNames().empty());
        APSARA_TEST_TRUE(processConfigDiff.IsEmpty());
        APSARA_TEST_TRUE(processConfigDiff.mRemoved.empty());

        provider.Stop();
    }
}

UNIT_TEST_CASE(CommonConfigProviderUnittest, TestInit)
UNIT_TEST_CASE(CommonConfigProviderUnittest, TestGetConfigUpdateAndConfigWatcher)

}; // namespace logtail


int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
