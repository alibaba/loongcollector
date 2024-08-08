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

#include <memory>
#include <string>

#include "common/FileSystemUtil.h"
#include "common/Flags.h"
#include "common/JsonUtil.h"
#include "config/PipelineConfig.h"
#include "event/Event.h"
#include "event_handler/EventHandler.h"
#include "file_server/FileServer.h"
#include "pipeline/Pipeline.h"
#include "queue/ProcessQueueManager.h"
#include "reader/LogFileReader.h"
#include "unittest/Unittest.h"

using namespace std;

DECLARE_FLAG_STRING(ilogtail_config);
DECLARE_FLAG_INT32(default_tail_limit_kb);

namespace logtail {
class ModifyHandlerUnittest : public ::testing::Test {
public:
    void TestHandleContainerStoppedEventWhenReadToEnd();
    void TestHandleContainerStoppedEventWhenNotReadToEnd();
    void TestHandleModifyEventWhenContainerStopped();
    void TestRecoverReaderFromCheckpoint();

protected:
    static void SetUpTestCase() {
        srand(time(NULL));
        gRootDir = GetProcessExecutionDir();
        gLogName = "test.log";
        if (PATH_SEPARATOR[0] == gRootDir.at(gRootDir.size() - 1))
            gRootDir.resize(gRootDir.size() - 1);
        gRootDir += PATH_SEPARATOR + "ModifyHandlerUnittest";
        bfs::remove_all(gRootDir);
    }

    static void TearDownTestCase() {}

    void SetUp() override {
        bfs::create_directories(gRootDir);
        // create a file for reader
        std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
        writeLog(logPath, "a sample log\n");

        // init pipeline and config
        unique_ptr<Json::Value> configJson;
        string configStr, errorMsg;
        unique_ptr<PipelineConfig> config;
        unique_ptr<Pipeline> pipeline;
        list<ProcessQueueInterface>::iterator que;

        // new pipeline
        configStr = R"(
            {
                "global": {
                    "ProcessPriority": 1
                },
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

        config.reset(new PipelineConfig(mConfigName, std::move(configJson)));
        APSARA_TEST_TRUE(config->Parse());
        pipeline.reset(new Pipeline());
        APSARA_TEST_TRUE(pipeline->Init(std::move(*config)));
        ctx.SetPipeline(*pipeline.get());
        ctx.SetConfigName(mConfigName);
        ctx.SetProcessQueueKey(0);

        discoveryOpts = FileDiscoveryOptions();
        discoveryOpts.Init(inputConfigJson, ctx, "test");
        mConfig = std::make_pair(&discoveryOpts, &ctx);
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;

        FileServer::GetInstance()->AddFileDiscoveryConfig(mConfigName, &discoveryOpts, &ctx);
        FileServer::GetInstance()->AddFileReaderConfig(mConfigName, &readerOpts, &ctx);
        FileServer::GetInstance()->AddMultilineConfig(mConfigName, &multilineOpts, &ctx);
        ProcessQueueManager::GetInstance()->CreateOrUpdateQueue(0, 0, ProcessQueueManager::QueueType::BOUNDED);

        // build a reader
        mReaderPtr = std::make_shared<LogFileReader>(
            gRootDir, gLogName, DevInode(), std::make_pair(&readerOpts, &ctx), std::make_pair(&multilineOpts, &ctx));
        mReaderPtr->UpdateReaderManual();
        APSARA_TEST_TRUE_FATAL(mReaderPtr->CheckFileSignatureAndOffset(true));

        // build a modify handler
        LogFileReaderPtrArray readerPtrArray{mReaderPtr};
        mHandlerPtr.reset(new ModifyHandler(mConfigName, mConfig));
        mHandlerPtr->mNameReaderMap[gLogName] = readerPtrArray;
        mReaderPtr->SetReaderArray(&mHandlerPtr->mNameReaderMap[gLogName]);
        mHandlerPtr->mDevInodeReaderMap[mReaderPtr->mDevInode] = mReaderPtr;
    }
    void TearDown() override { bfs::remove_all(gRootDir); }

    static std::string gRootDir;
    static std::string gLogName;

private:
    const std::string mConfigName = "##1.0##project-0$config-0";
    FileDiscoveryOptions discoveryOpts;
    FileReaderOptions readerOpts;
    MultilineOptions multilineOpts;
    PipelineContext ctx;
    FileDiscoveryConfig mConfig;

    std::shared_ptr<LogFileReader> mReaderPtr;
    std::shared_ptr<ModifyHandler> mHandlerPtr;

    void writeLog(const std::string& logPath, const std::string& logContent) {
        std::ofstream writer(logPath.c_str(), fstream::out | fstream::app);
        writer << logContent;
        writer.close();
    }
};

std::string ModifyHandlerUnittest::gRootDir;
std::string ModifyHandlerUnittest::gLogName;

UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleContainerStoppedEventWhenReadToEnd);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleContainerStoppedEventWhenNotReadToEnd);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleModifyEventWhenContainerStopped);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestRecoverReaderFromCheckpoint);

void ModifyHandlerUnittest::TestHandleContainerStoppedEventWhenReadToEnd() {
    LOG_INFO(sLogger, ("TestFindAllSubDirAndHandler() begin", time(NULL)));
    Event event1(gRootDir, "", EVENT_MODIFY, 0);
    LogBuffer logbuf;
    APSARA_TEST_TRUE_FATAL(!mReaderPtr->ReadLog(logbuf, &event1)); // false means no more data
    APSARA_TEST_TRUE_FATAL(mReaderPtr->mLogFileOp.IsOpen());

    // send event to close reader
    Event event2(gRootDir, "", EVENT_ISDIR | EVENT_CONTAINER_STOPPED, 0);
    mHandlerPtr->Handle(event2);
    APSARA_TEST_TRUE_FATAL(!mReaderPtr->mLogFileOp.IsOpen());
}

void ModifyHandlerUnittest::TestHandleContainerStoppedEventWhenNotReadToEnd() {
    LOG_INFO(sLogger, ("TestFindAllSubDirAndHandler() begin", time(NULL)));
    APSARA_TEST_TRUE_FATAL(mReaderPtr->mLogFileOp.IsOpen());

    // send event to close reader
    Event event(gRootDir, "", EVENT_ISDIR | EVENT_CONTAINER_STOPPED, 0);
    mHandlerPtr->Handle(event);
    APSARA_TEST_TRUE_FATAL(mReaderPtr->IsContainerStopped());
    APSARA_TEST_TRUE_FATAL(mReaderPtr->mLogFileOp.IsOpen());
}

void ModifyHandlerUnittest::TestHandleModifyEventWhenContainerStopped() {
    LOG_INFO(sLogger, ("TestFindAllSubDirAndHandler() begin", time(NULL)));
    APSARA_TEST_TRUE_FATAL(mReaderPtr->mLogFileOp.IsOpen());

    // SetContainerStopped to reader
    mReaderPtr->SetContainerStopped();
    // send event to read to end
    Event event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, mReaderPtr->mDevInode.dev, mReaderPtr->mDevInode.inode);
    mHandlerPtr->Handle(event);
    APSARA_TEST_TRUE_FATAL(mReaderPtr->IsReadToEnd());
    APSARA_TEST_TRUE_FATAL(!mReaderPtr->mLogFileOp.IsOpen());
}

void ModifyHandlerUnittest::TestRecoverReaderFromCheckpoint() {
    std::string basicLogName = "rotate.log";
    std::string logPath = gRootDir + PATH_SEPARATOR + basicLogName;
    std::string signature = "a sample log";
    auto sigSize = (uint32_t)signature.size();
    auto sigHash = (uint64_t)HashSignatureString(signature.c_str(), (size_t)sigSize);
    // build a modify handler
    auto handlerPtr = std::make_shared<ModifyHandler>(mConfigName, mConfig);
    writeLog(logPath, "a sample log\n");
    auto devInode = GetFileDevInode(logPath);
    // build readers in reader array

    std::string logPath1 = logPath + ".1";
    writeLog(logPath1, "a sample log\n");
    auto devInode1 = GetFileDevInode(logPath1);
    auto reader1 = std::make_shared<LogFileReader>(
        gRootDir, basicLogName, devInode1, std::make_pair(&readerOpts, &ctx), std::make_pair(&multilineOpts, &ctx));
    reader1->mRealLogPath = logPath1;
    reader1->mLastFileSignatureSize = sigSize;
    reader1->mLastFileSignatureHash = sigHash;

    std::string logPath2 = logPath + ".2";
    writeLog(logPath2, "a sample log\n");
    auto devInode2 = GetFileDevInode(logPath2);
    auto reader2 = std::make_shared<LogFileReader>(
        gRootDir, basicLogName, devInode2, std::make_pair(&readerOpts, &ctx), std::make_pair(&multilineOpts, &ctx));
    reader2->mRealLogPath = logPath2;
    reader2->mLastFileSignatureSize = sigSize;
    reader2->mLastFileSignatureHash = sigHash;

    LogFileReaderPtrArray readerPtrArray{reader2, reader1};
    handlerPtr->mNameReaderMap[logPath] = readerPtrArray;
    reader1->SetReaderArray(&handlerPtr->mNameReaderMap[logPath]);
    reader2->SetReaderArray(&handlerPtr->mNameReaderMap[logPath]);
    handlerPtr->mDevInodeReaderMap[reader1->mDevInode] = reader1;
    handlerPtr->mDevInodeReaderMap[reader2->mDevInode] = reader2;

    // build readers not in reader array
    std::string logPath3 = logPath + ".3";
    writeLog(logPath3, "a sample log\n");
    auto devInode3 = GetFileDevInode(logPath3);
    auto reader3 = std::make_shared<LogFileReader>(
        gRootDir, basicLogName, devInode3, std::make_pair(&readerOpts, &ctx), std::make_pair(&multilineOpts, &ctx));
    reader3->mRealLogPath = logPath3;
    reader3->mLastFileSignatureSize = sigSize;
    reader3->mLastFileSignatureHash = sigHash;

    std::string logPath4 = logPath + ".4";
    writeLog(logPath4, "a sample log\n");
    auto devInode4 = GetFileDevInode(logPath4);
    auto reader4 = std::make_shared<LogFileReader>(
        gRootDir, basicLogName, devInode4, std::make_pair(&readerOpts, &ctx), std::make_pair(&multilineOpts, &ctx));
    reader4->mRealLogPath = logPath4;
    reader4->mLastFileSignatureSize = sigSize;
    reader4->mLastFileSignatureHash = sigHash;

    handlerPtr->mRotatorReaderMap[reader3->mDevInode] = reader3;
    handlerPtr->mRotatorReaderMap[reader4->mDevInode] = reader4;

    handlerPtr->DumpReaderMeta(true, false);
    handlerPtr->DumpReaderMeta(false, false);
    // clear reader map
    handlerPtr.reset(new ModifyHandler(mConfigName, mConfig));
    // new reader
    handlerPtr->CreateLogFileReaderPtr(gRootDir,
                                       basicLogName,
                                       devInode,
                                       std::make_pair(&readerOpts, &ctx),
                                       std::make_pair(&multilineOpts, &ctx),
                                       std::make_pair(&discoveryOpts, &ctx),
                                       0,
                                       false);
    // recover reader from checkpoint, random order
    handlerPtr->CreateLogFileReaderPtr(gRootDir,
                                       basicLogName,
                                       devInode4,
                                       std::make_pair(&readerOpts, &ctx),
                                       std::make_pair(&multilineOpts, &ctx),
                                       std::make_pair(&discoveryOpts, &ctx),
                                       0,
                                       false);
    handlerPtr->CreateLogFileReaderPtr(gRootDir,
                                       basicLogName,
                                       devInode2,
                                       std::make_pair(&readerOpts, &ctx),
                                       std::make_pair(&multilineOpts, &ctx),
                                       std::make_pair(&discoveryOpts, &ctx),
                                       0,
                                       false);
    handlerPtr->CreateLogFileReaderPtr(gRootDir,
                                       basicLogName,
                                       devInode1,
                                       std::make_pair(&readerOpts, &ctx),
                                       std::make_pair(&multilineOpts, &ctx),
                                       std::make_pair(&discoveryOpts, &ctx),
                                       0,
                                       false);
    handlerPtr->CreateLogFileReaderPtr(gRootDir,
                                       basicLogName,
                                       devInode3,
                                       std::make_pair(&readerOpts, &ctx),
                                       std::make_pair(&multilineOpts, &ctx),
                                       std::make_pair(&discoveryOpts, &ctx),
                                       0,
                                       false);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mNameReaderMap.size(), 1);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mNameReaderMap[basicLogName].size(), 3);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mDevInodeReaderMap.size(), 3);
    auto readerArray = handlerPtr->mNameReaderMap[basicLogName];
    APSARA_TEST_EQUAL_FATAL(readerArray[0]->mDevInode.dev, devInode2.dev);
    APSARA_TEST_EQUAL_FATAL(readerArray[0]->mDevInode.inode, devInode2.inode);
    APSARA_TEST_EQUAL_FATAL(readerArray[1]->mDevInode.dev, devInode1.dev);
    APSARA_TEST_EQUAL_FATAL(readerArray[1]->mDevInode.inode, devInode1.inode);
    APSARA_TEST_EQUAL_FATAL(readerArray[2]->mDevInode.dev, devInode.dev);
    APSARA_TEST_EQUAL_FATAL(readerArray[2]->mDevInode.inode, devInode.inode);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mRotatorReaderMap.size(), 2);
}

} // end of namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
