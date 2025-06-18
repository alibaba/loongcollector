// Copyright 2022 iLogtail Authors
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

#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <filesystem>
#include <memory>
#include <string>

#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "common/FileSystemUtil.h"
#include "common/Flags.h"
#include "common/JsonUtil.h"
#include "config/CollectionConfig.h"
#include "file_server/ConfigManager.h"
#include "file_server/event/Event.h"
#include "file_server/event_handler/EventHandler.h"
#include "unittest/Unittest.h"
#include "unittest/UnittestHelper.h"

using namespace std;

DECLARE_FLAG_STRING(ilogtail_config);

namespace logtail {
class MockModifyHandler : public ModifyHandler {
public:
    MockModifyHandler(const std::string& configName, const FileDiscoveryConfig& pConfig)
        : ModifyHandler(configName, pConfig) {}
    virtual void Handle(const Event& event) { ++handle_count; }
    virtual void HandleTimeOut() { ++handle_timeout_count; }
    virtual bool DumpReaderMeta(bool isRotatorReader, bool checkConfigFlag) { return true; }
    void Reset() {
        handle_count = 0;
        handle_timeout_count = 0;
    }
    int handle_count = 0;
    int handle_timeout_count = 0;
};

class CreateModifyHandlerUnittest : public ::testing::Test {
public:
    void TestHandleContainerStoppedEvent();

protected:
    static void SetUpTestCase() {
        srand(time(NULL));
        gRootDir = GetProcessExecutionDir();
        gLogName = "test.log";
        if (PATH_SEPARATOR[0] == gRootDir.at(gRootDir.size() - 1))
            gRootDir.resize(gRootDir.size() - 1);
        gRootDir += PATH_SEPARATOR + "ModifyHandlerUnittest";
        filesystem::remove_all(gRootDir);
    }

    static void TearDownTestCase() {}

    void SetUp() override {
        bfs::create_directories(gRootDir);
        // create a file for reader
        std::string logPath = UnitTestHelper::JsonEscapeDirPath(gRootDir + PATH_SEPARATOR + gLogName);
        writeLog(logPath, "a sample log\n");

        // init pipeline and config
        unique_ptr<Json::Value> configJson;
        string configStr, errorMsg;
        unique_ptr<CollectionConfig> config;
        unique_ptr<CollectionPipeline> pipeline;

        // new pipeline
        configStr = R"(
            {
                "inputs": [
                    {
                        "Type": "input_file",
                        "FilePaths": [
                            ")"
            + logPath + R"("
                        ]
                    }
                ],
                "flushers": [
                    {
                        "Type": "flusher_sls",
                        "Project": "test_project",
                        "Logstore": "test_logstore",
                        "Region": "test_region",
                        "Endpoint": "test_endpoint"
                    }
                ]
            }
        )";
        configJson.reset(new Json::Value());
        APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
        Json::Value inputConfigJson = (*configJson)["inputs"][0];

        config.reset(new CollectionConfig(mConfigName, std::move(configJson)));
        APSARA_TEST_TRUE(config->Parse());
        pipeline.reset(new CollectionPipeline());
        APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
        ctx.SetPipeline(*pipeline.get());
        ctx.SetConfigName(mConfigName);
        ctx.SetProcessQueueKey(0);
        discoveryOpts = FileDiscoveryOptions();
        discoveryOpts.Init(inputConfigJson, ctx, "test");
        discoveryOpts.SetDeduceAndSetContainerBaseDirFunc(
            [](ContainerInfo& containerInfo, const CollectionPipelineContext* ctx, const FileDiscoveryOptions* opts) {
                containerInfo.mRealBaseDir = containerInfo.mUpperDir;
                return true;
            });
        mConfig = std::make_pair(&discoveryOpts, &ctx);
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;

        FileServer::GetInstance()->AddFileDiscoveryConfig(mConfigName, &discoveryOpts, &ctx);
        FileServer::GetInstance()->AddFileReaderConfig(mConfigName, &readerOpts, &ctx);
        FileServer::GetInstance()->AddMultilineConfig(mConfigName, &multilineOpts, &ctx);
        FileServer::GetInstance()->AddFileTagConfig(mConfigName, &tagOpts, &ctx);

        ProcessQueueManager::GetInstance()->CreateOrUpdateBoundedQueue(0, 0, ctx);

        // build a reader
        mReaderPtr = std::make_shared<LogFileReader>(gRootDir,
                                                     gLogName,
                                                     DevInode(),
                                                     std::make_pair(&readerOpts, &ctx),
                                                     std::make_pair(&multilineOpts, &ctx),
                                                     std::make_pair(&tagOpts, &ctx));
        mReaderPtr->UpdateReaderManual();
        mReaderPtr->SetContainerID("1");
        APSARA_TEST_TRUE_FATAL(mReaderPtr->CheckFileSignatureAndOffset(true));

        // build a modify handler
        LogFileReaderPtrArray readerPtrArray{mReaderPtr};
        mHandlerPtr.reset(new ModifyHandler(mConfigName, mConfig));
        mHandlerPtr->mNameReaderMap[gLogName] = readerPtrArray;
        mReaderPtr->SetReaderArray(&mHandlerPtr->mNameReaderMap[gLogName]);
        mHandlerPtr->mDevInodeReaderMap[mReaderPtr->mDevInode] = mReaderPtr;

        auto containerInfo = std::make_shared<std::vector<ContainerInfo>>();
        discoveryOpts.SetContainerInfo(containerInfo);
    }

    void TearDown() override { filesystem::remove_all(gRootDir); }

    static std::string gRootDir;
    static std::string gLogName;

private:
    const std::string mConfigName = "##1.0##project-0$config-0";
    FileDiscoveryOptions discoveryOpts;
    FileReaderOptions readerOpts;
    MultilineOptions multilineOpts;
    FileTagOptions tagOpts;
    CollectionPipelineContext ctx;
    FileDiscoveryConfig mConfig;

    std::shared_ptr<LogFileReader> mReaderPtr;
    std::shared_ptr<ModifyHandler> mHandlerPtr;
    CreateHandler mCreateHandler;

    void writeLog(const std::string& logPath, const std::string& logContent) {
        std::ofstream writer(logPath.c_str(), fstream::out | fstream::app | ios_base::binary);
        writer << logContent;
        writer.close();
    }
};

void CreateModifyHandlerUnittest::TestHandleContainerStoppedEvent() {
    LOG_INFO(sLogger, ("TestFindAllSubDirAndHandler() begin", time(NULL)));
    CreateModifyHandler createModifyHandler(&mCreateHandler);

    MockModifyHandler* pHanlder = new MockModifyHandler(mConfigName, mConfig); // released by ~CreateModifyHandler
    createModifyHandler.mModifyHandlerPtrMap.insert(std::make_pair(mConfigName, pHanlder));

    Event event1("/not_exist", "", EVENT_ISDIR | EVENT_CONTAINER_STOPPED, 0);
    event1.SetConfigName(mConfigName);
    createModifyHandler.Handle(event1);
    APSARA_TEST_EQUAL_FATAL(pHanlder->handle_count, 1);

    Event event2(gRootDir, "", EVENT_ISDIR | EVENT_CONTAINER_STOPPED, 0);
    event2.SetConfigName(mConfigName);
    createModifyHandler.Handle(event2);
    APSARA_TEST_EQUAL_FATAL(pHanlder->handle_count, 2);
}

std::string CreateModifyHandlerUnittest::gRootDir;
std::string CreateModifyHandlerUnittest::gLogName;

UNIT_TEST_CASE(CreateModifyHandlerUnittest, TestHandleContainerStoppedEvent);
} // end of namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
