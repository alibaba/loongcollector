/*
 * Copyright 2022 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "common/StringTools.h"
#include "common/CircularBuffer.h"
#include "config/Config.h"

namespace logtail {

struct HistoryFileEvent {
    std::string mConfigName;
    std::string mDirName;
    std::string mFileName;
    int64_t mStartPos;
    std::shared_ptr<Config> mConfig;

    HistoryFileEvent() : mStartPos(0) {}

    std::string String() const {
        return std::string("config:") + mConfigName + ", dir:" + mDirName + ", filename:" + mFileName,
               +", pos:" + ToString(mStartPos);
    }
};

class HistoryFileImporter {
public:
    HistoryFileImporter();

    static HistoryFileImporter* GetInstance() {
        static HistoryFileImporter sFileImporter;
        return &sFileImporter;
    }

    void PushEvent(const HistoryFileEvent& event);

private:
    void Run();

    // @todo
    void LoadCheckPoint();

    // @todo multi line, flush last buffer
    void ProcessEvent(const HistoryFileEvent& event, const std::vector<std::string>& fileNames);

    static const int32_t HISTORY_EVENT_MAX = 10000;
    CircularBufferSem<HistoryFileEvent, HISTORY_EVENT_MAX> mEventQueue;
    std::unordered_map<std::string, int64_t> mCheckPoints;
    FILE* mCheckPointPtr;
};

} // namespace logtail
