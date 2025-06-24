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

#include <memory>
#include <string>
#include <vector>

#include "Application.h"
#include "collection_pipeline/plugin/PluginRegistry.h"
#include "collection_pipeline/queue/BoundedProcessQueue.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "collection_pipeline/queue/QueueKeyManager.h"
#include "collection_pipeline/queue/SLSSenderQueueItem.h"
#include "collection_pipeline/queue/SenderQueueManager.h"
#include "common/JsonUtil.h"
#include "config/CollectionConfig.h"
#include "file_server/EventDispatcher.h"
#include "file_server/event_handler/LogInput.h"
#include "runner/FlusherRunner.h"
#include "runner/ProcessorRunner.h"
#include "unittest/Unittest.h"
#include "unittest/config/PipelineManagerMock.h"
#include "unittest/pipeline/HttpSinkMock.h"
#include "unittest/pipeline/LogtailPluginMock.h"
#include "unittest/plugin/PluginMock.h"

using namespace std;

namespace logtail {

class InputFileMock : public InputMock {
public:
    static const std::string sName;
};

const std::string InputFileMock::sName = "input_file_mock";

class InputFileMock2 : public InputMock {
public:
    static const std::string sName;
};

const std::string InputFileMock2::sName = "input_file_mock2";

class ProcessorMock2 : public ProcessorMock {
public:
    static const std::string sName;
};

const std::string ProcessorMock2::sName = "processor_mock2";

class FlusherSLSMock : public FlusherSLS {
public:
    static const std::string sName;

    bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) override {
        string errorMsg;
        uint32_t delay = 0;
        GetOptionalUIntParam(config, "SendDelay", delay, errorMsg);
        mSendDelay = std::chrono::milliseconds(delay);
        return FlusherSLS::Init(config, optionalGoPipeline);
    }

    bool Send(PipelineEventGroup&& g) override {
        if (mSendDelay > std::chrono::milliseconds(0)) {
            std::this_thread::sleep_for(mSendDelay);
        }
        return FlusherSLS::Send(std::move(g));
    }

    bool BuildRequest(SenderQueueItem* item,
                      std::unique_ptr<HttpSinkRequest>& req,
                      bool* keepItem,
                      std::string* errMsg) override {
        auto data = static_cast<SLSSenderQueueItem*>(item);
        std::map<std::string, std::string> header;
        req = std::make_unique<HttpSinkRequest>(
            "POST", false, "test-host", 80, "/test-operation", "", header, data->mData, item);
        return true;
    }

private:
    std::chrono::milliseconds mSendDelay = std::chrono::milliseconds(0);
};

const std::string FlusherSLSMock::sName = "flusher_sls_mock";

class FlusherSLSMock2 : public FlusherSLSMock {
public:
    static const std::string sName;
};

const std::string FlusherSLSMock2::sName = "flusher_sls_mock2";

class PipelineUpdateUnittest : public testing::Test {
public:
    void TestFileServerStart();
    void TestPipelineParamUpdateCase1() const;
    void TestPipelineParamUpdateCase2() const;
    void TestPipelineParamUpdateCase3() const;
    void TestPipelineParamUpdateCase4() const;
    void TestPipelineTypeUpdateCase1() const;
    void TestPipelineTypeUpdateCase2() const;
    void TestPipelineTypeUpdateCase3() const;
    void TestPipelineTypeUpdateCase4() const;
    void TestPipelineTopoUpdateCase1() const;
    void TestPipelineTopoUpdateCase2() const;
    void TestPipelineTopoUpdateCase3() const;
    void TestPipelineTopoUpdateCase4() const;
    void TestPipelineTopoUpdateCase5() const;
    void TestPipelineTopoUpdateCase6() const;
    void TestPipelineTopoUpdateCase7() const;
    void TestPipelineTopoUpdateCase8() const;
    void TestPipelineTopoUpdateCase9() const;
    void TestPipelineTopoUpdateCase10() const;
    void TestPipelineTopoUpdateCase11() const;
    void TestPipelineTopoUpdateCase12() const;
    void TestPipelineInputBlock() const;
    void TestPipelineGoInputBlockCase1() const;
    void TestPipelineGoInputBlockCase2() const;
    void TestPipelineIsolationCase1() const;
    void TestPipelineIsolationCase2() const;
    void TestPipelineUpdateManyCase1() const;
    void TestPipelineUpdateManyCase2() const;
    void TestPipelineUpdateManyCase3() const;
    void TestPipelineUpdateManyCase4() const;
    void TestPipelineUpdateManyCase5() const;
    void TestPipelineUpdateManyCase6() const;
    void TestPipelineUpdateManyCase7() const;
    void TestPipelineUpdateManyCase8() const;
    void TestPipelineUpdateManyCase9() const;
    void TestPipelineUpdateManyCase10() const;
    void TestPipelineRelease() const;

protected:
    static void SetUpTestCase() {
        PluginRegistry::GetInstance()->LoadPlugins();
        LoadPluginMock();
        PluginRegistry::GetInstance()->RegisterInputCreator(new StaticInputCreator<InputFileMock>());
        PluginRegistry::GetInstance()->RegisterInputCreator(new StaticInputCreator<InputFileMock2>());
        PluginRegistry::GetInstance()->RegisterProcessorCreator(new StaticProcessorCreator<ProcessorMock2>());
        PluginRegistry::GetInstance()->RegisterFlusherCreator(new StaticFlusherCreator<FlusherSLSMock>());
        PluginRegistry::GetInstance()->RegisterFlusherCreator(new StaticFlusherCreator<FlusherSLSMock2>());

        FlusherRunner::GetInstance()->mEnableRateLimiter = false;
        SenderQueueManager::GetInstance()->mDefaultQueueParam.mCapacity = 1; // test extra buffer
        ProcessQueueManager::GetInstance()->mBoundedQueueParam.mCapacity = 100;
        ProcessQueueManager::GetInstance()->mBoundedQueueParam.mLowWatermark = 50;
        ProcessQueueManager::GetInstance()->mBoundedQueueParam.mHighWatermark = 80;
        FLAGS_sls_client_send_compress = false;
        AppConfig::GetInstance()->mSendRequestConcurrency = 100;
        AppConfig::GetInstance()->mSendRequestGlobalConcurrency = 200;
    }

    static void TearDownTestCase() {
        PluginRegistry::GetInstance()->UnloadPlugins();
        Application::GetInstance()->SetSigTermSignalFlag(true);
        FileServer::GetInstance()->Stop();
    }

    void SetUp() override { ProcessorRunner::GetInstance()->Init(); }

    void TearDown() override {
        EventDispatcher::GetInstance()->CleanEnviroments();
        for (auto& pipeline : CollectionPipelineManager::GetInstance()->GetAllPipelines()) {
            pipeline.second->Stop(true);
        }
        CollectionPipelineManager::GetInstance()->mPipelineNameEntityMap.clear();
        ProcessorRunner::GetInstance()->Stop();
        FlusherRunner::GetInstance()->Stop();
        HttpSink::GetInstance()->Stop();
    }

private:
    Json::Value GeneratePipelineConfigJson(const string& inputConfig,
                                           const string& processorConfig,
                                           const string& flusherConfig) const {
        Json::Value json;
        string errorMsg;
        ParseJsonTable(R"(
            {
                "valid": true,
                "inputs": [)"
                           + inputConfig + R"(],
                "processors": [)"
                           + processorConfig + R"(],
                "flushers": [)"
                           + flusherConfig + R"(]
            })",
                       json,
                       errorMsg);
        return json;
    }

    void AddDataToProcessQueue(const string& configName, const string& data) const {
        auto key = QueueKeyManager::GetInstance()->GetKey(configName);
        PipelineEventGroup g(std::make_shared<SourceBuffer>());
        auto event = g.AddLogEvent();
        event->SetContent("content", data);
        std::unique_ptr<ProcessQueueItem> item = std::make_unique<ProcessQueueItem>(std::move(g), 0);
        {
            auto manager = ProcessQueueManager::GetInstance();
            manager->CreateOrUpdateBoundedQueue(key, 0, CollectionPipelineContext{});
            lock_guard<mutex> lock(manager->mQueueMux);
            auto iter = manager->mQueues.find(key);
            APSARA_TEST_NOT_EQUAL(iter, manager->mQueues.end());
            static_cast<BoundedProcessQueue*>((*iter->second.first).get())->mValidToPush = true;
            APSARA_TEST_TRUE_FATAL((*iter->second.first)->Push(std::move(item)));
        }
    };

    void AddDataToProcessor(const string& configName, const string& data) const {
        auto key = QueueKeyManager::GetInstance()->GetKey(configName);
        PipelineEventGroup g(std::make_shared<SourceBuffer>());
        auto event = g.AddLogEvent();
        event->SetContent("content", data);
        ProcessorRunner::GetInstance()->PushQueue(key, 0, std::move(g));
    }

    void AddDataToSenderQueue(const string& configName, string&& data, Flusher* flusher) const {
        auto key = flusher->mQueueKey;
        std::unique_ptr<SLSSenderQueueItem> item = std::make_unique<SLSSenderQueueItem>(
            std::move(data), data.size(), flusher, key, "", RawDataType::EVENT_GROUP);
        {
            auto manager = SenderQueueManager::GetInstance();
            manager->CreateQueue(key, "", CollectionPipelineContext{});
            lock_guard<mutex> lock(manager->mQueueMux);
            auto iter = manager->mQueues.find(key);
            APSARA_TEST_NOT_EQUAL(iter, manager->mQueues.end());
            APSARA_TEST_TRUE_FATAL(iter->second.Push(std::move(item)));
        }
    }

    void BlockProcessor(std::string configName) const {
        auto pipeline = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
        auto processor
            = static_cast<ProcessorMock*>(const_cast<Processor*>(pipeline->mProcessorLine[0].get()->mPlugin.get()));
        processor->Block();
    }

    void UnBlockProcessor(std::string configName) const {
        auto pipeline = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
        auto processor
            = static_cast<ProcessorMock*>(const_cast<Processor*>(pipeline->mProcessorLine[0].get()->mPlugin.get()));
        processor->Unblock();
    }

    void VerifyData(std::string logstore, size_t from, size_t to, bool checkProcessorLocalData = false) const {
        size_t i = from;
        size_t j = 0;
        size_t retryTimes = 15;
        for (size_t retry = 0; retry < retryTimes; ++retry) {
            auto requests = HttpSinkMock::GetInstance()->GetRequests();
            i = from;
            j = 0;
            while ((i < to + 1) && j < requests.size()) {
                auto content = requests[j].mData;
                auto actualLogstore = static_cast<FlusherSLS*>(requests[j].mFlusher)->mLogstore;
                if (actualLogstore != logstore) {
                    ++j;
                    continue;
                }
                bool correctData = true;
                correctData &= content.find("test-data-" + to_string(i)) != string::npos;
                if (checkProcessorLocalData) {
                    correctData &= content.find(PROCESSOR_MOCK_LOCAL_CONTENT_KEY) != string::npos;
                    correctData &= content.find(PROCESSOR_MOCK_LOCAL_CONTENT_VALUE) != string::npos;
                }
                if (correctData) {
                    ++i;
                    continue;
                }
                ++j;
            }
            if (i == to + 1 || retry == retryTimes - 1) {
                APSARA_TEST_EQUAL_FATAL(to + 1, i);
                return;
            }
            this_thread::sleep_for(chrono::milliseconds(1000));
        }
    }

    string nativeInputFileConfig = R"(
        {
            "Type": "input_file",
            "FilePaths": [
                "/tmp/not_found.log"
            ]
        })";
    string nativeInputConfig = R"(
        {
            "Type": "input_file_mock",
            "FilePaths": [
                "/tmp/not_found.log"
            ]
        })";
    string nativeInputConfig2 = R"(
        {
            "Type": "input_file_mock",
            "FilePaths": [
                "/tmp/*.log"
            ]
        })";
    string nativeInputConfig3 = R"(
        {
            "Type": "input_file_mock2",
            "FilePaths": [
                "/tmp/not_found.log"
            ]
        })";
    string nativeProcessorConfig = R"(
        {
            "Type": "processor_mock"
        })";
    string nativeProcessorConfig2 = R"(
        {
            "Type": "processor_mock",
            "Regex": ".*"
        })";
    string nativeProcessorConfig3 = R"(
        {
            "Type": "processor_mock2"
        })";
    string nativeFlusherConfig = R"(
        {
            "Type": "flusher_sls_mock",
            "Project": "test_project",
            "Logstore": "test_logstore_1",
            "Region": "test_region",
            "Endpoint": "test_endpoint"
        })";
    string nativeFlusherConfig2 = R"(
        {
            "Type": "flusher_sls_mock",
            "Project": "test_project",
            "Logstore": "test_logstore_2",
            "Region": "test_region",
            "Endpoint": "test_endpoint"
        })";
    string slowFlusherConfig = R"(
        {
            "Type": "flusher_sls_mock",
            "Project": "test_project",
            "Logstore": "test_logstore_2",
            "Region": "test_region",
            "Endpoint": "test_endpoint",
            "SendDelay": 1000
        })";
    string nativeFlusherConfig3 = R"(
        {
            "Type": "flusher_sls_mock2",
            "Project": "test_project",
            "Logstore": "test_logstore_3",
            "Region": "test_region",
            "Endpoint": "test_endpoint"
        })";
    string goInputConfig = R"(
        {
            "Type": "service_docker_stdout_v2"
        })";
    string goInputConfig2 = R"(
        {
            "Type": "service_docker_stdout_v2",
            "Stdout": true
        })";
    string goInputConfig3 = R"(
        {
            "Type": "service_docker_stdout_v3"
        })";
    string goProcessorConfig = R"(
        {
            "Type": "processor_regex"
        })";
    string goProcessorConfig2 = R"(
        {
            "Type": "processor_regex",
            "Regex": ".*"
        })";
    string goProcessorConfig3 = R"(
        {
            "Type": "processor_regex2"
        })";
    string goFlusherConfig = R"(
        {
            "Type": "flusher_stdout"
        })";
    string goFlusherConfig2 = R"(
        {
            "Type": "flusher_stdout",
            "Stdout": true
        })";
    string goFlusherConfig3 = R"(
        {
            "Type": "flusher_stdout2"
        })";
};

void PipelineUpdateUnittest::TestFileServerStart() {
    Json::Value nativePipelineConfigJson
        = GeneratePipelineConfigJson(nativeInputFileConfig, nativeProcessorConfig, nativeFlusherConfig);
    Json::Value goPipelineConfigJson = GeneratePipelineConfigJson(goInputConfig, goProcessorConfig, goFlusherConfig);
    auto pipelineManager = PipelineManagerMock::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig nativePipelineConfigObj
        = CollectionConfig("test-file-1", make_unique<Json::Value>(nativePipelineConfigJson));
    nativePipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(nativePipelineConfigObj));
    CollectionConfig goPipelineConfigObj
        = CollectionConfig("test-file-2", make_unique<Json::Value>(goPipelineConfigJson));
    goPipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(goPipelineConfigObj));

    pipelineManager->UpdatePipelines(diff);
    APSARA_TEST_EQUAL_FATAL(2U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(false, LogInput::GetInstance()->mInteruptFlag);
}

void PipelineUpdateUnittest::TestPipelineParamUpdateCase1() const {
    // C++ -> C++ -> C++
    const std::string configName = "test1";
    // load old pipeline
    Json::Value pipelineConfigJson
        = GeneratePipelineConfigJson(nativeInputConfig, nativeProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    // Add data without trigger
    auto pipeline = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto flusher = const_cast<Flusher*>(pipeline->GetFlushers()[0].get()->GetPlugin());
    auto processor
        = static_cast<ProcessorMock*>(const_cast<Processor*>(pipeline->mProcessorLine[0].get()->mPlugin.get()));
    AddDataToSenderQueue(configName, "test-data-1", flusher);
    AddDataToSenderQueue(configName, "test-data-2", flusher);
    AddDataToSenderQueue(configName, "test-data-3", flusher);

    AddDataToProcessor(configName, "test-data-4");

    AddDataToProcessQueue(configName, "test-data-5");
    AddDataToProcessQueue(configName, "test-data-6");
    AddDataToProcessQueue(configName, "test-data-7");

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate
        = GeneratePipelineConfigJson(nativeInputConfig2, nativeProcessorConfig2, nativeFlusherConfig2);
    CollectionConfigDiff diffUpdate;
    CollectionConfig pipelineConfigObjUpdate
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate));
    pipelineConfigObjUpdate.Parse();
    diffUpdate.mModified.push_back(std::move(pipelineConfigObjUpdate));
    auto result = async(launch::async, [&]() {
        this_thread::sleep_for(chrono::milliseconds(1000));
        processor->Unblock();
    });
    pipelineManager->UpdatePipelines(diffUpdate);
    result.get();
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    AddDataToProcessor(configName, "test-data-8");
    AddDataToProcessor(configName, "test-data-9");
    AddDataToProcessor(configName, "test-data-10");

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    VerifyData("test_logstore_1", 1, 4);
    VerifyData("test_logstore_2", 5, 10);
}

void PipelineUpdateUnittest::TestPipelineParamUpdateCase2() const {
    // Go -> Go -> Go
    const std::string configName = "test2";
    // load old pipeline
    Json::Value pipelineConfigJson = GeneratePipelineConfigJson(goInputConfig, goProcessorConfig, goFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate
        = GeneratePipelineConfigJson(goInputConfig2, goProcessorConfig2, goFlusherConfig2);
    CollectionConfigDiff diffUpdate;
    CollectionConfig pipelineConfigObjUpdate
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate));
    pipelineConfigObjUpdate.Parse();
    diffUpdate.mModified.push_back(std::move(pipelineConfigObjUpdate));
    pipelineManager->UpdatePipelines(diffUpdate);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());
}

void PipelineUpdateUnittest::TestPipelineParamUpdateCase3() const {
    // Go -> Go -> C++
    const std::string configName = "test3";
    // load old pipeline
    Json::Value pipelineConfigJson = GeneratePipelineConfigJson(goInputConfig, goProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    // Add data without trigger
    auto pipeline = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto flusher = const_cast<Flusher*>(pipeline->GetFlushers()[0].get()->GetPlugin());
    AddDataToSenderQueue(configName, "test-data-1", flusher);
    AddDataToSenderQueue(configName, "test-data-2", flusher);
    AddDataToSenderQueue(configName, "test-data-3", flusher);

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate
        = GeneratePipelineConfigJson(goInputConfig2, goProcessorConfig2, nativeFlusherConfig2);
    CollectionConfigDiff diffUpdate;
    CollectionConfig pipelineConfigObjUpdate
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate));
    pipelineConfigObjUpdate.Parse();
    diffUpdate.mModified.push_back(std::move(pipelineConfigObjUpdate));
    pipelineManager->UpdatePipelines(diffUpdate);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    flusher = const_cast<Flusher*>(CollectionPipelineManager::GetInstance()
                                       ->GetAllPipelines()
                                       .at(configName)
                                       .get()
                                       ->GetFlushers()[0]
                                       .get()
                                       ->GetPlugin());
    AddDataToSenderQueue(configName, "test-data-4", flusher);
    AddDataToSenderQueue(configName, "test-data-5", flusher);
    AddDataToSenderQueue(configName, "test-data-6", flusher);

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    VerifyData("test_logstore_1", 1, 3);
    VerifyData("test_logstore_2", 4, 6);
}

void PipelineUpdateUnittest::TestPipelineParamUpdateCase4() const {
    // C++ -> Go -> C++
    const std::string configName = "test4";
    // load old pipeline
    Json::Value pipelineConfigJson
        = GeneratePipelineConfigJson(nativeInputConfig, goProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    // Add data without trigger
    auto pipeline = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto flusher = const_cast<Flusher*>(pipeline->GetFlushers()[0].get()->GetPlugin());
    LogtailPluginMock::GetInstance()->BlockProcess();
    AddDataToSenderQueue(configName, "test-data-1", flusher);
    AddDataToSenderQueue(configName, "test-data-2", flusher);
    AddDataToSenderQueue(configName, "test-data-3", flusher);

    AddDataToProcessor(configName, "test-data-4");

    AddDataToProcessQueue(configName, "test-data-5");
    AddDataToProcessQueue(configName, "test-data-6");
    AddDataToProcessQueue(configName, "test-data-7");

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate
        = GeneratePipelineConfigJson(nativeInputConfig2, goProcessorConfig2, nativeFlusherConfig2);
    CollectionConfigDiff diffUpdate;
    CollectionConfig pipelineConfigObjUpdate
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate));
    pipelineConfigObjUpdate.Parse();
    diffUpdate.mModified.push_back(std::move(pipelineConfigObjUpdate));
    auto result = async(launch::async, [&]() {
        this_thread::sleep_for(chrono::milliseconds(1000));
        LogtailPluginMock::GetInstance()->UnblockProcess();
    });
    pipelineManager->UpdatePipelines(diffUpdate);
    result.get();
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    AddDataToProcessor(configName, "test-data-8");
    AddDataToProcessor(configName, "test-data-9");
    AddDataToProcessor(configName, "test-data-10");

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    VerifyData("test_logstore_1", 1, 4);
    VerifyData("test_logstore_2", 5, 10);
}

void PipelineUpdateUnittest::TestPipelineTypeUpdateCase1() const {
    // C++ -> C++ -> C++
    const std::string configName = "test1";
    // load old pipeline
    Json::Value pipelineConfigJson
        = GeneratePipelineConfigJson(nativeInputConfig, nativeProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    // Add data without trigger
    auto pipeline = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto flusher = const_cast<Flusher*>(pipeline->GetFlushers()[0].get()->GetPlugin());
    auto processor
        = static_cast<ProcessorMock*>(const_cast<Processor*>(pipeline->mProcessorLine[0].get()->mPlugin.get()));
    AddDataToSenderQueue(configName, "test-data-1", flusher);
    AddDataToSenderQueue(configName, "test-data-2", flusher);
    AddDataToSenderQueue(configName, "test-data-3", flusher);

    AddDataToProcessor(configName, "test-data-4");

    AddDataToProcessQueue(configName, "test-data-5");
    AddDataToProcessQueue(configName, "test-data-6");
    AddDataToProcessQueue(configName, "test-data-7");

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate
        = GeneratePipelineConfigJson(nativeInputConfig3, nativeProcessorConfig3, nativeFlusherConfig3);
    CollectionConfigDiff diffUpdate;
    CollectionConfig pipelineConfigObjUpdate
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate));
    pipelineConfigObjUpdate.Parse();
    diffUpdate.mModified.push_back(std::move(pipelineConfigObjUpdate));
    auto result = async(launch::async, [&]() {
        this_thread::sleep_for(chrono::milliseconds(1000));
        processor->Unblock();
    });
    pipelineManager->UpdatePipelines(diffUpdate);
    result.get();
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    AddDataToProcessor(configName, "test-data-8");
    AddDataToProcessor(configName, "test-data-9");
    AddDataToProcessor(configName, "test-data-10");

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    VerifyData("test_logstore_1", 1, 4);
    VerifyData("test_logstore_3", 5, 10);
}

void PipelineUpdateUnittest::TestPipelineTypeUpdateCase2() const {
    // Go -> Go -> Go
    const std::string configName = "test2";
    // load old pipeline
    Json::Value pipelineConfigJson = GeneratePipelineConfigJson(goInputConfig, goProcessorConfig, goFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate
        = GeneratePipelineConfigJson(goInputConfig3, goProcessorConfig3, goFlusherConfig3);
    CollectionConfigDiff diffUpdate;
    CollectionConfig pipelineConfigObjUpdate
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate));
    pipelineConfigObjUpdate.Parse();
    diffUpdate.mModified.push_back(std::move(pipelineConfigObjUpdate));
    pipelineManager->UpdatePipelines(diffUpdate);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());
}

void PipelineUpdateUnittest::TestPipelineTypeUpdateCase3() const {
    // Go -> Go -> C++
    const std::string configName = "test3";
    // load old pipeline
    Json::Value pipelineConfigJson = GeneratePipelineConfigJson(goInputConfig, goProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    // Add data without trigger
    auto pipeline = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto flusher = const_cast<Flusher*>(pipeline->GetFlushers()[0].get()->GetPlugin());
    AddDataToSenderQueue(configName, "test-data-1", flusher);
    AddDataToSenderQueue(configName, "test-data-2", flusher);
    AddDataToSenderQueue(configName, "test-data-3", flusher);

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate
        = GeneratePipelineConfigJson(goInputConfig3, goProcessorConfig3, nativeFlusherConfig3);
    CollectionConfigDiff diffUpdate;
    CollectionConfig pipelineConfigObjUpdate
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate));
    pipelineConfigObjUpdate.Parse();
    diffUpdate.mModified.push_back(std::move(pipelineConfigObjUpdate));
    pipelineManager->UpdatePipelines(diffUpdate);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    flusher = const_cast<Flusher*>(CollectionPipelineManager::GetInstance()
                                       ->GetAllPipelines()
                                       .at(configName)
                                       .get()
                                       ->GetFlushers()[0]
                                       .get()
                                       ->GetPlugin());
    AddDataToSenderQueue(configName, "test-data-4", flusher);
    AddDataToSenderQueue(configName, "test-data-5", flusher);
    AddDataToSenderQueue(configName, "test-data-6", flusher);

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    VerifyData("test_logstore_1", 1, 3);
    VerifyData("test_logstore_3", 4, 6);
}

void PipelineUpdateUnittest::TestPipelineTypeUpdateCase4() const {
    // C++ -> Go -> C++
    const std::string configName = "test4";
    // load old pipeline
    Json::Value pipelineConfigJson
        = GeneratePipelineConfigJson(nativeInputConfig, goProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    // Add data without trigger
    auto pipeline = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto flusher = const_cast<Flusher*>(pipeline->GetFlushers()[0].get()->GetPlugin());
    LogtailPluginMock::GetInstance()->BlockProcess();
    AddDataToSenderQueue(configName, "test-data-1", flusher);
    AddDataToSenderQueue(configName, "test-data-2", flusher);
    AddDataToSenderQueue(configName, "test-data-3", flusher);

    AddDataToProcessor(configName, "test-data-4");

    AddDataToProcessQueue(configName, "test-data-5");
    AddDataToProcessQueue(configName, "test-data-6");
    AddDataToProcessQueue(configName, "test-data-7");

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate
        = GeneratePipelineConfigJson(nativeInputConfig3, goProcessorConfig3, nativeFlusherConfig3);
    CollectionConfigDiff diffUpdate;
    CollectionConfig pipelineConfigObjUpdate
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate));
    pipelineConfigObjUpdate.Parse();
    diffUpdate.mModified.push_back(std::move(pipelineConfigObjUpdate));
    auto result = async(launch::async, [&]() {
        this_thread::sleep_for(chrono::milliseconds(1000));
        LogtailPluginMock::GetInstance()->UnblockProcess();
    });
    pipelineManager->UpdatePipelines(diffUpdate);
    result.get();
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    AddDataToProcessor(configName, "test-data-8");
    AddDataToProcessor(configName, "test-data-9");
    AddDataToProcessor(configName, "test-data-10");

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    VerifyData("test_logstore_1", 1, 4);
    VerifyData("test_logstore_3", 5, 10);
}

void PipelineUpdateUnittest::TestPipelineTopoUpdateCase1() const {
    // C++ -> C++ -> C++ => Go -> Go -> Go
    const std::string configName = "test1";
    // load old pipeline
    Json::Value pipelineConfigJson
        = GeneratePipelineConfigJson(nativeInputConfig, nativeProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    // Add data without trigger
    auto pipeline = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto flusher = const_cast<Flusher*>(pipeline->GetFlushers()[0].get()->GetPlugin());
    auto processor
        = static_cast<ProcessorMock*>(const_cast<Processor*>(pipeline->mProcessorLine[0].get()->mPlugin.get()));
    AddDataToSenderQueue(configName, "test-data-1", flusher);
    AddDataToSenderQueue(configName, "test-data-2", flusher);
    AddDataToSenderQueue(configName, "test-data-3", flusher);

    AddDataToProcessor(configName, "test-data-4");

    AddDataToProcessQueue(configName, "test-data-5");
    AddDataToProcessQueue(configName, "test-data-6");
    AddDataToProcessQueue(configName, "test-data-7");

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate
        = GeneratePipelineConfigJson(goInputConfig, goProcessorConfig, goFlusherConfig);
    CollectionConfigDiff diffUpdate;
    CollectionConfig pipelineConfigObjUpdate
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate));
    pipelineConfigObjUpdate.Parse();
    diffUpdate.mModified.push_back(std::move(pipelineConfigObjUpdate));
    auto result = async(launch::async, [&]() {
        this_thread::sleep_for(chrono::milliseconds(1000));
        processor->Unblock();
    });
    pipelineManager->UpdatePipelines(diffUpdate);
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());
    result.get();
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    VerifyData("test_logstore_1", 1, 4);
}

void PipelineUpdateUnittest::TestPipelineTopoUpdateCase2() const {
    // C++ -> C++ -> C++ => Go -> Go -> C++
    const std::string configName = "test2";
    // load old pipeline
    Json::Value pipelineConfigJson
        = GeneratePipelineConfigJson(nativeInputConfig, nativeProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    // Add data without trigger
    auto pipeline = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto flusher = const_cast<Flusher*>(pipeline->GetFlushers()[0].get()->GetPlugin());
    auto processor
        = static_cast<ProcessorMock*>(const_cast<Processor*>(pipeline->mProcessorLine[0].get()->mPlugin.get()));
    AddDataToSenderQueue(configName, "test-data-1", flusher);
    AddDataToSenderQueue(configName, "test-data-2", flusher);
    AddDataToSenderQueue(configName, "test-data-3", flusher);

    AddDataToProcessor(configName, "test-data-4");

    AddDataToProcessQueue(configName, "test-data-5");
    AddDataToProcessQueue(configName, "test-data-6");
    AddDataToProcessQueue(configName, "test-data-7");

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate
        = GeneratePipelineConfigJson(goInputConfig, goProcessorConfig, nativeFlusherConfig3);
    CollectionConfigDiff diffUpdate;
    CollectionConfig pipelineConfigObjUpdate
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate));
    pipelineConfigObjUpdate.Parse();
    diffUpdate.mModified.push_back(std::move(pipelineConfigObjUpdate));
    auto result = async(launch::async, [&]() {
        this_thread::sleep_for(chrono::milliseconds(1000));
        processor->Unblock();
    });
    pipelineManager->UpdatePipelines(diffUpdate);
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());
    result.get();
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    flusher = const_cast<Flusher*>(CollectionPipelineManager::GetInstance()
                                       ->GetAllPipelines()
                                       .at(configName)
                                       .get()
                                       ->GetFlushers()[0]
                                       .get()
                                       ->GetPlugin());
    AddDataToSenderQueue(configName, "test-data-8", flusher);
    AddDataToSenderQueue(configName, "test-data-9", flusher);
    AddDataToSenderQueue(configName, "test-data-10", flusher);

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    VerifyData("test_logstore_1", 1, 4);
    VerifyData("test_logstore_3", 8, 10);
}

void PipelineUpdateUnittest::TestPipelineTopoUpdateCase3() const {
    // C++ -> C++ -> C++ => C++ -> Go -> C++
    const std::string configName = "test3";
    // load old pipeline
    Json::Value pipelineConfigJson
        = GeneratePipelineConfigJson(nativeInputConfig, nativeProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    // Add data without trigger
    auto pipeline = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto flusher = const_cast<Flusher*>(pipeline->GetFlushers()[0].get()->GetPlugin());
    auto processor
        = static_cast<ProcessorMock*>(const_cast<Processor*>(pipeline->mProcessorLine[0].get()->mPlugin.get()));
    AddDataToSenderQueue(configName, "test-data-1", flusher);
    AddDataToSenderQueue(configName, "test-data-2", flusher);
    AddDataToSenderQueue(configName, "test-data-3", flusher);

    AddDataToProcessor(configName, "test-data-4");

    AddDataToProcessQueue(configName, "test-data-5");
    AddDataToProcessQueue(configName, "test-data-6");
    AddDataToProcessQueue(configName, "test-data-7");

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate
        = GeneratePipelineConfigJson(nativeInputConfig3, goProcessorConfig, nativeFlusherConfig3);
    CollectionConfigDiff diffUpdate;
    CollectionConfig pipelineConfigObjUpdate
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate));
    pipelineConfigObjUpdate.Parse();
    diffUpdate.mModified.push_back(std::move(pipelineConfigObjUpdate));
    auto result = async(launch::async, [&]() {
        this_thread::sleep_for(chrono::milliseconds(1000));
        processor->Unblock();
    });
    pipelineManager->UpdatePipelines(diffUpdate);
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());
    result.get();
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    AddDataToProcessor(configName, "test-data-8");
    AddDataToProcessor(configName, "test-data-9");
    AddDataToProcessor(configName, "test-data-10");

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    VerifyData("test_logstore_1", 1, 4);
    VerifyData("test_logstore_3", 5, 10);
}

void PipelineUpdateUnittest::TestPipelineTopoUpdateCase4() const {
    // Go -> Go -> Go => C++ -> C++ -> C++
    const std::string configName = "test4";
    // load old pipeline
    Json::Value pipelineConfigJson = GeneratePipelineConfigJson(goInputConfig, goProcessorConfig, goFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate
        = GeneratePipelineConfigJson(nativeInputConfig3, nativeProcessorConfig3, nativeFlusherConfig3);
    CollectionConfigDiff diffUpdate;
    CollectionConfig pipelineConfigObjUpdate
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate));
    pipelineConfigObjUpdate.Parse();
    diffUpdate.mModified.push_back(std::move(pipelineConfigObjUpdate));
    pipelineManager->UpdatePipelines(diffUpdate);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(false, LogtailPluginMock::GetInstance()->IsStarted());

    AddDataToProcessor(configName, "test-data-1");
    AddDataToProcessor(configName, "test-data-2");
    AddDataToProcessor(configName, "test-data-3");

    UnBlockProcessor(configName);
    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    VerifyData("test_logstore_3", 1, 3);
}

void PipelineUpdateUnittest::TestPipelineTopoUpdateCase5() const {
    // Go -> Go -> Go => Go -> Go -> C++
    const std::string configName = "test5";
    // load old pipeline
    Json::Value pipelineConfigJson = GeneratePipelineConfigJson(goInputConfig, goProcessorConfig, goFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate
        = GeneratePipelineConfigJson(goInputConfig3, goProcessorConfig3, nativeFlusherConfig3);
    CollectionConfigDiff diffUpdate;
    CollectionConfig pipelineConfigObjUpdate
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate));
    pipelineConfigObjUpdate.Parse();
    diffUpdate.mModified.push_back(std::move(pipelineConfigObjUpdate));
    pipelineManager->UpdatePipelines(diffUpdate);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    auto flusher = const_cast<Flusher*>(CollectionPipelineManager::GetInstance()
                                            ->GetAllPipelines()
                                            .at(configName)
                                            .get()
                                            ->GetFlushers()[0]
                                            .get()
                                            ->GetPlugin());
    AddDataToSenderQueue(configName, "test-data-1", flusher);
    AddDataToSenderQueue(configName, "test-data-2", flusher);
    AddDataToSenderQueue(configName, "test-data-3", flusher);

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    VerifyData("test_logstore_3", 1, 3);
}

void PipelineUpdateUnittest::TestPipelineTopoUpdateCase6() const {
    // Go -> Go -> Go => C++ -> Go -> C++
    const std::string configName = "test6";
    // load old pipeline
    Json::Value pipelineConfigJson = GeneratePipelineConfigJson(goInputConfig, goProcessorConfig, goFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate
        = GeneratePipelineConfigJson(nativeInputConfig3, goProcessorConfig3, nativeFlusherConfig3);
    CollectionConfigDiff diffUpdate;
    CollectionConfig pipelineConfigObjUpdate
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate));
    pipelineConfigObjUpdate.Parse();
    diffUpdate.mModified.push_back(std::move(pipelineConfigObjUpdate));
    pipelineManager->UpdatePipelines(diffUpdate);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    AddDataToProcessor(configName, "test-data-1");
    AddDataToProcessor(configName, "test-data-2");
    AddDataToProcessor(configName, "test-data-3");

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    VerifyData("test_logstore_3", 1, 3);
}

void PipelineUpdateUnittest::TestPipelineTopoUpdateCase7() const {
    // Go -> Go -> C++ => C++ -> C++ -> C++
    const std::string configName = "test7";
    // load old pipeline
    Json::Value pipelineConfigJson = GeneratePipelineConfigJson(goInputConfig, goProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    // Add data without trigger
    auto pipeline = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto flusher = const_cast<Flusher*>(pipeline->GetFlushers()[0].get()->GetPlugin());
    AddDataToSenderQueue(configName, "test-data-1", flusher);
    AddDataToSenderQueue(configName, "test-data-2", flusher);
    AddDataToSenderQueue(configName, "test-data-3", flusher);

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate
        = GeneratePipelineConfigJson(nativeInputConfig3, nativeProcessorConfig3, nativeFlusherConfig3);
    CollectionConfigDiff diffUpdate;
    CollectionConfig pipelineConfigObjUpdate
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate));
    pipelineConfigObjUpdate.Parse();
    diffUpdate.mModified.push_back(std::move(pipelineConfigObjUpdate));
    pipelineManager->UpdatePipelines(diffUpdate);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(false, LogtailPluginMock::GetInstance()->IsStarted());

    AddDataToProcessor(configName, "test-data-4");
    AddDataToProcessor(configName, "test-data-5");
    AddDataToProcessor(configName, "test-data-6");

    UnBlockProcessor(configName);
    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    VerifyData("test_logstore_1", 1, 3);
    VerifyData("test_logstore_3", 4, 6);
}

void PipelineUpdateUnittest::TestPipelineTopoUpdateCase8() const {
    // Go -> Go -> C++ => Go -> Go -> Go
    const std::string configName = "test8";
    // load old pipeline
    Json::Value pipelineConfigJson = GeneratePipelineConfigJson(goInputConfig, goProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    // Add data without trigger
    auto pipeline = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto flusher = const_cast<Flusher*>(pipeline->GetFlushers()[0].get()->GetPlugin());
    AddDataToSenderQueue(configName, "test-data-1", flusher);
    AddDataToSenderQueue(configName, "test-data-2", flusher);
    AddDataToSenderQueue(configName, "test-data-3", flusher);

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate
        = GeneratePipelineConfigJson(goInputConfig3, goProcessorConfig3, goFlusherConfig3);
    CollectionConfigDiff diffUpdate;
    CollectionConfig pipelineConfigObjUpdate
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate));
    pipelineConfigObjUpdate.Parse();
    diffUpdate.mModified.push_back(std::move(pipelineConfigObjUpdate));
    pipelineManager->UpdatePipelines(diffUpdate);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    VerifyData("test_logstore_1", 1, 3);
}

void PipelineUpdateUnittest::TestPipelineTopoUpdateCase9() const {
    // Go -> Go -> C++ => C++ -> Go -> C++
    const std::string configName = "test9";
    // load old pipeline
    Json::Value pipelineConfigJson = GeneratePipelineConfigJson(goInputConfig, goProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    // Add data without trigger
    auto pipeline = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto flusher = const_cast<Flusher*>(pipeline->GetFlushers()[0].get()->GetPlugin());
    AddDataToSenderQueue(configName, "test-data-1", flusher);
    AddDataToSenderQueue(configName, "test-data-2", flusher);
    AddDataToSenderQueue(configName, "test-data-3", flusher);

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate
        = GeneratePipelineConfigJson(nativeInputConfig3, goProcessorConfig3, nativeFlusherConfig3);
    CollectionConfigDiff diffUpdate;
    CollectionConfig pipelineConfigObjUpdate
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate));
    pipelineConfigObjUpdate.Parse();
    diffUpdate.mModified.push_back(std::move(pipelineConfigObjUpdate));
    pipelineManager->UpdatePipelines(diffUpdate);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    AddDataToProcessor(configName, "test-data-4");
    AddDataToProcessor(configName, "test-data-5");
    AddDataToProcessor(configName, "test-data-6");

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    VerifyData("test_logstore_1", 1, 3);
    VerifyData("test_logstore_3", 4, 6);
}

void PipelineUpdateUnittest::TestPipelineTopoUpdateCase10() const {
    // C++ -> Go -> C++ => C++ -> C++ -> C++
    const std::string configName = "test10";
    // load old pipeline
    Json::Value pipelineConfigJson
        = GeneratePipelineConfigJson(nativeInputConfig, goProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    // Add data without trigger
    auto pipeline = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto flusher = const_cast<Flusher*>(pipeline->GetFlushers()[0].get()->GetPlugin());
    LogtailPluginMock::GetInstance()->BlockProcess();
    AddDataToSenderQueue(configName, "test-data-1", flusher);
    AddDataToSenderQueue(configName, "test-data-2", flusher);
    AddDataToSenderQueue(configName, "test-data-3", flusher);

    AddDataToProcessor(configName, "test-data-4");

    AddDataToProcessQueue(configName, "test-data-5");
    AddDataToProcessQueue(configName, "test-data-6");
    AddDataToProcessQueue(configName, "test-data-7");

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate
        = GeneratePipelineConfigJson(nativeInputConfig3, nativeProcessorConfig3, nativeFlusherConfig3);
    CollectionConfigDiff diffUpdate;
    CollectionConfig pipelineConfigObjUpdate
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate));
    pipelineConfigObjUpdate.Parse();
    diffUpdate.mModified.push_back(std::move(pipelineConfigObjUpdate));
    auto result = async(launch::async, [&]() {
        this_thread::sleep_for(chrono::milliseconds(1000));
        LogtailPluginMock::GetInstance()->UnblockProcess();
    });
    pipelineManager->UpdatePipelines(diffUpdate);
    BlockProcessor(configName);
    result.get();
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(false, LogtailPluginMock::GetInstance()->IsStarted());

    AddDataToProcessor(configName, "test-data-8");
    AddDataToProcessor(configName, "test-data-9");
    AddDataToProcessor(configName, "test-data-10");

    UnBlockProcessor(configName);
    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    VerifyData("test_logstore_1", 1, 4);
    VerifyData("test_logstore_3", 5, 10);
}

void PipelineUpdateUnittest::TestPipelineTopoUpdateCase11() const {
    // C++ -> Go -> C++ => Go -> Go -> Go
    const std::string configName = "test11";
    // load old pipeline
    Json::Value pipelineConfigJson
        = GeneratePipelineConfigJson(nativeInputConfig, goProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    // Add data without trigger
    auto pipeline = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto flusher = const_cast<Flusher*>(pipeline->GetFlushers()[0].get()->GetPlugin());
    LogtailPluginMock::GetInstance()->BlockProcess();
    AddDataToSenderQueue(configName, "test-data-1", flusher);
    AddDataToSenderQueue(configName, "test-data-2", flusher);
    AddDataToSenderQueue(configName, "test-data-3", flusher);

    AddDataToProcessor(configName, "test-data-4");

    AddDataToProcessQueue(configName, "test-data-5");
    AddDataToProcessQueue(configName, "test-data-6");
    AddDataToProcessQueue(configName, "test-data-7");

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate
        = GeneratePipelineConfigJson(goInputConfig3, goProcessorConfig3, goFlusherConfig3);
    CollectionConfigDiff diffUpdate;
    CollectionConfig pipelineConfigObjUpdate
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate));
    pipelineConfigObjUpdate.Parse();
    diffUpdate.mModified.push_back(std::move(pipelineConfigObjUpdate));
    auto result = async(launch::async, [&]() {
        this_thread::sleep_for(chrono::milliseconds(1000));
        LogtailPluginMock::GetInstance()->UnblockProcess();
    });
    pipelineManager->UpdatePipelines(diffUpdate);
    result.get();
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    VerifyData("test_logstore_1", 1, 4);
}

void PipelineUpdateUnittest::TestPipelineTopoUpdateCase12() const {
    // C++ -> Go -> C++ => Go -> Go -> C++
    const std::string configName = "test12";
    // load old pipeline
    Json::Value pipelineConfigJson
        = GeneratePipelineConfigJson(nativeInputConfig, goProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    // Add data without trigger
    auto pipeline = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto flusher = const_cast<Flusher*>(pipeline->GetFlushers()[0].get()->GetPlugin());
    LogtailPluginMock::GetInstance()->BlockProcess();
    AddDataToSenderQueue(configName, "test-data-1", flusher);
    AddDataToSenderQueue(configName, "test-data-2", flusher);
    AddDataToSenderQueue(configName, "test-data-3", flusher);

    AddDataToProcessor(configName, "test-data-4");

    AddDataToProcessQueue(configName, "test-data-5");
    AddDataToProcessQueue(configName, "test-data-6");
    AddDataToProcessQueue(configName, "test-data-7");

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate
        = GeneratePipelineConfigJson(goInputConfig3, goProcessorConfig3, nativeFlusherConfig3);
    CollectionConfigDiff diffUpdate;
    CollectionConfig pipelineConfigObjUpdate
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate));
    pipelineConfigObjUpdate.Parse();
    diffUpdate.mModified.push_back(std::move(pipelineConfigObjUpdate));
    auto result = async(launch::async, [&]() {
        this_thread::sleep_for(chrono::milliseconds(1000));
        LogtailPluginMock::GetInstance()->UnblockProcess();
    });
    pipelineManager->UpdatePipelines(diffUpdate);
    result.get();
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());


    flusher = const_cast<Flusher*>(CollectionPipelineManager::GetInstance()
                                       ->GetAllPipelines()
                                       .at(configName)
                                       .get()
                                       ->GetFlushers()[0]
                                       .get()
                                       ->GetPlugin());
    AddDataToSenderQueue(configName, "test-data-8", flusher);
    AddDataToSenderQueue(configName, "test-data-9", flusher);
    AddDataToSenderQueue(configName, "test-data-10", flusher);

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    VerifyData("test_logstore_1", 1, 4);
    VerifyData("test_logstore_3", 8, 10);
}

void PipelineUpdateUnittest::TestPipelineInputBlock() const {
    // C++ -> C++ -> C++
    const std::string configName = "test1";
    // load old pipeline
    Json::Value pipelineConfigJson
        = GeneratePipelineConfigJson(nativeInputConfig, nativeProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    // Add data without trigger
    auto pipeline = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto input = static_cast<InputMock*>(const_cast<Input*>(pipeline->GetInputs()[0].get()->GetPlugin()));
    auto flusher = const_cast<Flusher*>(pipeline->GetFlushers()[0].get()->GetPlugin());
    auto processor
        = static_cast<ProcessorMock*>(const_cast<Processor*>(pipeline->mProcessorLine[0].get()->mPlugin.get()));
    input->Block();
    AddDataToSenderQueue(configName, "test-data-1", flusher);
    AddDataToSenderQueue(configName, "test-data-2", flusher);
    AddDataToSenderQueue(configName, "test-data-3", flusher);

    AddDataToProcessor(configName, "test-data-4");

    AddDataToProcessQueue(configName, "test-data-5");
    AddDataToProcessQueue(configName, "test-data-6");
    AddDataToProcessQueue(configName, "test-data-7");

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate
        = GeneratePipelineConfigJson(nativeInputConfig2, nativeProcessorConfig2, nativeFlusherConfig2);
    CollectionConfigDiff diffUpdate;
    CollectionConfig pipelineConfigObjUpdate
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate));
    pipelineConfigObjUpdate.Parse();
    diffUpdate.mModified.push_back(std::move(pipelineConfigObjUpdate));
    auto result1 = async(launch::async, [&]() {
        pipelineManager->UpdatePipelines(diffUpdate);
        BlockProcessor(configName);
    });
    this_thread::sleep_for(chrono::milliseconds(1000));
    APSARA_TEST_NOT_EQUAL_FATAL(future_status::ready, result1.wait_for(chrono::milliseconds(0)));
    input->Unblock();
    auto result2 = async(launch::async, [&]() {
        this_thread::sleep_for(chrono::milliseconds(1000));
        processor->Unblock();
    });
    result1.get();
    result2.get();
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    AddDataToProcessor(configName, "test-data-8");
    AddDataToProcessor(configName, "test-data-9");
    AddDataToProcessor(configName, "test-data-10");

    UnBlockProcessor(configName);
    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    VerifyData("test_logstore_1", 1, 4);
    VerifyData("test_logstore_2", 5, 10);
}

void PipelineUpdateUnittest::TestPipelineGoInputBlockCase1() const {
    // Go -> Go -> C++ => Go -> Go -> C++
    const std::string configName = "test1";
    // load old pipeline
    Json::Value pipelineConfigJson = GeneratePipelineConfigJson(goInputConfig, goProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    // Add data without trigger
    auto pipeline = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto flusher = const_cast<Flusher*>(pipeline->GetFlushers()[0].get()->GetPlugin());
    LogtailPluginMock::GetInstance()->BlockStop();
    AddDataToSenderQueue(configName, "test-data-1", flusher);
    AddDataToSenderQueue(configName, "test-data-2", flusher);
    AddDataToSenderQueue(configName, "test-data-3", flusher);

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate
        = GeneratePipelineConfigJson(goInputConfig3, goProcessorConfig3, nativeFlusherConfig3);
    CollectionConfigDiff diffUpdate;
    CollectionConfig pipelineConfigObjUpdate
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate));
    pipelineConfigObjUpdate.Parse();
    diffUpdate.mModified.push_back(std::move(pipelineConfigObjUpdate));
    auto result = async(launch::async, [&]() { pipelineManager->UpdatePipelines(diffUpdate); });
    this_thread::sleep_for(chrono::milliseconds(1000));
    APSARA_TEST_NOT_EQUAL_FATAL(future_status::ready, result.wait_for(chrono::milliseconds(0)));
    LogtailPluginMock::GetInstance()->UnblockStop();
    result.get();

    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    flusher = const_cast<Flusher*>(CollectionPipelineManager::GetInstance()
                                       ->GetAllPipelines()
                                       .at(configName)
                                       .get()
                                       ->GetFlushers()[0]
                                       .get()
                                       ->GetPlugin());
    AddDataToSenderQueue(configName, "test-data-4", flusher);
    AddDataToSenderQueue(configName, "test-data-5", flusher);
    AddDataToSenderQueue(configName, "test-data-6", flusher);

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    VerifyData("test_logstore_1", 1, 3);
    VerifyData("test_logstore_3", 4, 6);
}

void PipelineUpdateUnittest::TestPipelineGoInputBlockCase2() const {
    // Go -> Go -> C++ => C++ -> Go -> C++
    const std::string configName = "test1";
    // load old pipeline
    Json::Value pipelineConfigJson = GeneratePipelineConfigJson(goInputConfig, goProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(true, LogtailPluginMock::GetInstance()->IsStarted());

    // Add data without trigger
    auto pipeline = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto flusher = const_cast<Flusher*>(pipeline->GetFlushers()[0].get()->GetPlugin());
    LogtailPluginMock::GetInstance()->BlockStop();
    AddDataToSenderQueue(configName, "test-data-1", flusher);
    AddDataToSenderQueue(configName, "test-data-2", flusher);
    AddDataToSenderQueue(configName, "test-data-3", flusher);

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate
        = GeneratePipelineConfigJson(nativeInputConfig3, nativeFlusherConfig3, nativeFlusherConfig3);
    CollectionConfigDiff diffUpdate;
    CollectionConfig pipelineConfigObjUpdate
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate));
    pipelineConfigObjUpdate.Parse();
    diffUpdate.mModified.push_back(std::move(pipelineConfigObjUpdate));
    auto result = async(launch::async, [&]() { pipelineManager->UpdatePipelines(diffUpdate); });
    this_thread::sleep_for(chrono::milliseconds(1000));
    APSARA_TEST_NOT_EQUAL_FATAL(future_status::ready, result.wait_for(chrono::milliseconds(0)));
    LogtailPluginMock::GetInstance()->UnblockStop();
    result.get();

    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(false, LogtailPluginMock::GetInstance()->IsStarted());

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    VerifyData("test_logstore_1", 1, 3);
}

void PipelineUpdateUnittest::TestPipelineIsolationCase1() const {
    CollectionConfigDiff diff;
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    // C++ -> C++ -> C++
    Json::Value pipelineConfigJson1
        = GeneratePipelineConfigJson(nativeInputConfig, nativeProcessorConfig, nativeFlusherConfig);
    CollectionConfig pipelineConfigObj1 = CollectionConfig("test1", make_unique<Json::Value>(pipelineConfigJson1));
    pipelineConfigObj1.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj1));
    // Go -> Go -> Go
    Json::Value pipelineConfigJson2 = GeneratePipelineConfigJson(goInputConfig, goProcessorConfig, goFlusherConfig);
    CollectionConfig pipelineConfigObj2 = CollectionConfig("test2", make_unique<Json::Value>(pipelineConfigJson2));
    pipelineConfigObj2.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj2));
    // Go -> Go -> C++
    Json::Value pipelineConfigJson3 = GeneratePipelineConfigJson(goInputConfig, goProcessorConfig, nativeFlusherConfig);
    CollectionConfig pipelineConfigObj3 = CollectionConfig("test3", make_unique<Json::Value>(pipelineConfigJson3));
    pipelineConfigObj3.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj3));
    // C++ -> Go -> C++
    Json::Value pipelineConfigJson4
        = GeneratePipelineConfigJson(nativeInputConfig, goProcessorConfig, nativeFlusherConfig);
    CollectionConfig pipelineConfigObj4 = CollectionConfig("test4", make_unique<Json::Value>(pipelineConfigJson4));
    pipelineConfigObj4.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj4));

    pipelineManager->UpdatePipelines(diff);
    APSARA_TEST_EQUAL_FATAL(4U, pipelineManager->GetAllPipelines().size());

    CollectionConfigDiff diffUpdate;
    diffUpdate.mRemoved.push_back("test1");
    auto pipeline = pipelineManager->GetAllPipelines().at("test1");
    auto input = static_cast<InputMock*>(const_cast<Input*>(pipeline->GetInputs()[0].get()->GetPlugin()));
    input->Block();

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    auto result = async(launch::async, [&]() { pipelineManager->UpdatePipelines(diffUpdate); });
    { // add data to Go -> Go -> C++
        std::string configName = "test3";
        auto pipeline = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
        auto flusher = const_cast<Flusher*>(pipeline->GetFlushers()[0].get()->GetPlugin());
        AddDataToSenderQueue(configName, "test-data-1", flusher);
        AddDataToSenderQueue(configName, "test-data-2", flusher);
        AddDataToSenderQueue(configName, "test-data-3", flusher);
        VerifyData("test_logstore_1", 1, 3);
    }
    HttpSinkMock::GetInstance()->ClearRequests();
    { // add data to C++ -> Go -> C++
        std::string configName = "test4";
        AddDataToProcessQueue(configName, "test-data-1");
        AddDataToProcessQueue(configName, "test-data-2");
        AddDataToProcessQueue(configName, "test-data-3");
        VerifyData("test_logstore_1", 1, 3);
    }

    input->Unblock();
    result.get();
    APSARA_TEST_EQUAL_FATAL(3U, pipelineManager->GetAllPipelines().size());
}

void PipelineUpdateUnittest::TestPipelineIsolationCase2() const {
    CollectionConfigDiff diff;
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    // C++ -> C++ -> C++
    Json::Value pipelineConfigJson1
        = GeneratePipelineConfigJson(nativeInputConfig, nativeProcessorConfig, nativeFlusherConfig);
    CollectionConfig pipelineConfigObj1 = CollectionConfig("test1", make_unique<Json::Value>(pipelineConfigJson1));
    pipelineConfigObj1.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj1));
    // Go -> Go -> Go
    Json::Value pipelineConfigJson2 = GeneratePipelineConfigJson(goInputConfig, goProcessorConfig, goFlusherConfig);
    CollectionConfig pipelineConfigObj2 = CollectionConfig("test2", make_unique<Json::Value>(pipelineConfigJson2));
    pipelineConfigObj2.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj2));
    // Go -> Go -> C++
    Json::Value pipelineConfigJson3 = GeneratePipelineConfigJson(goInputConfig, goProcessorConfig, nativeFlusherConfig);
    CollectionConfig pipelineConfigObj3 = CollectionConfig("test3", make_unique<Json::Value>(pipelineConfigJson3));
    pipelineConfigObj3.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj3));
    // C++ -> Go -> C++
    Json::Value pipelineConfigJson4
        = GeneratePipelineConfigJson(nativeInputConfig, goProcessorConfig, nativeFlusherConfig);
    CollectionConfig pipelineConfigObj4 = CollectionConfig("test4", make_unique<Json::Value>(pipelineConfigJson4));
    pipelineConfigObj4.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj4));

    pipelineManager->UpdatePipelines(diff);
    APSARA_TEST_EQUAL_FATAL(4U, pipelineManager->GetAllPipelines().size());

    CollectionConfigDiff diffUpdate;
    diffUpdate.mRemoved.push_back("test4");
    auto pipeline = pipelineManager->GetAllPipelines().at("test4");
    auto input = static_cast<InputMock*>(const_cast<Input*>(pipeline->GetInputs()[0].get()->GetPlugin()));
    input->Block();

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    auto result = async(launch::async, [&]() { pipelineManager->UpdatePipelines(diffUpdate); });
    { // add data to C++ -> C++ -> C++
        std::string configName = "test1";
        AddDataToProcessor(configName, "test-data-1");
        AddDataToProcessor(configName, "test-data-2");
        AddDataToProcessor(configName, "test-data-3");
        VerifyData("test_logstore_1", 1, 3);
    }
    HttpSinkMock::GetInstance()->ClearRequests();
    { // add data to Go -> Go -> C++
        std::string configName = "test3";
        auto pipeline = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
        auto flusher = const_cast<Flusher*>(pipeline->GetFlushers()[0].get()->GetPlugin());
        AddDataToSenderQueue(configName, "test-data-1", flusher);
        AddDataToSenderQueue(configName, "test-data-2", flusher);
        AddDataToSenderQueue(configName, "test-data-3", flusher);
        VerifyData("test_logstore_1", 1, 3);
    }

    input->Unblock();
    result.get();
    APSARA_TEST_EQUAL_FATAL(3U, pipelineManager->GetAllPipelines().size());
}

void PipelineUpdateUnittest::TestPipelineUpdateManyCase1() const {
    // update 3 times
    // 1. process queue not empty, send queue not empty
    // 2. add data
    const std::string configName = "test1";
    ProcessorRunner::GetInstance()->Stop();
    // load old pipeline
    Json::Value pipelineConfigJson
        = GeneratePipelineConfigJson(nativeInputConfig, nativeProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    // Add data without trigger
    auto pipeline1 = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto flusher1 = const_cast<Flusher*>(pipeline1->GetFlushers()[0].get()->GetPlugin());
    AddDataToSenderQueue(configName, "test-data-1", flusher1);
    AddDataToSenderQueue(configName, "test-data-2", flusher1);
    AddDataToSenderQueue(configName, "test-data-3", flusher1);

    AddDataToProcessQueue(configName, "test-data-4"); // will be popped to processor
    AddDataToProcessQueue(configName, "test-data-5");
    AddDataToProcessQueue(configName, "test-data-6");
    AddDataToProcessQueue(configName, "test-data-7");

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate2
        = GeneratePipelineConfigJson(nativeInputConfig2, nativeProcessorConfig2, nativeFlusherConfig2);
    CollectionConfigDiff diffUpdate2;
    CollectionConfig pipelineConfigObjUpdate2
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate2));
    pipelineConfigObjUpdate2.Parse();
    diffUpdate2.mModified.push_back(std::move(pipelineConfigObjUpdate2));
    pipelineManager->UpdatePipelines(diffUpdate2);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    auto pipeline2 = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    AddDataToProcessQueue(configName, "test-data-8");
    AddDataToProcessQueue(configName, "test-data-9");
    AddDataToProcessQueue(configName, "test-data-10");

    ProcessorRunner::GetInstance()->Init();
    // load new pipeline
    Json::Value pipelineConfigJsonUpdate3
        = GeneratePipelineConfigJson(nativeInputConfig3, nativeProcessorConfig3, nativeFlusherConfig3);
    CollectionConfigDiff diffUpdate3;
    CollectionConfig pipelineConfigObjUpdate3
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate3));
    pipelineConfigObjUpdate3.Parse();
    diffUpdate3.mModified.push_back(std::move(pipelineConfigObjUpdate3));
    auto result = async(launch::async, [&]() {
        this_thread::sleep_for(chrono::milliseconds(1000));
        auto processor2
            = static_cast<ProcessorMock*>(const_cast<Processor*>(pipeline2->mProcessorLine[0].get()->mPlugin.get()));
        processor2->Unblock();
    });
    pipelineManager->UpdatePipelines(diffUpdate3);
    result.get();
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    AddDataToProcessQueue(configName, "test-data-11");
    AddDataToProcessQueue(configName, "test-data-12");
    AddDataToProcessQueue(configName, "test-data-13");

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    UnBlockProcessor(configName);
    VerifyData("test_logstore_1", 1, 3);
    VerifyData("test_logstore_2", 4, 4, true);
    VerifyData("test_logstore_3", 5, 13, true);
}

void PipelineUpdateUnittest::TestPipelineUpdateManyCase2() const {
    // update 3 times
    // 1. process queue not empty, send queue not empty
    // 2. not add data
    const std::string configName = "test1";
    ProcessorRunner::GetInstance()->Stop();
    // load old pipeline
    Json::Value pipelineConfigJson
        = GeneratePipelineConfigJson(nativeInputConfig, nativeProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    // Add data without trigger
    auto pipeline1 = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto flusher1 = const_cast<Flusher*>(pipeline1->GetFlushers()[0].get()->GetPlugin());
    AddDataToSenderQueue(configName, "test-data-1", flusher1);
    AddDataToSenderQueue(configName, "test-data-2", flusher1);
    AddDataToSenderQueue(configName, "test-data-3", flusher1);

    AddDataToProcessQueue(configName, "test-data-4"); // will be popped to processor
    AddDataToProcessQueue(configName, "test-data-5");
    AddDataToProcessQueue(configName, "test-data-6");
    AddDataToProcessQueue(configName, "test-data-7");

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate2
        = GeneratePipelineConfigJson(nativeInputConfig2, nativeProcessorConfig2, nativeFlusherConfig2);
    CollectionConfigDiff diffUpdate2;
    CollectionConfig pipelineConfigObjUpdate2
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate2));
    pipelineConfigObjUpdate2.Parse();
    diffUpdate2.mModified.push_back(std::move(pipelineConfigObjUpdate2));
    pipelineManager->UpdatePipelines(diffUpdate2);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    auto pipeline2 = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();

    ProcessorRunner::GetInstance()->Init();
    // load new pipeline
    Json::Value pipelineConfigJsonUpdate3
        = GeneratePipelineConfigJson(nativeInputConfig3, nativeProcessorConfig3, nativeFlusherConfig3);
    CollectionConfigDiff diffUpdate3;
    CollectionConfig pipelineConfigObjUpdate3
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate3));
    pipelineConfigObjUpdate3.Parse();
    diffUpdate3.mModified.push_back(std::move(pipelineConfigObjUpdate3));
    auto result = async(launch::async, [&]() {
        this_thread::sleep_for(chrono::milliseconds(1000));
        auto processor2
            = static_cast<ProcessorMock*>(const_cast<Processor*>(pipeline2->mProcessorLine[0].get()->mPlugin.get()));
        processor2->Unblock();
    });
    pipelineManager->UpdatePipelines(diffUpdate3);
    BlockProcessor(configName);
    result.get();
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    AddDataToProcessQueue(configName, "test-data-8");
    AddDataToProcessQueue(configName, "test-data-9");
    AddDataToProcessQueue(configName, "test-data-10");

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    UnBlockProcessor(configName);
    VerifyData("test_logstore_1", 1, 3);
    VerifyData("test_logstore_2", 4, 4, true);
    VerifyData("test_logstore_3", 5, 10, true);
}

void PipelineUpdateUnittest::TestPipelineUpdateManyCase3() const {
    // update 3 times
    // 1. process queue empty, send queue not empty
    // 2. add data
    const std::string configName = "test1";
    ProcessorRunner::GetInstance()->Stop();
    // load old pipeline
    Json::Value pipelineConfigJson
        = GeneratePipelineConfigJson(nativeInputConfig, nativeProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    // Add data without trigger
    auto pipeline1 = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto flusher1 = const_cast<Flusher*>(pipeline1->GetFlushers()[0].get()->GetPlugin());
    AddDataToSenderQueue(configName, "test-data-1", flusher1);
    AddDataToSenderQueue(configName, "test-data-2", flusher1);
    AddDataToSenderQueue(configName, "test-data-3", flusher1);

    AddDataToProcessQueue(configName, "test-data-4"); // will be popped to processor

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate2
        = GeneratePipelineConfigJson(nativeInputConfig2, nativeProcessorConfig2, nativeFlusherConfig2);
    CollectionConfigDiff diffUpdate2;
    CollectionConfig pipelineConfigObjUpdate2
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate2));
    pipelineConfigObjUpdate2.Parse();
    diffUpdate2.mModified.push_back(std::move(pipelineConfigObjUpdate2));
    pipelineManager->UpdatePipelines(diffUpdate2);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    auto pipeline2 = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    AddDataToProcessQueue(configName, "test-data-5");
    AddDataToProcessQueue(configName, "test-data-6");
    AddDataToProcessQueue(configName, "test-data-7");

    ProcessorRunner::GetInstance()->Init();
    // load new pipeline
    Json::Value pipelineConfigJsonUpdate3
        = GeneratePipelineConfigJson(nativeInputConfig3, nativeProcessorConfig3, nativeFlusherConfig3);
    CollectionConfigDiff diffUpdate3;
    CollectionConfig pipelineConfigObjUpdate3
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate3));
    pipelineConfigObjUpdate3.Parse();
    diffUpdate3.mModified.push_back(std::move(pipelineConfigObjUpdate3));
    auto result = async(launch::async, [&]() {
        this_thread::sleep_for(chrono::milliseconds(1000));
        auto processor2
            = static_cast<ProcessorMock*>(const_cast<Processor*>(pipeline2->mProcessorLine[0].get()->mPlugin.get()));
        processor2->Unblock();
    });
    pipelineManager->UpdatePipelines(diffUpdate3);
    BlockProcessor(configName);
    result.get();
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    AddDataToProcessQueue(configName, "test-data-8");
    AddDataToProcessQueue(configName, "test-data-9");
    AddDataToProcessQueue(configName, "test-data-10");

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    UnBlockProcessor(configName);
    VerifyData("test_logstore_1", 1, 3);
    VerifyData("test_logstore_2", 4, 4, true);
    VerifyData("test_logstore_3", 5, 10, true);
}

void PipelineUpdateUnittest::TestPipelineUpdateManyCase4() const {
    // update 3 times
    // 1. process queue empty, send queue not empty
    // 2. not add data
    const std::string configName = "test1";
    ProcessorRunner::GetInstance()->Stop();
    // load old pipeline
    Json::Value pipelineConfigJson
        = GeneratePipelineConfigJson(nativeInputConfig, nativeProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    // Add data without trigger
    auto pipeline1 = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto flusher1 = const_cast<Flusher*>(pipeline1->GetFlushers()[0].get()->GetPlugin());
    AddDataToSenderQueue(configName, "test-data-1", flusher1);
    AddDataToSenderQueue(configName, "test-data-2", flusher1);
    AddDataToSenderQueue(configName, "test-data-3", flusher1);

    AddDataToProcessQueue(configName, "test-data-4"); // will be popped to processor

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate2
        = GeneratePipelineConfigJson(nativeInputConfig2, nativeProcessorConfig2, nativeFlusherConfig2);
    CollectionConfigDiff diffUpdate2;
    CollectionConfig pipelineConfigObjUpdate2
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate2));
    pipelineConfigObjUpdate2.Parse();
    diffUpdate2.mModified.push_back(std::move(pipelineConfigObjUpdate2));
    pipelineManager->UpdatePipelines(diffUpdate2);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    auto pipeline2 = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();

    ProcessorRunner::GetInstance()->Init();
    // load new pipeline
    Json::Value pipelineConfigJsonUpdate3
        = GeneratePipelineConfigJson(nativeInputConfig3, nativeProcessorConfig3, nativeFlusherConfig3);
    CollectionConfigDiff diffUpdate3;
    CollectionConfig pipelineConfigObjUpdate3
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate3));
    pipelineConfigObjUpdate3.Parse();
    diffUpdate3.mModified.push_back(std::move(pipelineConfigObjUpdate3));
    auto result = async(launch::async, [&]() {
        this_thread::sleep_for(chrono::milliseconds(1000));
        auto processor2
            = static_cast<ProcessorMock*>(const_cast<Processor*>(pipeline2->mProcessorLine[0].get()->mPlugin.get()));
        processor2->Unblock();
    });
    pipelineManager->UpdatePipelines(diffUpdate3);
    BlockProcessor(configName);
    result.get();
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    AddDataToProcessQueue(configName, "test-data-5");
    AddDataToProcessQueue(configName, "test-data-6");
    AddDataToProcessQueue(configName, "test-data-7");

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    UnBlockProcessor(configName);
    VerifyData("test_logstore_1", 1, 3);
    VerifyData("test_logstore_2", 4, 4, true);
    VerifyData("test_logstore_3", 5, 7, true);
}

void PipelineUpdateUnittest::TestPipelineUpdateManyCase5() const {
    // update 3 times
    // 1. process queue not empty, send queue empty
    // 2. add data
    const std::string configName = "test1";
    ProcessorRunner::GetInstance()->Stop();
    // load old pipeline
    Json::Value pipelineConfigJson
        = GeneratePipelineConfigJson(nativeInputConfig, nativeProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    // Add data without trigger
    auto pipeline1 = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    AddDataToProcessQueue(configName, "test-data-1"); // will be popped to processor
    AddDataToProcessQueue(configName, "test-data-2");
    AddDataToProcessQueue(configName, "test-data-3");
    AddDataToProcessQueue(configName, "test-data-4");

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate2
        = GeneratePipelineConfigJson(nativeInputConfig2, nativeProcessorConfig2, nativeFlusherConfig2);
    CollectionConfigDiff diffUpdate2;
    CollectionConfig pipelineConfigObjUpdate2
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate2));
    pipelineConfigObjUpdate2.Parse();
    diffUpdate2.mModified.push_back(std::move(pipelineConfigObjUpdate2));
    pipelineManager->UpdatePipelines(diffUpdate2);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    auto pipeline2 = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    AddDataToProcessQueue(configName, "test-data-5");
    AddDataToProcessQueue(configName, "test-data-6");
    AddDataToProcessQueue(configName, "test-data-7");

    ProcessorRunner::GetInstance()->Init();
    // load new pipeline
    Json::Value pipelineConfigJsonUpdate3
        = GeneratePipelineConfigJson(nativeInputConfig3, nativeProcessorConfig3, nativeFlusherConfig3);
    CollectionConfigDiff diffUpdate3;
    CollectionConfig pipelineConfigObjUpdate3
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate3));
    pipelineConfigObjUpdate3.Parse();
    diffUpdate3.mModified.push_back(std::move(pipelineConfigObjUpdate3));
    auto result = async(launch::async, [&]() {
        this_thread::sleep_for(chrono::milliseconds(1000));
        auto processor2
            = static_cast<ProcessorMock*>(const_cast<Processor*>(pipeline2->mProcessorLine[0].get()->mPlugin.get()));
        processor2->Unblock();
    });
    pipelineManager->UpdatePipelines(diffUpdate3);
    BlockProcessor(configName);
    result.get();
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    AddDataToProcessQueue(configName, "test-data-8");
    AddDataToProcessQueue(configName, "test-data-9");
    AddDataToProcessQueue(configName, "test-data-10");

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    UnBlockProcessor(configName);
    VerifyData("test_logstore_2", 1, 1, true);
    VerifyData("test_logstore_3", 2, 10, true);
}

void PipelineUpdateUnittest::TestPipelineUpdateManyCase6() const {
    // update 3 times
    // 1. process queue not empty, send queue empty
    // 2. not add data
    const std::string configName = "test1";
    ProcessorRunner::GetInstance()->Stop();
    // load old pipeline
    Json::Value pipelineConfigJson
        = GeneratePipelineConfigJson(nativeInputConfig, nativeProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    // Add data without trigger
    auto pipeline1 = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    AddDataToProcessQueue(configName, "test-data-1"); // will be popped to processor
    AddDataToProcessQueue(configName, "test-data-2");
    AddDataToProcessQueue(configName, "test-data-3");
    AddDataToProcessQueue(configName, "test-data-4");

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate2
        = GeneratePipelineConfigJson(nativeInputConfig2, nativeProcessorConfig2, nativeFlusherConfig2);
    CollectionConfigDiff diffUpdate2;
    CollectionConfig pipelineConfigObjUpdate2
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate2));
    pipelineConfigObjUpdate2.Parse();
    diffUpdate2.mModified.push_back(std::move(pipelineConfigObjUpdate2));
    pipelineManager->UpdatePipelines(diffUpdate2);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    auto pipeline2 = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();

    ProcessorRunner::GetInstance()->Init();
    // load new pipeline
    Json::Value pipelineConfigJsonUpdate3
        = GeneratePipelineConfigJson(nativeInputConfig3, nativeProcessorConfig3, nativeFlusherConfig3);
    CollectionConfigDiff diffUpdate3;
    CollectionConfig pipelineConfigObjUpdate3
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate3));
    pipelineConfigObjUpdate3.Parse();
    diffUpdate3.mModified.push_back(std::move(pipelineConfigObjUpdate3));
    auto result = async(launch::async, [&]() {
        this_thread::sleep_for(chrono::milliseconds(1000));
        auto processor2
            = static_cast<ProcessorMock*>(const_cast<Processor*>(pipeline2->mProcessorLine[0].get()->mPlugin.get()));
        processor2->Unblock();
    });
    pipelineManager->UpdatePipelines(diffUpdate3);
    BlockProcessor(configName);
    result.get();
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    AddDataToProcessQueue(configName, "test-data-5");
    AddDataToProcessQueue(configName, "test-data-6");
    AddDataToProcessQueue(configName, "test-data-7");

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    UnBlockProcessor(configName);
    VerifyData("test_logstore_2", 1, 1, true);
    VerifyData("test_logstore_3", 2, 7, true);
}

void PipelineUpdateUnittest::TestPipelineUpdateManyCase7() const {
    // update 3 times
    // 1. process queue empty, send queue empty
    // 2. add data
    const std::string configName = "test1";
    ProcessorRunner::GetInstance()->Stop();
    // load old pipeline
    Json::Value pipelineConfigJson
        = GeneratePipelineConfigJson(nativeInputConfig, nativeProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    // Add data without trigger
    auto pipeline1 = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    AddDataToProcessQueue(configName, "test-data-1"); // will be popped to processor

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate2
        = GeneratePipelineConfigJson(nativeInputConfig2, nativeProcessorConfig2, nativeFlusherConfig2);
    CollectionConfigDiff diffUpdate2;
    CollectionConfig pipelineConfigObjUpdate2
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate2));
    pipelineConfigObjUpdate2.Parse();
    diffUpdate2.mModified.push_back(std::move(pipelineConfigObjUpdate2));
    pipelineManager->UpdatePipelines(diffUpdate2);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    auto pipeline2 = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    AddDataToProcessQueue(configName, "test-data-2");
    AddDataToProcessQueue(configName, "test-data-3");
    AddDataToProcessQueue(configName, "test-data-4");

    ProcessorRunner::GetInstance()->Init();
    // load new pipeline
    Json::Value pipelineConfigJsonUpdate3
        = GeneratePipelineConfigJson(nativeInputConfig3, nativeProcessorConfig3, nativeFlusherConfig3);
    CollectionConfigDiff diffUpdate3;
    CollectionConfig pipelineConfigObjUpdate3
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate3));
    pipelineConfigObjUpdate3.Parse();
    diffUpdate3.mModified.push_back(std::move(pipelineConfigObjUpdate3));
    auto result = async(launch::async, [&]() {
        this_thread::sleep_for(chrono::milliseconds(1000));
        auto processor2
            = static_cast<ProcessorMock*>(const_cast<Processor*>(pipeline2->mProcessorLine[0].get()->mPlugin.get()));
        processor2->Unblock();
    });
    pipelineManager->UpdatePipelines(diffUpdate3);
    BlockProcessor(configName);
    result.get();
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    AddDataToProcessQueue(configName, "test-data-5");
    AddDataToProcessQueue(configName, "test-data-6");
    AddDataToProcessQueue(configName, "test-data-7");

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    UnBlockProcessor(configName);
    VerifyData("test_logstore_2", 1, 1, true);
    VerifyData("test_logstore_3", 2, 7, true);
}

void PipelineUpdateUnittest::TestPipelineUpdateManyCase8() const {
    // update 3 times
    // 1. process queue empty, send queue empty
    // 2. not add data
    const std::string configName = "test1";
    ProcessorRunner::GetInstance()->Stop();
    // load old pipeline
    Json::Value pipelineConfigJson
        = GeneratePipelineConfigJson(nativeInputConfig, nativeProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    // Add data without trigger
    auto pipeline1 = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    AddDataToProcessQueue(configName, "test-data-1"); // will be popped to processor

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate2
        = GeneratePipelineConfigJson(nativeInputConfig2, nativeProcessorConfig2, nativeFlusherConfig2);
    CollectionConfigDiff diffUpdate2;
    CollectionConfig pipelineConfigObjUpdate2
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate2));
    pipelineConfigObjUpdate2.Parse();
    diffUpdate2.mModified.push_back(std::move(pipelineConfigObjUpdate2));
    pipelineManager->UpdatePipelines(diffUpdate2);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    auto pipeline2 = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();

    ProcessorRunner::GetInstance()->Init();
    // load new pipeline
    Json::Value pipelineConfigJsonUpdate3
        = GeneratePipelineConfigJson(nativeInputConfig3, nativeProcessorConfig3, nativeFlusherConfig3);
    CollectionConfigDiff diffUpdate3;
    CollectionConfig pipelineConfigObjUpdate3
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate3));
    pipelineConfigObjUpdate3.Parse();
    diffUpdate3.mModified.push_back(std::move(pipelineConfigObjUpdate3));
    auto result = async(launch::async, [&]() {
        this_thread::sleep_for(chrono::milliseconds(1000));
        auto processor2
            = static_cast<ProcessorMock*>(const_cast<Processor*>(pipeline2->mProcessorLine[0].get()->mPlugin.get()));
        processor2->Unblock();
    });
    pipelineManager->UpdatePipelines(diffUpdate3);
    BlockProcessor(configName);
    result.get();
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    AddDataToProcessQueue(configName, "test-data-2");
    AddDataToProcessQueue(configName, "test-data-3");
    AddDataToProcessQueue(configName, "test-data-4");

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    UnBlockProcessor(configName);
    VerifyData("test_logstore_2", 1, 1, true);
    VerifyData("test_logstore_3", 2, 4, true);
}

void PipelineUpdateUnittest::TestPipelineUpdateManyCase9() const {
    // update 3 times
    // 1. process queue empty, send queue not empty
    // 2. add data to send queue
    const std::string configName = "test1";
    ProcessorRunner::GetInstance()->Stop();
    // load old pipeline
    Json::Value pipelineConfigJson
        = GeneratePipelineConfigJson(nativeInputConfig, nativeProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    // Add data without trigger
    auto pipeline1 = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto flusher1 = const_cast<Flusher*>(pipeline1->GetFlushers()[0].get()->GetPlugin());
    AddDataToSenderQueue(configName, "test-data-1", flusher1);
    AddDataToSenderQueue(configName, "test-data-2", flusher1);
    AddDataToSenderQueue(configName, "test-data-3", flusher1);

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate2
        = GeneratePipelineConfigJson(nativeInputConfig2, nativeProcessorConfig2, nativeFlusherConfig2);
    CollectionConfigDiff diffUpdate2;
    CollectionConfig pipelineConfigObjUpdate2
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate2));
    pipelineConfigObjUpdate2.Parse();
    diffUpdate2.mModified.push_back(std::move(pipelineConfigObjUpdate2));
    pipelineManager->UpdatePipelines(diffUpdate2);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    auto pipeline2 = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto flusher2 = const_cast<Flusher*>(pipeline2->GetFlushers()[0].get()->GetPlugin());
    AddDataToSenderQueue(configName, "test-data-4", flusher2);
    AddDataToSenderQueue(configName, "test-data-5", flusher2);
    AddDataToSenderQueue(configName, "test-data-6", flusher2);

    ProcessorRunner::GetInstance()->Init();
    // load new pipeline
    Json::Value pipelineConfigJsonUpdate3
        = GeneratePipelineConfigJson(nativeInputConfig3, nativeProcessorConfig3, nativeFlusherConfig3);
    CollectionConfigDiff diffUpdate3;
    CollectionConfig pipelineConfigObjUpdate3
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate3));
    pipelineConfigObjUpdate3.Parse();
    diffUpdate3.mModified.push_back(std::move(pipelineConfigObjUpdate3));
    pipelineManager->UpdatePipelines(diffUpdate3);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    AddDataToProcessQueue(configName, "test-data-7");
    AddDataToProcessQueue(configName, "test-data-8");
    AddDataToProcessQueue(configName, "test-data-9");

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    UnBlockProcessor(configName);
    VerifyData("test_logstore_1", 1, 3);
    VerifyData("test_logstore_2", 4, 6);
    VerifyData("test_logstore_3", 7, 9, true);
}

void PipelineUpdateUnittest::TestPipelineUpdateManyCase10() const {
    // update 3 times
    // 1. process queue empty, send queue not empty
    // 2. not add data to send queue
    const std::string configName = "test1";
    ProcessorRunner::GetInstance()->Stop();
    // load old pipeline
    Json::Value pipelineConfigJson
        = GeneratePipelineConfigJson(nativeInputConfig, nativeProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    // Add data without trigger
    auto pipeline1 = CollectionPipelineManager::GetInstance()->GetAllPipelines().at(configName).get();
    auto flusher1 = const_cast<Flusher*>(pipeline1->GetFlushers()[0].get()->GetPlugin());
    AddDataToSenderQueue(configName, "test-data-1", flusher1);
    AddDataToSenderQueue(configName, "test-data-2", flusher1);
    AddDataToSenderQueue(configName, "test-data-3", flusher1);

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate2
        = GeneratePipelineConfigJson(nativeInputConfig2, nativeProcessorConfig2, nativeFlusherConfig2);
    CollectionConfigDiff diffUpdate2;
    CollectionConfig pipelineConfigObjUpdate2
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate2));
    pipelineConfigObjUpdate2.Parse();
    diffUpdate2.mModified.push_back(std::move(pipelineConfigObjUpdate2));
    pipelineManager->UpdatePipelines(diffUpdate2);
    BlockProcessor(configName);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    ProcessorRunner::GetInstance()->Init();
    // load new pipeline
    Json::Value pipelineConfigJsonUpdate3
        = GeneratePipelineConfigJson(nativeInputConfig3, nativeProcessorConfig3, nativeFlusherConfig3);
    CollectionConfigDiff diffUpdate3;
    CollectionConfig pipelineConfigObjUpdate3
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate3));
    pipelineConfigObjUpdate3.Parse();
    diffUpdate3.mModified.push_back(std::move(pipelineConfigObjUpdate3));
    pipelineManager->UpdatePipelines(diffUpdate3);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    AddDataToProcessQueue(configName, "test-data-4");
    AddDataToProcessQueue(configName, "test-data-5");
    AddDataToProcessQueue(configName, "test-data-6");

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    UnBlockProcessor(configName);
    VerifyData("test_logstore_1", 1, 3);
    VerifyData("test_logstore_3", 4, 6);
}

void PipelineUpdateUnittest::TestPipelineRelease() const {
    const std::string configName = "test1";
    ProcessorRunner::GetInstance()->Stop();
    // load old pipeline
    Json::Value pipelineConfigJson
        = GeneratePipelineConfigJson(nativeInputConfig, nativeProcessorConfig, nativeFlusherConfig);
    auto pipelineManager = CollectionPipelineManager::GetInstance();
    CollectionConfigDiff diff;
    CollectionConfig pipelineConfigObj = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJson));
    pipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(pipelineConfigObj));
    pipelineManager->UpdatePipelines(diff);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    // Add data without trigger
    AddDataToProcessQueue(configName, "test-data-1");
    AddDataToProcessQueue(configName, "test-data-2");
    AddDataToProcessQueue(configName, "test-data-3");

    // load new pipeline
    Json::Value pipelineConfigJsonUpdate2
        = GeneratePipelineConfigJson(nativeInputConfig2, nativeProcessorConfig2, slowFlusherConfig);
    CollectionConfigDiff diffUpdate2;
    CollectionConfig pipelineConfigObjUpdate2
        = CollectionConfig(configName, make_unique<Json::Value>(pipelineConfigJsonUpdate2));
    pipelineConfigObjUpdate2.Parse();
    diffUpdate2.mModified.push_back(std::move(pipelineConfigObjUpdate2));
    pipelineManager->UpdatePipelines(diffUpdate2);
    APSARA_TEST_EQUAL_FATAL(1U, pipelineManager->GetAllPipelines().size());

    ProcessorRunner::GetInstance()->Init();
    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();
    VerifyData("test_logstore_2", 1, 3, true);
    LOG_INFO(sLogger, ("test", "end"));
}

UNIT_TEST_CASE(PipelineUpdateUnittest, TestFileServerStart)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineParamUpdateCase1)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineParamUpdateCase2)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineParamUpdateCase3)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineParamUpdateCase4)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineTypeUpdateCase1)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineTypeUpdateCase2)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineTypeUpdateCase3)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineTypeUpdateCase4)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineTopoUpdateCase1)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineTopoUpdateCase2)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineTopoUpdateCase3)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineTopoUpdateCase4)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineTopoUpdateCase5)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineTopoUpdateCase6)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineTopoUpdateCase7)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineTopoUpdateCase8)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineTopoUpdateCase9)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineTopoUpdateCase10)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineTopoUpdateCase11)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineTopoUpdateCase12)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineInputBlock)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineGoInputBlockCase1)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineGoInputBlockCase2)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineIsolationCase1)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineIsolationCase2)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineUpdateManyCase1)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineUpdateManyCase2)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineUpdateManyCase3)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineUpdateManyCase4)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineUpdateManyCase5)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineUpdateManyCase6)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineUpdateManyCase7)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineUpdateManyCase8)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineUpdateManyCase9)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineUpdateManyCase10)
UNIT_TEST_CASE(PipelineUpdateUnittest, TestPipelineRelease)

} // namespace logtail

UNIT_TEST_MAIN
