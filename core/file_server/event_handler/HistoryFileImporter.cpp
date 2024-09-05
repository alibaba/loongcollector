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

#include "app_config/AppConfig.h"
#include "common/FileSystemUtil.h"
#include "common/RuntimeUtil.h"
#include "common/Thread.h"
#include "common/TimeUtil.h"
#include "file_server/ConfigManager.h"
#include "logger/Logger.h"
#include "runner/LogProcess.h"
#include "pipeline/queue/ProcessQueueManager.h"
#include "file_server/reader/LogFileReader.h"

namespace logtail {

HistoryFileImporter::HistoryFileImporter() {
    LOG_INFO(sLogger, ("HistoryFileImporter", "init"));
    mThread = CreateThread([this]() { Run(); });
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

        LogFileReaderPtr readerSharePtr(LogFileReader::CreateLogFileReader(event.mDirName,
                                                                           fileName,
                                                                           devInode,
                                                                           event.mReaderConfig,
                                                                           event.mMultilineConfig,
                                                                           event.mDiscoveryconfig,
                                                                           event.mEOConcurrency,
                                                                           true));
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
        readerSharePtr->CheckFileSignatureAndOffset(false);

        bool doneFlag = false;
        while (true) {
            while (!ProcessQueueManager::GetInstance()->IsValidToPush(readerSharePtr->GetQueueKey())) {
                usleep(1000 * 10);
            }
            std::unique_ptr<LogBuffer> logBuffer(new LogBuffer);
            readerSharePtr->ReadLog(*logBuffer, nullptr);
            if (!logBuffer->rawBuffer.empty()) {
                logBuffer->logFileReader = readerSharePtr;

                PipelineEventGroup group = LogFileReader::GenerateEventGroup(readerSharePtr, logBuffer.get());

                // TODO: currently only 1 input is allowed, so we assume 0 here. It should be the actual input seq after
                // refactorization.
                logProcess->PushBuffer(readerSharePtr->GetQueueKey(), 0, std::move(group), 100000000);
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
                 ("[progress]", std::string("[") + ToString(i + 1) + "/" + ToString(fileNames.size()) + "]")("process",
                                                                                                             "done")(
                     "file", filePath)("offset", readerSharePtr->GetLastFilePos())("time(ms)", doneTime - startTime));
    }
}
} // namespace logtail
