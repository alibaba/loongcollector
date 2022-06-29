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

#include "HistoryFileImporter.h"
#include "common/Thread.h"
#include "common/TimeUtil.h"
#include "common/RuntimeUtil.h"
#include "common/FileSystemUtil.h"
#include "config_manager/ConfigManager.h"
#include "processor/LogProcess.h"
#include "logger/Logger.h"
#include "reader/LogFileReader.h"

namespace logtail {

HistoryFileImporter::HistoryFileImporter() {
    LOG_INFO(sLogger, ("HistoryFileImporter", "init"));
    static auto _doNotQuitThread = CreateThread([this]() { Run(); });
}

void HistoryFileImporter::PushEvent(const HistoryFileEvent& event) {
    LOG_INFO(sLogger, ("push event", event.String()));
    mEventQueue.PushItem(event);
}

void HistoryFileImporter::Run() {
    while (true) {
        HistoryFileEvent event;
        mEventQueue.PopItem(event);
        std::vector<std::string> objList;
        if (!GetAllFiles(event.mDirName, event.mFileName, objList)) {
            LOG_WARNING(sLogger, ("get all files", "failed"));
            continue;
        }
        ProcessEvent(event, objList);
    }
}

void HistoryFileImporter::LoadCheckPoint() {
    std::string historyDataPath = GetProcessExecutionDir() + "history_file_checkpoint";
    FILE* readPtr = fopen(historyDataPath.c_str(), "r");
    if (readPtr != NULL) {
        fclose(readPtr);
    }
}

void HistoryFileImporter::ProcessEvent(const HistoryFileEvent& event, const std::vector<std::string>& fileNames) {
    static LogProcess* logProcess = LogProcess::GetInstance();

    auto& config = event.mConfig;
    LOG_INFO(sLogger, ("begin load history files, count", fileNames.size())("file list", ToString(fileNames)));
    for (size_t i = 0; i < fileNames.size(); ++i) {
        auto startTime = GetCurrentTimeInMilliSeconds();
        const std::string& fileName = fileNames[i];
        const std::string filePath = PathJoin(event.mDirName, fileName);
        LOG_INFO(sLogger,
                 ("[progress]", std::string("[") + ToString(i + 1) + "/" + ToString(fileNames.size()) + "]")(
                     "process", "begin")("file", filePath));

        // create reader
        DevInode devInode = GetFileDevInode(filePath);
        if (!devInode.IsValid()) {
            LOG_WARNING(sLogger,
                        ("[progress]", std::string("[") + ToString(i + 1) + "/" + ToString(fileNames.size()) + "]")(
                            "process", "failed")("file", filePath)("reason", "invalid dev inode"));
            continue;
        }

        LogFileReaderPtr readerSharePtr(config->CreateLogFileReader(event.mDirName, fileName, devInode, true));
        if (readerSharePtr == NULL) {
            LOG_WARNING(sLogger,
                        ("[progress]", std::string("[") + ToString(i + 1) + "/" + ToString(fileNames.size()) + "]")(
                            "process", "failed")("file", filePath)("reason", "create log file reader failed"));
            continue;
        }
        if (!readerSharePtr->UpdateFilePtr()) {
            LOG_WARNING(sLogger,
                        ("[progress]", std::string("[") + ToString(i + 1) + "/" + ToString(fileNames.size()) + "]")(
                            "process", "failed")("file", filePath)("reason", "open file ptr failed"));
            continue;
        }
        readerSharePtr->SetLastFilePos(event.mStartPos);
        int64_t fileSize = 0;
        readerSharePtr->CheckFileSignatureAndOffset(fileSize);

        bool doneFlag = false;
        while (true) {
            while (!logProcess->IsValidToReadLog(readerSharePtr->GetLogstoreKey())) {
                usleep(1000 * 10);
            }
            LogBuffer* logBuffer = NULL;
            readerSharePtr->ReadLog(logBuffer);
            if (logBuffer != NULL) {
                logBuffer->logFileReader = readerSharePtr;
                logProcess->PushBuffer(logBuffer, 100000000);
            } else {
                // when ReadLog return false, retry once
                if (doneFlag) {
                    break;
                }
                doneFlag = true;
            }
        }
        auto doneTime = GetCurrentTimeInMilliSeconds();
        LOG_INFO(sLogger,
                 ("[progress]", std::string("[") + ToString(i + 1) + "/" + ToString(fileNames.size()) + "]")(
                     "process", "done")("file", filePath)("file size", fileSize)(
                     "offset", readerSharePtr->GetLastFilePos())("time(ms)", doneTime - startTime));
    }
}
} // namespace logtail
