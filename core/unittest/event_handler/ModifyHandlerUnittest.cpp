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

#include "checkpoint/CheckPointManager.h"
#include "checkpoint/CheckpointManagerV2.h"
#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "common/FileSystemUtil.h"
#include "common/Flags.h"
#include "common/JsonUtil.h"
#include "config/CollectionConfig.h"
#include "file_server/FileServer.h"
#include "file_server/event/Event.h"
#include "file_server/event_handler/EventHandler.h"
#include "file_server/reader/LogFileReader.h"
#include "unittest/Unittest.h"
#include "unittest/UnittestHelper.h"

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
    void TestRecoverReaderFromCheckpointContainer();
    void TestHandleModifyEventWhenContainerRestartCase1();
    void TestHandleModifyEventWhenContainerRestartCase2();
    void TestHandleModifyEventWhenContainerRestartCase3();
    void TestHandleModifyEventWhenContainerRestartCase4();
    void TestHandleModifyEventWhenContainerRestartCase5();
    void TestHandleModifyEventWhenContainerRestartCase6();
    void TestHandleModifyEvnetWhenContainerStopTwice();

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
        std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
        writeLog(logPath, "a sample log\n");

        // init pipeline and config
        unique_ptr<Json::Value> configJson;
        string configStr, errorMsg;
        unique_ptr<CollectionConfig> config;
        unique_ptr<CollectionPipeline> pipeline;

        std::string jsonLogPath = UnitTestHelper::JsonEscapeDirPath(logPath);
        // new pipeline
        configStr = R"(
            {
                "inputs": [
                    {
                        "Type": "input_file",
                        "FilePaths": [
                            ")"
            + jsonLogPath + R"("
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
        discoveryOpts.SetEnableContainerDiscoveryFlag(true);
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
        addContainerInfo("1");
    }
    void TearDown() override {
        filesystem::remove_all(gRootDir);
        ProcessQueueManager::GetInstance()->Clear();
    }

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

    void writeLog(const std::string& logPath, const std::string& logContent) {
        std::ofstream writer(logPath.c_str(), fstream::out | fstream::app | ios_base::binary);
        writer << logContent;
        writer.close();
    }

    void addContainerInfo(const std::string containerID) {
        std::string errorMsg;
        std::string containerStr = R"(
            {
                "ID": ")"
            + containerID + R"(",
                "Mounts": [
                    {
                        "Source": ")"
            + UnitTestHelper::JsonEscapeDirPath(gRootDir + PATH_SEPARATOR + gLogName) + R"(",
                        "Destination" : ")"
            + UnitTestHelper::JsonEscapeDirPath(gRootDir + PATH_SEPARATOR + gLogName) + R"("
                    }
                ],
                "UpperDir": ")"
            + UnitTestHelper::JsonEscapeDirPath(gRootDir) + R"(",
                "LogPath": ")"
            + UnitTestHelper::JsonEscapeDirPath(gRootDir + PATH_SEPARATOR + gLogName) + R"(",
                "MetaDatas": [
                    "_container_name_",
                    "test-container"
                ],
                "Path": ")"
            + UnitTestHelper::JsonEscapeDirPath(gRootDir + PATH_SEPARATOR + gLogName) + R"("
            }
        )";
        Json::Value containerJson;
        APSARA_TEST_TRUE_FATAL(ParseJsonTable(containerStr, containerJson, errorMsg));
        APSARA_TEST_TRUE_FATAL(discoveryOpts.UpdateContainerInfo(containerJson, &ctx));
    }

    void stopContainer(const std::string containerID) {
        for (auto& containerInfo : *(discoveryOpts.mContainerInfos)) {
            if (containerInfo.mID == containerID) {
                containerInfo.mStopped = true;
                break;
            }
        }
    }
};

std::string ModifyHandlerUnittest::gRootDir;
std::string ModifyHandlerUnittest::gLogName;

UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleContainerStoppedEventWhenReadToEnd);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleContainerStoppedEventWhenNotReadToEnd);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleModifyEventWhenContainerStopped);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestRecoverReaderFromCheckpoint);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestRecoverReaderFromCheckpointContainer);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleModifyEventWhenContainerRestartCase1);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleModifyEventWhenContainerRestartCase2);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleModifyEventWhenContainerRestartCase3);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleModifyEventWhenContainerRestartCase4);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleModifyEventWhenContainerRestartCase5);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleModifyEventWhenContainerRestartCase6);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleModifyEvnetWhenContainerStopTwice);

void ModifyHandlerUnittest::TestHandleContainerStoppedEventWhenReadToEnd() {
    LOG_INFO(sLogger, ("TestHandleContainerStoppedEventWhenReadToEnd() begin", time(NULL)));
    Event event1(gRootDir, "", EVENT_MODIFY, 0);
    LogBuffer logbuf;
    APSARA_TEST_TRUE_FATAL(!mReaderPtr->ReadLog(logbuf, &event1)); // false means no more data
    APSARA_TEST_TRUE_FATAL(mReaderPtr->mLogFileOp.IsOpen());

    // different container id, should not close reader
    Event event2(gRootDir, "", EVENT_ISDIR | EVENT_CONTAINER_STOPPED, 0);
    event2.SetContainerID("3");
    mHandlerPtr->Handle(event2);
    APSARA_TEST_TRUE_FATAL(mReaderPtr->mLogFileOp.IsOpen());

    // send event to close reader
    Event event3(gRootDir, "", EVENT_ISDIR | EVENT_CONTAINER_STOPPED, 0);
    event3.SetContainerID("1");
    mHandlerPtr->Handle(event3);
    APSARA_TEST_TRUE_FATAL(!mReaderPtr->mLogFileOp.IsOpen());
}

void ModifyHandlerUnittest::TestHandleContainerStoppedEventWhenNotReadToEnd() {
    LOG_INFO(sLogger, ("TestHandleContainerStoppedEventWhenNotReadToEnd() begin", time(NULL)));
    APSARA_TEST_TRUE_FATAL(mReaderPtr->mLogFileOp.IsOpen());

    // send event to close reader
    Event event(gRootDir, "", EVENT_ISDIR | EVENT_CONTAINER_STOPPED, 0);
    event.SetContainerID("1");
    mHandlerPtr->Handle(event);
    APSARA_TEST_TRUE_FATAL(mReaderPtr->IsContainerStopped());
    APSARA_TEST_TRUE_FATAL(mReaderPtr->mLogFileOp.IsOpen());
}

void ModifyHandlerUnittest::TestHandleModifyEventWhenContainerStopped() {
    LOG_INFO(sLogger, ("TestHandleModifyEventWhenContainerStopped() begin", time(NULL)));
    APSARA_TEST_TRUE_FATAL(mReaderPtr->mLogFileOp.IsOpen());

    // SetContainerStopped to reader
    mReaderPtr->SetContainerStopped();
    // send event to read to end
    Event event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, mReaderPtr->mDevInode.dev, mReaderPtr->mDevInode.inode);
    event.SetContainerID("1");
    mHandlerPtr->Handle(event);
    APSARA_TEST_TRUE_FATAL(mReaderPtr->IsReadToEnd());
    APSARA_TEST_TRUE_FATAL(!mReaderPtr->mLogFileOp.IsOpen());
}

void ModifyHandlerUnittest::TestRecoverReaderFromCheckpoint() {
    LOG_INFO(sLogger, ("TestRecoverReaderFromCheckpoint() begin", time(NULL)));
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
    auto reader1 = std::make_shared<LogFileReader>(gRootDir,
                                                   basicLogName,
                                                   devInode1,
                                                   std::make_pair(&readerOpts, &ctx),
                                                   std::make_pair(&multilineOpts, &ctx),
                                                   std::make_pair(&tagOpts, &ctx));
    reader1->mRealLogPath = logPath1;
    reader1->mLastFileSignatureSize = sigSize;
    reader1->mLastFileSignatureHash = sigHash;

    std::string logPath2 = logPath + ".2";
    writeLog(logPath2, "a sample log\n");
    auto devInode2 = GetFileDevInode(logPath2);
    auto reader2 = std::make_shared<LogFileReader>(gRootDir,
                                                   basicLogName,
                                                   devInode2,
                                                   std::make_pair(&readerOpts, &ctx),
                                                   std::make_pair(&multilineOpts, &ctx),
                                                   std::make_pair(&tagOpts, &ctx));
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
    auto reader3 = std::make_shared<LogFileReader>(gRootDir,
                                                   basicLogName,
                                                   devInode3,
                                                   std::make_pair(&readerOpts, &ctx),
                                                   std::make_pair(&multilineOpts, &ctx),
                                                   std::make_pair(&tagOpts, &ctx));
    reader3->mRealLogPath = logPath3;
    reader3->mLastFileSignatureSize = sigSize;
    reader3->mLastFileSignatureHash = sigHash;

    std::string logPath4 = logPath + ".4";
    writeLog(logPath4, "a sample log\n");
    auto devInode4 = GetFileDevInode(logPath4);
    auto reader4 = std::make_shared<LogFileReader>(gRootDir,
                                                   basicLogName,
                                                   devInode4,
                                                   std::make_pair(&readerOpts, &ctx),
                                                   std::make_pair(&multilineOpts, &ctx),
                                                   std::make_pair(&tagOpts, &ctx));
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
                                       std::make_pair(&tagOpts, &ctx),
                                       0,
                                       false);
    // recover reader from checkpoint, random order
    handlerPtr->CreateLogFileReaderPtr(gRootDir,
                                       basicLogName,
                                       devInode4,
                                       std::make_pair(&readerOpts, &ctx),
                                       std::make_pair(&multilineOpts, &ctx),
                                       std::make_pair(&discoveryOpts, &ctx),
                                       std::make_pair(&tagOpts, &ctx),
                                       0,
                                       false);
    handlerPtr->CreateLogFileReaderPtr(gRootDir,
                                       basicLogName,
                                       devInode2,
                                       std::make_pair(&readerOpts, &ctx),
                                       std::make_pair(&multilineOpts, &ctx),
                                       std::make_pair(&discoveryOpts, &ctx),
                                       std::make_pair(&tagOpts, &ctx),
                                       0,
                                       false);
    handlerPtr->CreateLogFileReaderPtr(gRootDir,
                                       basicLogName,
                                       devInode1,
                                       std::make_pair(&readerOpts, &ctx),
                                       std::make_pair(&multilineOpts, &ctx),
                                       std::make_pair(&discoveryOpts, &ctx),
                                       std::make_pair(&tagOpts, &ctx),
                                       0,
                                       false);
    handlerPtr->CreateLogFileReaderPtr(gRootDir,
                                       basicLogName,
                                       devInode3,
                                       std::make_pair(&readerOpts, &ctx),
                                       std::make_pair(&multilineOpts, &ctx),
                                       std::make_pair(&discoveryOpts, &ctx),
                                       std::make_pair(&tagOpts, &ctx),
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
    handlerPtr.reset(new ModifyHandler(mConfigName, mConfig));
}


void ModifyHandlerUnittest::TestRecoverReaderFromCheckpointContainer() {
    LOG_INFO(sLogger, ("TestRecoverReaderFromCheckpointContainer() begin", time(NULL)));
    std::string basicLogName = "rotate_test.log";
    std::string basicLogName1 = "rotate_test.log.1";
    std::string basicLogName2 = "rotate_test.log.2";
    std::string logPath = gRootDir + PATH_SEPARATOR + basicLogName;
    std::string logPath1 = gRootDir + PATH_SEPARATOR + basicLogName1;
    std::string logPath2 = gRootDir + PATH_SEPARATOR + basicLogName2;
    std::string signature = "a sample log\n";
    auto sigSize = (uint32_t)signature.size();
    auto sigHash = (uint64_t)HashSignatureString(signature.c_str(), (size_t)sigSize);
    // build a modify handler
    auto handlerPtr = std::make_shared<ModifyHandler>(mConfigName, mConfig);
    writeLog(logPath, signature);
    writeLog(logPath1, signature);
    writeLog(logPath2, signature);
    auto devInode = GetFileDevInode(logPath);
    auto devInode1 = GetFileDevInode(logPath1);
    auto devInode2 = GetFileDevInode(logPath2);

    addContainerInfo("1");
    CheckPoint* checkPointPtr
        = new CheckPoint(logPath, 13, sigSize, sigHash, devInode, mConfigName, logPath, false, true, "1", false);
    // use last event time as checkpoint's last update time
    checkPointPtr->mLastUpdateTime = time(NULL);
    checkPointPtr->mCache = "";
    checkPointPtr->mIdxInReaderArray = 0;
    CheckPointManager::Instance()->AddCheckPoint(checkPointPtr);

    // not set container stopped for rotator reader
    CheckPoint* checkPointPtr1
        = new CheckPoint(logPath, 13, sigSize, sigHash, devInode1, mConfigName, logPath1, false, false, "1", false);
    checkPointPtr1->mLastUpdateTime = time(NULL);
    checkPointPtr1->mCache = "";
    checkPointPtr1->mIdxInReaderArray = -2;
    CheckPointManager::Instance()->AddCheckPoint(checkPointPtr1);


    // set container stopped for rotator reader
    CheckPoint* checkPointPtr2
        = new CheckPoint(logPath, 13, sigSize, sigHash, devInode2, mConfigName, logPath2, false, true, "1", false);
    checkPointPtr2->mLastUpdateTime = time(NULL);
    checkPointPtr2->mCache = "";
    checkPointPtr2->mIdxInReaderArray = -2;
    CheckPointManager::Instance()->AddCheckPoint(checkPointPtr2);

    Event event(gRootDir, basicLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    event.SetConfigName(mConfigName);
    handlerPtr->Handle(event);

    Event event1(gRootDir, basicLogName, EVENT_MODIFY, 0, 0, devInode1.dev, devInode1.inode);
    event1.SetConfigName(mConfigName);
    handlerPtr->Handle(event1);

    Event event2(gRootDir, basicLogName, EVENT_MODIFY, 0, 0, devInode2.dev, devInode2.inode);
    event2.SetConfigName(mConfigName);
    handlerPtr->Handle(event2);

    APSARA_TEST_EQUAL_FATAL(handlerPtr->mNameReaderMap[basicLogName].size(), 1);
    APSARA_TEST_TRUE_FATAL(handlerPtr->mNameReaderMap[basicLogName][0]->mLogFileOp.IsOpen() == false);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mRotatorReaderMap.size(), 2);
    APSARA_TEST_TRUE_FATAL(handlerPtr->mRotatorReaderMap[devInode1]->mLogFileOp.IsOpen() == true);
    APSARA_TEST_TRUE_FATAL(handlerPtr->mRotatorReaderMap[devInode2]->mLogFileOp.IsOpen() == false);


    Event event3(gRootDir, "", EVENT_CONTAINER_STOPPED, 0);
    event3.SetContainerID("1");
    handlerPtr->Handle(event3);

    APSARA_TEST_TRUE_FATAL(handlerPtr->mNameReaderMap[basicLogName][0]->IsContainerStopped());
    APSARA_TEST_TRUE_FATAL(handlerPtr->mRotatorReaderMap[devInode1]->IsContainerStopped());
    APSARA_TEST_TRUE_FATAL(handlerPtr->mRotatorReaderMap[devInode2]->IsContainerStopped());

    LOG_INFO(sLogger, ("TestRecoverReaderFromCheckpointContainer() end", time(NULL)));
}

void ModifyHandlerUnittest::TestHandleModifyEventWhenContainerRestartCase1() {
    // stop -> start -> modify
    // stop
    mReaderPtr->SetContainerStopped();
    // start
    addContainerInfo("2");
    // modify
    Event event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, mReaderPtr->mDevInode.dev, mReaderPtr->mDevInode.inode);
    mHandlerPtr->Handle(event);
    APSARA_TEST_TRUE_FATAL(!mReaderPtr->IsContainerStopped());
    APSARA_TEST_TRUE_FATAL(mReaderPtr->mLogFileOp.IsOpen());
    APSARA_TEST_EQUAL_FATAL(mReaderPtr->mContainerID, "2");
}

void ModifyHandlerUnittest::TestHandleModifyEventWhenContainerRestartCase2() {
    // stop -> modify -> start
    // stop
    mReaderPtr->SetContainerStopped();
    // modify
    Event event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, mReaderPtr->mDevInode.dev, mReaderPtr->mDevInode.inode);
    mHandlerPtr->Handle(event);
    APSARA_TEST_TRUE_FATAL(mReaderPtr->IsContainerStopped());
    APSARA_TEST_TRUE_FATAL(!mReaderPtr->mLogFileOp.IsOpen());
    APSARA_TEST_EQUAL_FATAL(mReaderPtr->mContainerID, "1");
    // start
    addContainerInfo("2");

    Event event2(gRootDir, gLogName, EVENT_MODIFY, 0, 0, mReaderPtr->mDevInode.dev, mReaderPtr->mDevInode.inode);
    mHandlerPtr->Handle(event2);
    APSARA_TEST_TRUE_FATAL(!mReaderPtr->IsContainerStopped());
    APSARA_TEST_TRUE_FATAL(mReaderPtr->mLogFileOp.IsOpen());
    APSARA_TEST_EQUAL_FATAL(mReaderPtr->mContainerID, "2");
}

void ModifyHandlerUnittest::TestHandleModifyEventWhenContainerRestartCase3() {
    // start -> modify -> stop
    // start
    addContainerInfo("2");
    // modify
    Event event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, mReaderPtr->mDevInode.dev, mReaderPtr->mDevInode.inode);
    mHandlerPtr->Handle(event);
    APSARA_TEST_TRUE_FATAL(!mReaderPtr->IsContainerStopped());
    APSARA_TEST_TRUE_FATAL(mReaderPtr->mLogFileOp.IsOpen());
    APSARA_TEST_EQUAL_FATAL(mReaderPtr->mContainerID, "1");
    // stop
    mReaderPtr->SetContainerStopped();

    Event event2(gRootDir, gLogName, EVENT_MODIFY, 0, 0, mReaderPtr->mDevInode.dev, mReaderPtr->mDevInode.inode);
    mHandlerPtr->Handle(event2);
    APSARA_TEST_TRUE_FATAL(!mReaderPtr->IsContainerStopped());
    APSARA_TEST_TRUE_FATAL(mReaderPtr->mLogFileOp.IsOpen());
    APSARA_TEST_EQUAL_FATAL(mReaderPtr->mContainerID, "2");
}

void ModifyHandlerUnittest::TestHandleModifyEventWhenContainerRestartCase4() {
    // start -> stop -> modify
    // start
    addContainerInfo("2");
    // stop
    mReaderPtr->SetContainerStopped();
    // modify
    Event event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, mReaderPtr->mDevInode.dev, mReaderPtr->mDevInode.inode);
    mHandlerPtr->Handle(event);
    APSARA_TEST_TRUE_FATAL(!mReaderPtr->IsContainerStopped());
    APSARA_TEST_TRUE_FATAL(mReaderPtr->mLogFileOp.IsOpen());
    APSARA_TEST_EQUAL_FATAL(mReaderPtr->mContainerID, "2");
}

void ModifyHandlerUnittest::TestHandleModifyEventWhenContainerRestartCase5() {
    // modify -> stop -> start
    // modify
    Event event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, mReaderPtr->mDevInode.dev, mReaderPtr->mDevInode.inode);
    mHandlerPtr->Handle(event);
    // stop
    mReaderPtr->SetContainerStopped();
    // start
    addContainerInfo("2");

    Event event2(gRootDir, gLogName, EVENT_MODIFY, 0, 0, mReaderPtr->mDevInode.dev, mReaderPtr->mDevInode.inode);
    mHandlerPtr->Handle(event2);
    APSARA_TEST_TRUE_FATAL(!mReaderPtr->IsContainerStopped());
    APSARA_TEST_TRUE_FATAL(mReaderPtr->mLogFileOp.IsOpen());
    APSARA_TEST_EQUAL_FATAL(mReaderPtr->mContainerID, "2");
}

void ModifyHandlerUnittest::TestHandleModifyEventWhenContainerRestartCase6() {
    // modify -> start -> stop
    // modify
    Event event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, mReaderPtr->mDevInode.dev, mReaderPtr->mDevInode.inode);
    mHandlerPtr->Handle(event);
    // start
    addContainerInfo("2");
    // stop
    mReaderPtr->SetContainerStopped();

    Event event2(gRootDir, gLogName, EVENT_MODIFY, 0, 0, mReaderPtr->mDevInode.dev, mReaderPtr->mDevInode.inode);
    mHandlerPtr->Handle(event2);
    APSARA_TEST_TRUE_FATAL(!mReaderPtr->IsContainerStopped());
    APSARA_TEST_TRUE_FATAL(mReaderPtr->mLogFileOp.IsOpen());
    APSARA_TEST_EQUAL_FATAL(mReaderPtr->mContainerID, "2");
}

void ModifyHandlerUnittest::TestHandleModifyEvnetWhenContainerStopTwice() {
    addContainerInfo("2");
    stopContainer("1");
    stopContainer("2");
    mReaderPtr->SetContainerStopped();

    Event event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, mReaderPtr->mDevInode.dev, mReaderPtr->mDevInode.inode);
    mHandlerPtr->Handle(event);
    APSARA_TEST_TRUE_FATAL(mReaderPtr->IsContainerStopped());
    APSARA_TEST_TRUE_FATAL(!mReaderPtr->mLogFileOp.IsOpen());
    APSARA_TEST_EQUAL_FATAL(mReaderPtr->mContainerID, "2");
}

} // end of namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
