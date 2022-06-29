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
#include <map>
#include <vector>
#include <deque>
#include <unordered_map>
#include <ctime>
#include "common/SplitedFilePath.h"

namespace logtail {

struct DirFileCache {
    DirFileCache() {}
    DirFileCache(bool configMatched) : mConfigMatched(configMatched) {}

    void SetConfigMatched(bool configMatched) { mConfigMatched = configMatched; }
    bool HasMatchedConfig() const { return mConfigMatched; }

    void SetCheckRound(uint64_t curRound) { mLastCheckRound = curRound; }
    uint64_t GetLastCheckRound() const { return mLastCheckRound; }

    void SetLastModifyTime(int64_t lastMTime) { mLastModifyTime = lastMTime; }
    int64_t GetLastModifyTime() const { return mLastModifyTime; }

    void SetEventFlag(bool eventFlag) { mEventFlag = eventFlag; }
    bool HasEventFlag() const { return mEventFlag; }

    void SetLastEventTime(int32_t curTime) { mLastEventTime = curTime; }
    int32_t GetLastEventTime() const { return mLastEventTime; }

private:
    // It indicates if the related file/dir has generated event.
    bool mEventFlag = false;
    // The event time of related file/dir.
    int32_t mLastEventTime = 0;

    bool mConfigMatched = false;
    uint64_t mLastCheckRound = 0;
    // Last modified time on filesystem in nanoseconds.
    int64_t mLastModifyTime = 0;
};

typedef std::unordered_map<std::string, DirFileCache> DirCheckCacheMap;
typedef std::unordered_map<std::string, DirFileCache> FileCheckCacheMap;

struct ModifyCheckCache {
    ModifyCheckCache() : mDev(0), mInode(0), mFileSize(0), mNotExistTimes(0) {
        mModifyTime.tv_sec = 0;
        mModifyTime.tv_nsec = 0;
    }
    ModifyCheckCache(uint64_t dev, uint64_t inode, uint64_t fileSize, timespec modifyTime)
        : mDev(dev), mInode(inode), mFileSize(fileSize), mModifyTime(modifyTime), mNotExistTimes(0) {}

    void UpdateFileProperty(uint64_t fileSize, timespec modifyTime);

    void UpdateFileProperty(uint64_t dev, uint64_t inode, uint64_t fileSize, timespec modifyTime);

    // return : true as deleted, false as normal.
    bool UpdateFileNotExist();

    uint64_t mDev;
    uint64_t mInode;
    uint64_t mFileSize;
    timespec mModifyTime;
    int32_t mNotExistTimes;
};

typedef std::map<SplitedFilePath, ModifyCheckCache> ModifyCheckCacheMap;

} // namespace logtail
