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
#ifndef _MSC_VER
#include <unistd.h>
#endif

#include <boost/filesystem/operations.hpp>
#include <filesystem>
#include <memory>
#include <string>

#include "app_config/AppConfig.h"
#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "common/FileSystemUtil.h"
#include "common/Flags.h"
#include "common/JsonUtil.h"
#include "config/CollectionConfig.h"
#include "file_server/ConfigManager.h"
#include "file_server/EventDispatcher.h"
#include "file_server/FileServer.h"
#include "file_server/event/Event.h"
#include "file_server/event_handler/EventHandler.h"
#include "file_server/event_handler/LogInput.h"
#include "file_server/polling/PollingDirFile.h"
#include "file_server/polling/PollingModify.h"
#include "file_server/reader/LogFileReader.h"
#include "unittest/Unittest.h"
#include "unittest/UnittestHelper.h"

using namespace std;

DECLARE_FLAG_STRING(ilogtail_config);

namespace logtail {

class LogInputReaderUnittest : public ::testing::Test {
public:
    // Normal file test cases
    void CreateEmpty_Modify_Modify();
    void CreateNonEmpty_Modify_Modify();
    void CreateEmpty_Modify_Change_Close_Modify();
    void CreateEmpty_Modify_Close_Change_Modify();
    void CreateEmpty_Modify_Close_Modify();
    void CreateNonEmpty_Modify_Change_Close_Modify_SameSignature();
    void CreateNonEmpty_Modify_Close_Change_Modify_SameSignature();
    void CreateNonEmpty_Modify_Change_Close_Modify_DiffSignature();
    void CreateNonEmpty_Modify_Close_Change_Modify_DiffSignature();
    void CreateNonEmpty_Modify_Close_Modify();
    void CreateEmpty_Change_Close_Modify();
    void CreateEmpty_Close_Change_Modify();
    void CreateEmpty_Close_Modify();
    void CreateNonEmpty_Change_Close_Modify_SameSignature();
    void CreateNonEmpty_Close_Change_Modify_SameSignature();
    void CreateNonEmpty_Change_Close_Modify_DiffSignature();
    void CreateNonEmpty_Close_Change_Modify_DiffSignature();
    void CreateNonEmpty_Close_Modify();

    // Normal file test cases - reader created by modify event
    void ModifyEmpty_Modify_Modify();
    void ModifyNonEmpty_Modify_Modify();
    void ModifyEmpty_Change_Close_Modify();
    void ModifyEmpty_Close_Change_Modify();
    void ModifyEmpty_Close_Modify();
    void ModifyNonEmpty_Change_Close_Modify_SameSignature();
    void ModifyNonEmpty_Close_Change_Modify_SameSignature();
    void ModifyNonEmpty_Change_Close_Modify_DiffSignature();
    void ModifyNonEmpty_Close_Change_Modify_DiffSignature();
    void ModifyNonEmpty_Close_Modify();

#ifndef _MSC_VER
    // Symbolic link test cases
    void SymlinkCreateEmpty_Modify_Modify();
    void SymlinkCreateNonEmpty_Modify_Modify();
    void SymlinkCreateEmpty_Modify_Change_Close_Modify();
    void SymlinkCreateEmpty_Modify_Close_Change_Modify();
    void SymlinkCreateEmpty_Modify_Close_Modify();
    void SymlinkCreateNonEmpty_Modify_Change_Close_Modify_SameSignature();
    void SymlinkCreateNonEmpty_Modify_Close_Change_Modify_SameSignature();
    void SymlinkCreateNonEmpty_Modify_Change_Close_Modify_DiffSignature();
    void SymlinkCreateNonEmpty_Modify_Close_Change_Modify_DiffSignature();
    void SymlinkCreateNonEmpty_Modify_Close_Modify();
    void SymlinkCreateEmpty_Change_Close_Modify();
    void SymlinkCreateEmpty_Close_Change_Modify();
    void SymlinkCreateEmpty_Close_Modify();
    void SymlinkCreateNonEmpty_Change_Close_Modify_SameSignature();
    void SymlinkCreateNonEmpty_Close_Change_Modify_SameSignature();
    void SymlinkCreateNonEmpty_Change_Close_Modify_DiffSignature();
    void SymlinkCreateNonEmpty_Close_Change_Modify_DiffSignature();
    void SymlinkCreateNonEmpty_Close_Modify();
    void SymlinkCreateEmpty_Close_ModifyWrite_Change_Close_Modify();

    // Symbolic link test cases - reader created by modify event
    void SymlinkModifyEmpty_Modify_Modify();
    void SymlinkModifyNonEmpty_Modify_Modify();
    void SymlinkModifyEmpty_Change_Close_Modify();
    void SymlinkModifyEmpty_Close_Change_Modify();
    void SymlinkModifyEmpty_Close_Modify();
    void SymlinkModifyNonEmpty_Change_Close_Modify_SameSignature();
    void SymlinkModifyNonEmpty_Close_Change_Modify_SameSignature();
    void SymlinkModifyNonEmpty_Change_Close_Modify_DiffSignature();
    void SymlinkModifyNonEmpty_Close_Change_Modify_DiffSignature();
    void SymlinkModifyNonEmpty_Close_Modify();
#endif

protected:
    static void SetUpTestCase() {
        gRootDir = GetProcessExecutionDir();
        gLogName = "test.log";
        if (PATH_SEPARATOR[0] == gRootDir.at(gRootDir.size() - 1)) {
            gRootDir.resize(gRootDir.size() - 1);
        }
        gRootDir += PATH_SEPARATOR + "LogInputReaderUnittest";
        filesystem::remove_all(gRootDir);
    }

    static void TearDownTestCase() {}

    void SetUp() override {
        // CRITICAL: Remove FileServer configs FIRST to avoid stale pointers
        // These configs hold pointers to member variables (ctx, discoveryOpts) that will be reinitialized
        FileServer::GetInstance()->RemoveFileDiscoveryConfig(mConfigName);
        FileServer::GetInstance()->RemoveFileReaderConfig(mConfigName);
        FileServer::GetInstance()->RemoveMultilineConfig(mConfigName);

        // Clear ConfigManager cache to avoid stale pointers
        ConfigManager::GetInstance()->ClearFilePipelineMatchCache();

        // Force cleanup: ensure path is completely unregistered before setting up
        // Always cleanup regardless of registration status to ensure clean state
        EventHandler* oldHandler = EventDispatcher::GetInstance()->GetHandler(gRootDir.c_str());
        if (oldHandler != nullptr) {
            CreateModifyHandler* createModifyHandler = dynamic_cast<CreateModifyHandler*>(oldHandler);
            if (createModifyHandler != nullptr) {
                // Delete all ModifyHandlers to avoid holding stale references
                for (auto iter = createModifyHandler->mModifyHandlerPtrMap.begin();
                     iter != createModifyHandler->mModifyHandlerPtrMap.end();
                     ++iter) {
                    delete iter->second;
                }
                createModifyHandler->mModifyHandlerPtrMap.clear();
            }
            EventDispatcher::GetInstance()->UnregisterEventHandler(gRootDir);
        }
        // Always remove from ConfigManager to ensure clean state
        ConfigManager::GetInstance()->RemoveHandler(gRootDir);

        // Double-check: ensure path is not registered
        APSARA_TEST_TRUE_FATAL(!EventDispatcher::GetInstance()->IsRegistered(gRootDir));

        bfs::create_directories(gRootDir);
        // create a file for reader
        std::string logPath = gRootDir + PATH_SEPARATOR + "**" + PATH_SEPARATOR + "*.log";
        writeLog(logPath, "");

        // init pipeline and config
        unique_ptr<Json::Value> configJson;
        string configStr;
        string errorMsg;
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
                        ],
                        "MaxDirSearchDepth": 10,
                        "ExcludeDirs": [
                            ")"
            + gRootDir + PATH_SEPARATOR + "real" + R"("
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
        ctx.SetPipeline(*pipeline);
        ctx.SetConfigName(mConfigName);
        ctx.SetProcessQueueKey(0);

        discoveryOpts = FileDiscoveryOptions();
        discoveryOpts.Init(inputConfigJson, ctx, "test");
        mConfig = std::make_pair(&discoveryOpts, &ctx);
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;

        FileServer::GetInstance()->AddFileDiscoveryConfig(mConfigName, &discoveryOpts, &ctx);
        FileServer::GetInstance()->AddFileReaderConfig(mConfigName, &readerOpts, &ctx);
        FileServer::GetInstance()->AddMultilineConfig(mConfigName, &multilineOpts, &ctx);
        ProcessQueueManager::GetInstance()->CreateOrUpdateBoundedQueue(0, 0, ctx);

        // Register handler - ensure we always create a new handler
        EventHandler* handler = new CreateModifyHandler(new CreateHandler());
        EventHandler* registeredHandler = handler; // Keep track of what we pass
        bool registered = EventDispatcher::GetInstance()->RegisterEventHandler(gRootDir, mConfig, registeredHandler);
        APSARA_TEST_TRUE_FATAL(registered);
        // Verify that we're using the handler we created, not a reused one
        // If RegisterEventHandler reused an old handler, registeredHandler will be different from handler
        if (registeredHandler != handler) {
            // Old handler was reused - clean up its ModifyHandlers to avoid stale references
            CreateModifyHandler* reusedHandler = dynamic_cast<CreateModifyHandler*>(registeredHandler);
            if (reusedHandler != nullptr) {
                // Delete all ModifyHandlers in the reused handler to avoid holding stale references
                for (auto iter = reusedHandler->mModifyHandlerPtrMap.begin();
                     iter != reusedHandler->mModifyHandlerPtrMap.end();
                     ++iter) {
                    delete iter->second;
                }
                reusedHandler->mModifyHandlerPtrMap.clear();
            }
            // Delete the new handler we created since it's not being used
            delete handler;
        }
        ConfigManager::GetInstance()->AddNewHandler(gRootDir, registeredHandler);

        // Pre-create and register handler for link directory (for symlink tests)
        std::string linkDir = gRootDir + PATH_SEPARATOR + "link";
        bfs::create_directories(linkDir);
        EventHandler* linkHandler = new CreateModifyHandler(new CreateHandler());
        EventHandler* registeredLinkHandler = linkHandler;
        bool linkRegistered
            = EventDispatcher::GetInstance()->RegisterEventHandler(linkDir, mConfig, registeredLinkHandler);
        APSARA_TEST_TRUE_FATAL(linkRegistered);
        if (registeredLinkHandler != linkHandler) {
            CreateModifyHandler* reusedHandler = dynamic_cast<CreateModifyHandler*>(registeredLinkHandler);
            if (reusedHandler != nullptr) {
                for (auto iter = reusedHandler->mModifyHandlerPtrMap.begin();
                     iter != reusedHandler->mModifyHandlerPtrMap.end();
                     ++iter) {
                    delete iter->second;
                }
                reusedHandler->mModifyHandlerPtrMap.clear();
            }
            delete linkHandler;
        }
        ConfigManager::GetInstance()->AddNewHandler(linkDir, registeredLinkHandler);

        AppConfig::GetInstance()->mInotifyBlackList.insert(gRootDir);

        // Stop polling threads to avoid interference from automatic events
        PollingDirFile::GetInstance()->Stop();
        PollingModify::GetInstance()->Stop();
    }

    void TearDown() override {
        AppConfig::GetInstance()->mInotifyBlackList.erase(gRootDir);

        // Clean up handler and all ModifyHandlers before cleaning other resources
        EventHandler* handler = EventDispatcher::GetInstance()->GetHandler(gRootDir.c_str());
        if (handler != nullptr) {
            CreateModifyHandler* createModifyHandler = dynamic_cast<CreateModifyHandler*>(handler);
            if (createModifyHandler != nullptr) {
                // Delete all ModifyHandlers to avoid holding stale references
                for (auto iter = createModifyHandler->mModifyHandlerPtrMap.begin();
                     iter != createModifyHandler->mModifyHandlerPtrMap.end();
                     ++iter) {
                    delete iter->second;
                }
                createModifyHandler->mModifyHandlerPtrMap.clear();
                delete createModifyHandler->mCreateHandlerPtr;
                createModifyHandler->mCreateHandlerPtr = nullptr;
            }
        }

        // Also clean up linkDir handler if it exists (for symlink tests)
        std::string linkDir = getLinkDir();
        EventHandler* linkHandler = EventDispatcher::GetInstance()->GetHandler(linkDir.c_str());
        if (linkHandler != nullptr) {
            CreateModifyHandler* createModifyHandler = dynamic_cast<CreateModifyHandler*>(linkHandler);
            if (createModifyHandler != nullptr) {
                for (auto iter = createModifyHandler->mModifyHandlerPtrMap.begin();
                     iter != createModifyHandler->mModifyHandlerPtrMap.end();
                     ++iter) {
                    delete iter->second;
                }
                createModifyHandler->mModifyHandlerPtrMap.clear();
                delete createModifyHandler->mCreateHandlerPtr;
                createModifyHandler->mCreateHandlerPtr = nullptr;
            }
        }

        // Remove configs from FileServer to avoid holding stale pointers
        FileServer::GetInstance()->RemoveFileDiscoveryConfig(mConfigName);
        FileServer::GetInstance()->RemoveFileReaderConfig(mConfigName);
        FileServer::GetInstance()->RemoveMultilineConfig(mConfigName);

        // Clear ConfigManager cache to avoid stale pointers
        ConfigManager::GetInstance()->ClearFilePipelineMatchCache();

        filesystem::remove_all(gRootDir);
        ProcessQueueManager::GetInstance()->Clear();
        LogInput::GetInstance()->CleanEnviroments();
        EventDispatcher::GetInstance()->UnregisterEventHandler(gRootDir);
        ConfigManager::GetInstance()->RemoveHandler(gRootDir);
        // Also unregister linkDir if it was registered
        EventDispatcher::GetInstance()->UnregisterEventHandler(linkDir);
        ConfigManager::GetInstance()->RemoveHandler(linkDir);

        LOG_INFO(sLogger, ("TearDown() end", time(nullptr)));
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

    void writeLog(const std::string& logPath, const std::string& logContent) {
        std::ofstream writer(logPath.c_str(), fstream::out | fstream::app | ios_base::binary);
        writer << logContent;
        writer.close();
    }

    void overwriteLog(const std::string& logPath, const std::string& logContent) {
        std::ofstream writer(logPath.c_str(), fstream::out | fstream::trunc | ios_base::binary);
        writer << logContent;
        writer.close();
    }

    void createSymlink(const std::string& target, const std::string& linkPath) {
        boost::filesystem::remove(linkPath);
        boost::filesystem::create_symlink(target, linkPath);
    }

    // Helper function to setup symlink test environment
    // Creates directory structure: gRootDir/real/real/test.log with directory symlink gRootDir/link ->
    // gRootDir/real/real Returns pair of (realPath, linkPath)
    std::pair<std::string, std::string> setupSymlinkEnvironment(const std::string& content) {
        std::string realDir = gRootDir + PATH_SEPARATOR + "real";
        std::string linkDir = gRootDir + PATH_SEPARATOR + "link";

        // Remove existing link directory if it exists
        bfs::remove(linkDir);

        // Create real directory structure
        bfs::create_directories(realDir);

        // Create real file
        std::string realPath = realDir + PATH_SEPARATOR + "test.log";
        bfs::remove(realPath);
        writeLog(realPath, content);

        // Create directory symlink: link -> real
        createSymlink(realDir, linkDir);

        // linkPath is accessed through the symlink directory
        std::string linkPath = linkDir + PATH_SEPARATOR + "test.log";

        return std::make_pair(realPath, linkPath);
    }

    // Helper function to get link directory path
    std::string getLinkDir() { return gRootDir + PATH_SEPARATOR + "link"; }

    // Helper function to get real directory path
    std::string getRealDir() { return gRootDir + PATH_SEPARATOR + "real"; }

    void ProcessEventSequence(const std::vector<Event*>& events) {
        LogInput* logInput = LogInput::GetInstance();
        EventDispatcher* dispatcher = EventDispatcher::GetInstance();

        for (Event* ev : events) {
            logInput->PushEventQueue(ev);
        }

        for (size_t i = 0; i < events.size(); ++i) {
            Event* ev = logInput->PopEventQueue();
            if (ev != nullptr) {
                logInput->ProcessEvent(dispatcher, ev);
            }
        }
    }

    void ProcessSingleEvent(Event* ev) {
        LogInput* logInput = LogInput::GetInstance();
        EventDispatcher* dispatcher = EventDispatcher::GetInstance();

        // Clear any events that might have been automatically generated by inotify/polling
        // to avoid interference with manually created test events
        while (logInput->mInotifyEventQueue.size() != 0) {
            Event* autoEv = logInput->PopEventQueue();
            if (autoEv != nullptr) {
                delete autoEv;
            }
        }

        // Now process only our manually created event
        if (ev != nullptr) {
            logInput->PushEventQueue(ev);
        }
        while (logInput->mInotifyEventQueue.size() != 0) {
            Event* ev2 = logInput->PopEventQueue();
            LOG_INFO(sLogger, ("Processing event", ToString(ev2->GetType())));
            if (ev2 != nullptr) {
                logInput->ProcessEvent(dispatcher, ev2);
            }
        }
    }

    void VerifyReaderExists(const DevInode& devInode, bool shouldExist) {
        VerifyReaderExists(devInode, shouldExist, gRootDir);
    }

    // Overload for specifying custom directory (e.g., symlink directories)
    void VerifyReaderExists(const DevInode& devInode, bool shouldExist, const std::string& dir) {
        EventHandler* handler = EventDispatcher::GetInstance()->GetHandler(dir.c_str());
        APSARA_TEST_TRUE_FATAL(handler != nullptr);
        CreateModifyHandler* createModifyHandler = dynamic_cast<CreateModifyHandler*>(handler);
        APSARA_TEST_TRUE_FATAL(createModifyHandler != nullptr);

        // Get or create modify handler - it will be created when processing events
        ModifyHandler* modifyHandler = createModifyHandler->GetOrCreateModifyHandler(mConfigName, mConfig);
        APSARA_TEST_TRUE_FATAL(modifyHandler != nullptr);

        bool exists = modifyHandler->mDevInodeReaderMap.find(devInode) != modifyHandler->mDevInodeReaderMap.end();
        APSARA_TEST_EQUAL_FATAL(exists, shouldExist);
    }

    LogFileReaderPtr GetReader(const DevInode& devInode) { return GetReader(devInode, gRootDir); }

    // Overload for specifying custom directory (e.g., symlink directories)
    LogFileReaderPtr GetReader(const DevInode& devInode, const std::string& dir) {
        EventHandler* handler = EventDispatcher::GetInstance()->GetHandler(dir.c_str());
        if (handler == nullptr) {
            return LogFileReaderPtr();
        }
        CreateModifyHandler* createModifyHandler = dynamic_cast<CreateModifyHandler*>(handler);
        if (createModifyHandler == nullptr) {
            return LogFileReaderPtr();
        }

        ModifyHandler* modifyHandler = createModifyHandler->GetOrCreateModifyHandler(mConfigName, mConfig);
        if (modifyHandler == nullptr) {
            return LogFileReaderPtr();
        }

        auto it = modifyHandler->mDevInodeReaderMap.find(devInode);
        if (it != modifyHandler->mDevInodeReaderMap.end()) {
            return it->second;
        }
        return LogFileReaderPtr();
    }

    void VerifyReaderNotRecreated(const DevInode& devInode, const LogFileReaderPtr& originalReader, bool sameReader) {
        VerifyReaderNotRecreated(devInode, originalReader, sameReader, gRootDir);
    }

    // Overload for specifying custom directory (e.g., symlink directories)
    void VerifyReaderNotRecreated(const DevInode& devInode,
                                  const LogFileReaderPtr& originalReader,
                                  bool sameReader,
                                  const std::string& dir) {
        LogFileReaderPtr reader = GetReader(devInode, dir);
        if (!sameReader) {
            // Reader should be recreated (file empty and changed)
            APSARA_TEST_TRUE_FATAL(reader.get() != nullptr);
            // New reader should be a different pointer
            APSARA_TEST_TRUE_FATAL(reader.get() != originalReader.get());
        } else {
            // Reader should NOT be recreated (same reader)
            APSARA_TEST_TRUE_FATAL(reader.get() != nullptr);
            // Reader should be the same pointer
            APSARA_TEST_EQUAL_FATAL(reader.get(), originalReader.get());
        }
    }

    DevInode GetFileDevInode(const std::string& path) { return logtail::GetFileDevInode(path); }
};

std::string LogInputReaderUnittest::gRootDir;
std::string LogInputReaderUnittest::gLogName;

// Normal file test cases
void LogInputReaderUnittest::CreateEmpty_Modify_Modify() {
    LOG_INFO(sLogger, ("TestNormal_CreateModifyModify_EmptyUnchanged() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    writeLog(logPath, "");

    DevInode devInode = GetFileDevInode(logPath);

    // Process create event
    Event* createEv = new Event(gRootDir, gLogName, EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    // Process first modify event
    Event* modifyEv1 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Process second modify event (file unchanged)
    Event* modifyEv2 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, true);
}

void LogInputReaderUnittest::CreateNonEmpty_Modify_Modify() {
    LOG_INFO(sLogger, ("TestNormal_CreateModifyModify_NonEmptyUnchanged() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    writeLog(logPath, "test content\n");

    DevInode devInode = GetFileDevInode(logPath);

    // Process create event
    Event* createEv = new Event(gRootDir, gLogName, EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    // Process first modify event
    Event* modifyEv1 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Process second modify event (file unchanged)
    Event* modifyEv2 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, true);
}

void LogInputReaderUnittest::CreateEmpty_Modify_Change_Close_Modify() {
    LOG_INFO(sLogger, ("TestNormal_CreateModifyCloseModify_EmptyChanged_BeforeClose() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    writeLog(logPath, "");

    DevInode devInode = GetFileDevInode(logPath);

    // Process create event
    Event* createEv = new Event(gRootDir, gLogName, EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    // Process modify event
    Event* modifyEv1 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    std::string newLogPath = gRootDir + PATH_SEPARATOR + "new.log";
    bfs::rename(logPath, newLogPath);

    // Process close event
    Event* closeEv = new Event(gRootDir, gLogName, EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    std::string newLogName = "new.log";
    Event* modifyEv2 = new Event(gRootDir, newLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, false);
}

void LogInputReaderUnittest::CreateEmpty_Modify_Close_Change_Modify() {
    LOG_INFO(sLogger, ("TestNormal_CreateModifyCloseModify_EmptyChanged_AfterClose() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    writeLog(logPath, "");

    DevInode devInode = GetFileDevInode(logPath);

    // Process create event
    Event* createEv = new Event(gRootDir, gLogName, EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    // Process modify event
    Event* modifyEv1 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Process close event FIRST
    Event* closeEv = new Event(gRootDir, gLogName, EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    std::string newLogPath = gRootDir + PATH_SEPARATOR + "new.log";
    bfs::rename(logPath, newLogPath);

    std::string newLogName = "new.log";
    Event* modifyEv2 = new Event(gRootDir, newLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, false);
}

void LogInputReaderUnittest::CreateEmpty_Modify_Close_Modify() {
    LOG_INFO(sLogger, ("TestNormal_CreateModifyCloseModify_EmptyUnchanged() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    writeLog(logPath, "");

    DevInode devInode = GetFileDevInode(logPath);

    // Process create event
    Event* createEv = new Event(gRootDir, gLogName, EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    // Process modify event
    Event* modifyEv1 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Process close event
    Event* closeEv = new Event(gRootDir, gLogName, EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    Event* modifyEv2 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, true);
}

void LogInputReaderUnittest::CreateNonEmpty_Modify_Change_Close_Modify_SameSignature() {
    LOG_INFO(sLogger,
             ("TestNormal_CreateModifyCloseModify_NonEmptyChanged_SameSignature_BeforeClose() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    std::string signature = "test content\n";
    writeLog(logPath, signature);

    DevInode devInode = GetFileDevInode(logPath);

    // Process create event
    Event* createEv = new Event(gRootDir, gLogName, EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    // Process modify event
    Event* modifyEv1 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    std::string newLogPath = gRootDir + PATH_SEPARATOR + "new.log";
    bfs::rename(logPath, newLogPath);

    // Process close event
    Event* closeEv = new Event(gRootDir, gLogName, EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    std::string newLogName = "new.log";
    Event* modifyEv2 = new Event(gRootDir, newLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, true);
}

void LogInputReaderUnittest::CreateNonEmpty_Modify_Close_Change_Modify_SameSignature() {
    LOG_INFO(sLogger,
             ("TestNormal_CreateModifyCloseModify_NonEmptyChanged_SameSignature_AfterClose() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    std::string signature = "test content\n";
    writeLog(logPath, signature);

    DevInode devInode = GetFileDevInode(logPath);

    // Process create event
    Event* createEv = new Event(gRootDir, gLogName, EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    // Process modify event
    Event* modifyEv1 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Process close event FIRST
    Event* closeEv = new Event(gRootDir, gLogName, EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    std::string newLogPath = gRootDir + PATH_SEPARATOR + "new.log";
    bfs::rename(logPath, newLogPath);

    std::string newLogName = "new.log";
    Event* modifyEv2 = new Event(gRootDir, newLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, true);
}

void LogInputReaderUnittest::CreateNonEmpty_Modify_Change_Close_Modify_DiffSignature() {
    LOG_INFO(sLogger,
             ("TestNormal_CreateModifyCloseModify_NonEmptyChanged_DiffSignature_BeforeClose() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    writeLog(logPath, "old content\n");

    DevInode devInode = GetFileDevInode(logPath);

    // Process create event
    Event* createEv = new Event(gRootDir, gLogName, EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    // Process modify event
    Event* modifyEv1 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    std::string newLogPath = gRootDir + PATH_SEPARATOR + "new.log";
    bfs::rename(logPath, newLogPath);
    overwriteLog(newLogPath, "new content\n");

    // Process close event
    Event* closeEv = new Event(gRootDir, gLogName, EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    std::string newLogName = "new.log";
    Event* modifyEv2 = new Event(gRootDir, newLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, false);
}

void LogInputReaderUnittest::CreateNonEmpty_Modify_Close_Change_Modify_DiffSignature() {
    LOG_INFO(sLogger,
             ("TestNormal_CreateModifyCloseModify_NonEmptyChanged_DiffSignature_AfterClose() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    writeLog(logPath, "old content\n");

    DevInode devInode = GetFileDevInode(logPath);

    // Process create event
    Event* createEv = new Event(gRootDir, gLogName, EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    // Process modify event
    Event* modifyEv1 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Process close event FIRST
    Event* closeEv = new Event(gRootDir, gLogName, EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    std::string newLogPath = gRootDir + PATH_SEPARATOR + "new.log";
    bfs::rename(logPath, newLogPath);
    overwriteLog(newLogPath, "new content\n");

    std::string newLogName = "new.log";
    Event* modifyEv2 = new Event(gRootDir, newLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, false);
}

void LogInputReaderUnittest::CreateNonEmpty_Modify_Close_Modify() {
    LOG_INFO(sLogger, ("TestNormal_CreateModifyCloseModify_NonEmptyUnchanged() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    writeLog(logPath, "test content\n");

    DevInode devInode = GetFileDevInode(logPath);

    // Process create event
    Event* createEv = new Event(gRootDir, gLogName, EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    // Process modify event
    Event* modifyEv1 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Process close event
    Event* closeEv = new Event(gRootDir, gLogName, EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    Event* modifyEv2 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, true);
}

void LogInputReaderUnittest::CreateEmpty_Change_Close_Modify() {
    LOG_INFO(sLogger, ("TestNormal_CreateCloseModify_EmptyChanged_BeforeClose() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    writeLog(logPath, "");

    DevInode devInode = GetFileDevInode(logPath);

    // Process create event
    Event* createEv = new Event(gRootDir, gLogName, EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    std::string newLogPath = gRootDir + PATH_SEPARATOR + "new.log";
    bfs::rename(logPath, newLogPath);

    // Process close event
    Event* closeEv = new Event(gRootDir, gLogName, EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    std::string newLogName = "new.log";
    Event* modifyEv = new Event(gRootDir, newLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, false);
}

void LogInputReaderUnittest::CreateEmpty_Close_Change_Modify() {
    LOG_INFO(sLogger, ("TestNormal_CreateCloseModify_EmptyChanged_AfterClose() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    writeLog(logPath, "");

    DevInode devInode = GetFileDevInode(logPath);

    // Process create event
    Event* createEv = new Event(gRootDir, gLogName, EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    // Process close event FIRST
    Event* closeEv = new Event(gRootDir, gLogName, EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    std::string newLogPath = gRootDir + PATH_SEPARATOR + "new.log";
    bfs::rename(logPath, newLogPath);

    std::string newLogName = "new.log";
    Event* modifyEv = new Event(gRootDir, newLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, false);
}

void LogInputReaderUnittest::CreateEmpty_Close_Modify() {
    LOG_INFO(sLogger, ("TestNormal_CreateCloseModify_EmptyUnchanged() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    writeLog(logPath, "");

    DevInode devInode = GetFileDevInode(logPath);

    // Process create event
    Event* createEv = new Event(gRootDir, gLogName, EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    // Process close event
    Event* closeEv = new Event(gRootDir, gLogName, EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    Event* modifyEv = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, true);
}

void LogInputReaderUnittest::CreateNonEmpty_Change_Close_Modify_SameSignature() {
    LOG_INFO(sLogger,
             ("TestNormal_CreateCloseModify_NonEmptyChanged_SameSignature_BeforeClose() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    std::string signature = "test content\n";
    writeLog(logPath, signature);

    DevInode devInode = GetFileDevInode(logPath);

    // Process create event
    Event* createEv = new Event(gRootDir, gLogName, EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    std::string newLogPath = gRootDir + PATH_SEPARATOR + "new.log";
    bfs::rename(logPath, newLogPath);

    // Process close event
    Event* closeEv = new Event(gRootDir, gLogName, EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    std::string newLogName = "new.log";
    Event* modifyEv = new Event(gRootDir, newLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, true);
}

void LogInputReaderUnittest::CreateNonEmpty_Close_Change_Modify_SameSignature() {
    LOG_INFO(sLogger, ("TestNormal_CreateCloseModify_NonEmptyChanged_SameSignature_AfterClose() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    std::string signature = "test content\n";
    writeLog(logPath, signature);

    DevInode devInode = GetFileDevInode(logPath);

    // Process create event
    Event* createEv = new Event(gRootDir, gLogName, EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    // Process close event FIRST
    Event* closeEv = new Event(gRootDir, gLogName, EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    std::string newLogPath = gRootDir + PATH_SEPARATOR + "new.log";
    bfs::rename(logPath, newLogPath);

    std::string newLogName = "new.log";
    Event* modifyEv = new Event(gRootDir, newLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, true);
}

void LogInputReaderUnittest::CreateNonEmpty_Change_Close_Modify_DiffSignature() {
    LOG_INFO(sLogger,
             ("TestNormal_CreateCloseModify_NonEmptyChanged_DiffSignature_BeforeClose() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    writeLog(logPath, "old content\n");

    DevInode devInode = GetFileDevInode(logPath);

    // Process create event
    Event* createEv = new Event(gRootDir, gLogName, EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    std::string newLogPath = gRootDir + PATH_SEPARATOR + "new.log";
    bfs::rename(logPath, newLogPath);
    overwriteLog(newLogPath, "new content\n");

    // Process close event
    Event* closeEv = new Event(gRootDir, gLogName, EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    std::string newLogName = "new.log";
    Event* modifyEv = new Event(gRootDir, newLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, true);
}

void LogInputReaderUnittest::CreateNonEmpty_Close_Change_Modify_DiffSignature() {
    LOG_INFO(sLogger, ("TestNormal_CreateCloseModify_NonEmptyChanged_DiffSignature_AfterClose() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    writeLog(logPath, "old content\n");

    DevInode devInode = GetFileDevInode(logPath);

    // Process create event
    Event* createEv = new Event(gRootDir, gLogName, EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    // Process close event FIRST
    Event* closeEv = new Event(gRootDir, gLogName, EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    std::string newLogPath = gRootDir + PATH_SEPARATOR + "new.log";
    bfs::rename(logPath, newLogPath);
    overwriteLog(newLogPath, "new content\n");

    std::string newLogName = "new.log";
    Event* modifyEv = new Event(gRootDir, newLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, true);
}

void LogInputReaderUnittest::CreateNonEmpty_Close_Modify() {
    LOG_INFO(sLogger, ("TestNormal_CreateCloseModify_NonEmptyUnchanged() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    writeLog(logPath, "test content\n");

    DevInode devInode = GetFileDevInode(logPath);

    // Process create event
    Event* createEv = new Event(gRootDir, gLogName, EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    // Process close event
    Event* closeEv = new Event(gRootDir, gLogName, EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    Event* modifyEv = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, true);
}

#ifndef _MSC_VER
// Symbolic link test cases
void LogInputReaderUnittest::SymlinkCreateEmpty_Modify_Modify() {
    LOG_INFO(sLogger, ("TestSymlink_CreateModifyModify_EmptyUnchanged() begin", time(nullptr)));

    // Setup symlink environment: real/real/test.log with link -> real/real
    auto [realPath, linkPath] = setupSymlinkEnvironment("");
    std::string linkDir = getLinkDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process create event
    Event* createEv = new Event(linkDir, "test.log", EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation - search in linkDir's handler
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Process first modify event
    Event* modifyEv1 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Process second modify event (file unchanged)
    Event* modifyEv2 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, true, linkDir);
}

void LogInputReaderUnittest::SymlinkCreateNonEmpty_Modify_Modify() {
    LOG_INFO(sLogger, ("TestSymlink_CreateModifyModify_NonEmptyUnchanged() begin", time(nullptr)));

    // Setup symlink environment: real/real/test.log with link -> real/real
    auto [realPath, linkPath] = setupSymlinkEnvironment("test content\n");
    std::string linkDir = getLinkDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process create event
    Event* createEv = new Event(linkDir, "test.log", EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation - search in linkDir's handler
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Process first modify event
    Event* modifyEv1 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Process second modify event (file unchanged)
    Event* modifyEv2 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, true, linkDir);
}

void LogInputReaderUnittest::SymlinkCreateEmpty_Modify_Change_Close_Modify() {
    LOG_INFO(sLogger, ("TestSymlink_CreateModifyCloseModify_EmptyChanged_BeforeClose() begin", time(nullptr)));

    // Setup symlink environment: real/real/test.log with link -> real/real
    auto [realPath, linkPath] = setupSymlinkEnvironment("");
    std::string linkDir = getLinkDir();
    std::string realDir = getRealDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process create event
    Event* createEv = new Event(linkDir, "test.log", EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Process modify event
    Event* modifyEv1 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation and first modify - search in linkDir's handler
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Real file is renamed to a new file - BEFORE close
    std::string newRealPath = realDir + PATH_SEPARATOR + "new_test.log";
    bfs::rename(realPath, newRealPath);

    // Process close event
    Event* closeEv = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    Event* modifyEv2 = new Event(linkDir, "new_test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, false, linkDir);
}

void LogInputReaderUnittest::SymlinkCreateEmpty_Modify_Close_Change_Modify() {
    LOG_INFO(sLogger, ("TestSymlink_CreateModifyCloseModify_EmptyChanged_AfterClose() begin", time(nullptr)));

    // Setup symlink environment: real/real/test.log with link -> real/real
    auto [realPath, linkPath] = setupSymlinkEnvironment("");
    std::string linkDir = getLinkDir();
    std::string realDir = getRealDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process create event
    Event* createEv = new Event(linkDir, "test.log", EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Process modify event
    Event* modifyEv1 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation and first modify - search in linkDir's handler
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Process close event FIRST
    Event* closeEv = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    // Real file is renamed to a new file - this happens AFTER close
    std::string newRealPath = realDir + PATH_SEPARATOR + "new_test.log";
    bfs::rename(realPath, newRealPath);

    Event* modifyEv2 = new Event(linkDir, "new_test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, false, linkDir);
}

void LogInputReaderUnittest::SymlinkCreateEmpty_Modify_Close_Modify() {
    LOG_INFO(sLogger, ("TestSymlink_CreateModifyCloseModify_EmptyUnchanged() begin", time(nullptr)));

    // Setup symlink environment: real/real/test.log with link -> real/real
    auto [realPath, linkPath] = setupSymlinkEnvironment("");
    std::string linkDir = getLinkDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process create event
    Event* createEv = new Event(linkDir, "test.log", EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Process modify event
    Event* modifyEv1 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation and first modify - search in linkDir's handler
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Process close event
    Event* closeEv = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    Event* modifyEv2 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, true, linkDir);
}

void LogInputReaderUnittest::SymlinkCreateNonEmpty_Modify_Change_Close_Modify_SameSignature() {
    LOG_INFO(sLogger,
             ("TestSymlink_CreateModifyCloseModify_NonEmptyChanged_SameSignature_BeforeClose() begin", time(nullptr)));

    // Setup symlink environment: real/real/test.log with link -> real/real
    std::string signature = "test content\n";
    auto [realPath, linkPath] = setupSymlinkEnvironment(signature);
    std::string linkDir = getLinkDir();
    std::string realDir = getRealDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process create event
    Event* createEv = new Event(linkDir, "test.log", EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Process modify event
    Event* modifyEv1 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation and first modify - search in linkDir's handler
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Real file is renamed to a new file - this happens BEFORE close
    std::string newRealPath = realDir + PATH_SEPARATOR + "new_test.log";
    bfs::rename(realPath, newRealPath);

    // Process close event
    Event* closeEv = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    Event* modifyEv2 = new Event(linkDir, "new_test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, true, linkDir);
}

void LogInputReaderUnittest::SymlinkCreateNonEmpty_Modify_Close_Change_Modify_SameSignature() {
    LOG_INFO(sLogger,
             ("TestSymlink_CreateModifyCloseModify_NonEmptyChanged_SameSignature_AfterClose() begin", time(nullptr)));

    // Setup symlink environment: real/real/test.log with link -> real/real
    std::string signature = "test content\n";
    auto [realPath, linkPath] = setupSymlinkEnvironment(signature);
    std::string linkDir = getLinkDir();
    std::string realDir = getRealDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process create event
    Event* createEv = new Event(linkDir, "test.log", EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Process modify event
    Event* modifyEv1 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation and first modify - search in linkDir's handler
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Process close event FIRST
    Event* closeEv = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    // Real file is renamed to a new file - this happens AFTER close
    std::string newRealPath = realDir + PATH_SEPARATOR + "new_test.log";
    bfs::rename(realPath, newRealPath);

    Event* modifyEv2 = new Event(linkDir, "new_test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, true, linkDir);
}

void LogInputReaderUnittest::SymlinkCreateNonEmpty_Modify_Change_Close_Modify_DiffSignature() {
    LOG_INFO(sLogger,
             ("TestSymlink_CreateModifyCloseModify_NonEmptyChanged_DiffSignature_BeforeClose() begin", time(nullptr)));

    // Setup symlink environment: real/real/test.log with link -> real/real
    auto [realPath, linkPath] = setupSymlinkEnvironment("old content\n");
    std::string linkDir = getLinkDir();
    std::string realDir = getRealDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process create event
    Event* createEv = new Event(linkDir, "test.log", EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Process modify event
    Event* modifyEv1 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation and first modify - search in linkDir's handler
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Real file is renamed to a new file - this happens BEFORE close
    // Create new symlink pointing to the new real file
    std::string newRealPath = realDir + PATH_SEPARATOR + "new_test.log";
    bfs::rename(realPath, newRealPath);
    overwriteLog(newRealPath, "new content\n");

    // Process close event
    Event* closeEv = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    Event* modifyEv2 = new Event(linkDir, "new_test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, false, linkDir);
}

void LogInputReaderUnittest::SymlinkCreateNonEmpty_Modify_Close_Change_Modify_DiffSignature() {
    LOG_INFO(sLogger,
             ("TestSymlink_CreateModifyCloseModify_NonEmptyChanged_DiffSignature_AfterClose() begin", time(nullptr)));

    // Setup symlink environment: real/real/test.log with link -> real/real
    auto [realPath, linkPath] = setupSymlinkEnvironment("old content\n");
    std::string linkDir = getLinkDir();
    std::string realDir = getRealDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process create event
    Event* createEv = new Event(linkDir, "test.log", EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Process modify event
    Event* modifyEv1 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation and first modify - search in linkDir's handler
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Process close event FIRST
    Event* closeEv = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    // Real file is renamed to a new file - this happens AFTER close
    // Since linkDir is a directory symlink, no need to recreate file symlinks
    std::string newRealPath = realDir + PATH_SEPARATOR + "new_test.log";
    bfs::rename(realPath, newRealPath);
    overwriteLog(newRealPath, "new content\n");

    Event* modifyEv2 = new Event(linkDir, "new_test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, false, linkDir);
}

void LogInputReaderUnittest::SymlinkCreateNonEmpty_Modify_Close_Modify() {
    LOG_INFO(sLogger, ("TestSymlink_CreateModifyCloseModify_NonEmptyUnchanged() begin", time(nullptr)));

    // Setup symlink environment: real/real/test.log with link -> real/real
    auto [realPath, linkPath] = setupSymlinkEnvironment("test content\n");
    std::string linkDir = getLinkDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process create event
    Event* createEv = new Event(linkDir, "test.log", EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Process modify event
    Event* modifyEv1 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation and first modify - search in linkDir's handler
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Process close event
    Event* closeEv = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    Event* modifyEv2 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, true, linkDir);
}

void LogInputReaderUnittest::SymlinkCreateEmpty_Change_Close_Modify() {
    LOG_INFO(sLogger, ("TestSymlink_CreateCloseModify_EmptyChanged_BeforeClose() begin", time(nullptr)));

    auto [realPath, linkPath] = setupSymlinkEnvironment("");
    std::string linkDir = getLinkDir();
    std::string realDir = getRealDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process create event
    Event* createEv = new Event(linkDir, "test.log", EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Real file is renamed to a new file - BEFORE close
    // Since linkDir is a directory symlink, no need to recreate file symlinks
    std::string newRealPath = realDir + PATH_SEPARATOR + "new_test.log";
    bfs::rename(realPath, newRealPath);

    // Process close event
    Event* closeEv = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    Event* modifyEv = new Event(linkDir, "new_test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv);

    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, false, linkDir);
}

void LogInputReaderUnittest::SymlinkCreateEmpty_Close_Change_Modify() {
    LOG_INFO(sLogger, ("TestSymlink_CreateCloseModify_EmptyChanged_AfterClose() begin", time(nullptr)));

    auto [realPath, linkPath] = setupSymlinkEnvironment("");
    std::string linkDir = getLinkDir();
    std::string realDir = getRealDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process create event
    Event* createEv = new Event(linkDir, "test.log", EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Process close event FIRST
    Event* closeEv = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    // Real file is renamed to a new file - this happens AFTER close
    // Since linkDir is a directory symlink, no need to recreate file symlinks
    std::string newRealPath = realDir + PATH_SEPARATOR + "new_test.log";
    bfs::rename(realPath, newRealPath);

    Event* modifyEv = new Event(linkDir, "new_test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, false, linkDir);
}

void LogInputReaderUnittest::SymlinkCreateEmpty_Close_Modify() {
    LOG_INFO(sLogger, ("TestSymlink_CreateCloseModify_EmptyUnchanged() begin", time(nullptr)));

    auto [realPath, linkPath] = setupSymlinkEnvironment("");
    std::string linkDir = getLinkDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process create event
    Event* createEv = new Event(linkDir, "test.log", EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation - search in linkDir's handler
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Process close event
    Event* closeEv = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    Event* modifyEv = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, true, linkDir);
}

void LogInputReaderUnittest::SymlinkCreateNonEmpty_Change_Close_Modify_SameSignature() {
    LOG_INFO(sLogger,
             ("TestSymlink_CreateCloseModify_NonEmptyChanged_SameSignature_BeforeClose() begin", time(nullptr)));

    // Setup symlink environment: real/real/test.log with link -> real/real
    std::string signature = "test content\n";
    auto [realPath, linkPath] = setupSymlinkEnvironment(signature);
    std::string linkDir = getLinkDir();
    std::string realDir = getRealDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process create event
    Event* createEv = new Event(linkDir, "test.log", EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Real file is renamed to a new file - this happens BEFORE close
    std::string newRealPath = realDir + PATH_SEPARATOR + "new_test.log";
    bfs::rename(realPath, newRealPath);

    // Process close event
    Event* closeEv = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    Event* modifyEv = new Event(linkDir, "new_test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, true, linkDir);
}

void LogInputReaderUnittest::SymlinkCreateNonEmpty_Close_Change_Modify_SameSignature() {
    LOG_INFO(sLogger,
             ("TestSymlink_CreateCloseModify_NonEmptyChanged_SameSignature_AfterClose() begin", time(nullptr)));

    std::string signature = "test content\n";
    auto [realPath, linkPath] = setupSymlinkEnvironment(signature);
    std::string linkDir = getLinkDir();
    std::string realDir = getRealDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process create event
    Event* createEv = new Event(linkDir, "test.log", EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Process close event FIRST
    Event* closeEv = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    // Real file is renamed to a new file - this happens AFTER close
    // Since linkDir is a directory symlink, no need to recreate file symlinks
    std::string newRealPath = realDir + PATH_SEPARATOR + "new_test.log";
    bfs::rename(realPath, newRealPath);

    Event* modifyEv = new Event(linkDir, "new_test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, true, linkDir);
}

void LogInputReaderUnittest::SymlinkCreateNonEmpty_Change_Close_Modify_DiffSignature() {
    LOG_INFO(sLogger,
             ("TestSymlink_CreateCloseModify_NonEmptyChanged_DiffSignature_BeforeClose() begin", time(nullptr)));

    auto [realPath, linkPath] = setupSymlinkEnvironment("old content\n");
    std::string linkDir = getLinkDir();
    std::string realDir = getRealDir();

    DevInode devInode = GetFileDevInode(realPath);

    // Process create event
    Event* createEv = new Event(linkDir, "test.log", EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Real file is renamed to a new file - this happens BEFORE close
    std::string newRealPath = realDir + PATH_SEPARATOR + "new_test.log";
    bfs::rename(realPath, newRealPath);
    overwriteLog(newRealPath, "new content\n");

    // Process close event
    Event* closeEv = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    Event* modifyEv = new Event(linkDir, "new_test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, true, linkDir);
}

void LogInputReaderUnittest::SymlinkCreateNonEmpty_Close_Change_Modify_DiffSignature() {
    LOG_INFO(sLogger,
             ("TestSymlink_CreateCloseModify_NonEmptyChanged_DiffSignature_AfterClose() begin", time(nullptr)));

    auto [realPath, linkPath] = setupSymlinkEnvironment("old content\n");
    std::string linkDir = getLinkDir();
    std::string realDir = getRealDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process create event
    Event* createEv = new Event(linkDir, "test.log", EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Process close event FIRST
    Event* closeEv = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    // Real file is renamed to a new file - this happens AFTER close
    // Since linkDir is a directory symlink, no need to recreate file symlinks
    std::string newRealPath = realDir + PATH_SEPARATOR + "new_test.log";
    bfs::rename(realPath, newRealPath);
    overwriteLog(newRealPath, "new content\n");

    Event* modifyEv = new Event(linkDir, "new_test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, true, linkDir);
}

void LogInputReaderUnittest::SymlinkCreateNonEmpty_Close_Modify() {
    LOG_INFO(sLogger, ("TestSymlink_CreateCloseModify_NonEmptyUnchanged() begin", time(nullptr)));

    auto [realPath, linkPath] = setupSymlinkEnvironment("test content\n");
    std::string linkDir = getLinkDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process create event
    Event* createEv = new Event(linkDir, "test.log", EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation - search in linkDir's handler
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Process close event
    Event* closeEv = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    Event* modifyEv = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, true, linkDir);
}

void LogInputReaderUnittest::SymlinkCreateEmpty_Close_ModifyWrite_Change_Close_Modify() {
    LOG_INFO(sLogger, ("TestSymlink_CreateCloseModifyChangeCloseModify_Empty() begin", time(nullptr)));

    auto [realPath, linkPath] = setupSymlinkEnvironment("");
    std::string linkDir = getLinkDir();
    std::string realDir = getRealDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process create event
    Event* createEv = new Event(linkDir, "test.log", EVENT_CREATE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(createEv);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Process close event
    Event* closeEv1 = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv1);

    // Process modify event
    writeLog(realPath, "test content\n");
    Event* modifyEv1 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Real file is renamed to a new file - this happens after first modify
    std::string newRealPath = realDir + PATH_SEPARATOR + "new_test.log";
    bfs::rename(realPath, newRealPath);

    // Process close event again with new file name
    Event* closeEv2 = new Event(linkDir, "new_test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv2);

    // Process modify event with new file name
    Event* modifyEv2 = new Event(linkDir, "new_test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, true, linkDir);
}
#endif

// Normal file test cases - reader created by modify event
void LogInputReaderUnittest::ModifyEmpty_Modify_Modify() {
    LOG_INFO(sLogger, ("TestNormal_ModifyModifyModify_EmptyUnchanged() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    writeLog(logPath, "");

    DevInode devInode = GetFileDevInode(logPath);

    // Process first modify event (creates reader)
    Event* modifyEv1 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    // Process second modify event (file unchanged)
    Event* modifyEv2 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    // Process third modify event (file unchanged)
    Event* modifyEv3 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv3);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, true);
}

void LogInputReaderUnittest::ModifyNonEmpty_Modify_Modify() {
    LOG_INFO(sLogger, ("TestNormal_ModifyModifyModify_NonEmptyUnchanged() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    writeLog(logPath, "test content\n");

    DevInode devInode = GetFileDevInode(logPath);

    // Process first modify event (creates reader)
    Event* modifyEv1 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    // Process second modify event (file unchanged)
    Event* modifyEv2 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    // Process third modify event (file unchanged)
    Event* modifyEv3 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv3);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, true);
}

void LogInputReaderUnittest::ModifyEmpty_Change_Close_Modify() {
    LOG_INFO(sLogger, ("TestNormal_ModifyCloseModify_EmptyChanged_BeforeClose() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    writeLog(logPath, "");

    DevInode devInode = GetFileDevInode(logPath);

    // Process first modify event (creates reader)
    Event* modifyEv1 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    std::string newLogPath = gRootDir + PATH_SEPARATOR + "new.log";
    bfs::rename(logPath, newLogPath);

    // Process close event
    Event* closeEv = new Event(gRootDir, gLogName, EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    std::string newLogName = "new.log";
    Event* modifyEv2 = new Event(gRootDir, newLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, false);
}

void LogInputReaderUnittest::ModifyEmpty_Close_Change_Modify() {
    LOG_INFO(sLogger, ("TestNormal_ModifyCloseModify_EmptyChanged_AfterClose() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    writeLog(logPath, "");

    DevInode devInode = GetFileDevInode(logPath);

    // Process first modify event (creates reader)
    Event* modifyEv1 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    // Process close event FIRST
    Event* closeEv = new Event(gRootDir, gLogName, EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    std::string newLogPath = gRootDir + PATH_SEPARATOR + "new.log";
    bfs::rename(logPath, newLogPath);

    std::string newLogName = "new.log";
    Event* modifyEv2 = new Event(gRootDir, newLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, false);
}

void LogInputReaderUnittest::ModifyEmpty_Close_Modify() {
    LOG_INFO(sLogger, ("TestNormal_ModifyCloseModify_EmptyUnchanged() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    writeLog(logPath, "");

    DevInode devInode = GetFileDevInode(logPath);

    // Process first modify event (creates reader)
    Event* modifyEv1 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    // Process close event
    Event* closeEv = new Event(gRootDir, gLogName, EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    Event* modifyEv2 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, true);
}

void LogInputReaderUnittest::ModifyNonEmpty_Change_Close_Modify_SameSignature() {
    LOG_INFO(sLogger,
             ("TestNormal_ModifyCloseModify_NonEmptyChanged_SameSignature_BeforeClose() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    std::string signature = "test content\n";
    writeLog(logPath, signature);

    DevInode devInode = GetFileDevInode(logPath);

    // Process first modify event (creates reader)
    Event* modifyEv1 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    std::string newLogPath = gRootDir + PATH_SEPARATOR + "new.log";
    bfs::rename(logPath, newLogPath);

    // Process close event
    Event* closeEv = new Event(gRootDir, gLogName, EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    std::string newLogName = "new.log";
    Event* modifyEv2 = new Event(gRootDir, newLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, true);
}

void LogInputReaderUnittest::ModifyNonEmpty_Close_Change_Modify_SameSignature() {
    LOG_INFO(sLogger, ("TestNormal_ModifyCloseModify_NonEmptyChanged_SameSignature_AfterClose() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    std::string signature = "test content\n";
    writeLog(logPath, signature);

    DevInode devInode = GetFileDevInode(logPath);

    // Process first modify event (creates reader)
    Event* modifyEv1 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    // Process close event FIRST
    Event* closeEv = new Event(gRootDir, gLogName, EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    std::string newLogPath = gRootDir + PATH_SEPARATOR + "new.log";
    bfs::rename(logPath, newLogPath);

    std::string newLogName = "new.log";
    Event* modifyEv2 = new Event(gRootDir, newLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, true);
}

void LogInputReaderUnittest::ModifyNonEmpty_Change_Close_Modify_DiffSignature() {
    LOG_INFO(sLogger,
             ("TestNormal_ModifyCloseModify_NonEmptyChanged_DiffSignature_BeforeClose() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    writeLog(logPath, "old content\n");

    DevInode devInode = GetFileDevInode(logPath);

    // Process first modify event (creates reader)
    Event* modifyEv1 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    std::string newLogPath = gRootDir + PATH_SEPARATOR + "new.log";
    bfs::rename(logPath, newLogPath);
    overwriteLog(newLogPath, "new content\n");

    // Process close event
    Event* closeEv = new Event(gRootDir, gLogName, EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    std::string newLogName = "new.log";
    Event* modifyEv2 = new Event(gRootDir, newLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, false);
}

void LogInputReaderUnittest::ModifyNonEmpty_Close_Change_Modify_DiffSignature() {
    LOG_INFO(sLogger, ("TestNormal_ModifyCloseModify_NonEmptyChanged_DiffSignature_AfterClose() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    writeLog(logPath, "old content\n");

    DevInode devInode = GetFileDevInode(logPath);

    // Process first modify event (creates reader)
    Event* modifyEv1 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    // Process close event FIRST
    Event* closeEv = new Event(gRootDir, gLogName, EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    std::string newLogPath = gRootDir + PATH_SEPARATOR + "new.log";
    bfs::rename(logPath, newLogPath);
    overwriteLog(newLogPath, "new content\n");

    std::string newLogName = "new.log";
    Event* modifyEv2 = new Event(gRootDir, newLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, false);
}

void LogInputReaderUnittest::ModifyNonEmpty_Close_Modify() {
    LOG_INFO(sLogger, ("TestNormal_ModifyCloseModify_NonEmptyUnchanged() begin", time(nullptr)));
    std::string logPath = gRootDir + PATH_SEPARATOR + gLogName;
    bfs::remove(logPath);
    writeLog(logPath, "test content\n");

    DevInode devInode = GetFileDevInode(logPath);

    // Process first modify event (creates reader)
    Event* modifyEv1 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode);

    // Process close event
    Event* closeEv = new Event(gRootDir, gLogName, EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    Event* modifyEv2 = new Event(gRootDir, gLogName, EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    VerifyReaderExists(devInode, true);
    VerifyReaderNotRecreated(devInode, originalReader, true);
}

#ifndef _MSC_VER
// Symbolic link test cases - reader created by modify event
void LogInputReaderUnittest::SymlinkModifyEmpty_Modify_Modify() {
    LOG_INFO(sLogger, ("TestSymlink_ModifyModifyModify_EmptyUnchanged() begin", time(nullptr)));

    auto [realPath, linkPath] = setupSymlinkEnvironment("");
    std::string linkDir = getLinkDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process first modify event (creates reader)
    Event* modifyEv1 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation - search in linkDir's handler
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Process second modify event (file unchanged)
    Event* modifyEv2 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    // Process third modify event (file unchanged)
    Event* modifyEv3 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv3);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, true, linkDir);
}

void LogInputReaderUnittest::SymlinkModifyNonEmpty_Modify_Modify() {
    LOG_INFO(sLogger, ("TestSymlink_ModifyModifyModify_NonEmptyUnchanged() begin", time(nullptr)));

    auto [realPath, linkPath] = setupSymlinkEnvironment("test content\n");
    std::string linkDir = getLinkDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process first modify event (creates reader)
    Event* modifyEv1 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation - search in linkDir's handler
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Process second modify event (file unchanged)
    Event* modifyEv2 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    // Process third modify event (file unchanged)
    Event* modifyEv3 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv3);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, true, linkDir);
}

void LogInputReaderUnittest::SymlinkModifyEmpty_Change_Close_Modify() {
    LOG_INFO(sLogger, ("TestSymlink_ModifyCloseModify_EmptyChanged_BeforeClose() begin", time(nullptr)));

    auto [realPath, linkPath] = setupSymlinkEnvironment("");
    std::string linkDir = getLinkDir();
    std::string realDir = getRealDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process first modify event (creates reader)
    Event* modifyEv1 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Real file is renamed to a new file - BEFORE close
    // Since linkDir is a directory symlink, no need to recreate file symlinks
    std::string newRealPath = realDir + PATH_SEPARATOR + "new_test.log";
    bfs::rename(realPath, newRealPath);

    // Process close event
    Event* closeEv = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    Event* modifyEv2 = new Event(linkDir, "new_test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, false, linkDir);
}

void LogInputReaderUnittest::SymlinkModifyEmpty_Close_Change_Modify() {
    LOG_INFO(sLogger, ("TestSymlink_ModifyCloseModify_EmptyChanged_AfterClose() begin", time(nullptr)));

    auto [realPath, linkPath] = setupSymlinkEnvironment("");
    std::string linkDir = getLinkDir();
    std::string realDir = getRealDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process first modify event (creates reader)
    Event* modifyEv1 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Process close event FIRST
    Event* closeEv = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    // Real file is renamed to a new file - this happens AFTER close
    // Since linkDir is a directory symlink, no need to recreate file symlinks
    std::string newRealPath = realDir + PATH_SEPARATOR + "new_test.log";
    std::string newLinkPath = linkDir + PATH_SEPARATOR + "new_test.log";
    bfs::rename(realPath, newRealPath);

    Event* modifyEv2 = new Event(linkDir, "new_test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, false, linkDir);
}

void LogInputReaderUnittest::SymlinkModifyEmpty_Close_Modify() {
    LOG_INFO(sLogger, ("TestSymlink_ModifyCloseModify_EmptyUnchanged() begin", time(nullptr)));

    auto [realPath, linkPath] = setupSymlinkEnvironment("");
    std::string linkDir = getLinkDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process first modify event (creates reader)
    Event* modifyEv1 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation - search in linkDir's handler
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Process close event
    Event* closeEv = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    Event* modifyEv2 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, true, linkDir);
}

void LogInputReaderUnittest::SymlinkModifyNonEmpty_Change_Close_Modify_SameSignature() {
    LOG_INFO(sLogger,
             ("TestSymlink_ModifyCloseModify_NonEmptyChanged_SameSignature_BeforeClose() begin", time(nullptr)));

    std::string signature = "test content\n";
    auto [realPath, linkPath] = setupSymlinkEnvironment(signature);
    std::string linkDir = getLinkDir();
    std::string realDir = getRealDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process first modify event (creates reader)
    Event* modifyEv1 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Real file is renamed to a new file - this happens BEFORE close
    std::string newRealPath = realDir + PATH_SEPARATOR + "new_test.log";
    bfs::rename(realPath, newRealPath);

    // Process close event
    Event* closeEv = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    Event* modifyEv2 = new Event(linkDir, "new_test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, true, linkDir);
}

void LogInputReaderUnittest::SymlinkModifyNonEmpty_Close_Change_Modify_SameSignature() {
    LOG_INFO(sLogger,
             ("TestSymlink_ModifyCloseModify_NonEmptyChanged_SameSignature_AfterClose() begin", time(nullptr)));

    std::string signature = "test content\n";
    auto [realPath, linkPath] = setupSymlinkEnvironment(signature);
    std::string linkDir = getLinkDir();
    std::string realDir = getRealDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process first modify event (creates reader)
    Event* modifyEv1 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Process close event FIRST
    Event* closeEv = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    // Real file is renamed to a new file - this happens AFTER close
    // Since linkDir is a directory symlink, no need to recreate file symlinks
    std::string newRealPath = realDir + PATH_SEPARATOR + "new_test.log";
    bfs::rename(realPath, newRealPath);

    Event* modifyEv2 = new Event(linkDir, "new_test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, true, linkDir);
}

void LogInputReaderUnittest::SymlinkModifyNonEmpty_Change_Close_Modify_DiffSignature() {
    LOG_INFO(sLogger,
             ("TestSymlink_ModifyCloseModify_NonEmptyChanged_DiffSignature_BeforeClose() begin", time(nullptr)));

    auto [realPath, linkPath] = setupSymlinkEnvironment("old content\n");
    std::string linkDir = getLinkDir();
    std::string realDir = getRealDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process first modify event (creates reader)
    Event* modifyEv1 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Real file is renamed to a new file - this happens BEFORE close
    // Create new symlink pointing to the new real file
    std::string newRealPath = realDir + PATH_SEPARATOR + "new_test.log";
    bfs::rename(realPath, newRealPath);
    overwriteLog(newRealPath, "new content\n");

    // Process close event
    Event* closeEv = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    Event* modifyEv2 = new Event(linkDir, "new_test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, false, linkDir);
}

void LogInputReaderUnittest::SymlinkModifyNonEmpty_Close_Change_Modify_DiffSignature() {
    LOG_INFO(sLogger,
             ("TestSymlink_ModifyCloseModify_NonEmptyChanged_DiffSignature_AfterClose() begin", time(nullptr)));

    auto [realPath, linkPath] = setupSymlinkEnvironment("old content\n");
    std::string linkDir = getLinkDir();
    std::string realDir = getRealDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process first modify event (creates reader)
    Event* modifyEv1 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Process close event FIRST
    Event* closeEv = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    // Real file is renamed to a new file - this happens AFTER close
    std::string newRealPath = realDir + PATH_SEPARATOR + "new_test.log";
    bfs::rename(realPath, newRealPath);
    overwriteLog(newRealPath, "new content\n");

    Event* modifyEv2 = new Event(linkDir, "new_test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, false, linkDir);
}

void LogInputReaderUnittest::SymlinkModifyNonEmpty_Close_Modify() {
    LOG_INFO(sLogger, ("TestSymlink_ModifyCloseModify_NonEmptyUnchanged() begin", time(nullptr)));

    auto [realPath, linkPath] = setupSymlinkEnvironment("test content\n");
    std::string linkDir = getLinkDir();

    // For symlink, event path is symlink path, but dev/inode should be from real file
    DevInode devInode = GetFileDevInode(realPath);

    // Process first modify event (creates reader)
    Event* modifyEv1 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv1);

    // Get reader pointer after creation - search in linkDir's handler
    LogFileReaderPtr originalReader = GetReader(devInode, linkDir);

    // Process close event
    Event* closeEv = new Event(linkDir, "test.log", EVENT_DELETE, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(closeEv);

    Event* modifyEv2 = new Event(linkDir, "test.log", EVENT_MODIFY, 0, 0, devInode.dev, devInode.inode);
    ProcessSingleEvent(modifyEv2);

    // Verify reader exists and was not recreated - search in linkDir's handler
    VerifyReaderExists(devInode, true, linkDir);
    VerifyReaderNotRecreated(devInode, originalReader, true, linkDir);
}
#endif

// Register all test cases
UNIT_TEST_CASE(LogInputReaderUnittest, CreateEmpty_Modify_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, CreateNonEmpty_Modify_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, CreateEmpty_Modify_Change_Close_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, CreateEmpty_Modify_Close_Change_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, CreateEmpty_Modify_Close_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, CreateNonEmpty_Modify_Change_Close_Modify_SameSignature);
UNIT_TEST_CASE(LogInputReaderUnittest, CreateNonEmpty_Modify_Close_Change_Modify_SameSignature);
UNIT_TEST_CASE(LogInputReaderUnittest, CreateNonEmpty_Modify_Change_Close_Modify_DiffSignature);
UNIT_TEST_CASE(LogInputReaderUnittest, CreateNonEmpty_Modify_Close_Change_Modify_DiffSignature);
UNIT_TEST_CASE(LogInputReaderUnittest, CreateNonEmpty_Modify_Close_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, CreateEmpty_Change_Close_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, CreateEmpty_Close_Change_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, CreateEmpty_Close_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, CreateNonEmpty_Change_Close_Modify_SameSignature);
UNIT_TEST_CASE(LogInputReaderUnittest, CreateNonEmpty_Close_Change_Modify_SameSignature);
UNIT_TEST_CASE(LogInputReaderUnittest, CreateNonEmpty_Change_Close_Modify_DiffSignature);
UNIT_TEST_CASE(LogInputReaderUnittest, CreateNonEmpty_Close_Change_Modify_DiffSignature);
UNIT_TEST_CASE(LogInputReaderUnittest, CreateNonEmpty_Close_Modify);

// Normal file test cases - reader created by modify event
UNIT_TEST_CASE(LogInputReaderUnittest, ModifyEmpty_Modify_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, ModifyNonEmpty_Modify_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, ModifyEmpty_Change_Close_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, ModifyEmpty_Close_Change_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, ModifyEmpty_Close_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, ModifyNonEmpty_Change_Close_Modify_SameSignature);
UNIT_TEST_CASE(LogInputReaderUnittest, ModifyNonEmpty_Close_Change_Modify_SameSignature);
UNIT_TEST_CASE(LogInputReaderUnittest, ModifyNonEmpty_Change_Close_Modify_DiffSignature);
UNIT_TEST_CASE(LogInputReaderUnittest, ModifyNonEmpty_Close_Change_Modify_DiffSignature);
UNIT_TEST_CASE(LogInputReaderUnittest, ModifyNonEmpty_Close_Modify);

#ifndef _MSC_VER // Unnecessary on platforms without symbolic link support
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkCreateEmpty_Modify_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkCreateNonEmpty_Modify_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkCreateEmpty_Modify_Change_Close_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkCreateEmpty_Modify_Close_Change_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkCreateEmpty_Modify_Close_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkCreateNonEmpty_Modify_Change_Close_Modify_SameSignature);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkCreateNonEmpty_Modify_Close_Change_Modify_SameSignature);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkCreateNonEmpty_Modify_Change_Close_Modify_DiffSignature);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkCreateNonEmpty_Modify_Close_Change_Modify_DiffSignature);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkCreateNonEmpty_Modify_Close_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkCreateEmpty_Change_Close_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkCreateEmpty_Close_Change_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkCreateEmpty_Close_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkCreateEmpty_Close_ModifyWrite_Change_Close_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkCreateNonEmpty_Change_Close_Modify_SameSignature);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkCreateNonEmpty_Close_Change_Modify_SameSignature);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkCreateNonEmpty_Change_Close_Modify_DiffSignature);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkCreateNonEmpty_Close_Change_Modify_DiffSignature);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkCreateNonEmpty_Close_Modify);

// Symbolic link test cases - reader created by modify event
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkModifyEmpty_Modify_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkModifyNonEmpty_Modify_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkModifyEmpty_Change_Close_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkModifyEmpty_Close_Change_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkModifyEmpty_Close_Modify);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkModifyNonEmpty_Change_Close_Modify_SameSignature);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkModifyNonEmpty_Close_Change_Modify_SameSignature);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkModifyNonEmpty_Change_Close_Modify_DiffSignature);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkModifyNonEmpty_Close_Change_Modify_DiffSignature);
UNIT_TEST_CASE(LogInputReaderUnittest, SymlinkModifyNonEmpty_Close_Modify);
#endif

} // end of namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
