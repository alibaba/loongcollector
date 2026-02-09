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
#include <utility>

#include "checkpoint/CheckPointManager.h"
#include "checkpoint/CheckpointManagerV2.h"
#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "common/FileSystemUtil.h"
#include "common/Flags.h"
#include "common/JsonUtil.h"
#include "config/CollectionConfig.h"
#include "constants/TagConstants.h"
#include "file_server/ContainerInfo.h"
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
    void TestHandleBasicCreateEvent();
    void TestHandleBasicDeleteEvent();
    void TestHandleBasicModifyEvent();
    void TestDoublePopFrontBugWhenFileDeletedWithMultipleReaders();
    void TestFileDeletedWithTwoReaders_FileReallyDeleted();
    void TestFileDeletedWithTwoReaders_FileNotDeleted();
    void TestFileDeletedWithSingleReader_FileReallyDeleted();
    void TestFileDeletedWithSingleReader_FileNotDeleted();
    void TestContainerStoppedWithSingleReader_FileReallyDeleted();
    void TestContainerStoppedWithSingleReader_FileNotDeleted();
    void TestHandleContainerStoppedEventWhenReadToEnd();
    void TestHandleContainerStoppedEventWhenNotReadToEnd();
    void TestHandleModifyEventWhenContainerStopped();
    void TestRecoverReaderFromCheckpoint();
    void TestRecoverReaderFromCheckpointRotateLog();
    void TestRecoverReaderFromCheckpointContainer();
    void TestHandleModifyEventWhenContainerRestartCase1();
    void TestHandleModifyEventWhenContainerRestartCase2();
    void TestHandleModifyEventWhenContainerRestartCase3();
    void TestHandleModifyEventWhenContainerRestartCase4();
    void TestHandleModifyEventWhenContainerRestartCase5();
    void TestHandleModifyEventWhenContainerRestartCase6();
    void TestHandleModifyEvnetWhenContainerStopTwice();
    void TestClearReaderWhenFileDeleted();

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

        config.reset(new CollectionConfig(mConfigName, std::move(configJson), "/fake/path"));
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
                containerInfo.mRealBaseDirs.push_back(containerInfo.mRawContainerInfo->mUpperDir);
                return true;
            });
        mConfig = std::make_pair(&discoveryOpts, &ctx);
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;

        FileServer::GetInstance()->AddFileDiscoveryConfig(mConfigName, &discoveryOpts, &ctx);
        FileServer::GetInstance()->AddFileReaderConfig(mConfigName, &readerOpts, &ctx);
        FileServer::GetInstance()->AddMultilineConfig(mConfigName, &multilineOpts, &ctx);
        ProcessQueueManager::GetInstance()->CreateOrUpdateCountBoundedQueue(0, 0, ctx);

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
        // Create RawContainerInfo object directly
        auto rawContainerInfo = std::make_shared<RawContainerInfo>();
        rawContainerInfo->mID = containerID;
        rawContainerInfo->mUpperDir = gRootDir;
        rawContainerInfo->mLogPath = gRootDir + PATH_SEPARATOR + gLogName;

        // Add mount information
        Mount mount;
        mount.mSource = gRootDir + PATH_SEPARATOR + gLogName;
        mount.mDestination = gRootDir + PATH_SEPARATOR + gLogName;
        rawContainerInfo->mMounts.push_back(mount);

        // Add metadata
        rawContainerInfo->mMetadatas.push_back(std::make_pair(TagKey::CONTAINER_NAME_TAG_KEY, "test-container"));

        // Set K8s info
        rawContainerInfo->mK8sInfo.mContainerName = "test-container";

        APSARA_TEST_TRUE_FATAL(discoveryOpts.UpdateRawContainerInfo(rawContainerInfo, &ctx));
    }

    void stopContainer(const std::string containerID) {
        for (auto& containerInfo : *(discoveryOpts.mContainerInfos)) {
            if (containerInfo.mRawContainerInfo->mID == containerID) {
                containerInfo.mRawContainerInfo->mStopped = true;
                break;
            }
        }
    }

    // Helper function to verify the invariant relationships between three maps:
    // 1. Sum of all values' sizes in mNameReaderMap == size of mDevInodeReaderMap
    // 2. mDevInodeReaderMap and mRotatorReaderMap have no intersection (disjoint sets)
    void VerifyMapInvariants(const std::shared_ptr<ModifyHandler>& handlerPtr,
                             size_t expectedActiveReaderSize,
                             size_t expectedRotatorMapSize,
                             const std::string& testContext = "") {
        // Calculate actual sizes
        size_t actualNameReaderMapTotalSize = 0;
        for (const auto& pair : handlerPtr->mNameReaderMap) {
            actualNameReaderMapTotalSize += pair.second.size();
        }
        size_t actualDevInodeMapSize = handlerPtr->mDevInodeReaderMap.size();
        size_t actualRotatorMapSize = handlerPtr->mRotatorReaderMap.size();

        // Verify expected values
        APSARA_TEST_EQUAL_FATAL(actualNameReaderMapTotalSize, expectedActiveReaderSize);
        APSARA_TEST_EQUAL_FATAL(actualDevInodeMapSize, expectedActiveReaderSize);
        APSARA_TEST_EQUAL_FATAL(actualRotatorMapSize, expectedRotatorMapSize);

        // Invariant 1: sum of reader arrays should equal devInodeMap size
        APSARA_TEST_EQUAL_FATAL(actualNameReaderMapTotalSize, actualDevInodeMapSize);

        // Invariant 2: mDevInodeReaderMap and mRotatorReaderMap should have no intersection
        for (const auto& devInodePair : handlerPtr->mDevInodeReaderMap) {
            APSARA_TEST_EQUAL_FATAL(handlerPtr->mRotatorReaderMap.count(devInodePair.first), 0);
        }

        LOG_INFO(sLogger,
                 ("Map invariants verified", testContext)("nameReaderMap total size", actualNameReaderMapTotalSize)(
                     "devInodeMap size", actualDevInodeMapSize)("rotatorMap size", actualRotatorMapSize)(
                     "invariant 1", "nameReaderMap total == devInodeMap size: PASS")(
                     "invariant 2", "devInodeMap ∩ rotatorMap = ∅: PASS"));
    }
};

std::string ModifyHandlerUnittest::gRootDir;
std::string ModifyHandlerUnittest::gLogName;

UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleBasicCreateEvent);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleBasicModifyEvent);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestFileDeletedWithTwoReaders_FileNotDeleted);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestFileDeletedWithSingleReader_FileNotDeleted);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestContainerStoppedWithSingleReader_FileNotDeleted);
#ifndef _MSC_VER // Windows cannot delete files while they are open
UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleBasicDeleteEvent);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestDoublePopFrontBugWhenFileDeletedWithMultipleReaders);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestFileDeletedWithTwoReaders_FileReallyDeleted);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestFileDeletedWithSingleReader_FileReallyDeleted);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestContainerStoppedWithSingleReader_FileReallyDeleted);
#endif
UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleContainerStoppedEventWhenReadToEnd);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleContainerStoppedEventWhenNotReadToEnd);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleModifyEventWhenContainerStopped);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestRecoverReaderFromCheckpoint);
#ifndef _MSC_VER // Unnecessary on platforms without symbolic.
UNIT_TEST_CASE(ModifyHandlerUnittest, TestRecoverReaderFromCheckpointRotateLog);
#endif
UNIT_TEST_CASE(ModifyHandlerUnittest, TestRecoverReaderFromCheckpointContainer);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleModifyEventWhenContainerRestartCase1);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleModifyEventWhenContainerRestartCase2);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleModifyEventWhenContainerRestartCase3);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleModifyEventWhenContainerRestartCase4);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleModifyEventWhenContainerRestartCase5);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleModifyEventWhenContainerRestartCase6);
UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleModifyEvnetWhenContainerStopTwice);
#ifndef _MSC_VER
UNIT_TEST_CASE(ModifyHandlerUnittest, TestClearReaderWhenFileDeleted);
#endif

void ModifyHandlerUnittest::TestHandleBasicCreateEvent() {
    LOG_INFO(sLogger, ("TestHandleBasicCreateEvent() begin", time(NULL)));

    // Create a new file with matching pattern (test.log is the configured pattern)
    std::string newLogName = "test2.log";
    std::string newLogPath = gRootDir + PATH_SEPARATOR + newLogName;
    writeLog(newLogPath, "new log content\n");

    // Get dev inode for the new file
    DevInode newDevInode = GetFileDevInode(newLogPath);
    APSARA_TEST_TRUE_FATAL(newDevInode.IsValid());

    // Verify reader doesn't exist before create event
    APSARA_TEST_EQUAL_FATAL(mHandlerPtr->mDevInodeReaderMap.count(newDevInode), 0);

    // Send CREATE event with ConfigName set to bypass IsMatch check
    Event createEvent(gRootDir, newLogName, EVENT_CREATE, 0, 0, newDevInode.dev, newDevInode.inode);
    createEvent.SetConfigName(mConfigName);
    mHandlerPtr->Handle(createEvent);

    // Verify new reader is created and added to maps
    APSARA_TEST_EQUAL_FATAL(mHandlerPtr->mDevInodeReaderMap.count(newDevInode), 1);
    APSARA_TEST_EQUAL_FATAL(mHandlerPtr->mNameReaderMap[newLogName].size(), 1);

    // Verify map invariants: 2 active readers (original + new), 0 rotator
    VerifyMapInvariants(mHandlerPtr, 2, 0, "After CREATE event");

    // Verify the created reader has correct properties
    auto newReaderPtr = mHandlerPtr->mDevInodeReaderMap[newDevInode];
    APSARA_TEST_EQUAL_FATAL(newReaderPtr->GetHostLogPathFile(), newLogName);
    APSARA_TEST_TRUE_FATAL(newReaderPtr->GetDevInode().dev == newDevInode.dev);
    APSARA_TEST_TRUE_FATAL(newReaderPtr->GetDevInode().inode == newDevInode.inode);

    LOG_INFO(sLogger, ("TestHandleBasicCreateEvent() end", time(NULL)));
}

void ModifyHandlerUnittest::TestHandleBasicDeleteEvent() {
    LOG_INFO(sLogger, ("TestHandleBasicDeleteEvent() begin", time(NULL)));

    // Read log to end first
    Event modifyEvent(gRootDir, "", EVENT_MODIFY, 0);
    LogBuffer logbuf;
    APSARA_TEST_TRUE_FATAL(!mReaderPtr->ReadLog(logbuf, &modifyEvent));
    APSARA_TEST_TRUE_FATAL(mReaderPtr->IsReadToEnd());
    APSARA_TEST_TRUE_FATAL(mReaderPtr->mLogFileOp.IsOpen());

    // Verify reader exists before delete event
    APSARA_TEST_EQUAL_FATAL(mHandlerPtr->mDevInodeReaderMap.size(), 1);
    APSARA_TEST_EQUAL_FATAL(mHandlerPtr->mNameReaderMap[gLogName].size(), 1);

    // Actually delete the file
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);

    // Send DELETE event
    Event deleteEvent(gRootDir, gLogName, EVENT_DELETE, 0);
    mHandlerPtr->Handle(deleteEvent);

    // Verify file deleted flag is set
    APSARA_TEST_TRUE_FATAL(mReaderPtr->IsFileDeleted());

    // Verify file descriptor is closed
    APSARA_TEST_TRUE_FATAL(!mReaderPtr->mLogFileOp.IsOpen());

    // Verify reader is removed from maps when file is really deleted
    APSARA_TEST_EQUAL_FATAL(mHandlerPtr->mDevInodeReaderMap.size(), 0);
    APSARA_TEST_EQUAL_FATAL(mHandlerPtr->mNameReaderMap[gLogName].size(), 0);

    // Verify map invariants: 0 active readers, 0 rotator
    VerifyMapInvariants(mHandlerPtr, 0, 0, "After DELETE event");

    LOG_INFO(sLogger, ("TestHandleBasicDeleteEvent() end", time(NULL)));
}

void ModifyHandlerUnittest::TestHandleBasicModifyEvent() {
    LOG_INFO(sLogger, ("TestHandleBasicModifyEvent() begin", time(NULL)));

    // Write additional data to the log file
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    writeLog(logPath, "additional log line\n");

    // Record initial file position
    int64_t initialPos = mReaderPtr->GetLastFilePos();

    // Close file first if it's open (from SetUp)
    if (mReaderPtr->mLogFileOp.IsOpen()) {
        mReaderPtr->CloseFilePtr();
    }

    // Verify file is closed before handling modify event
    APSARA_TEST_TRUE_FATAL(!mReaderPtr->mLogFileOp.IsOpen());

    // Send MODIFY event
    Event modifyEvent(gRootDir, gLogName, EVENT_MODIFY, 0, 0, mReaderPtr->mDevInode.dev, mReaderPtr->mDevInode.inode);
    mHandlerPtr->Handle(modifyEvent);

    // Verify file was opened and read (file position advanced)
    APSARA_TEST_TRUE_FATAL(mReaderPtr->GetLastFilePos() > initialPos);

    // Verify reader still exists in maps
    APSARA_TEST_EQUAL_FATAL(mHandlerPtr->mDevInodeReaderMap.size(), 1);
    APSARA_TEST_EQUAL_FATAL(mHandlerPtr->mNameReaderMap[gLogName].size(), 1);
    APSARA_TEST_EQUAL_FATAL(mHandlerPtr->mDevInodeReaderMap[mReaderPtr->mDevInode], mReaderPtr);

    // Verify map invariants: 1 active reader, 0 rotator
    VerifyMapInvariants(mHandlerPtr, 1, 0, "After MODIFY event");

    LOG_INFO(sLogger, ("TestHandleBasicModifyEvent() end", time(NULL)));
}


// Test case for double pop front bug when file deleted with multiple readers
// reader array size is 3, and the first reader is marked as deleted
// when processing MODIFY event, ReadLog() returns hasMoreData=false (read to end)
// CloseFilePtr() returns isDeleted=true (file path contains " (deleted)")
void ModifyHandlerUnittest::TestDoublePopFrontBugWhenFileDeletedWithMultipleReaders() {
    LOG_INFO(sLogger, ("TestDoublePopFrontBugWhenFileDeletedWithMultipleReaders() begin", time(NULL)));

    // This test reproduces a REAL bug where pop_front() is called twice:
    // 1. First time at line 850-852 when IsFileDeleted() && isDeleted
    // 2. Second time at line 931-932 because !hasMoreData && readerArray.size() > 1
    //
    // Scenario:
    // - We have 3 readers in the queue (simulating log rotation)
    // - The first reader (oldest rotated file) is marked as deleted
    // - When processing MODIFY event, ReadLog() returns hasMoreData=false (read to end)
    // - CloseFilePtr() returns isDeleted=true (file path contains " (deleted)")
    // - This triggers the first pop_front() at line 851
    // - After break, the condition at line 919 is still true (size is now 2 > 1)
    // - This triggers the second pop_front() at line 931
    // - Result: TWO readers are removed instead of ONE
    //
    // We simulate the " (deleted)" suffix by manually setting mRealLogPath,
    // which mimics Linux's behavior when a file is deleted while fd is still open.

    std::string signature = "a sample log\n";
    auto sigSize = (uint32_t)signature.size();
    auto sigHash = (uint64_t)HashSignatureString(signature.c_str(), (size_t)sigSize);

    // Create 3 rotated log files
    std::string logPath1 = gRootDir + PATH_SEPARATOR + gLogName + ".2"; // oldest
    std::string logPath2 = gRootDir + PATH_SEPARATOR + gLogName + ".1";
    std::string logPath3 = gRootDir + PATH_SEPARATOR + gLogName; // newest

    writeLog(logPath1, signature);
    writeLog(logPath2, signature);
    writeLog(logPath3, signature);

    auto devInode1 = GetFileDevInode(logPath1);
    auto devInode2 = GetFileDevInode(logPath2);
    auto devInode3 = GetFileDevInode(logPath3);

    // Create 3 readers with proper file names
    // Important: Use actual file names (test.log.2, test.log.1, test.log) not just gLogName
    auto reader1 = std::make_shared<LogFileReader>(gRootDir,
                                                   gLogName + ".2", // Actual rotated file name
                                                   devInode1,
                                                   std::make_pair(&readerOpts, &ctx),
                                                   std::make_pair(&multilineOpts, &ctx),
                                                   std::make_pair(&tagOpts, &ctx));
    reader1->mLastFileSignatureSize = sigSize;
    reader1->mLastFileSignatureHash = sigHash;
    reader1->mLastFilePos = signature.size(); // Already at end
    reader1->UpdateReaderManual();

    LOG_INFO(sLogger,
             ("Created reader1", "path info")("mHostLogPath", reader1->GetHostLogPath())(
                 "mRealLogPath", reader1->mRealLogPath)("expected", logPath1));

    auto reader2 = std::make_shared<LogFileReader>(gRootDir,
                                                   gLogName + ".1", // Actual rotated file name
                                                   devInode2,
                                                   std::make_pair(&readerOpts, &ctx),
                                                   std::make_pair(&multilineOpts, &ctx),
                                                   std::make_pair(&tagOpts, &ctx));
    reader2->mLastFileSignatureSize = sigSize;
    reader2->mLastFileSignatureHash = sigHash;
    reader2->mLastFilePos = signature.size(); // Already at end
    reader2->UpdateReaderManual();

    LOG_INFO(sLogger,
             ("Created reader2", "path info")("mHostLogPath", reader2->GetHostLogPath())(
                 "mRealLogPath", reader2->mRealLogPath)("expected", logPath2));

    auto reader3 = std::make_shared<LogFileReader>(gRootDir,
                                                   gLogName, // Current active log file
                                                   devInode3,
                                                   std::make_pair(&readerOpts, &ctx),
                                                   std::make_pair(&multilineOpts, &ctx),
                                                   std::make_pair(&tagOpts, &ctx));
    reader3->mLastFileSignatureSize = sigSize;
    reader3->mLastFileSignatureHash = sigHash;
    reader3->mLastFilePos = signature.size(); // Already at end
    reader3->UpdateReaderManual();

    LOG_INFO(sLogger,
             ("Created reader3", "path info")("mHostLogPath", reader3->GetHostLogPath())(
                 "mRealLogPath", reader3->mRealLogPath)("expected", logPath3));

    // Build handler with 3 readers
    auto handlerPtr = std::make_shared<ModifyHandler>(mConfigName, mConfig);

    // For rotated logs, all readers should use the same name in mNameReaderMap (base log name)
    // This simulates the real scenario where test.log.2, test.log.1, test.log all belong to same log file family
    LogFileReaderPtrArray readerArray{reader1, reader2, reader3};
    handlerPtr->mNameReaderMap[gLogName] = readerArray;
    reader1->SetReaderArray(&handlerPtr->mNameReaderMap[gLogName]);
    reader2->SetReaderArray(&handlerPtr->mNameReaderMap[gLogName]);
    reader3->SetReaderArray(&handlerPtr->mNameReaderMap[gLogName]);

    // Update mHostLogPath for rotated files to point to the base log name
    // In real scenarios, mHostLogPath is the original pattern (test.log)
    // while mRealLogPath is the actual file (test.log.2)
    reader1->mHostLogPath = gRootDir + PATH_SEPARATOR + gLogName;
    reader2->mHostLogPath = gRootDir + PATH_SEPARATOR + gLogName;
    reader3->mHostLogPath = gRootDir + PATH_SEPARATOR + gLogName;

    handlerPtr->mDevInodeReaderMap[devInode1] = reader1;
    handlerPtr->mDevInodeReaderMap[devInode2] = reader2;
    handlerPtr->mDevInodeReaderMap[devInode3] = reader3;

    LOG_INFO(sLogger,
             ("Setup complete", "reader paths")("reader1 host", reader1->GetHostLogPath())("reader1 real",
                                                                                           reader1->mRealLogPath)(
                 "reader2 host", reader2->GetHostLogPath())("reader2 real", reader2->mRealLogPath)(
                 "reader3 host", reader3->GetHostLogPath())("reader3 real", reader3->mRealLogPath));

    // Verify initial state
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mNameReaderMap[gLogName].size(), 3);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mDevInodeReaderMap.size(), 3);

    // Mark reader1 as deleted (simulating DELETE event received earlier)
    reader1->SetFileDeleted(true);

    // Open reader1's file, then delete it while keeping fd open
    APSARA_TEST_TRUE_FATAL(reader1->UpdateFilePtr());
    APSARA_TEST_TRUE_FATAL(reader1->IsFileOpened());

    // Delete the physical file
    // In production, Linux marks the fd path with " (deleted)" suffix
    // In test environment, this may not be reliably detected
    bfs::remove(logPath1);

    // Check if file still exists on filesystem
    bool fileExists = bfs::exists(logPath1);
    LOG_INFO(sLogger,
             ("File status", "after deletion")("file exists", fileExists)("fd is open", reader1->IsFileOpened()));

    LOG_INFO(sLogger,
             ("Test setup complete", "sending MODIFY event")("reader1 deleted flag", reader1->IsFileDeleted())(
                 "reader1 file opened", reader1->IsFileOpened())("reader array size",
                                                                 handlerPtr->mNameReaderMap[gLogName].size()));

    // Send MODIFY event to reader1
    Event modifyEvent(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode1.dev, devInode1.inode);
    handlerPtr->Handle(modifyEvent);

    LOG_INFO(sLogger,
             ("After Handle()", "checking results")("reader array size", handlerPtr->mNameReaderMap[gLogName].size())(
                 "devInodeMap size", handlerPtr->mDevInodeReaderMap.size())("rotatorMap size",
                                                                            handlerPtr->mRotatorReaderMap.size()));

    // Expected behavior (OLD buggy code before fix):
    // 1. hasMoreData = false (already at end)
    // 2. IsFileDeleted() is true (we set it)
    // 3. CloseFilePtr() returns isDeleted = true (file was deleted from filesystem)
    // 4. Enter first if-branch at line 850: pop_front() is called - removes reader1
    // 5. Break from switch statement
    // 6. Continue to line 919: readerArrayPtr->size() > 1 is still true (size is now 2)
    // 7. Enter second if-branch at line 931: pop_front() is called AGAIN - removes reader2
    // 8. Result: TWO readers removed instead of ONE (the double pop_front bug)
    //
    // Expected behavior (NEW fixed code after refactoring):
    // 1. hasMoreData = false (already at end)
    // 2. Check readerArrayPtr->size() > 1 first (size is 3, condition is true)
    // 3. Enter the size > 1 branch first (higher priority)
    // 4. ForceReadLogAndPush called
    // 5. RemoveReaderFromArrayAndMap called - reader1 removed from array and map
    // 6. CloseFilePtr called - detects file was deleted
    // 7. isFileReallyDeleted = true (file physically deleted from filesystem)
    // 8. reader1 NOT added to mRotatorReaderMap (because file really deleted)
    // 9. The IsFileDeleted branch is NOT entered (because we break after size > 1 branch)
    // Result: ONLY reader1 removed, reader2 and reader3 remain (bug fixed)

    // After handling, we should have:
    // - Active readers: 2 (reader2 and reader3 remain, reader1 removed)
    // - RotatorMap size: 0 (reader1 not added because file was really deleted)

    // Verify map invariants and expected sizes
    VerifyMapInvariants(handlerPtr, 2, 0, "After handling MODIFY event");

    // Verify specific reader states
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mNameReaderMap[gLogName].size(), 2);

    // Reader1 should be completely removed from both devInodeMap and rotatorMap
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mDevInodeReaderMap.count(devInode1), 0);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mRotatorReaderMap.count(devInode1), 0);

    // Reader2 and Reader3 should still exist in both array and devInodeMap
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mDevInodeReaderMap.count(devInode2), 1);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mDevInodeReaderMap.count(devInode3), 1);

    // Verify the remaining readers in the array are reader2 and reader3
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mNameReaderMap[gLogName][0]->GetDevInode().inode, devInode2.inode);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mNameReaderMap[gLogName][1]->GetDevInode().inode, devInode3.inode);

    // In this test scenario, the file was physically deleted via bfs::remove()
    // while the file descriptor was still open. With the refactored code, the
    // size > 1 condition is checked first, preventing the double pop_front bug.
    // CloseFilePtr should detect the deletion and return isFileReallyDeleted = true.
    // Therefore, reader1 should NOT be added to mRotatorReaderMap.
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mRotatorReaderMap.size(), 0);

    LOG_INFO(sLogger,
             ("Test result", "success")("behavior", "size > 1 branch executed first, preventing double pop_front bug")(
                 "array size", handlerPtr->mNameReaderMap[gLogName].size())(
                 "devInodeMap size", handlerPtr->mDevInodeReaderMap.size())("rotator size",
                                                                            handlerPtr->mRotatorReaderMap.size())(
                 "note", "Only reader1 removed (file really deleted), reader2 and reader3 remain - bug fixed"));

    LOG_INFO(sLogger,
             ("TestDoublePopFrontBugWhenFileDeletedWithMultipleReaders() end",
              "Test passed - verifies bug fix with refactored code"));
}

// Test case for file deleted with queue length = 2 - File Really Deleted scenario
// This tests the new refactored code path where size > 1 is checked first
void ModifyHandlerUnittest::TestFileDeletedWithTwoReaders_FileReallyDeleted() {
    LOG_INFO(sLogger, ("TestFileDeletedWithTwoReaders_FileReallyDeleted() begin", time(NULL)));

    // Test scenario:
    // - Queue has 2 readers (simulating one rotated file + current file)
    // - The first reader (oldest rotated file) is marked as deleted
    // - File is physically deleted from filesystem
    // - When processing MODIFY event, ReadLog() returns hasMoreData=false (read to end)
    // - The new code should:
    //   1. Check size > 1 first (condition is true)
    //   2. Call ForceReadLogAndPush
    //   3. Call RemoveReaderFromArrayAndMap
    //   4. Call CloseFilePtr - detects file really deleted
    //   5. reader1 NOT added to rotator map (because file really deleted)

    std::string signature = "a sample log\n";
    auto sigSize = (uint32_t)signature.size();
    auto sigHash = (uint64_t)HashSignatureString(signature.c_str(), (size_t)sigSize);

    // Create 2 log files (rotated scenario)
    std::string logPath1 = gRootDir + PATH_SEPARATOR + gLogName + ".1"; // rotated file
    std::string logPath2 = gRootDir + PATH_SEPARATOR + gLogName; // current file

    writeLog(logPath1, signature);
    writeLog(logPath2, signature);

    auto devInode1 = GetFileDevInode(logPath1);
    auto devInode2 = GetFileDevInode(logPath2);

    // Create 2 readers
    auto reader1 = std::make_shared<LogFileReader>(gRootDir,
                                                   gLogName + ".1",
                                                   devInode1,
                                                   std::make_pair(&readerOpts, &ctx),
                                                   std::make_pair(&multilineOpts, &ctx),
                                                   std::make_pair(&tagOpts, &ctx));
    reader1->mLastFileSignatureSize = sigSize;
    reader1->mLastFileSignatureHash = sigHash;
    reader1->mLastFilePos = signature.size(); // Already at end
    reader1->UpdateReaderManual();

    LOG_INFO(sLogger,
             ("Created reader1", "path info")("mHostLogPath", reader1->GetHostLogPath())(
                 "mRealLogPath", reader1->mRealLogPath)("expected", logPath1));

    auto reader2 = std::make_shared<LogFileReader>(gRootDir,
                                                   gLogName,
                                                   devInode2,
                                                   std::make_pair(&readerOpts, &ctx),
                                                   std::make_pair(&multilineOpts, &ctx),
                                                   std::make_pair(&tagOpts, &ctx));
    reader2->mLastFileSignatureSize = sigSize;
    reader2->mLastFileSignatureHash = sigHash;
    reader2->mLastFilePos = signature.size(); // Already at end
    reader2->UpdateReaderManual();

    LOG_INFO(sLogger,
             ("Created reader2", "path info")("mHostLogPath", reader2->GetHostLogPath())(
                 "mRealLogPath", reader2->mRealLogPath)("expected", logPath2));

    // Build handler with 2 readers
    auto handlerPtr = std::make_shared<ModifyHandler>(mConfigName, mConfig);

    // For rotated logs, all readers should use the same name in mNameReaderMap (base log name)
    LogFileReaderPtrArray readerArray{reader1, reader2};
    handlerPtr->mNameReaderMap[gLogName] = readerArray;
    reader1->SetReaderArray(&handlerPtr->mNameReaderMap[gLogName]);
    reader2->SetReaderArray(&handlerPtr->mNameReaderMap[gLogName]);

    // Update mHostLogPath for rotated file to point to the base log name
    reader1->mHostLogPath = gRootDir + PATH_SEPARATOR + gLogName;
    reader2->mHostLogPath = gRootDir + PATH_SEPARATOR + gLogName;

    handlerPtr->mDevInodeReaderMap[devInode1] = reader1;
    handlerPtr->mDevInodeReaderMap[devInode2] = reader2;

    LOG_INFO(sLogger,
             ("Setup complete", "reader paths")("reader1 host", reader1->GetHostLogPath())("reader1 real",
                                                                                           reader1->mRealLogPath)(
                 "reader2 host", reader2->GetHostLogPath())("reader2 real", reader2->mRealLogPath));

    // Verify initial state
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mNameReaderMap[gLogName].size(), 2);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mDevInodeReaderMap.size(), 2);

    // Mark reader1 as deleted (simulating DELETE event received earlier)
    reader1->SetFileDeleted(true);

    // Open reader1's file
    APSARA_TEST_TRUE_FATAL(reader1->UpdateFilePtr());
    APSARA_TEST_TRUE_FATAL(reader1->IsFileOpened());

    // Physically delete the file while fd is still open
    // This simulates the real scenario where file is deleted from filesystem
    bfs::remove(logPath1);

    bool fileExists = bfs::exists(logPath1);
    LOG_INFO(
        sLogger,
        ("File status", "file physically deleted")("file exists", fileExists)("fd is open", reader1->IsFileOpened()));

    LOG_INFO(sLogger,
             ("Test setup complete", "sending MODIFY event")("reader1 deleted flag", reader1->IsFileDeleted())(
                 "reader1 file opened", reader1->IsFileOpened())("reader array size",
                                                                 handlerPtr->mNameReaderMap[gLogName].size()));

    // Send MODIFY event to reader1
    Event modifyEvent(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode1.dev, devInode1.inode);
    handlerPtr->Handle(modifyEvent);

    LOG_INFO(sLogger,
             ("After Handle()", "checking results")("reader array size", handlerPtr->mNameReaderMap[gLogName].size())(
                 "devInodeMap size", handlerPtr->mDevInodeReaderMap.size())("rotatorMap size",
                                                                            handlerPtr->mRotatorReaderMap.size()));

    // Expected behavior with new code (size > 1 checked first):
    // 1. hasMoreData = false (already at end)
    // 2. readerArrayPtr->size() > 1 is true (size is 2)
    // 3. Enter the first branch (not IsFileDeleted branch)
    // 4. ForceReadLogAndPush called
    // 5. RemoveReaderFromArrayAndMap called - reader1 removed from array and map
    // 6. CloseFilePtr called - detects file was deleted (bfs::remove was called)
    // 7. isFileReallyDeleted = true (file physically deleted from filesystem)
    // 8. reader1 NOT added to mRotatorReaderMap (because file really deleted)
    // Result: reader1 removed completely, only reader2 remains in array

    // After handling, we should have:
    // - Active readers: 1 (only reader2 remains)
    // - RotatorMap size: 0 (reader1 not added because file was really deleted)

    // Verify map invariants and expected sizes
    VerifyMapInvariants(handlerPtr, 1, 0, "After handling MODIFY event");

    // Verify specific reader states
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mNameReaderMap[gLogName].size(), 1);

    // Reader1 should be completely removed from both devInodeMap and rotatorMap
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mDevInodeReaderMap.count(devInode1), 0);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mRotatorReaderMap.count(devInode1), 0);

    // Reader2 should remain in both array and map
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mDevInodeReaderMap.count(devInode2), 1);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mNameReaderMap[gLogName][0]->GetDevInode().inode, devInode2.inode);

    // In this test scenario, the file was physically deleted via bfs::remove()
    // while the file descriptor was still open. This means CloseFilePtr should
    // detect the deletion and return isFileReallyDeleted = true.
    // Therefore, reader1 should NOT be added to mRotatorReaderMap.

    LOG_INFO(sLogger,
             ("Test result", "success")("behavior", "size > 1 branch executed correctly")(
                 "array size", handlerPtr->mNameReaderMap[gLogName].size())(
                 "devInodeMap size", handlerPtr->mDevInodeReaderMap.size())("rotator size",
                                                                            handlerPtr->mRotatorReaderMap.size())(
                 "note", "reader1 completely removed (file really deleted), reader2 remains"));

    LOG_INFO(sLogger, ("TestFileDeletedWithTwoReaders_FileReallyDeleted() end", "Test passed"));
}

// Test case for file deleted with queue length = 2 - File Not Deleted scenario
// This tests the new refactored code path where size > 1 is checked first
void ModifyHandlerUnittest::TestFileDeletedWithTwoReaders_FileNotDeleted() {
    LOG_INFO(sLogger, ("TestFileDeletedWithTwoReaders_FileNotDeleted() begin", time(NULL)));

    // Test scenario:
    // - Queue has 2 readers (simulating one rotated file + current file)
    // - The first reader (oldest rotated file) is marked as deleted
    // - BUT file is NOT physically deleted from filesystem (still exists)
    // - When processing MODIFY event, ReadLog() returns hasMoreData=false (read to end)
    // - The new code should:
    //   1. Check size > 1 first (condition is true)
    //   2. Call ForceReadLogAndPush
    //   3. Call RemoveReaderFromArrayAndMap
    //   4. Call CloseFilePtr - file still exists, isFileReallyDeleted = false
    //   5. reader1 added to rotator map and new event pushed

    std::string signature = "a sample log\n";
    auto sigSize = (uint32_t)signature.size();
    auto sigHash = (uint64_t)HashSignatureString(signature.c_str(), (size_t)sigSize);

    // Create 2 log files (rotated scenario)
    std::string logPath1 = gRootDir + PATH_SEPARATOR + gLogName + ".1"; // rotated file
    std::string logPath2 = gRootDir + PATH_SEPARATOR + gLogName; // current file

    writeLog(logPath1, signature);
    writeLog(logPath2, signature);

    auto devInode1 = GetFileDevInode(logPath1);
    auto devInode2 = GetFileDevInode(logPath2);

    // Create 2 readers
    auto reader1 = std::make_shared<LogFileReader>(gRootDir,
                                                   gLogName + ".1",
                                                   devInode1,
                                                   std::make_pair(&readerOpts, &ctx),
                                                   std::make_pair(&multilineOpts, &ctx),
                                                   std::make_pair(&tagOpts, &ctx));
    reader1->mLastFileSignatureSize = sigSize;
    reader1->mLastFileSignatureHash = sigHash;
    reader1->mLastFilePos = signature.size(); // Already at end
    reader1->UpdateReaderManual();

    LOG_INFO(sLogger,
             ("Created reader1", "path info")("mHostLogPath", reader1->GetHostLogPath())(
                 "mRealLogPath", reader1->mRealLogPath)("expected", logPath1));

    auto reader2 = std::make_shared<LogFileReader>(gRootDir,
                                                   gLogName,
                                                   devInode2,
                                                   std::make_pair(&readerOpts, &ctx),
                                                   std::make_pair(&multilineOpts, &ctx),
                                                   std::make_pair(&tagOpts, &ctx));
    reader2->mLastFileSignatureSize = sigSize;
    reader2->mLastFileSignatureHash = sigHash;
    reader2->mLastFilePos = signature.size(); // Already at end
    reader2->UpdateReaderManual();

    LOG_INFO(sLogger,
             ("Created reader2", "path info")("mHostLogPath", reader2->GetHostLogPath())(
                 "mRealLogPath", reader2->mRealLogPath)("expected", logPath2));

    // Build handler with 2 readers
    auto handlerPtr = std::make_shared<ModifyHandler>(mConfigName, mConfig);

    // For rotated logs, all readers should use the same name in mNameReaderMap (base log name)
    LogFileReaderPtrArray readerArray{reader1, reader2};
    handlerPtr->mNameReaderMap[gLogName] = readerArray;
    reader1->SetReaderArray(&handlerPtr->mNameReaderMap[gLogName]);
    reader2->SetReaderArray(&handlerPtr->mNameReaderMap[gLogName]);

    // Update mHostLogPath for rotated file to point to the base log name
    reader1->mHostLogPath = gRootDir + PATH_SEPARATOR + gLogName;
    reader2->mHostLogPath = gRootDir + PATH_SEPARATOR + gLogName;

    handlerPtr->mDevInodeReaderMap[devInode1] = reader1;
    handlerPtr->mDevInodeReaderMap[devInode2] = reader2;

    LOG_INFO(sLogger,
             ("Setup complete", "reader paths")("reader1 host", reader1->GetHostLogPath())("reader1 real",
                                                                                           reader1->mRealLogPath)(
                 "reader2 host", reader2->GetHostLogPath())("reader2 real", reader2->mRealLogPath));

    // Verify initial state
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mNameReaderMap[gLogName].size(), 2);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mDevInodeReaderMap.size(), 2);

    // Mark reader1 as deleted (simulating DELETE event received earlier)
    reader1->SetFileDeleted(true);

    // Open reader1's file
    APSARA_TEST_TRUE_FATAL(reader1->UpdateFilePtr());
    APSARA_TEST_TRUE_FATAL(reader1->IsFileOpened());

    // DON'T delete the file - it still exists on filesystem
    // This simulates the case where DELETE event was received but file still exists

    bool fileExists = bfs::exists(logPath1);
    LOG_INFO(sLogger,
             ("File status", "file NOT physically deleted")("file exists", fileExists)(
                 "fd is open", reader1->IsFileOpened())("deleted flag", reader1->IsFileDeleted()));

    LOG_INFO(sLogger,
             ("Test setup complete", "sending MODIFY event")("reader1 deleted flag", reader1->IsFileDeleted())(
                 "reader1 file opened", reader1->IsFileOpened())("reader array size",
                                                                 handlerPtr->mNameReaderMap[gLogName].size()));

    // Send MODIFY event to reader1
    Event modifyEvent(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode1.dev, devInode1.inode);
    handlerPtr->Handle(modifyEvent);

    LOG_INFO(sLogger,
             ("After Handle()", "checking results")("reader array size", handlerPtr->mNameReaderMap[gLogName].size())(
                 "devInodeMap size", handlerPtr->mDevInodeReaderMap.size())("rotatorMap size",
                                                                            handlerPtr->mRotatorReaderMap.size()));

    // Expected behavior with new code (size > 1 checked first):
    // 1. hasMoreData = false (already at end)
    // 2. readerArrayPtr->size() > 1 is true (size is 2)
    // 3. Enter the first branch (not IsFileDeleted branch)
    // 4. ForceReadLogAndPush called
    // 5. RemoveReaderFromArrayAndMap called - reader1 removed from array and map
    // 6. CloseFilePtr called - file still exists, isFileReallyDeleted = false
    // 7. Because isFileReallyDeleted = false:
    //    - reader1 is added to mRotatorReaderMap
    //    - A new event is pushed for reader2
    // Result: reader1 moved to rotator map, only reader2 remains in array

    // After handling, we should have:
    // - Active readers: 1 (only reader2 remains)
    // - RotatorMap size: 1 (reader1 moved to rotator map)

    // Verify map invariants and expected sizes
    VerifyMapInvariants(handlerPtr, 1, 1, "After handling MODIFY event");

    // Verify specific reader states
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mNameReaderMap[gLogName].size(), 1);

    // Reader1 should be removed from devInodeMap but added to rotatorMap
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mDevInodeReaderMap.count(devInode1), 0);

    // Reader2 should remain in both array and map
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mDevInodeReaderMap.count(devInode2), 1);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mNameReaderMap[gLogName][0]->GetDevInode().inode, devInode2.inode);

    // In this test scenario, the file was NOT physically deleted (file still exists).
    // Even though reader1 is marked as deleted, CloseFilePtr will detect that
    // the file still exists and return isFileReallyDeleted = false.
    // Therefore, reader1 should be moved to mRotatorReaderMap (not discarded).
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mRotatorReaderMap.size(), 1);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mRotatorReaderMap.count(devInode1), 1);

    LOG_INFO(sLogger,
             ("Test result", "success")("behavior", "size > 1 branch executed correctly")(
                 "array size", handlerPtr->mNameReaderMap[gLogName].size())(
                 "devInodeMap size", handlerPtr->mDevInodeReaderMap.size())("rotator size",
                                                                            handlerPtr->mRotatorReaderMap.size())(
                 "note", "reader1 moved to rotator map (file not really deleted), reader2 remains"));

    LOG_INFO(sLogger, ("TestFileDeletedWithTwoReaders_FileNotDeleted() end", "Test passed"));
}

// Test case for file deleted with queue length = 1 - File Really Deleted scenario
// This tests the IsFileDeleted() branch when size == 1
void ModifyHandlerUnittest::TestFileDeletedWithSingleReader_FileReallyDeleted() {
    LOG_INFO(sLogger, ("TestFileDeletedWithSingleReader_FileReallyDeleted() begin", time(NULL)));

    // Test scenario:
    // - Queue has only 1 reader (no rotation, single log file)
    // - The reader's file is marked as deleted
    // - File is physically deleted from filesystem
    // - When processing MODIFY event, ReadLog() returns hasMoreData=false (read to end)
    // - The new code should:
    //   1. Check size > 1 first (condition is false, size is 1)
    //   2. Check IsFileDeleted() (condition is true)
    //   3. Call CloseFilePtr - detects file really deleted
    //   4. Because isFileReallyDeleted is true:
    //      - Call RemoveReaderFromArrayAndMap

    std::string signature = "a sample log\n";
    auto sigSize = (uint32_t)signature.size();
    auto sigHash = (uint64_t)HashSignatureString(signature.c_str(), (size_t)sigSize);

    // Create single log file
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    writeLog(logPath, signature);

    auto devInode = GetFileDevInode(logPath);

    // Create single reader
    auto reader = std::make_shared<LogFileReader>(gRootDir,
                                                  gLogName,
                                                  devInode,
                                                  std::make_pair(&readerOpts, &ctx),
                                                  std::make_pair(&multilineOpts, &ctx),
                                                  std::make_pair(&tagOpts, &ctx));
    reader->mLastFileSignatureSize = sigSize;
    reader->mLastFileSignatureHash = sigHash;
    reader->mLastFilePos = signature.size(); // Already at end
    reader->UpdateReaderManual();

    LOG_INFO(sLogger,
             ("Created reader", "path info")("mHostLogPath", reader->GetHostLogPath())(
                 "mRealLogPath", reader->mRealLogPath)("expected", logPath));

    // Build handler with single reader
    auto handlerPtr = std::make_shared<ModifyHandler>(mConfigName, mConfig);

    LogFileReaderPtrArray readerArray{reader};
    handlerPtr->mNameReaderMap[gLogName] = readerArray;
    reader->SetReaderArray(&handlerPtr->mNameReaderMap[gLogName]);

    handlerPtr->mDevInodeReaderMap[devInode] = reader;

    LOG_INFO(sLogger,
             ("Setup complete", "reader path")("reader host", reader->GetHostLogPath())("reader real",
                                                                                        reader->mRealLogPath));

    // Verify initial state
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mNameReaderMap[gLogName].size(), 1);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mDevInodeReaderMap.size(), 1);

    // Mark reader as deleted (simulating DELETE event received earlier)
    reader->SetFileDeleted(true);

    // Open reader's file
    APSARA_TEST_TRUE_FATAL(reader->UpdateFilePtr());
    APSARA_TEST_TRUE_FATAL(reader->IsFileOpened());

    // Physically delete the file while fd is still open
    bfs::remove(logPath);

    bool fileExists = bfs::exists(logPath);
    LOG_INFO(
        sLogger,
        ("File status", "file physically deleted")("file exists", fileExists)("fd is open", reader->IsFileOpened()));

    LOG_INFO(sLogger,
             ("Test setup complete", "sending MODIFY event")("reader deleted flag", reader->IsFileDeleted())(
                 "reader file opened", reader->IsFileOpened())("reader array size",
                                                               handlerPtr->mNameReaderMap[gLogName].size()));

    // Send MODIFY event
    Event modifyEvent(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    handlerPtr->Handle(modifyEvent);

    LOG_INFO(sLogger,
             ("After Handle()", "checking results")("reader array size", handlerPtr->mNameReaderMap[gLogName].size())(
                 "devInodeMap size", handlerPtr->mDevInodeReaderMap.size())("rotatorMap size",
                                                                            handlerPtr->mRotatorReaderMap.size()));

    // Expected behavior with new code (IsFileDeleted branch):
    // 1. hasMoreData = false (already at end)
    // 2. readerArrayPtr->size() > 1 is false (size is 1)
    // 3. IsFileDeleted() is true - enter the second branch
    // 4. CloseFilePtr called - detects file was deleted (bfs::remove was called)
    // 5. isFileReallyDeleted = true (file physically deleted from filesystem)
    // 6. RemoveReaderFromArrayAndMap called - reader removed from array and map
    // Result: reader completely removed from both array and map

    // After handling, we should have:
    // - Active readers: 0 (reader removed)
    // - RotatorMap size: 0 (not added to rotator because file was really deleted)

    // Verify map invariants and expected sizes
    VerifyMapInvariants(handlerPtr, 0, 0, "After handling MODIFY event");

    // Verify specific reader states
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mNameReaderMap[gLogName].size(), 0);

    // Reader should be completely removed from both devInodeMap and rotatorMap
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mDevInodeReaderMap.count(devInode), 0);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mRotatorReaderMap.count(devInode), 0);

    LOG_INFO(sLogger,
             ("Test result", "success")("behavior", "IsFileDeleted branch executed correctly")(
                 "array size", handlerPtr->mNameReaderMap[gLogName].size())(
                 "devInodeMap size", handlerPtr->mDevInodeReaderMap.size())("rotator size",
                                                                            handlerPtr->mRotatorReaderMap.size())(
                 "note", "reader completely removed when file was really deleted"));

    LOG_INFO(sLogger, ("TestFileDeletedWithSingleReader_FileReallyDeleted() end", "Test passed"));
}

// Test case for file deleted with queue length = 1 - File Not Deleted scenario
// This tests the IsFileDeleted() branch when size == 1 but file still exists
void ModifyHandlerUnittest::TestFileDeletedWithSingleReader_FileNotDeleted() {
    LOG_INFO(sLogger, ("TestFileDeletedWithSingleReader_FileNotDeleted() begin", time(NULL)));

    // Test scenario:
    // - Queue has only 1 reader (no rotation, single log file)
    // - The reader's file is marked as deleted
    // - BUT file is NOT physically deleted from filesystem (still exists)
    // - When processing MODIFY event, ReadLog() returns hasMoreData=false (read to end)
    // - The new code should:
    //   1. Check size > 1 first (condition is false, size is 1)
    //   2. Check IsFileDeleted() (condition is true)
    //   3. Call CloseFilePtr - file still exists, isFileReallyDeleted = false
    //   4. Because isFileReallyDeleted is false:
    //      - Do NOT call RemoveReaderFromArrayAndMap
    //      - Reader remains in array/map (will wait for next event)

    std::string signature = "a sample log\n";
    auto sigSize = (uint32_t)signature.size();
    auto sigHash = (uint64_t)HashSignatureString(signature.c_str(), (size_t)sigSize);

    // Create single log file
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    writeLog(logPath, signature);

    auto devInode = GetFileDevInode(logPath);

    // Create single reader
    auto reader = std::make_shared<LogFileReader>(gRootDir,
                                                  gLogName,
                                                  devInode,
                                                  std::make_pair(&readerOpts, &ctx),
                                                  std::make_pair(&multilineOpts, &ctx),
                                                  std::make_pair(&tagOpts, &ctx));
    reader->mLastFileSignatureSize = sigSize;
    reader->mLastFileSignatureHash = sigHash;
    reader->mLastFilePos = signature.size(); // Already at end
    reader->UpdateReaderManual();

    LOG_INFO(sLogger,
             ("Created reader", "path info")("mHostLogPath", reader->GetHostLogPath())(
                 "mRealLogPath", reader->mRealLogPath)("expected", logPath));

    // Build handler with single reader
    auto handlerPtr = std::make_shared<ModifyHandler>(mConfigName, mConfig);

    LogFileReaderPtrArray readerArray{reader};
    handlerPtr->mNameReaderMap[gLogName] = readerArray;
    reader->SetReaderArray(&handlerPtr->mNameReaderMap[gLogName]);

    handlerPtr->mDevInodeReaderMap[devInode] = reader;

    LOG_INFO(sLogger,
             ("Setup complete", "reader path")("reader host", reader->GetHostLogPath())("reader real",
                                                                                        reader->mRealLogPath));

    // Verify initial state
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mNameReaderMap[gLogName].size(), 1);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mDevInodeReaderMap.size(), 1);

    // Mark reader as deleted (simulating DELETE event received earlier)
    reader->SetFileDeleted(true);

    // Open reader's file
    APSARA_TEST_TRUE_FATAL(reader->UpdateFilePtr());
    APSARA_TEST_TRUE_FATAL(reader->IsFileOpened());

    // DON'T delete the file - it still exists on filesystem
    // This simulates the case where DELETE event was received but file still exists

    bool fileExists = bfs::exists(logPath);
    LOG_INFO(sLogger,
             ("File status", "file NOT physically deleted")("file exists", fileExists)(
                 "fd is open", reader->IsFileOpened())("deleted flag", reader->IsFileDeleted()));

    LOG_INFO(sLogger,
             ("Test setup complete", "sending MODIFY event")("reader deleted flag", reader->IsFileDeleted())(
                 "reader file opened", reader->IsFileOpened())("reader array size",
                                                               handlerPtr->mNameReaderMap[gLogName].size()));

    // Send MODIFY event
    Event modifyEvent(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    handlerPtr->Handle(modifyEvent);

    LOG_INFO(sLogger,
             ("After Handle()", "checking results")("reader array size", handlerPtr->mNameReaderMap[gLogName].size())(
                 "devInodeMap size", handlerPtr->mDevInodeReaderMap.size())("rotatorMap size",
                                                                            handlerPtr->mRotatorReaderMap.size()));

    // Expected behavior with new code (IsFileDeleted branch):
    // 1. hasMoreData = false (already at end)
    // 2. readerArrayPtr->size() > 1 is false (size is 1)
    // 3. IsFileDeleted() is true - enter the second branch
    // 4. CloseFilePtr called - file still exists, isFileReallyDeleted = false
    // 5. Because isFileReallyDeleted is false:
    //    - RemoveReaderFromArrayAndMap is NOT called
    // Result: reader remains in both array and map

    // After handling, we should have:
    // - Active readers: 1 (reader still exists)
    // - RotatorMap size: 0 (not added to rotator)

    // Verify map invariants and expected sizes
    VerifyMapInvariants(handlerPtr, 1, 0, "After handling MODIFY event");

    // Verify specific reader states
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mNameReaderMap[gLogName].size(), 1);

    // Reader should remain in devInodeMap
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mDevInodeReaderMap.count(devInode), 1);

    // Reader should NOT be in rotator map
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mRotatorReaderMap.count(devInode), 0);

    LOG_INFO(sLogger,
             ("Test result", "success")("behavior", "IsFileDeleted branch executed correctly")(
                 "array size", handlerPtr->mNameReaderMap[gLogName].size())(
                 "devInodeMap size", handlerPtr->mDevInodeReaderMap.size())("rotator size",
                                                                            handlerPtr->mRotatorReaderMap.size())(
                 "note", "reader remains in array/map (file not really deleted, will wait for next event)"));

    LOG_INFO(sLogger, ("TestFileDeletedWithSingleReader_FileNotDeleted() end", "Test passed"));
}

// Test case for container stopped with queue length = 1 - File Really Deleted scenario
// This tests the IsContainerStopped() branch when size == 1 and file is deleted
void ModifyHandlerUnittest::TestContainerStoppedWithSingleReader_FileReallyDeleted() {
    LOG_INFO(sLogger, ("TestContainerStoppedWithSingleReader_FileReallyDeleted() begin", time(NULL)));

    // Test scenario:
    // - Queue has only 1 reader (no rotation, single log file)
    // - The reader's container is stopped
    // - File is physically deleted from filesystem
    // - When processing MODIFY event, ReadLog() returns hasMoreData=false (read to end)
    // - The new code should:
    //   1. Check size > 1 first (condition is false, size is 1)
    //   2. Check IsFileDeleted() (condition is false)
    //   3. Check IsContainerStopped() (condition is true)
    //   4. UpdateContainerInfo and check if still stopped
    //   5. Call ForceReadLogAndPush
    //   6. Call CloseFilePtr - detects file really deleted
    //   7. Because isFileReallyDeleted is true:
    //      - Call RemoveReaderFromArrayAndMap

    std::string signature = "a sample log\n";
    auto sigSize = (uint32_t)signature.size();
    auto sigHash = (uint64_t)HashSignatureString(signature.c_str(), (size_t)sigSize);

    // Create single log file
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    writeLog(logPath, signature);

    auto devInode = GetFileDevInode(logPath);

    // Create single reader with container info
    auto reader = std::make_shared<LogFileReader>(gRootDir,
                                                  gLogName,
                                                  devInode,
                                                  std::make_pair(&readerOpts, &ctx),
                                                  std::make_pair(&multilineOpts, &ctx),
                                                  std::make_pair(&tagOpts, &ctx));
    reader->mLastFileSignatureSize = sigSize;
    reader->mLastFileSignatureHash = sigHash;
    reader->mLastFilePos = signature.size(); // Already at end
    reader->UpdateReaderManual();

    // Set container info - use the same container ID as in SetUp ("1")
    // and mark it as stopped
    reader->SetContainerStopped();
    reader->mContainerID = "1";

    // Make sure the container in discoveryOpts is also marked as stopped
    // This ensures UpdateContainerInfo() won't think the container restarted
    stopContainer("1");

    LOG_INFO(
        sLogger,
        ("Created reader", "path info")("mHostLogPath", reader->GetHostLogPath())("mRealLogPath", reader->mRealLogPath)(
            "expected", logPath)("container stopped", reader->IsContainerStopped()));

    // Build handler with single reader
    auto handlerPtr = std::make_shared<ModifyHandler>(mConfigName, mConfig);

    LogFileReaderPtrArray readerArray{reader};
    handlerPtr->mNameReaderMap[gLogName] = readerArray;
    reader->SetReaderArray(&handlerPtr->mNameReaderMap[gLogName]);

    handlerPtr->mDevInodeReaderMap[devInode] = reader;

    LOG_INFO(sLogger,
             ("Setup complete", "reader path")("reader host", reader->GetHostLogPath())("reader real",
                                                                                        reader->mRealLogPath));

    // Verify initial state
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mNameReaderMap[gLogName].size(), 1);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mDevInodeReaderMap.size(), 1);

    // Open reader's file
    APSARA_TEST_TRUE_FATAL(reader->UpdateFilePtr());
    APSARA_TEST_TRUE_FATAL(reader->IsFileOpened());

    // Physically delete the file while fd is still open
    bfs::remove(logPath);

    bool fileExists = bfs::exists(logPath);
    LOG_INFO(sLogger,
             ("File status", "file physically deleted")("file exists", fileExists)(
                 "fd is open", reader->IsFileOpened())("container stopped", reader->IsContainerStopped()));

    LOG_INFO(sLogger,
             ("Test setup complete", "sending MODIFY event")("reader container stopped", reader->IsContainerStopped())(
                 "reader file opened", reader->IsFileOpened())("reader array size",
                                                               handlerPtr->mNameReaderMap[gLogName].size()));

    // Send MODIFY event
    Event modifyEvent(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    handlerPtr->Handle(modifyEvent);

    LOG_INFO(sLogger,
             ("After Handle()", "checking results")("reader array size", handlerPtr->mNameReaderMap[gLogName].size())(
                 "devInodeMap size", handlerPtr->mDevInodeReaderMap.size())("rotatorMap size",
                                                                            handlerPtr->mRotatorReaderMap.size()));

    // Expected behavior with new code (IsContainerStopped branch):
    // 1. hasMoreData = false (already at end)
    // 2. readerArrayPtr->size() > 1 is false (size is 1)
    // 3. IsFileDeleted() is false
    // 4. IsContainerStopped() is true - enter the third branch
    // 5. UpdateContainerInfo() - still stopped
    // 6. ForceReadLogAndPush called
    // 7. CloseFilePtr called - detects file was deleted (bfs::remove was called)
    // 8. isFileReallyDeleted = true (file physically deleted from filesystem)
    // 9. RemoveReaderFromArrayAndMap called - reader removed from array and map
    // Result: reader completely removed from both array and map

    // After handling, we should have:
    // - Active readers: 0 (reader removed)
    // - RotatorMap size: 0 (not added to rotator because file was really deleted)

    // Verify map invariants and expected sizes
    VerifyMapInvariants(handlerPtr, 0, 0, "After handling MODIFY event");

    // Verify specific reader states
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mNameReaderMap[gLogName].size(), 0);

    // Reader should be completely removed from both devInodeMap and rotatorMap
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mDevInodeReaderMap.count(devInode), 0);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mRotatorReaderMap.count(devInode), 0);

    LOG_INFO(sLogger,
             ("Test result", "success")("behavior", "IsContainerStopped branch executed correctly")(
                 "array size", handlerPtr->mNameReaderMap[gLogName].size())(
                 "devInodeMap size", handlerPtr->mDevInodeReaderMap.size())("rotator size",
                                                                            handlerPtr->mRotatorReaderMap.size())(
                 "note", "reader completely removed when container stopped and file was really deleted"));

    LOG_INFO(sLogger, ("TestContainerStoppedWithSingleReader_FileReallyDeleted() end", "Test passed"));
}

// Test case for container stopped with queue length = 1 - File Not Deleted scenario
// This tests the IsContainerStopped() branch when size == 1 but file still exists
void ModifyHandlerUnittest::TestContainerStoppedWithSingleReader_FileNotDeleted() {
    LOG_INFO(sLogger, ("TestContainerStoppedWithSingleReader_FileNotDeleted() begin", time(NULL)));

    // Test scenario:
    // - Queue has only 1 reader (no rotation, single log file)
    // - The reader's container is stopped
    // - BUT file is NOT physically deleted from filesystem (still exists)
    // - When processing MODIFY event, ReadLog() returns hasMoreData=false (read to end)
    // - The new code should:
    //   1. Check size > 1 first (condition is false, size is 1)
    //   2. Check IsFileDeleted() (condition is false)
    //   3. Check IsContainerStopped() (condition is true)
    //   4. UpdateContainerInfo and check if still stopped
    //   5. Call ForceReadLogAndPush
    //   6. Call CloseFilePtr - file still exists, isFileReallyDeleted = false
    //   7. Because isFileReallyDeleted is false:
    //      - Do NOT call RemoveReaderFromArrayAndMap
    //      - Reader remains in array/map

    std::string signature = "a sample log\n";
    auto sigSize = (uint32_t)signature.size();
    auto sigHash = (uint64_t)HashSignatureString(signature.c_str(), (size_t)sigSize);

    // Create single log file
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    writeLog(logPath, signature);

    auto devInode = GetFileDevInode(logPath);

    // Create single reader with container info
    auto reader = std::make_shared<LogFileReader>(gRootDir,
                                                  gLogName,
                                                  devInode,
                                                  std::make_pair(&readerOpts, &ctx),
                                                  std::make_pair(&multilineOpts, &ctx),
                                                  std::make_pair(&tagOpts, &ctx));
    reader->mLastFileSignatureSize = sigSize;
    reader->mLastFileSignatureHash = sigHash;
    reader->mLastFilePos = signature.size(); // Already at end
    reader->UpdateReaderManual();

    // Set container info - use the same container ID as in SetUp ("1")
    // and mark it as stopped
    reader->SetContainerStopped();
    reader->mContainerID = "1";

    // Make sure the container in discoveryOpts is also marked as stopped
    // This ensures UpdateContainerInfo() won't think the container restarted
    stopContainer("1");

    LOG_INFO(
        sLogger,
        ("Created reader", "path info")("mHostLogPath", reader->GetHostLogPath())("mRealLogPath", reader->mRealLogPath)(
            "expected", logPath)("container stopped", reader->IsContainerStopped()));

    // Build handler with single reader
    auto handlerPtr = std::make_shared<ModifyHandler>(mConfigName, mConfig);

    LogFileReaderPtrArray readerArray{reader};
    handlerPtr->mNameReaderMap[gLogName] = readerArray;
    reader->SetReaderArray(&handlerPtr->mNameReaderMap[gLogName]);

    handlerPtr->mDevInodeReaderMap[devInode] = reader;

    LOG_INFO(sLogger,
             ("Setup complete", "reader path")("reader host", reader->GetHostLogPath())("reader real",
                                                                                        reader->mRealLogPath));

    // Verify initial state
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mNameReaderMap[gLogName].size(), 1);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mDevInodeReaderMap.size(), 1);

    // Open reader's file
    APSARA_TEST_TRUE_FATAL(reader->UpdateFilePtr());
    APSARA_TEST_TRUE_FATAL(reader->IsFileOpened());

    // DON'T delete the file - it still exists on filesystem
    // This simulates the case where container is stopped but file still exists

    bool fileExists = bfs::exists(logPath);
    LOG_INFO(sLogger,
             ("File status", "file NOT physically deleted")("file exists", fileExists)(
                 "fd is open", reader->IsFileOpened())("container stopped", reader->IsContainerStopped()));

    LOG_INFO(sLogger,
             ("Test setup complete", "sending MODIFY event")("reader container stopped", reader->IsContainerStopped())(
                 "reader file opened", reader->IsFileOpened())("reader array size",
                                                               handlerPtr->mNameReaderMap[gLogName].size()));

    // Send MODIFY event
    Event modifyEvent(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    handlerPtr->Handle(modifyEvent);

    LOG_INFO(sLogger,
             ("After Handle()", "checking results")("reader array size", handlerPtr->mNameReaderMap[gLogName].size())(
                 "devInodeMap size", handlerPtr->mDevInodeReaderMap.size())("rotatorMap size",
                                                                            handlerPtr->mRotatorReaderMap.size()));

    // Expected behavior with new code (IsContainerStopped branch):
    // 1. hasMoreData = false (already at end)
    // 2. readerArrayPtr->size() > 1 is false (size is 1)
    // 3. IsFileDeleted() is false
    // 4. IsContainerStopped() is true - enter the third branch
    // 5. UpdateContainerInfo() - still stopped
    // 6. ForceReadLogAndPush called
    // 7. CloseFilePtr called - file still exists, isFileReallyDeleted = false
    // 8. Because isFileReallyDeleted is false:
    //    - RemoveReaderFromArrayAndMap is NOT called
    // Result: reader remains in both array and map

    // After handling, we should have:
    // - Active readers: 1 (reader still exists)
    // - RotatorMap size: 0 (not added to rotator)

    // Verify map invariants and expected sizes
    VerifyMapInvariants(handlerPtr, 1, 0, "After handling MODIFY event");

    // Verify specific reader states
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mNameReaderMap[gLogName].size(), 1);

    // Reader should remain in devInodeMap
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mDevInodeReaderMap.count(devInode), 1);

    // Reader should NOT be in rotator map
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mRotatorReaderMap.count(devInode), 0);

    LOG_INFO(sLogger,
             ("Test result", "success")("behavior", "IsContainerStopped branch executed correctly")(
                 "array size", handlerPtr->mNameReaderMap[gLogName].size())(
                 "devInodeMap size", handlerPtr->mDevInodeReaderMap.size())("rotator size",
                                                                            handlerPtr->mRotatorReaderMap.size())(
                 "note", "reader remains in array/map (container stopped but file not really deleted)"));

    LOG_INFO(sLogger, ("TestContainerStoppedWithSingleReader_FileNotDeleted() end", "Test passed"));
}

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
    reader1->mLastFilePos = signature.size();

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
    reader2->mLastFilePos = signature.size();

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
    reader3->mLastFilePos = signature.size();

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
    reader4->mLastFilePos = signature.size();

    handlerPtr->mRotatorReaderMap[reader3->mDevInode] = reader3;
    handlerPtr->mRotatorReaderMap[reader4->mDevInode] = reader4;

    handlerPtr->DumpReaderMeta(true, false);
    handlerPtr->DumpReaderMeta(false, false);

    // upgrade from old version
    std::string logPath5 = logPath + ".5";
    writeLog(logPath5, "a sample log\n");
    auto devInode5 = GetFileDevInode(logPath5);
    auto reader5 = std::make_shared<LogFileReader>(gRootDir,
                                                   basicLogName,
                                                   devInode5,
                                                   std::make_pair(&readerOpts, &ctx),
                                                   std::make_pair(&multilineOpts, &ctx),
                                                   std::make_pair(&tagOpts, &ctx));
    reader5->mRealLogPath = logPath5;
    reader5->mLastFileSignatureSize = sigSize;
    reader5->mLastFileSignatureHash = sigHash;
    reader5->mLastFilePos = signature.size();
    reader5->DumpMetaToMem(false, LogFileReader::CHECKPOINT_IDX_UNDEFINED);

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
                                       devInode5,
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
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mNameReaderMap[basicLogName].size(), 4);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mDevInodeReaderMap.size(), 4);
    auto readerArray = handlerPtr->mNameReaderMap[basicLogName];
    APSARA_TEST_EQUAL_FATAL(readerArray[0]->mDevInode.dev, devInode5.dev);
    APSARA_TEST_EQUAL_FATAL(readerArray[0]->mDevInode.inode, devInode5.inode);
    APSARA_TEST_EQUAL_FATAL(readerArray[0]->mLastFilePos, signature.size());
    APSARA_TEST_EQUAL_FATAL(readerArray[1]->mDevInode.dev, devInode2.dev);
    APSARA_TEST_EQUAL_FATAL(readerArray[1]->mDevInode.inode, devInode2.inode);
    APSARA_TEST_EQUAL_FATAL(readerArray[1]->mLastFilePos, signature.size());
    APSARA_TEST_EQUAL_FATAL(readerArray[2]->mDevInode.dev, devInode1.dev);
    APSARA_TEST_EQUAL_FATAL(readerArray[2]->mDevInode.inode, devInode1.inode);
    APSARA_TEST_EQUAL_FATAL(readerArray[2]->mLastFilePos, signature.size());
    APSARA_TEST_EQUAL_FATAL(readerArray[3]->mDevInode.dev, devInode.dev);
    APSARA_TEST_EQUAL_FATAL(readerArray[3]->mDevInode.inode, devInode.inode);
    APSARA_TEST_EQUAL_FATAL(readerArray[3]->mLastFilePos, 0);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mRotatorReaderMap.size(), 2);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mRotatorReaderMap[devInode3]->mDevInode.dev, devInode3.dev);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mRotatorReaderMap[devInode3]->mDevInode.inode, devInode3.inode);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mRotatorReaderMap[devInode3]->mLastFilePos, signature.size());
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mRotatorReaderMap[devInode4]->mDevInode.dev, devInode4.dev);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mRotatorReaderMap[devInode4]->mDevInode.inode, devInode4.inode);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mRotatorReaderMap[devInode4]->mLastFilePos, signature.size());
    handlerPtr.reset(new ModifyHandler(mConfigName, mConfig));
}

void ModifyHandlerUnittest::TestRecoverReaderFromCheckpointRotateLog() {
    LOG_INFO(sLogger, ("TestRecoverReaderFromCheckpointRotateLog() begin", time(NULL)));
    std::string basicLogName = "rotate.log";
    std::string logPath = gRootDir + PATH_SEPARATOR + basicLogName;
    std::string signature = "a sample log";
    auto sigSize = (uint32_t)signature.size();
    auto sigHash = (uint64_t)HashSignatureString(signature.c_str(), (size_t)sigSize);
    // build a modify handler
    auto handlerPtr = std::make_shared<ModifyHandler>(mConfigName, mConfig);
    writeLog(logPath, "a sample log\n");

    auto devInode = GetFileDevInode(logPath);
    auto reader = std::make_shared<LogFileReader>(gRootDir,
                                                  basicLogName,
                                                  devInode,
                                                  std::make_pair(&readerOpts, &ctx),
                                                  std::make_pair(&multilineOpts, &ctx),
                                                  std::make_pair(&tagOpts, &ctx));
    reader->mRealLogPath = logPath;
    reader->mLastFileSignatureSize = sigSize;
    reader->mLastFileSignatureHash = sigHash;
    reader->mLastFilePos = signature.size();

    std::string logPath1 = logPath + ".1";
    writeLog(logPath1, "a sample log\n");
    auto devInode1 = GetFileDevInode(logPath1);
    auto reader1 = std::make_shared<LogFileReader>(gRootDir,
                                                   basicLogName,
                                                   devInode1,
                                                   std::make_pair(&readerOpts, &ctx),
                                                   std::make_pair(&multilineOpts, &ctx),
                                                   std::make_pair(&tagOpts, &ctx));
    reader1->mHostLogPath = logPath;
    reader1->mRealLogPath = logPath1;
    reader1->mLastFileSignatureSize = sigSize;
    reader1->mLastFileSignatureHash = sigHash;
    reader1->mLastFilePos = signature.size();

    LogFileReaderPtrArray readerPtrArray{reader1, reader};
    handlerPtr->mNameReaderMap[logPath] = readerPtrArray;
    reader1->SetReaderArray(&handlerPtr->mNameReaderMap[logPath]);
    reader->SetReaderArray(&handlerPtr->mNameReaderMap[logPath]);
    handlerPtr->mDevInodeReaderMap[reader1->mDevInode] = reader1;
    handlerPtr->mDevInodeReaderMap[reader->mDevInode] = reader;

    std::string logPath2 = logPath + ".2";
    writeLog(logPath2, "a sample log\n");
    auto devInode2 = GetFileDevInode(logPath2);
    auto reader2 = std::make_shared<LogFileReader>(gRootDir,
                                                   basicLogName,
                                                   devInode2,
                                                   std::make_pair(&readerOpts, &ctx),
                                                   std::make_pair(&multilineOpts, &ctx),
                                                   std::make_pair(&tagOpts, &ctx));
    reader2->mHostLogPath = logPath;
    reader2->mRealLogPath = logPath2;
    reader2->mLastFileSignatureSize = sigSize;
    reader2->mLastFileSignatureHash = sigHash;
    reader2->mLastFilePos = signature.size();

    handlerPtr->mRotatorReaderMap[reader2->mDevInode] = reader2;

    handlerPtr->DumpReaderMeta(true, false);
    handlerPtr->DumpReaderMeta(false, false);

    // upgrade from old version
    std::string logPath3 = logPath + ".3";
    writeLog(logPath3, "a sample log\n");
    auto devInode3 = GetFileDevInode(logPath3);
    auto reader3 = std::make_shared<LogFileReader>(gRootDir,
                                                   basicLogName,
                                                   devInode3,
                                                   std::make_pair(&readerOpts, &ctx),
                                                   std::make_pair(&multilineOpts, &ctx),
                                                   std::make_pair(&tagOpts, &ctx));
    reader3->mHostLogPath = logPath;
    reader3->mRealLogPath = logPath3;
    reader3->mLastFileSignatureSize = sigSize;
    reader3->mLastFileSignatureHash = sigHash;
    reader3->mLastFilePos = signature.size();
    reader3->DumpMetaToMem(false, LogFileReader::CHECKPOINT_IDX_UNDEFINED);

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
                                       devInode3,
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
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mNameReaderMap.size(), 1);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mNameReaderMap[basicLogName].size(), 3);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mDevInodeReaderMap.size(), 3);
    auto readerArray = handlerPtr->mNameReaderMap[basicLogName];
    APSARA_TEST_EQUAL_FATAL(readerArray[0]->mDevInode.dev, devInode3.dev);
    APSARA_TEST_EQUAL_FATAL(readerArray[0]->mDevInode.inode, devInode3.inode);
    APSARA_TEST_EQUAL_FATAL(readerArray[0]->mLastFilePos, signature.size());
    APSARA_TEST_EQUAL_FATAL(readerArray[1]->mDevInode.dev, devInode1.dev);
    APSARA_TEST_EQUAL_FATAL(readerArray[1]->mDevInode.inode, devInode1.inode);
    APSARA_TEST_EQUAL_FATAL(readerArray[1]->mLastFilePos, signature.size());
    APSARA_TEST_EQUAL_FATAL(readerArray[2]->mDevInode.dev, devInode.dev);
    APSARA_TEST_EQUAL_FATAL(readerArray[2]->mDevInode.inode, devInode.inode);
    APSARA_TEST_EQUAL_FATAL(readerArray[2]->mLastFilePos, signature.size());
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mRotatorReaderMap.size(), 1);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mRotatorReaderMap[devInode2]->mDevInode.dev, devInode2.dev);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mRotatorReaderMap[devInode2]->mDevInode.inode, devInode2.inode);
    APSARA_TEST_EQUAL_FATAL(handlerPtr->mRotatorReaderMap[devInode2]->mLastFilePos, signature.size());
    handlerPtr.reset(new ModifyHandler(mConfigName, mConfig));
    LOG_INFO(sLogger, ("TestRecoverReaderFromCheckpointRotateLog() end", time(NULL)));
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
    CheckPoint* checkPointPtr = new CheckPoint(
        logPath, logPath, 13, sigSize, sigHash, devInode, mConfigName, logPath, false, true, "1", false);
    // use last event time as checkpoint's last update time
    checkPointPtr->mLastUpdateTime = time(NULL);
    checkPointPtr->mCache = "";
    checkPointPtr->mIdxInReaderArray = 0;
    CheckPointManager::Instance()->AddCheckPoint(checkPointPtr);

    // not set container stopped for rotator reader
    CheckPoint* checkPointPtr1 = new CheckPoint(
        logPath, logPath, 13, sigSize, sigHash, devInode1, mConfigName, logPath1, false, false, "1", false);
    checkPointPtr1->mLastUpdateTime = time(NULL);
    checkPointPtr1->mCache = "";
    checkPointPtr1->mIdxInReaderArray = -2;
    CheckPointManager::Instance()->AddCheckPoint(checkPointPtr1);


    // set container stopped for rotator reader
    CheckPoint* checkPointPtr2 = new CheckPoint(
        logPath, logPath, 13, sigSize, sigHash, devInode2, mConfigName, logPath2, false, true, "1", false);
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
    APSARA_TEST_TRUE_FATAL(handlerPtr->mRotatorReaderMap[devInode1]->mLogFileOp.IsOpen() == false);
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

void ModifyHandlerUnittest::TestClearReaderWhenFileDeleted() {
    LOG_INFO(sLogger, ("TestClearReaderWhenFileDeleted() begin", time(NULL)));

    // Read log to end to ensure IsReadToEnd() returns true
    Event event1(gRootDir, "", EVENT_MODIFY, 0);
    LogBuffer logbuf;
    APSARA_TEST_TRUE_FATAL(!mReaderPtr->ReadLog(logbuf, &event1)); // false means no more data
    APSARA_TEST_TRUE_FATAL(mReaderPtr->mLogFileOp.IsOpen());

    // Verify reader is read to end
    APSARA_TEST_TRUE_FATAL(mReaderPtr->IsReadToEnd());

    // Actually delete the file while file descriptor is still open
    // This simulates the real scenario where file is deleted but fd is held
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);

    // Verify reader exists before handling event
    APSARA_TEST_EQUAL_FATAL(mHandlerPtr->mDevInodeReaderMap.size(), 1);
    APSARA_TEST_EQUAL_FATAL(mHandlerPtr->mNameReaderMap[gLogName].size(), 1);

    // Send delete event to trigger close and cleanup
    Event event2(gRootDir, gLogName, EVENT_DELETE, 0);
    mHandlerPtr->Handle(event2);

    // Verify file descriptor is closed
    APSARA_TEST_TRUE_FATAL(!mReaderPtr->mLogFileOp.IsOpen());

    // Verify reader has been removed from mDevInodeReaderMap
    APSARA_TEST_EQUAL_FATAL(mHandlerPtr->mDevInodeReaderMap.size(), 0);

    // Verify reader has been removed from readerArray
    APSARA_TEST_EQUAL_FATAL(mHandlerPtr->mNameReaderMap[gLogName].size(), 0);

    LOG_INFO(sLogger, ("TestClearReaderWhenFileDeleted() end", time(NULL)));
}

} // end of namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
