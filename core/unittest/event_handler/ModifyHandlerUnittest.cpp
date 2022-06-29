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

#include "unittest/Unittest.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string>
#include <memory>
#include "common/Flags.h"
#include "common/util.h"
#include "common/FileSystemUtil.h"
#include "event/Event.h"
#include "event_handler/EventHandler.h"
using namespace std;

DECLARE_FLAG_STRING(ilogtail_config);
DECLARE_FLAG_STRING(user_log_config);
DECLARE_FLAG_INT32(batch_send_interval);

namespace logtail {
class ModifyHandlerUnittest : public ::testing::Test {
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
        std::ofstream writer(logPath.c_str(), fstream::out);
        writer << "a sample log\n";
        writer.close();

        // build a reader
        mReaderPtr = std::make_shared<CommonRegLogFileReader>(
            "project-0", "logstore-0", gRootDir, gLogName, INT32_FLAG(default_tail_limit_kb), "", "", "");
        mReaderPtr->UpdateReaderManual();
        int64_t fileSize = 0L;
        APSARA_TEST_TRUE_FATAL(mReaderPtr->CheckFileSignatureAndOffset(fileSize));

        // build a modify handler
        LogFileReaderPtrArray readerPtrArray{mReaderPtr};
        mHandlerPtr.reset(new ModifyHandler("", nullptr));
        mHandlerPtr->mNameReaderMap[gLogName] = readerPtrArray;
        mReaderPtr->SetReaderArray(&mHandlerPtr->mNameReaderMap[gLogName]);
        mHandlerPtr->mDevInodeReaderMap[mReaderPtr->mDevInode] = mReaderPtr;
    }
    void TearDown() override { bfs::remove_all(gRootDir); }
    static std::string gRootDir;
    static std::string gLogName;
    LogFileReaderPtr mReaderPtr;
    std::unique_ptr<ModifyHandler> mHandlerPtr;

public:
    void TestHandleContainerStoppedEventWhenReadToEnd() {
        LOG_INFO(sLogger, ("TestFindAllSubDirAndHandler() begin", time(NULL)));
        LogBuffer* logbuf = nullptr;
        APSARA_TEST_TRUE_FATAL(!mReaderPtr->ReadLog(logbuf)); // false means no more data
        APSARA_TEST_TRUE_FATAL(mReaderPtr->mLogFileOp.IsOpen());

        // send event to close reader
        Event event(gRootDir, "", EVENT_ISDIR | EVENT_CONTAINER_STOPPED, 0);
        mHandlerPtr->Handle(event);
        APSARA_TEST_TRUE_FATAL(!mReaderPtr->mLogFileOp.IsOpen());
    }

    void TestHandleContainerStoppedEventWhenNotReadToEnd() {
        LOG_INFO(sLogger, ("TestFindAllSubDirAndHandler() begin", time(NULL)));
        APSARA_TEST_TRUE_FATAL(mReaderPtr->mLogFileOp.IsOpen());

        // send event to close reader
        Event event(gRootDir, "", EVENT_ISDIR | EVENT_CONTAINER_STOPPED, 0);
        mHandlerPtr->Handle(event);
        APSARA_TEST_TRUE_FATAL(mReaderPtr->IsContainerStopped());
        APSARA_TEST_TRUE_FATAL(mReaderPtr->mLogFileOp.IsOpen());
    }

    void TestHandleModifyEventWhenContainerStopped() {
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
};

std::string ModifyHandlerUnittest::gRootDir;
std::string ModifyHandlerUnittest::gLogName;

APSARA_UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleContainerStoppedEventWhenReadToEnd, 0);
APSARA_UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleContainerStoppedEventWhenNotReadToEnd, 0);
APSARA_UNIT_TEST_CASE(ModifyHandlerUnittest, TestHandleModifyEventWhenContainerStopped, 0);
} // end of namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
