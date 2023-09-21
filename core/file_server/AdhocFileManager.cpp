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

#include "AdhocFileManager.h"
#include "logger/Logger.h"

namespace logtail {

AdhocFileManager::AdhocFileManager() {
    // mRunFlag = false; 
};

void AdhocFileManager::Run() {
    // if (mRunFlag) {
    //     return;
    // } else {
    //     mRunFlag = true;
    // }

    // new Thread([this]() { ProcessLoop(); });
    // LOG_INFO(sLogger, ("AdhocFileManager", "Start"));
}

void AdhocFileManager::ProcessLoop() {
    // mAdhocCheckpointManager = AdhocCheckpointManager::GetInstance();

    // while (mRunFlag) {
    //     Event* ev = PopEventQueue();
    //     if (NULL == ev) {
    //         LOG_INFO(sLogger, ("AdhocFileManager", "All file loaded, exit"));
    //         mRunFlag = false;
    //         break;
    //     }
    //     switch (ev->GetType()) {
    //     case EVENT_STATIC_FILE:
    //         ProcessStaticFileEvent(ev);
    //         break;
    //     case EVENT_DELETE:
    //         ProcessDeleteEvent(ev);
    //         break;
    //     }
    // }
    // LOG_INFO(sLogger, ("AdhocFileManager", "Stop"));
}

void AdhocFileManager::ProcessStaticFileEvent(Event* ev) {
    // std::string jobName = ev->GetConfigName();
    // AdhocFileKey cpKey(DevInode(ev->GetDev(), ev->GetInode()), jobName, 0);
    // AdhocFileCheckpointPtr fileCpPtr = mAdhocCheckpointManager->GetAdhocFileCheckpoint(jobname, &cpKey);

    // // read file and change offset
    // ReadFile(ev, fileCpPtr);

    // // if file's status changed, dump job's checkpoint
    // if (jobCpPtr->UpdateAdhocFileCheckpoint(&cpKey, fileCpPtr)) {
    //     jobCpPtr->DumpAdhocCheckpoint();
    // }

    // // Push file of this job into queue
    // int32_t nowFileIndex = jobCpPtr->GetCurrentFileIndex();
    // if (nowFileIndex < mJobFileLists[jobName].size()) {
    //     StaticFile file = mJobFileLists[jobName][nowFileIndex];
    //     Event* ev = new Event(file.filePath, file.fileName, EVENT_STATIC_FILE, -1, 0, file.Dev, file.Inode);
    //     PushEventQueue(ev);
    // }
}

void AdhocFileManager::ReadFile(Event* ev, AdhocFileCheckpointPtr ptr) {
    // if (ptr->mStartTime == 0) {
    //     ptr->mStartTime = time(NULL);
    // }
    // if (ev->GetInode() == ptr->mDevInode.inode) { // check
    //     // read
    //     // ptr->mOffset += 1024;
    // } else {
    //     // file cannot find
    //     ptr->mOffset = -1;
    // }
}

void AdhocFileManager::ProcessDeleteEvent(Event* ev) {
    // std::string jobName = ev->GetConfigName();
    // mDeletedJobSet.insert(jobName);
    // mAdhocCheckpointManager->DeleteAdhocJobCheckpoint(jobName);
}

void AdhocFileManager::PushEventQueue(Event* ev) {
    // mEventQueue.push(ev);
}

Event* AdhocFileManager::PopEventQueue() {
    // while (mEventQueue.size() > 0) {
    //     Event* ev = mEventQueue.front();
    //     mEventQueue.pop();
    //     std::string jobName = ev->GetConfigName();
    //     if (mDeletedJobSet.find(jobName) != mDeletedJobSet.end()) {
    //         continue;
    //     }
    //     return ev;
    // }
    return NULL;
}

void AdhocFileManager::AddJob(std::string jobName, std::vector<StaticFile> fileList) {
    // mJobFileLists[jobName] = fileList;
    // std::vector<AdhocFileKey> keys;
    // for (StaticFile file : fileList) {
    //     AdhocFileKey key(DevInode(file.Dev, file.Inode), jobName, file.Size);
    //     keys.push_back(key);
    // }
    // mAdhocCheckpointManager->CreateAdhocJobCheckpoint(jobName, keys);

    // // push first file to queue
    // if  (fileList.size() > 0) {
    //     Event* ev = new Event(fileList[0].filePath, fileList[0].fileName, EVENT_STATIC_FILE, -1, 0, fileList[0].Dev, fileList[0].Inode);
    //     PushEventQueue(ev);
    // }

    // Run();
}

void AdhocFileManager::DeleteJob(std::string jobName) {
    // mJobFileLists.erase(jobName);
    // Event* stopEvent = new Event("", "", EVENT_CONTAINER_STOPPED, -1, 0);
    // stopEvent->SetConfigName(jobName);
    // PushEventQueue(stopEvent);
}

}
