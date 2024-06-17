// Copyright 2024 iLogtail Authors
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

#include "common/Constants.h"
#include "common/FileSystemUtil.h"
#include "common/Flags.h"
#include "common/JsonUtil.h"
#include "config_manager/ConfigManager.h"
#include "event/BlockEventManager.h"
#include "event/Event.h"
#include "event_handler/EventHandler.h"
#include "file_server/FileServer.h"
#include "logger/Logger.h"
#include "queue/ProcessQueueManager.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class ForceReadUnittest : public testing::Test {
public:
    void TestTimeoutForceRead();
    void TestFileCloseForceRead();
    void TestAddTimeoutEvent();

protected:
    void SetUp() override {
        logPathDir = GetProcessExecutionDir();
        if (PATH_SEPARATOR[0] == logPathDir.back()) {
            logPathDir.resize(logPathDir.size() - 1);
        }
        logPathDir += PATH_SEPARATOR + "testDataSet" + PATH_SEPARATOR + "ForceReadUnittest";
        utf8File = "utf8.txt";
        std::string filepath = logPathDir + PATH_SEPARATOR + utf8File;
        std::unique_ptr<FILE, decltype(&std::fclose)> fp(std::fopen(filepath.c_str(), "r"), &std::fclose);
        if (!fp.get()) {
            return;
        }
        std::fseek(fp.get(), 0, SEEK_END);
        long filesize = std::ftell(fp.get());
        std::fseek(fp.get(), 0, SEEK_SET);
        expectedContent.reset(new char[filesize + 1]);
        fread(expectedContent.get(), filesize, 1, fp.get());
        expectedContent[filesize] = '\0';
    }

    void Init() {
        // init pipeline and config
        unique_ptr<Json::Value> configJson;
        string configStr, errorMsg;
        unique_ptr<Config> config;
        unique_ptr<Pipeline> pipeline;
        list<ProcessQueue>::iterator que;

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
            + logPathDir + R"(/utf8.txt"
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

        config.reset(new Config(mConfigName, std::move(configJson)));
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
        ProcessQueueManager::GetInstance()->CreateOrUpdateQueue(0, 0);
    }

    void TearDown() override { remove(utf8File.c_str()); }

private:
    std::unique_ptr<char[]> expectedContent;
    static std::string logPathDir;
    static std::string utf8File;
    const std::string mConfigName = "##1.0##project-0$config-0";

    FileDiscoveryOptions discoveryOpts;
    FileReaderOptions readerOpts;
    MultilineOptions multilineOpts;
    PipelineContext ctx;
    FileDiscoveryConfig mConfig;
};

std::string ForceReadUnittest::logPathDir;
std::string ForceReadUnittest::utf8File;

void ForceReadUnittest::TestTimeoutForceRead() {
    {
        // read -> add timeout event -> handle timeout -> valid -> read empty -> not rollback
        Init();
        LogFileReader reader(
            logPathDir, utf8File, DevInode(), std::make_pair(&readerOpts, &ctx), std::make_pair(&multilineOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        reader.CheckFileSignatureAndOffset(true);

        ModifyHandler* pHanlder = new ModifyHandler(mConfigName, mConfig);
        pHanlder->mReadFileTimeSlice = 0; // force one read for one event

        Event e1 = Event(reader.mHostLogPathDir,
                         reader.mHostLogPathFile,
                         EVENT_MODIFY,
                         -1,
                         0,
                         reader.mDevInode.dev,
                         reader.mDevInode.inode);
        pHanlder->Handle(e1);
        std::unique_ptr<ProcessQueueItem> item;
        std::string configName;
        bool result = ProcessQueueManager::GetInstance()->PopItem(1, item, configName);
        APSARA_TEST_TRUE_FATAL(result);
        LogEvent& sourceEvent1 = item.get()->mEventGroup.MutableEvents()[0].Cast<LogEvent>();
        std::string expectedPart1(expectedContent.get());
        expectedPart1.resize(expectedPart1.rfind("\n"));
        APSARA_TEST_STREQ_FATAL(sourceEvent1.GetContent(DEFAULT_CONTENT_KEY).data(), expectedPart1.c_str());

        Event e2 = *pHanlder->mNameReaderMap[utf8File][0]->CreateFlushTimeoutEvent().get();
        pHanlder->Handle(e2);
        result = ProcessQueueManager::GetInstance()->PopItem(1, item, configName);
        APSARA_TEST_TRUE_FATAL(result);
        LogEvent& sourceEvent2 = item.get()->mEventGroup.MutableEvents()[0].Cast<LogEvent>();
        std::string expectedPart2(expectedContent.get());
        expectedPart2 = expectedPart2.substr(expectedPart2.rfind("\n") + 1);
        APSARA_TEST_STREQ_FATAL(sourceEvent2.GetContent(DEFAULT_CONTENT_KEY).data(), expectedPart2.c_str());
    }
    {
        // read -> write -> add timeout event -> handle timeout -> valid -> read not empty -> rollback
        Init();
        LogFileReader reader(
            logPathDir, utf8File, DevInode(), std::make_pair(&readerOpts, &ctx), std::make_pair(&multilineOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        reader.CheckFileSignatureAndOffset(true);

        std::string expectedPart1(expectedContent.get());
        expectedPart1.resize(expectedPart1.find("\n"));
        LogFileReader::BUFFER_SIZE = expectedPart1.size() + 1;
        ModifyHandler* pHanlder = new ModifyHandler(mConfigName, mConfig);
        pHanlder->mReadFileTimeSlice = 0; // force one read for one event

        Event e1 = Event(reader.mHostLogPathDir,
                         reader.mHostLogPathFile,
                         EVENT_MODIFY,
                         -1,
                         0,
                         reader.mDevInode.dev,
                         reader.mDevInode.inode);
        pHanlder->Handle(e1);
        std::unique_ptr<ProcessQueueItem> item;
        std::string configName;
        bool result = ProcessQueueManager::GetInstance()->PopItem(1, item, configName);
        APSARA_TEST_TRUE_FATAL(result);
        LogEvent& sourceEvent1 = item.get()->mEventGroup.MutableEvents()[0].Cast<LogEvent>();
        APSARA_TEST_STREQ_FATAL(sourceEvent1.GetContent(DEFAULT_CONTENT_KEY).data(), expectedPart1.c_str());

        Event e2 = *pHanlder->mNameReaderMap[utf8File][0]->CreateFlushTimeoutEvent().get();
        pHanlder->Handle(e2);
        result = ProcessQueueManager::GetInstance()->PopItem(1, item, configName);
        APSARA_TEST_TRUE_FATAL(result);
        LogEvent& sourceEvent2 = item.get()->mEventGroup.MutableEvents()[0].Cast<LogEvent>();
        std::string expectedPart2(expectedContent.get());
        // should rollback
        expectedPart2
            = expectedPart2.substr(expectedPart2.find("\n") + 1, expectedPart2.rfind("\n") - expectedPart1.size() - 1);
        APSARA_TEST_STREQ_FATAL(sourceEvent2.GetContent(DEFAULT_CONTENT_KEY).data(), expectedPart2.c_str());
    }
    {
        // read -> add timeout event -> write -> read -> handle timeout -> event invalid
        LOG_WARNING(sLogger, ("This case is difficult to test", "test"));
        Init();
        LogFileReader reader(
            logPathDir, utf8File, DevInode(), std::make_pair(&readerOpts, &ctx), std::make_pair(&multilineOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        reader.CheckFileSignatureAndOffset(true);

        std::string expectedPart1(expectedContent.get());
        expectedPart1.resize(expectedPart1.find("\n"));
        LogFileReader::BUFFER_SIZE = expectedPart1.size() + 1;
        ModifyHandler* pHanlder = new ModifyHandler(mConfigName, mConfig);
        pHanlder->mReadFileTimeSlice = 0; // force one read for one event

        Event e1 = Event(reader.mHostLogPathDir,
                         reader.mHostLogPathFile,
                         EVENT_MODIFY,
                         -1,
                         0,
                         reader.mDevInode.dev,
                         reader.mDevInode.inode);
        pHanlder->Handle(e1);
        std::unique_ptr<ProcessQueueItem> item;
        std::string configName;
        bool result = ProcessQueueManager::GetInstance()->PopItem(1, item, configName);
        APSARA_TEST_TRUE_FATAL(result);
        LogEvent& sourceEvent1 = item.get()->mEventGroup.MutableEvents()[0].Cast<LogEvent>();
        APSARA_TEST_STREQ_FATAL(sourceEvent1.GetContent(DEFAULT_CONTENT_KEY).data(), expectedPart1.c_str());

        LogFileReader::BUFFER_SIZE = 1024 * 512;
        Event e3 = *pHanlder->mNameReaderMap[utf8File][0]->CreateFlushTimeoutEvent().get();

        Event e2 = Event(reader.mHostLogPathDir,
                         reader.mHostLogPathFile,
                         EVENT_MODIFY,
                         -1,
                         0,
                         reader.mDevInode.dev,
                         reader.mDevInode.inode);
        pHanlder->Handle(e2);
        result = ProcessQueueManager::GetInstance()->PopItem(1, item, configName);
        APSARA_TEST_TRUE_FATAL(result);
        LogEvent& sourceEvent2 = item.get()->mEventGroup.MutableEvents()[0].Cast<LogEvent>();
        std::string expectedPart2(expectedContent.get());
        // should rollback
        expectedPart2
            = expectedPart2.substr(expectedPart2.find("\n") + 1, expectedPart2.rfind("\n") - expectedPart1.size() - 1);
        APSARA_TEST_STREQ_FATAL(sourceEvent2.GetContent(DEFAULT_CONTENT_KEY).data(), expectedPart2.c_str());

        pHanlder->Handle(e3);
        // Current timeout event is invalid
        result = ProcessQueueManager::GetInstance()->PopItem(1, item, configName);
        APSARA_TEST_FALSE_FATAL(result);
    }
    {
        // TODO: difficult to test, the behavior should be
        // read -> add timeout event -> handle timeout -> write -> event valid -> read not empty -> rollback
    }
    {
        // TODO: difficult to test, the behavior should be
        // read -> add timeout event -> handle timeout -> event valid -> write -> read not empty -> rollback
    }
    {
        // TODO: difficult to test, the behavior should be
        // read -> add timeout event -> handle timeout -> event valid -> read empty -> write -> not rollback
    }
    {
        // TODO: difficult to test, the behavior should be
        // read -> add timeout event -> handle timeout -> event valid -> read empty -> not rollback -> write
    }
}

void ForceReadUnittest::TestFileCloseForceRead() {
    {
        // file close -> handle timeout -> valid -> not rollback
        Init();
        LogFileReader reader(
            logPathDir, utf8File, DevInode(), std::make_pair(&readerOpts, &ctx), std::make_pair(&multilineOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        reader.CheckFileSignatureAndOffset(true);
        LogFileReader::BUFFER_SIZE = 1024 * 512;

        ModifyHandler* pHanlder = new ModifyHandler(mConfigName, mConfig);
        pHanlder->mReadFileTimeSlice = 0; // force one read for one event

        Event e1 = Event(reader.mHostLogPathDir,
                         reader.mHostLogPathFile,
                         EVENT_MODIFY,
                         -1,
                         0,
                         reader.mDevInode.dev,
                         reader.mDevInode.inode);
        pHanlder->Handle(e1);
        std::unique_ptr<ProcessQueueItem> item;
        std::string configName;
        bool result = ProcessQueueManager::GetInstance()->PopItem(1, item, configName);
        APSARA_TEST_TRUE_FATAL(result);
        LogEvent& sourceEvent1 = item.get()->mEventGroup.MutableEvents()[0].Cast<LogEvent>();
        std::string expectedPart1(expectedContent.get());
        expectedPart1.resize(expectedPart1.rfind("\n"));
        APSARA_TEST_STREQ_FATAL(sourceEvent1.GetContent(DEFAULT_CONTENT_KEY).data(), expectedPart1.c_str());

        Event e2 = *pHanlder->mNameReaderMap[utf8File][0]->CreateFlushTimeoutEvent().get();
        pHanlder->mNameReaderMap[utf8File][0]->CloseFilePtr();

        pHanlder->Handle(e2);
        result = ProcessQueueManager::GetInstance()->PopItem(1, item, configName);
        APSARA_TEST_TRUE_FATAL(result);
        LogEvent& sourceEvent2 = item.get()->mEventGroup.MutableEvents()[0].Cast<LogEvent>();
        std::string expectedPart2(expectedContent.get());
        expectedPart2 = expectedPart2.substr(expectedPart2.rfind("\n") + 1);
        APSARA_TEST_STREQ_FATAL(sourceEvent2.GetContent(DEFAULT_CONTENT_KEY).data(), expectedPart2.c_str());
    }
}

void ForceReadUnittest::TestAddTimeoutEvent() {
    {
        // read part -> not add timeout event
        Init();
        LogFileReader reader(
            logPathDir, utf8File, DevInode(), std::make_pair(&readerOpts, &ctx), std::make_pair(&multilineOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        reader.CheckFileSignatureAndOffset(true);
        LogFileReader::BUFFER_SIZE = 10;
        BlockedEventManager::GetInstance()->mBlockEventMap.clear();
        APSARA_TEST_EQUAL_FATAL(BlockedEventManager::GetInstance()->mBlockEventMap.size(), 0);

        ModifyHandler* pHanlder = new ModifyHandler(mConfigName, mConfig);
        pHanlder->mReadFileTimeSlice = 0; // force one read for one event

        Event e1 = Event(reader.mHostLogPathDir,
                         reader.mHostLogPathFile,
                         EVENT_MODIFY,
                         -1,
                         0,
                         reader.mDevInode.dev,
                         reader.mDevInode.inode);
        pHanlder->Handle(e1);
        APSARA_TEST_EQUAL_FATAL(BlockedEventManager::GetInstance()->mBlockEventMap.size(), 0);
    }
    {
        // read all -> add timeout event
        Init();
        LogFileReader reader(
            logPathDir, utf8File, DevInode(), std::make_pair(&readerOpts, &ctx), std::make_pair(&multilineOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        reader.CheckFileSignatureAndOffset(true);
        LogFileReader::BUFFER_SIZE = 1024 * 512;
        BlockedEventManager::GetInstance()->mBlockEventMap.clear();
        APSARA_TEST_EQUAL_FATAL(BlockedEventManager::GetInstance()->mBlockEventMap.size(), 0);

        ModifyHandler* pHanlder = new ModifyHandler(mConfigName, mConfig);
        pHanlder->mReadFileTimeSlice = 0; // force one read for one event

        Event e1 = Event(reader.mHostLogPathDir,
                         reader.mHostLogPathFile,
                         EVENT_MODIFY,
                         -1,
                         0,
                         reader.mDevInode.dev,
                         reader.mDevInode.inode);
        pHanlder->Handle(e1);
        APSARA_TEST_EQUAL_FATAL(BlockedEventManager::GetInstance()->mBlockEventMap.size(), 1);
    }
}

UNIT_TEST_CASE(ForceReadUnittest, TestTimeoutForceRead)
UNIT_TEST_CASE(ForceReadUnittest, TestFileCloseForceRead)
UNIT_TEST_CASE(ForceReadUnittest, TestAddTimeoutEvent)

} // namespace logtail

UNIT_TEST_MAIN