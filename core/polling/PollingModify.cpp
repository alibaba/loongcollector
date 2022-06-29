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

#include "PollingModify.h"
#include "PollingEventQueue.h"
#if defined(__linux__)
#include <sys/file.h>
#endif
#include <sys/stat.h>
#include "common/Flags.h"
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "common/FileSystemUtil.h"
#include "event/Event.h"
#include "logger/Logger.h"
#include "profiler/LogtailAlarm.h"
#include "monitor/Monitor.h"

using namespace std;

DEFINE_FLAG_INT32(modify_check_interval, "modify check interval ms", 1000);
DEFINE_FLAG_INT32(ignore_file_modify_timeout, "if file modify time is up to XXX seconds, ignore it", 180);
DEFINE_FLAG_INT32(modify_stat_count, "sleep when dir file stat count up to", 100);
DEFINE_FLAG_INT32(modify_stat_sleepMs, "sleep time when dir file stat up to 1000, ms", 10);
DEFINE_FLAG_INT32(modify_cache_max, "max modify chache size, if exceed, delete 0.2 oldest", 100000);
DEFINE_FLAG_INT32(modify_cache_make_space_interval, "second", 600);

namespace logtail {

PollingModify::PollingModify() {
}

PollingModify::~PollingModify() {
}

void PollingModify::Start() {
    ClearCache();
    mRuningFlag = true;
    mThreadPtr = CreateThread([this]() { Polling(); });
}

void PollingModify::Stop() {
    mRuningFlag = false;
    if (mThreadPtr != NULL) {
        try {
            mThreadPtr->Wait(5 * 1000000);
        } catch (...) {
            LOG_ERROR(sLogger, ("stop polling modify thread error", ToString((int)mThreadPtr->GetState())));
        }
    }
}

struct ModifySortItem {
    const SplitedFilePath* filePath;
    int32_t modifyTime;

    static bool Compare(const ModifySortItem& left, const ModifySortItem& right) {
        return left.modifyTime < right.modifyTime;
    }
};

// Clear count: 20 percents of the maximum size of modify cache.
// Clear policy: use the last modified time of the corresponding file as key, sort
// incremental, then remove from the front.
void PollingModify::MakeSpaceForNewFile() {
    static int32_t s_lastMakeSpaceTime = 0;
    int32_t curTime = (int32_t)time(NULL);
    if (curTime - s_lastMakeSpaceTime > INT32_FLAG(modify_cache_make_space_interval)) {
        s_lastMakeSpaceTime = curTime;
    } else {
        return;
    }

    int32_t removeCount = (int32_t)mModifyCacheMap.size() - (int32_t)(INT32_FLAG(modify_cache_max) * 0.8);
    if (removeCount <= 0 || removeCount > (int32_t)mModifyCacheMap.size()) {
        return;
    }

    LogtailAlarm::GetInstance()->SendAlarm(MODIFY_FILE_EXCEED_ALARM,
                                           string("modify cache is up limit, delete old cache, modify file count:")
                                               + ToString(mModifyCacheMap.size())
                                               + "  delete count : " + ToString(removeCount));
    LOG_ERROR(sLogger, ("modify cache is up limit, delete old cache, modify file count", mModifyCacheMap.size()));

    vector<ModifySortItem> sortedItemVec;
    sortedItemVec.resize(mModifyCacheMap.size());
    ModifyCheckCacheMap::iterator iter = mModifyCacheMap.begin();
    for (size_t i = 0; i < mModifyCacheMap.size(); ++i) {
        sortedItemVec[i].filePath = &(iter->first);
        sortedItemVec[i].modifyTime = iter->second.mModifyTime.tv_sec;
        ++iter;
    }
    sort(sortedItemVec.begin(), sortedItemVec.end(), ModifySortItem::Compare);
    for (int i = 0; i < removeCount; ++i) {
        mModifyCacheMap.erase(mModifyCacheMap.find(*(sortedItemVec[i].filePath)));
    }
}

void PollingModify::AddNewFile(const std::vector<SplitedFilePath>& fileNameVec) {
    mFileLock.lock();
    mNewFileNameQueue.insert(mNewFileNameQueue.end(), fileNameVec.begin(), fileNameVec.end());
    mFileLock.unlock();
}

void PollingModify::AddDeleteFile(const std::vector<SplitedFilePath>& fileNameVec) {
    mFileLock.lock();
    mDeletedFileNameQueue.insert(mDeletedFileNameQueue.end(), fileNameVec.begin(), fileNameVec.end());
    mFileLock.unlock();
}

void PollingModify::LoadFileNameInQueues() {
    decltype(mNewFileNameQueue) newFileNameQueue, deletedFileNameQueue;
    {
        std::lock_guard<std::mutex> lock(mFileLock);
        newFileNameQueue = std::move(mNewFileNameQueue);
        mNewFileNameQueue.clear();
        deletedFileNameQueue = std::move(mDeletedFileNameQueue);
        mDeletedFileNameQueue.clear();
    }

    bool hasSpace = true;
    if (mModifyCacheMap.size() >= (size_t)INT32_FLAG(modify_cache_max)) {
        MakeSpaceForNewFile();

        if (mModifyCacheMap.size() >= (size_t)INT32_FLAG(modify_cache_max)) {
            LOG_ERROR(sLogger, ("total modify polling stat count is exceeded, drop event", newFileNameQueue.size()));
            LogtailAlarm::GetInstance()->SendAlarm(MODIFY_FILE_EXCEED_ALARM,
                                                   string("total modify polling stat count is exceeded, drop event:")
                                                       + ToString(newFileNameQueue.size()));
            hasSpace = false;
        }
    }
    if (hasSpace) {
        for (auto iter = newFileNameQueue.begin(); iter != newFileNameQueue.end(); ++iter) {
            mModifyCacheMap[*iter];
        }
    }

    for (auto iter = deletedFileNameQueue.begin(); iter != deletedFileNameQueue.end(); ++iter) {
        mModifyCacheMap.erase(*iter);
    }
}

bool PollingModify::UpdateFile(const SplitedFilePath& filePath,
                               ModifyCheckCache& modifyCache,
                               uint64_t dev,
                               uint64_t inode,
                               uint64_t size,
                               timespec modifyTime,
                               std::vector<Event*>& eventVec) {
    // The first time to update the file.
    // Check the LMD of file, If the file is too old (180s), it will be ignored at
    // this round and wait for next round. Otherwise, a MODIFY event will be created
    // and pushed to @eventVec.
    if (modifyCache.mDev == NO_BLOCK_DEV && modifyCache.mInode == NO_BLOCK_INODE) {
        if (time(NULL) - modifyTime.tv_sec > INT32_FLAG(ignore_file_modify_timeout)) {
            LOG_DEBUG(sLogger,
                      ("first watch this file and the modify time is old, "
                       "wait for next polling",
                       filePath.mFileDir + '/' + filePath.mFileName)("last modify time", ToString(modifyTime.tv_sec)));
            modifyCache.UpdateFileProperty(dev, inode, size, modifyTime);
            return false;
        }

        Event* pModifyEvent = new Event(filePath.mFileDir, filePath.mFileName, EVENT_MODIFY, -1, 0, dev, inode);
        eventVec.push_back(pModifyEvent);
        modifyCache.UpdateFileProperty(dev, inode, size, modifyTime);
        return true;
    }

    // DevInode has changed. Still need to check LMD of the file.
    if (modifyCache.mDev != dev || modifyCache.mInode != inode) {
        // process rotate, need check modify time
        if (time(NULL) - modifyTime.tv_sec > INT32_FLAG(ignore_file_modify_timeout)) {
            LOG_DEBUG(sLogger,
                      ("rotate file, check modify time, wait for next polling",
                       filePath.mFileDir + '/' + filePath.mFileName)("last modify time", ToString(modifyTime.tv_sec)));
            modifyCache.UpdateFileProperty(dev, inode, size, modifyTime);
            return false;
        }

        Event* pModifyEvent = new Event(filePath.mFileDir, filePath.mFileName, EVENT_MODIFY, -1, 0, dev, inode);
        eventVec.push_back(pModifyEvent);
        modifyCache.UpdateFileProperty(dev, inode, size, modifyTime);
        return true;
    }

    // DevInode hasn't changed but the size or LMD of the file has changed, create a MODIFY event.
    if (modifyCache.mFileSize != size || modifyCache.mModifyTime.tv_sec != modifyTime.tv_sec
        || modifyCache.mModifyTime.tv_nsec != modifyTime.tv_nsec) {
        Event* pModifyEvent = new Event(filePath.mFileDir, filePath.mFileName, EVENT_MODIFY, -1, 0, dev, inode);
        eventVec.push_back(pModifyEvent);
        modifyCache.UpdateFileProperty(size, modifyTime);
        return true;
    }

    return false;
}

bool PollingModify::UpdateDeletedFile(const SplitedFilePath& filePath,
                                      ModifyCheckCache& modifyCache,
                                      std::vector<Event*>& eventVec) {
    if (modifyCache.UpdateFileNotExist()) {
        Event* pDeleteEvent = new Event(
            filePath.mFileDir, filePath.mFileName, EVENT_DELETE, -1, 0, modifyCache.mDev, modifyCache.mInode);
        eventVec.push_back(pDeleteEvent);
        return true;
    }
    return false;
}

void PollingModify::Polling() {
    LOG_INFO(sLogger, ("PollingModify::Polling", "start"));
    mHoldOnFlag = false;
    while (mRuningFlag) {
        {
            PTScopedLock threadLock(mPollingThreadLock);
            LoadFileNameInQueues();

            vector<SplitedFilePath> deletedFileVec;
            vector<Event*> pollingEventVec;
            int32_t statCount = 0;
            LogtailMonitor::Instance()->UpdateMetric("polling_modify_size", mModifyCacheMap.size());
            for (auto iter = mModifyCacheMap.begin(); iter != mModifyCacheMap.end(); ++iter) {
                if (!mRuningFlag || mHoldOnFlag)
                    break;

                const SplitedFilePath& filePath = iter->first;
                ModifyCheckCache& modifyCache = iter->second;
                fsutil::PathStat logFileStat;
                if (!fsutil::PathStat::stat(PathJoin(filePath.mFileDir, filePath.mFileName), logFileStat)) {
                    if (errno == ENOENT) {
                        LOG_DEBUG(sLogger, ("file deleted", PathJoin(filePath.mFileDir, filePath.mFileName)));
                        if (UpdateDeletedFile(filePath, modifyCache, pollingEventVec)) {
                            deletedFileVec.push_back(filePath);
                        }
                    } else {
                        LOG_DEBUG(sLogger, ("get file info error", PathJoin(filePath.mFileDir, filePath.mFileName)));
                    }
                } else {
                    int64_t sec, nsec;
                    logFileStat.GetLastWriteTime(sec, nsec);
                    timespec mtim{sec, nsec};
                    auto devInode = logFileStat.GetDevInode();
                    UpdateFile(filePath,
                               modifyCache,
                               devInode.dev,
                               devInode.inode,
                               logFileStat.GetFileSize(),
                               mtim,
                               pollingEventVec);
                }

                ++statCount;
                if (statCount % INT32_FLAG(modify_stat_count) == 0) {
                    usleep(1000 * INT32_FLAG(modify_stat_sleepMs));
                }
            }

            if (pollingEventVec.size() > 0) {
                PollingEventQueue::GetInstance()->PushEvent(pollingEventVec);
            }
            for (size_t i = 0; i < deletedFileVec.size(); ++i) {
                mModifyCacheMap.erase(deletedFileVec[i]);
            }
        }

        // Sleep for a while, by default, 1s.
        for (int i = 0; i < 10 && mRuningFlag; ++i) {
            usleep(INT32_FLAG(modify_check_interval) * 100);
        }
    }
    LOG_INFO(sLogger, ("PollingModify::Polling", "stop"));
}

#ifdef APSARA_UNIT_TEST_MAIN
bool PollingModify::FindNewFile(const std::string& dir, const std::string& fileName) {
    PTScopedLock lock(mFileLock);
    SplitedFilePath filePath(dir, fileName);
    for (auto iter = mNewFileNameQueue.begin(); iter != mNewFileNameQueue.end(); ++iter) {
        if (*iter == filePath) {
            return true;
        }
    }
    return false;
}
#endif

} // namespace logtail
