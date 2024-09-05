/*
 * Copyright 2023 iLogtail Authors
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
#include <queue>
#include <unordered_set>
#include "checkpoint/AdhocCheckpointManager.h"
#include "file_server/event/Event.h"

namespace logtail {

struct StaticFile {
    std::string mFilePath;
    DevInode mDevInode;
};

class AdhocFileManager {
private:
    AdhocFileManager();
    AdhocFileManager(const AdhocFileManager&) = delete;
    AdhocFileManager& operator=(const AdhocFileManager&) = delete;
    void ProcessLoop();
    static bool mRunFlag;

    void PushEventQueue(Event* ev);
    Event* PopEventQueue();
    void ProcessStaticFileEvent(Event* ev);
    void ProcessDeleteEvent(Event* ev);
    void ReadFile(Event* ev, AdhocFileCheckpointPtr cp);

    AdhocCheckpointManager* mAdhocCheckpointManager;
    std::queue<Event*> mEventQueue;
    std::unordered_set<std::string> mDeletedJobSet;
    std::unordered_map<std::string, std::vector<StaticFile> > mJobFileLists;
public:
    static AdhocFileManager* GetInstance() {
        static AdhocFileManager* ptr = new AdhocFileManager();
        return ptr;
    }

    void Run();
    void AddJob(std::string jobName, std::vector<StaticFile> fileList);
    void DeleteJob(std::string jobName);
};

}