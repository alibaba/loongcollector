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
#include "PollingCache.h"
#include <map>
#include <deque>
#include <vector>
#include "common/Lock.h"
#include "common/Thread.h"
#include "common/LogRunnable.h"
#ifdef APSARA_UINT_TEST_MAIN
#include "common/SplitedFilePath.h"
#endif

namespace logtail {

class Event;

class PollingModify : public LogRunnable {
public:
    static PollingModify* GetInstance() {
        static PollingModify* ptr = new PollingModify();
        return ptr;
    }

    void HoldOn() {
        mHoldOnFlag = true;
        mPollingThreadLock.lock();
    }

    void Resume() {
        mHoldOnFlag = false;
        mPollingThreadLock.unlock();
    }

    void ClearCache() {
        PTScopedLock lock(mFileLock);
        mModifyCacheMap.clear();
        mNewFileNameQueue.clear();
        mDeletedFileNameQueue.clear();
    }

    void Start();

    void Stop();

    // AddNewFile is called by PollingDirFile when it finds new files.
    void AddNewFile(const std::vector<SplitedFilePath>& fileNameVec);

    // AddDeleteFile is called by PollingDirFile when it finds files have to be removed.
    void AddDeleteFile(const std::vector<SplitedFilePath>& fileNameVec);

private:
    PollingModify();
    ~PollingModify();

    void Polling();

    // MakeSpaceForNewFile tries to release some space from modify cache
    // for LoadFileNameInQueues to add new files.
    void MakeSpaceForNewFile();
    // LoadFileNameInQueues loads files from two queues.
    void LoadFileNameInQueues();

    // UpdateFile updates corresponding cache of the file.
    // It compares the last state and current state of the file to decide if there is
    // a modification since last round.
    // @return true if a MODIFY event is created and pushed to @eventVec.
    bool UpdateFile(const SplitedFilePath& filePath,
                    ModifyCheckCache& modifyCache,
                    uint64_t dev,
                    uint64_t inode,
                    uint64_t size,
                    timespec modifyTime,
                    std::vector<Event*>& eventVec);

    // UpdateDeletedFile updates corresponding cache of the file (deleted).
    // @return true if the file needs to be removed from cache and a DELETE event
    //   is created and pushed to @eventVec.
    bool
    UpdateDeletedFile(const SplitedFilePath& filePath, ModifyCheckCache& modifyCache, std::vector<Event*>& eventVec);

private:
    PTMutex mPollingThreadLock;
    volatile bool mRuningFlag;
    volatile bool mHoldOnFlag;
    ThreadPtr mThreadPtr;

    // Cache the files addded or removed by PollingDirFile, they will be loaded and cleared
    // at the beginning of each polling round.
    std::mutex mFileLock;
    std::deque<SplitedFilePath> mNewFileNameQueue;
    std::deque<SplitedFilePath> mDeletedFileNameQueue;

    ModifyCheckCacheMap mModifyCacheMap;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class PollingUnittest;
    bool FindNewFile(const std::string& dir, const std::string& fileName);
#endif
};

} // namespace logtail
