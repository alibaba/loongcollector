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

#include "json/json.h"

#include "app_config/AppConfig.h"
#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "common/JsonUtil.h"
#include "common/http/AsynCurlRunner.h"
#include "common/timer/Timer.h"
#include "ebpf/Config.h"
#include "ebpf/EBPFServer.h"
#include "plugin/input/InputNetworkObserver.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class InputNetworkObserverUnittest : public testing::Test {
public:
    void TestName();
    void TestSupportAck();
    void OnSuccessfulInit();
    void OnFailedInit();
    void OnSuccessfulStart();
    void OnSuccessfulStop();
    void TestSaeConfig();

protected:
    void SetUp() override {
        p.mName = "test_config";
        ctx.SetConfigName("test_config");
        ctx.SetPipeline(p);
        ebpf::EBPFServer::GetInstance()->Init();
    }

    void TearDown() override {
        ebpf::EBPFServer::GetInstance()->Stop();
        Timer::GetInstance()->Stop();
        AsynCurlRunner::GetInstance()->Stop();
    }

private:
    CollectionPipeline p;
    CollectionPipelineContext ctx;
};

void InputNetworkObserverUnittest::TestName() {
    InputNetworkObserver input;
    std::string name = input.Name();
    APSARA_TEST_EQUAL(name, "input_network_observer");
}

void InputNetworkObserverUnittest::TestSupportAck() {
    InputNetworkObserver input;
    bool supportAck = input.SupportAck();
    APSARA_TEST_TRUE(supportAck);
}

void InputNetworkObserverUnittest::OnSuccessfulInit() {
    unique_ptr<InputNetworkObserver> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    // valid optional param
    configStr = R"(
        {
            "Type": "input_network_observer",
            "ProbeConfig": 
            {
                "L7Config": {
                    "Enable": true,
                    "SampleRate": 1.0,
                    "EnableLog": true,
                    "EnableMetric": true,
                    "EnableSpan": true,
                },
                "L4Config": {
                    "Enable": true
                },
                "ApmConfig": {
                    "Workspace": "prod",
                    "AppName": "prod-app",
                    "AppId": "xxx@xxx",
                    "ServiceId": "aaa@xxx",
                },
                "WorkloadSelectors": [
                    {
                        "WorkloadName": "default-workload-name",
                        "WorkloadKind": "default-workload-kind",
                        "Namespace": "default-ns"
                    },
                ]
                    
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputNetworkObserver());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->sName, "input_network_observer");
    logtail::ebpf::ObserverNetworkOption thisObserver = input->mNetworkOption;
    APSARA_TEST_EQUAL(thisObserver.mL7Config.mEnable, true);
    APSARA_TEST_EQUAL(thisObserver.mL7Config.mEnableLog, true);
    APSARA_TEST_EQUAL(thisObserver.mL7Config.mEnableMetric, true);
    APSARA_TEST_EQUAL(thisObserver.mL7Config.mEnableSpan, true);
    APSARA_TEST_EQUAL(thisObserver.mL7Config.mSampleRate, 1.0);

    APSARA_TEST_EQUAL(thisObserver.mL4Config.mEnable, true);

    APSARA_TEST_EQUAL(thisObserver.mApmConfig.mAppId, "xxx@xxx");
    APSARA_TEST_EQUAL(thisObserver.mApmConfig.mServiceId, "aaa@xxx");
    APSARA_TEST_EQUAL(thisObserver.mApmConfig.mAppName, "prod-app");
    APSARA_TEST_EQUAL(thisObserver.mApmConfig.mWorkspace, "prod");


    APSARA_TEST_EQUAL(thisObserver.mSelectors.size(), 1);
    APSARA_TEST_EQUAL(thisObserver.mSelectors[0].mNamespace, "default-ns");
    APSARA_TEST_EQUAL(thisObserver.mSelectors[0].mWorkloadKind, "default-workload-kind");
    APSARA_TEST_EQUAL(thisObserver.mSelectors[0].mWorkloadName, "default-workload-name");
}

void InputNetworkObserverUnittest::OnFailedInit() {
    unique_ptr<InputNetworkObserver> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    // invalid optional param
    configStr = R"(
        {
            "Type": "input_network_observer",
            "ProbeConfig": 
            {
                "L7Config": {
                    "Enable": true,
                    "SampleRate": 1.0,
                    "EnableLog": true,
                    "EnableMetric": 2, // invalid optional param
                },
                "L4Config": {
                    "Enable": true
                },
                "ApmConfig": {
                    "Workspace": "prod",
                    "AppName": "prod-app",
                    "AppId": "xxx@xxx",
                    "ServiceId": "aaa@xxx",
                },
                "WorkloadSelectors": [
                    {
                        "WorkloadName": "default-workload-name",
                        "WorkloadKind": "default-workload-kind",
                        "Namespace": "default-ns"
                    },
                ]
                    
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputNetworkObserver());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->sName, "input_network_observer");
    logtail::ebpf::ObserverNetworkOption thisObserver = input->mNetworkOption;
    APSARA_TEST_EQUAL(thisObserver.mL7Config.mEnable, true);
    APSARA_TEST_EQUAL(thisObserver.mL7Config.mEnableLog, true);
    APSARA_TEST_EQUAL(thisObserver.mL7Config.mEnableMetric, false);
    APSARA_TEST_EQUAL(thisObserver.mL7Config.mEnableSpan, false);
    APSARA_TEST_EQUAL(thisObserver.mL7Config.mSampleRate, 1.0);

    APSARA_TEST_EQUAL(thisObserver.mL4Config.mEnable, true);

    APSARA_TEST_EQUAL(thisObserver.mApmConfig.mAppId, "xxx@xxx");
    APSARA_TEST_EQUAL(thisObserver.mApmConfig.mServiceId, "aaa@xxx");
    APSARA_TEST_EQUAL(thisObserver.mApmConfig.mAppName, "prod-app");
    APSARA_TEST_EQUAL(thisObserver.mApmConfig.mWorkspace, "prod");

    APSARA_TEST_EQUAL(thisObserver.mSelectors.size(), 1);
    APSARA_TEST_EQUAL(thisObserver.mSelectors[0].mNamespace, "default-ns");
    APSARA_TEST_EQUAL(thisObserver.mSelectors[0].mWorkloadKind, "default-workload-kind");
    APSARA_TEST_EQUAL(thisObserver.mSelectors[0].mWorkloadName, "default-workload-name");

    // lack of optional param
    configStr = R"(
        {
            "Type": "input_network_observer",
            "ProbeConfig": 
            {
                "L7Config": {
                    "Enable": true,
                    "SampleRate": 1.0,
                    "EnableLog": true,
                    "EnableMetric": 2, // invalid optional param
                },
                "L4Config": {
                    "Enable": true
                },
                "ApmConfig": {
                    "Workspace": "prod",
                    "AppName": "prod-app",
                    "AppId": "xxx@xxx",
                    "ServiceId": "aaa@xxx",
                }  
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputNetworkObserver());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->sName, "input_network_observer");
    thisObserver = input->mNetworkOption;
    APSARA_TEST_EQUAL(thisObserver.mL7Config.mEnable, true);
    APSARA_TEST_EQUAL(thisObserver.mL7Config.mEnableLog, true);
    APSARA_TEST_EQUAL(thisObserver.mL7Config.mEnableMetric, false);
    APSARA_TEST_EQUAL(thisObserver.mL7Config.mEnableSpan, false);
    APSARA_TEST_EQUAL(thisObserver.mL7Config.mSampleRate, 1.0);

    APSARA_TEST_EQUAL(thisObserver.mL4Config.mEnable, true);

    APSARA_TEST_EQUAL(thisObserver.mApmConfig.mAppId, "xxx@xxx");
    APSARA_TEST_EQUAL(thisObserver.mApmConfig.mServiceId, "aaa@xxx");
    APSARA_TEST_EQUAL(thisObserver.mApmConfig.mAppName, "prod-app");
    APSARA_TEST_EQUAL(thisObserver.mApmConfig.mWorkspace, "prod");

    APSARA_TEST_EQUAL(thisObserver.mSelectors.size(), 0);

    // lag of mandatory param + error param level
    configStr = R"(
        {
            "Type": "input_network_observer",
            "L7Config": {
                "Enable": true,
                "SampleRate": 1.0,
                "EnableLog": true,
                "EnableMetric": true,
                "EnableSpan": true,
            },
            "L4Config": {
                "Enable": true
            },
            "ApmConfig": {
                "Workspace": "prod",
                "AppName": "prod-app",
                "AppId": "xxx@xxx",
                "ServiceId": "aaa@xxx",
            },
            "WorkloadSelectors": [
                {
                    "WorkloadName": "default-workload-name",
                    "WorkloadKind": "default-workload-kind",
                    "Namespace": "default-ns"
                },
            ]      
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputNetworkObserver());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "1");
    APSARA_TEST_FALSE(input->Init(configJson, optionalGoPipeline));
}

void InputNetworkObserverUnittest::OnSuccessfulStart() {
    unique_ptr<InputNetworkObserver> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "input_network_observer",
            "ProbeConfig": 
            {
                "L7Config": {
                    "Enable": true,
                    "SampleRate": 1.0,
                    "EnableLog": true,
                    "EnableMetric": 2, // invalid optional param
                },
                "L4Config": {
                    "Enable": true
                },
                "ApmConfig": {
                    "Workspace": "prod",
                    "AppName": "prod-app",
                    "AppId": "xxx@xxx",
                    "ServiceId": "aaa@xxx",
                },
                "WorkloadSelectors": [
                    {
                        "WorkloadName": "default-workload-name",
                        "WorkloadKind": "default-workload-kind",
                        "Namespace": "default-ns"
                    },
                ]
                    
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputNetworkObserver());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_TRUE(input->Start());
    APSARA_TEST_TRUE(input->Stop(true));
}

void InputNetworkObserverUnittest::OnSuccessfulStop() {
    unique_ptr<InputNetworkObserver> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "input_network_observer",
            "ProbeConfig": 
            {
                "L7Config": {
                    "Enable": true,
                    "SampleRate": 1.0,
                    "EnableLog": true,
                    "EnableMetric": true,
                    "EnableSpan": true,
                },
                "L4Config": {
                    "Enable": true
                },
                "ApmConfig": {
                    "Workspace": "prod",
                    "AppName": "prod-app",
                    "AppId": "xxx@xxx",
                    "ServiceId": "aaa@xxx",
                },
                "WorkloadSelectors": [
                    {
                        "WorkloadName": "default-workload-name",
                        "WorkloadKind": "default-workload-kind",
                        "Namespace": "default-ns"
                    },
                ]
                    
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputNetworkObserver());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_TRUE(input->Start());
}

void InputNetworkObserverUnittest::TestSaeConfig() {
    unique_ptr<InputNetworkObserver> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    configStr = R"(
        {
            "ProbeConfig": {
                "ApmConfig": {
                    "AppId": "76fe228e-2c2e-4363-9f08-dcc412502062",
                    "AppName": "zizhao-ebpf-test",
                    "ServiceId": "hc4fs1hkb3@71ec1c84e2ca4cc069064",
                    "Workspace": "default-cms-1760720386195983-cn-beijing"
                },
                "L4Config": {
                    "Enable": true
                },
                "L7Config": {
                    "Enable": true,
                    "EnableMetric": true,
                    "EnableProtocols": [
                        "http"
                    ],
                    "EnableSpan": true,
                    "SampleRate": 1
                }
            },
            "Type": "input_network_observer"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputNetworkObserver());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
}

UNIT_TEST_CASE(InputNetworkObserverUnittest, TestName)
UNIT_TEST_CASE(InputNetworkObserverUnittest, TestSupportAck)
UNIT_TEST_CASE(InputNetworkObserverUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(InputNetworkObserverUnittest, OnFailedInit)
UNIT_TEST_CASE(InputNetworkObserverUnittest, OnSuccessfulStart)
UNIT_TEST_CASE(InputNetworkObserverUnittest, OnSuccessfulStop)
UNIT_TEST_CASE(InputNetworkObserverUnittest, TestSaeConfig)

} // namespace logtail

UNIT_TEST_MAIN
