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
#include <map>
#include "common/LogRunnable.h"
#include "common/Lock.h"
#include "common/Thread.h"
#include "PollingCache.h"

namespace logtail {

namespace fsutil {
    class PathStat;
}

class Config;

class PollingDirFile : public LogRunnable {
public:
    static PollingDirFile* GetInstance() {
        static PollingDirFile* ptr = new PollingDirFile();
        return ptr;
    }

    void Start();
    void Stop();

    void HoldOn() {
        mHoldOnFlag = true;
        mPollingThreadLock.lock();
    }
    void Resume() {
        mHoldOnFlag = false;
        mPollingThreadLock.unlock();
    }

    // ClearCache clears all cache items and reset status.
    // It will be called if configs have been updated because match status of
    // file/directory might change after updating.
    void ClearCache() {
        mDirCacheMap.clear();
        mFileCacheMap.clear();
        mStatCount = 0;
        mNewFileVec.clear();
        mCurrentRound = 0;
    }

private:
    PollingDirFile();
    ~PollingDirFile();

    void Polling();

    // PollingNormalConfigPath polls config with normal base path recursively.
    // @config: config to poll.
    // @srcPath+@obj: directory path to poll, for base directory, @obj is empty.
    // @statBuf: stat of current object.
    // @depth: the depth of current level, used to detect max depth.
    // @return: it is used only by first call, returns true if poll successfully or
    //   error can be handled, otherwise false is returned.
    bool PollingNormalConfigPath(const Config* config,
                                 const std::string& srcPath,
                                 const std::string& obj,
                                 const fsutil::PathStat& statBuf,
                                 int depth);

    // PollingWildcardConfigPath polls config with wildcard base path recursively.
    // It will use PollingNormalConfigPath to poll if the path becomes normal.
    // @return true if at least one directory was found during polling.
    bool PollingWildcardConfigPath(const Config* pConfig, const std::string& dirPath, int depth);

    // CheckAndUpdateDirMatchCache updates dir cache (add if not existing).
    // The caller of this method should make sure that there is at least one config matches
    // @dirPath.
    // @dirPath: absolute path of the directory.
    // @statBuf: stat of the directory.
    // @newFlag: a boolean to indicate caller that it is a new directory, generate event for it.
    // @return a boolean to indicate should the directory be continued to poll.
    //   It will returns true always now (might change in future).
    bool CheckAndUpdateDirMatchCache(const std::string& dirPath, const fsutil::PathStat& statBuf, bool& newFlag);

    // CheckAndUpdateFileMatchCache updates file cache (add if not existing).
    // @fileDir+@fileName: absolute path of the file.
    // @needFindBestMatch: false indicates that the file has already found the
    //   best match in caller, so no need to match again.
    // @return a boolean to indicate caller that this is a new file and at least one config matches
    //   it, so the caller should add it to PollingModify.
    bool CheckAndUpdateFileMatchCache(const std::string& fileDir,
                                      const std::string& fileName,
                                      const fsutil::PathStat& statBuf,
                                      bool needFindBestMatch);

    // ClearUnavailableFileAndDir checks cache, remove unavailable items.
    // By default, it will be called every 20 rounds (flag check_not_exist_file_dir_round).
    void ClearUnavailableFileAndDir();

    // ClearTimeoutFileAndDir checks cache, remove overtime items.
    // By default, it will be called every 600s (flag polling_check_timeout_interval).
    void ClearTimeoutFileAndDir();

    // CheckConfigPollingStatCount checks if the stat count of @config exceeds limit.
    // If true, logs and alarms.
    void CheckConfigPollingStatCount(const int32_t lastStatCount, const Config* config, bool isDockerConfig);

private:
    PTMutex mPollingThreadLock;
    volatile bool mRuningFlag;
    volatile bool mHoldOnFlag;
    ThreadPtr mThreadPtr;

    // Directories and files cache to reduce overhead of polling.
    // When the status of a cache item becomes unavailable or timeout, it might be removed:
    // - Unavailable: cache item becomes unavailable when it was deleted on filesystem
    //   or no more configs can match it. It will be remove by ClearUnavailableFileAndDir
    //   if it keeps unavailable for a while (100 polling rounds by default).
    // - Timeout: cache item becomes overtime when it is still matched by at least one
    //   config, but it has not updated for a long time (12 hours by default). Only when
    //   cache size exceeds upper limit, overtime items will be removed.
    SpinLock mCacheLock;
    DirCheckCacheMap mDirCacheMap;
    FileCheckCacheMap mFileCacheMap;

    // Record how much times stat is called, if it exceeds limit, stop polling.
    int32_t mStatCount;
    // Record new files found in current round, will be pushed to PollingModify.
    std::vector<SplitedFilePath> mNewFileVec;
    // The sequence number of current round, uint64_t is used to avoid overflow.
    uint64_t mCurrentRound;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class PollingUnittest;
#endif
};

} // namespace logtail
