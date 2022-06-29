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

#include "PollingDirFile.h"
#if defined(__linux__)
#include <sys/file.h>
#include <fnmatch.h>
#elif defined(_MSC_VER)
#include <Shlwapi.h>
#endif
#include <sys/stat.h>
#include "common/Flags.h"
#include "common/StringTools.h"
#include "common/ErrorUtil.h"
#include "common/FileSystemUtil.h"
#include "common/TimeUtil.h"
#include "event/Event.h"
#include "logger/Logger.h"
#include "config_manager/ConfigManager.h"
#include "profiler/LogtailAlarm.h"
#include "monitor/Monitor.h"
#include "PollingModify.h"
#include "PollingEventQueue.h"

// Control the check frequency to call ClearUnavailableFileAndDir.
DEFINE_FLAG_INT32(check_not_exist_file_dir_round, "clear not exist file dir cache, round", 20);
// Control the number of out-dated round that a file/dir is considered unavailable.
DEFINE_FLAG_INT32(delete_dir_file_round, "delete not exist file dir, round", 100);
#if defined(__linux__)
DEFINE_FLAG_INT32(dirfile_check_interval_ms, "dir file check interval, ms", 5000);
#elif defined(_MSC_VER)
// Windows only supports polling, check more frequently.
DEFINE_FLAG_INT32(dirfile_check_interval_ms, "dir file check interval, ms", 1000);
#endif
DEFINE_FLAG_INT32(dirfile_stat_count, "sleep when dir file stat count up to", 100);
DEFINE_FLAG_INT32(dirfile_stat_sleep, "sleep time when dir file stat up to 1000, ms", 30);
DEFINE_FLAG_INT32(polling_dir_upperlimit, "try to remove unchanged dir if dir count is up to", 500000);
DEFINE_FLAG_INT32(polling_file_upperlimit, "try to remove unchanged file if file count is up to", 500000);
DEFINE_FLAG_INT32(polling_dir_timeout, "remove unchanged dir if modify time is older than time", 12 * 3600);
DEFINE_FLAG_INT32(polling_file_timeout, "remove unchanged file if modify time is older than time", 12 * 3600);
DEFINE_FLAG_INT32(polling_dir_first_watch_timeout, "do not post event if modify time is up to", 3 * 3600);
DEFINE_FLAG_INT32(polling_file_first_watch_timeout, "do not post event if modify time is up to", 3 * 3600);
DEFINE_FLAG_INT32(polling_check_timeout_interval, " ", 600);
DEFINE_FLAG_INT32(polling_max_stat_count, "max stat count in each round", 1000000);
DEFINE_FLAG_INT32(polling_max_stat_count_per_dir, "max stat count per dir in each round", 100000);
DEFINE_FLAG_INT32(polling_max_stat_count_per_config, "max stat count per config in each round", 100000);
DEFINE_FLAG_INT32(polling_modify_repush_interval, "polling modify event repush interval, seconds", 10);
DECLARE_FLAG_INT32(wildcard_max_sub_dir_count);

using namespace std;

namespace logtail {

static const int64_t NANO_CONVERTING = 1000000000;

void PollingDirFile::Start() {
    ClearCache();
    mRuningFlag = true;
    mThreadPtr = CreateThread([this]() { Polling(); });
}

void PollingDirFile::Stop() {
    mRuningFlag = false;
    if (mThreadPtr != nullptr) {
        try {
            mThreadPtr->Wait(5 * 1000000);
        } catch (...) {
            LOG_ERROR(sLogger, ("stop polling dir file thread error", ToString((int)mThreadPtr->GetState())));
        }
    }
}

void PollingDirFile::CheckConfigPollingStatCount(const int32_t lastStatCount,
                                                 const Config* config,
                                                 bool isDockerConfig) {
    auto diffCount = mStatCount - lastStatCount;
    if (diffCount <= INT32_FLAG(polling_max_stat_count_per_config))
        return;

    std::string msgBase = "The polling stat count of this ";
    if (isDockerConfig)
        msgBase += "docker ";
    msgBase += "config has exceeded limit";

    LOG_WARNING(sLogger, (msgBase, diffCount)(config->mBasePath, mStatCount)(config->mProjectName, config->mCategory));
    LogtailAlarm::GetInstance()->SendAlarm(STAT_LIMIT_ALARM,
                                           msgBase + ", current count: " + ToString(diffCount)
                                               + " total count:" + ToString(mStatCount) + " path: " + config->mBasePath,
                                           config->mProjectName,
                                           config->mCategory,
                                           config->mRegion);
}

void PollingDirFile::Polling() {
    mHoldOnFlag = false;
    while (mRuningFlag) {
        LOG_DEBUG(sLogger, ("start dir file polling, mCurrentRound", mCurrentRound));
        {
            PTScopedLock thradLock(mPollingThreadLock);
            mStatCount = 0;
            mNewFileVec.clear();
            ++mCurrentRound;

            // Get a copy of config list from ConfigManager.
            // PollingDirFile has to be held on at first because raw pointers are used here.
            vector<Config*> sortedConfigs;
            vector<Config*> wildcardConfigs;
            auto nameConfigMap = ConfigManager::GetInstance()->GetAllConfig();
            for (auto itr = nameConfigMap.begin(); itr != nameConfigMap.end(); ++itr) {
                if (itr->second->mLogType == STREAM_LOG || itr->second->mLogType == PLUGIN_LOG)
                    continue;
                if (itr->second->mWildcardPaths.size() == 0)
                    sortedConfigs.push_back(itr->second);
                else
                    wildcardConfigs.push_back(itr->second);
            }
            sort(sortedConfigs.begin(), sortedConfigs.end(), Config::CompareByPathLength);

            LogtailMonitor::Instance()->UpdateMetric("config_count", nameConfigMap.size());
            {
                ScopedSpinLock lock(mCacheLock);
                LogtailMonitor::Instance()->UpdateMetric("polling_dir_cache", mDirCacheMap.size());
                LogtailMonitor::Instance()->UpdateMetric("polling_file_cache", mFileCacheMap.size());
            }

            // Iterate all normal configs, make sure stat count will not exceed limit.
            for (auto itr = sortedConfigs.begin();
                 itr != sortedConfigs.end() && mStatCount <= INT32_FLAG(polling_max_stat_count);
                 ++itr) {
                if (!mRuningFlag || mHoldOnFlag)
                    break;

                Config* config = *itr;
                if (!config->mDockerFileFlag) {
                    fsutil::PathStat baseDirStat;
                    if (!fsutil::PathStat::stat(config->mBasePath, baseDirStat)) {
                        LOG_DEBUG(
                            sLogger,
                            ("get base dir info error: ", config->mBasePath)(config->mProjectName, config->mCategory));
                        continue;
                    }

                    int32_t lastConfigStatCount = mStatCount;
                    if (!PollingNormalConfigPath(config, config->mBasePath, string(), baseDirStat, 0)) {
                        LOG_DEBUG(sLogger,
                                  ("logPath in config not exist", config->mBasePath)(config->mProjectName,
                                                                                     config->mCategory));
                    }
                    CheckConfigPollingStatCount(lastConfigStatCount, config, false);
                } else {
                    for (size_t i = 0; i < config->mDockerContainerPaths->size(); ++i) {
                        const string& basePath = (*config->mDockerContainerPaths)[i].mContainerPath;
                        fsutil::PathStat baseDirStat;
                        if (!fsutil::PathStat::stat(basePath.c_str(), baseDirStat)) {
                            LOG_DEBUG(sLogger,
                                      ("get docker base dir info error: ", basePath)(config->mProjectName,
                                                                                     config->mCategory));
                            continue;
                        }
                        int32_t lastConfigStatCount = mStatCount;
                        if (!PollingNormalConfigPath(config, basePath, string(), baseDirStat, 0)) {
                            LOG_DEBUG(sLogger,
                                      ("docker logPath in config not exist", basePath)(config->mProjectName,
                                                                                       config->mCategory));
                        }
                        CheckConfigPollingStatCount(lastConfigStatCount, config, true);
                    }
                }
            }

            // Iterate all wildcard configs, make sure stat count will not exceed limit.
            for (auto itr = wildcardConfigs.begin();
                 itr != wildcardConfigs.end() && mStatCount <= INT32_FLAG(polling_max_stat_count);
                 ++itr) {
                if (!mRuningFlag || mHoldOnFlag)
                    break;

                Config* config = *itr;
                if (!config->mDockerFileFlag) {
                    int32_t lastConfigStatCount = mStatCount;
                    if (!PollingWildcardConfigPath(config, config->mWildcardPaths[0], 0)) {
                        LOG_DEBUG(sLogger,
                                  ("can not find matched path in config, Wildcard begin logPath",
                                   config->mBasePath)(config->mProjectName, config->mCategory));
                    }
                    CheckConfigPollingStatCount(lastConfigStatCount, config, false);
                } else {
                    for (size_t i = 0; i < config->mDockerContainerPaths->size(); ++i) {
                        const string& baseWildcardPath = (*config->mDockerContainerPaths)[i].mContainerPath;
                        int32_t lastConfigStatCount = mStatCount;
                        if (!PollingWildcardConfigPath(config, baseWildcardPath, 0)) {
                            LOG_DEBUG(sLogger,
                                      ("can not find matched path in config, "
                                       "Wildcard begin logPath ",
                                       baseWildcardPath)(config->mProjectName, config->mCategory));
                        }
                        CheckConfigPollingStatCount(lastConfigStatCount, config, true);
                    }
                }
            }

            // Add collected new files to PollingModify.
            PollingModify::GetInstance()->AddNewFile(mNewFileVec);

            // Check cache, clear unavailable and overtime items.
            if (mCurrentRound % INT32_FLAG(check_not_exist_file_dir_round) == 0) {
                ClearUnavailableFileAndDir();
            }
            ClearTimeoutFileAndDir();
        }

        // Sleep for a while, by default, 5s on Linux, 1s on Windows.
        for (int i = 0; i < 10 && mRuningFlag; ++i) {
            usleep(INT32_FLAG(dirfile_check_interval_ms) * 100);
        }
    }
    LOG_DEBUG(sLogger, ("dir file polling thread done", ""));
}

// Last Modified Time (LMD) of directory changes when a file or a subdirectory is added,
// removed or renamed. Howerver, modifying the content of a file within it will not update
// LMD, and add/remove/rename file/directory in its subdirectory will also not upadte LMD.
// NOTE: So, we can not find changes in subdirectories of the directory according to LMD.
bool PollingDirFile::CheckAndUpdateDirMatchCache(const string& dirPath,
                                                 const fsutil::PathStat& statBuf,
                                                 bool& newFlag) {
    int64_t sec, nsec;
    statBuf.GetLastWriteTime(sec, nsec);
    int64_t modifyTime = NANO_CONVERTING * sec + nsec;

    ScopedSpinLock lock(mCacheLock);
    auto iter = mDirCacheMap.find(dirPath);

    // New directory, add a new cache item for it.
    if (iter == mDirCacheMap.end()) {
        DirFileCache& dirCache = mDirCacheMap[dirPath];
        dirCache.SetConfigMatched(true);
        dirCache.SetCheckRound(mCurrentRound);
        dirCache.SetLastModifyTime(modifyTime);
        // Directories found at round 1 or too old are considered as old data.
        auto curTime = static_cast<int32_t>(time(NULL));
        if (mCurrentRound == 1 || curTime - sec > INT32_FLAG(polling_dir_first_watch_timeout)) {
            newFlag = false;
        } else {
            newFlag = true;
            dirCache.SetLastEventTime(curTime);
        }
        dirCache.SetEventFlag(newFlag);
        return true;
    }

    // Already cached, update last round and modified time.
    newFlag = false;
    iter->second.SetCheckRound(mCurrentRound);
    iter->second.SetLastModifyTime(modifyTime);
    return true; // iter->second.HasMatchedConfig().
}

bool PollingDirFile::CheckAndUpdateFileMatchCache(const string& fileDir,
                                                  const string& fileName,
                                                  const fsutil::PathStat& statBuf,
                                                  bool needFindBestMatch) {
    int64_t sec, nsec;
    statBuf.GetLastWriteTime(sec, nsec);
    int64_t modifyTime = NANO_CONVERTING * sec + nsec;

    bool newFlag = false;
    string filePath = PathJoin(fileDir, fileName);
    FileCheckCacheMap::iterator iter = mFileCacheMap.find(filePath);
    int32_t curTime = time(NULL);
    if (iter == mFileCacheMap.end()) {
        bool matchFlag
            = needFindBestMatch ? ConfigManager::GetInstance()->FindBestMatch(fileDir, fileName) != NULL : true;

        DirFileCache& fileCache = mFileCacheMap[filePath];
        fileCache.SetConfigMatched(matchFlag);
        fileCache.SetCheckRound(mCurrentRound);
        fileCache.SetLastModifyTime(modifyTime);

        // Files found at round 1 or too old are considered as old data.
        if (mCurrentRound == 1 || curTime - sec > INT32_FLAG(polling_file_first_watch_timeout)) {
            newFlag = false;
        } else {
            newFlag = true;
            fileCache.SetLastEventTime(curTime);
        }
        fileCache.SetEventFlag(newFlag);
        return matchFlag && newFlag;
    }

    // If the file is not overtime, repush it to PollingModify thread regularly (by default, 10s).
    // Mainly for case that file is deleted and recreated after a while.
    // In detail, it can avoid data missing when following things happen:
    // 1. File is created, and PollingDirFile add it to cache and push it to PollingModify.
    // 2. File is deleted, PollingModify removes it.
    // 3. File is recreated, because PollingDirFile already caches it, new flag will
    //    not be set.
    // 4. Now, PollingModify will not generate MODIFY event for the file because the file
    //    is not existing in polling file list. **We lose the file**.
    if ((curTime - sec < INT32_FLAG(polling_file_first_watch_timeout))
        && (!iter->second.HasEventFlag()
            || (curTime - iter->second.GetLastEventTime() >= INT32_FLAG(polling_modify_repush_interval)))) {
        newFlag = true;
        iter->second.SetEventFlag(newFlag);
        iter->second.SetLastEventTime(curTime);
    }
    iter->second.SetCheckRound(mCurrentRound);
    iter->second.SetLastModifyTime(modifyTime);
    return iter->second.HasMatchedConfig() && newFlag;
}

bool PollingDirFile::PollingNormalConfigPath(
    const Config* pConfig, const string& srcPath, const string& obj, const fsutil::PathStat& statBuf, int depth) {
    if (pConfig->mMaxDepth >= 0 && depth > pConfig->mMaxDepth)
        return false;
    if (!pConfig->mIsPreserve && depth > pConfig->mPreserveDepth)
        return false;

    string dirPath = obj.empty() ? srcPath : PathJoin(srcPath, obj);
    bool isNewDirectory = false;
    if (!CheckAndUpdateDirMatchCache(dirPath, statBuf, isNewDirectory))
        return true;
    if (isNewDirectory) {
        PollingEventQueue::GetInstance()->PushEvent(new Event(srcPath, obj, EVENT_CREATE | EVENT_ISDIR, -1, 0));
    }

    // Iterate directories and files in dirPath.
    fsutil::Dir dir(dirPath);
    if (!dir.Open()) {
        auto err = GetErrno();
        if (fsutil::Dir::IsENOENT(err)) {
            LOG_DEBUG(sLogger, ("Open dir error, ENOENT, dir", dirPath.c_str()));
            return false;
        } else {
            LogtailAlarm::GetInstance()->SendAlarm(LOGDIR_PERMINSSION_ALARM,
                                                   string("Failed to open dir : ") + dirPath
                                                       + ";\terrno : " + ToString(err),
                                                   pConfig->GetProjectName(),
                                                   pConfig->GetCategory());
            LOG_ERROR(sLogger, ("Open dir error", dirPath.c_str())("error", ErrnoToString(err)));
        }
        return true;
    }
    int32_t nowStatCount = 0;
    fsutil::Entry ent;
    while (ent = dir.ReadNext(false)) {
        if (!mRuningFlag || mHoldOnFlag)
            break;

        if (++mStatCount % INT32_FLAG(dirfile_stat_count) == 0) {
            usleep(INT32_FLAG(dirfile_stat_sleep) * 1000);
        }

        if (mStatCount > INT32_FLAG(polling_max_stat_count)) {
            LOG_WARNING(sLogger,
                        ("total dir's polling stat count is exceeded",
                         nowStatCount)(dirPath, mStatCount)(pConfig->mProjectName, pConfig->mCategory));
            LogtailAlarm::GetInstance()->SendAlarm(STAT_LIMIT_ALARM,
                                                   string("total dir's polling stat count is exceeded, now count:")
                                                       + ToString(nowStatCount) + " total count:" + ToString(mStatCount)
                                                       + " path: " + dirPath + " project:" + pConfig->mProjectName
                                                       + " logstore:" + pConfig->mCategory);
            break;
        }

        if (++nowStatCount > INT32_FLAG(polling_max_stat_count_per_dir)) {
            LOG_WARNING(sLogger,
                        ("this dir's polling stat count is exceeded",
                         nowStatCount)(dirPath, mStatCount)(pConfig->mProjectName, pConfig->mCategory));
            LogtailAlarm::GetInstance()->SendAlarm(STAT_LIMIT_ALARM,
                                                   string("this dir's polling stat count is exceeded, now count:")
                                                       + ToString(nowStatCount) + " total count:" + ToString(mStatCount)
                                                       + " path: " + dirPath + " project:" + pConfig->mProjectName
                                                       + " logstore:" + pConfig->mCategory,
                                                   pConfig->mRegion);
            break;
        }

        // If the type of item is raw directory or file, use MatchDirPattern or FindBestMatch
        // to check if there are configs that match it.
        auto entName = ent.Name();
        string item = PathJoin(dirPath, entName);
        bool needCheckDirMatch = true;
        bool needFindBestMatch = true;
        if (ent.IsDir()) {
            // Have to call MatchDirPattern, because we have no idea which config matches
            // the directory according to cache.
            // TODO: Refactor directory cache, maintain all configs that match the directory.
            needCheckDirMatch = false;
            if (!ConfigManager::GetInstance()->MatchDirPattern(pConfig, item)) {
                continue;
            }
        } else if (ent.IsRegFile()) {
            // TODO: Add file cache looking up here: we can skip the file if it is in cache
            // and the match flag is false (no config matches it).
            // There is a cache in FindBestMatch, so the overhead is acceptable now.
            needFindBestMatch = false;
            if (ConfigManager::GetInstance()->FindBestMatch(dirPath, entName) == NULL) {
                continue;
            }
        } else {
            // Symbolic link should be passed, while other types file should ignore.
            if (!ent.IsSymbolic()) {
                LOG_DEBUG(sLogger, ("should ignore, other type file", item.c_str()));
                continue;
            }
        }

        // Mainly for symbolic (Linux), we need to use stat to dig out the real type.
        fsutil::PathStat buf;
        if (!fsutil::PathStat::stat(item, buf)) {
            LOG_DEBUG(sLogger, ("get file info error", item.c_str())("errno", errno));
            continue;
        }

        // For directory, poll recursively; for file, update cache and add to mNewFileVec so that
        // it can be pushed to PollingModify at the end of polling.
        // If needCheckDirMatch or needFindBestMatch is true, that means the item is a symbolic link.
        // We should check file type again to make sure that the original file which linked by
        // a symbolic file is DIR or REG.
        if (buf.IsDir() && (!needCheckDirMatch || ConfigManager::GetInstance()->MatchDirPattern(pConfig, item))) {
            PollingNormalConfigPath(pConfig, dirPath, entName, buf, depth + 1);
        } else if (buf.IsRegFile()) {
            if (CheckAndUpdateFileMatchCache(dirPath, entName, buf, needFindBestMatch)) {
                LOG_DEBUG(sLogger, ("add to modify event", entName)("round", mCurrentRound));
                mNewFileVec.push_back(SplitedFilePath(dirPath, entName));
            }
        } else {
            // Ignore other file type.
            LOG_DEBUG(sLogger, ("other type file is linked by a symbolic link, should ignore", item.c_str()));
            continue;
        }
    }

    return true;
}

// PollingWildcardConfigPath will iterate mWildcardPaths one by one, and according to
// corresponding value in mConstWildcardPaths, call PollingNormalConfigPath or call
// PollingWildcardConfigPath recursively.
bool PollingDirFile::PollingWildcardConfigPath(const Config* pConfig, const string& dirPath, int depth) {
    auto const wildcardPathSize = static_cast<int>(pConfig->mWildcardPaths.size());
    if (depth - wildcardPathSize > pConfig->mMaxDepth)
        return false;

    bool finish = false;
    if ((depth + 1) < (wildcardPathSize - 1))
        finish = false;
    else if ((depth + 1) == (wildcardPathSize - 1))
        finish = true;
    else {
        // This should not happen.
        LOG_ERROR(
            sLogger,
            ("PollingWildcardConfigPath error: ", dirPath.c_str())(pConfig->GetProjectName(), pConfig->GetCategory()));
        return false;
    }

    // if sub path is const, we do not need to scan whole dir
    // Current part is constant, check if it is existing directly.
    if (!pConfig->mConstWildcardPaths[depth].empty()) {
        // Stat directly, stat failure means that the directory is not existing or we have no
        // permission to access it, just return true to stop polling.
        string item = PathJoin(dirPath, pConfig->mConstWildcardPaths[depth]);
        fsutil::PathStat baseDirStat;
        if (!fsutil::PathStat::stat(item, baseDirStat)) {
            LOG_DEBUG(sLogger,
                      ("get wildcard dir info error: ", pConfig->mBasePath)("stat path", item)(
                          pConfig->mProjectName, pConfig->mCategory)("error", ErrnoToString(GetErrno())));
            return true;
        }
        if (!baseDirStat.IsDir())
            return true;

        // finish indicates that current part is the last one, so we can
        // call PollingNormalConfigPath to iterate remaining content.
        // Otherwise, call PollingWildcardConfigPath to deal with remaining parts.
        if (finish) {
            PollingNormalConfigPath(pConfig, item, string(), baseDirStat, 0);
        } else {
            PollingWildcardConfigPath(pConfig, item, depth + 1);
        }
        return true;
    }

    // Current part is not constant (normal) path, so we have to iterate and match one by one.
    bool hasMatchFlag = false;
    fsutil::Dir dir(dirPath);
    if (!dir.Open()) {
        auto err = GetErrno();
        if (fsutil::Dir::IsENOENT(err)) {
            LOG_DEBUG(sLogger, ("Open dir fail, ENOENT, dir", dirPath.c_str()));
            return false;
        } else {
            LogtailAlarm::GetInstance()->SendAlarm(LOGDIR_PERMINSSION_ALARM,
                                                   string("Failed to open dir : ") + dirPath
                                                       + ";\terrno : " + ToString(err),
                                                   pConfig->GetProjectName(),
                                                   pConfig->GetCategory());
            LOG_WARNING(sLogger, ("Open dir fail", dirPath.c_str())("errno", err));
        }
        return true;
    }
    fsutil::Entry ent;
    int32_t dirCount = 0;
    while (ent = dir.ReadNext(false)) {
        if (!mRuningFlag || mHoldOnFlag)
            break;

        if (dirCount >= INT32_FLAG(wildcard_max_sub_dir_count)) {
            LOG_WARNING(
                sLogger,
                ("too many sub directoried for path", dirPath)("dirCount", dirCount)("basePath", pConfig->mBasePath));
            break;
        }

        if (++mStatCount % INT32_FLAG(dirfile_stat_count) == 0)
            usleep(INT32_FLAG(dirfile_stat_sleep) * 1000);

        if (mStatCount > INT32_FLAG(polling_max_stat_count)) {
            LOG_WARNING(sLogger,
                        ("total dir's polling stat count is exceeded", "")(dirPath, mStatCount)(pConfig->mProjectName,
                                                                                                pConfig->mCategory));
            LogtailAlarm::GetInstance()->SendAlarm(
                STAT_LIMIT_ALARM,
                string("total dir's polling stat count is exceeded, total count:" + ToString(mStatCount) + " path: "
                       + dirPath + " project:" + pConfig->mProjectName + " logstore:" + pConfig->mCategory));
            break;
        }

        auto entName = ent.Name();
        string item = PathJoin(dirPath, entName);
        fsutil::PathStat buf;
        if (!fsutil::PathStat::stat(item, buf)) {
            LOG_WARNING(sLogger, ("get file info fail", item.c_str())("errno", GetErrno()));
            continue;
        }
        if (buf.IsDir()) {
            ++dirCount;

            // Use the next part to match the entry name.
            size_t dirIndex = 0;
            if (!BOOL_FLAG(enable_root_path_collection)) {
                // Handle special path /.
                dirIndex = pConfig->mWildcardPaths[depth].size() + 1;
                if (dirIndex == (size_t)2) {
                    dirIndex = 1;
                }
            } else {
                // A better logic, but only enabled when flag enable_root_path_collection
                //   is set for backward compatibility.
                dirIndex = pConfig->mWildcardPaths[depth].size();
                if (PATH_SEPARATOR[0] == pConfig->mWildcardPaths[depth + 1][dirIndex]) {
                    ++dirIndex;
                }
            }
            if (fnmatch(&(pConfig->mWildcardPaths[depth + 1].at(dirIndex)), entName.c_str(), FNM_PATHNAME) == 0) {
                if (finish) {
                    hasMatchFlag = true;
                    PollingNormalConfigPath(pConfig, item, string(), buf, 0);
                } else {
                    hasMatchFlag |= PollingWildcardConfigPath(pConfig, item, depth + 1);
                }
            }
        }
    }
    return hasMatchFlag;
}

void PollingDirFile::ClearTimeoutFileAndDir() {
    int32_t curTime = (int32_t)time(NULL);
    static int32_t s_lastClearTime = 0;
    if (curTime - s_lastClearTime < INT32_FLAG(polling_check_timeout_interval)) {
        return;
    }

    // Collect deleted files, so that it can notify PollingModify later.
    std::vector<SplitedFilePath> deleteFileVec;
    {
        ScopedSpinLock lock(mCacheLock);
        if (mDirCacheMap.size() > (size_t)INT32_FLAG(polling_dir_upperlimit)) {
            LOG_INFO(sLogger, ("start clear dir cache", mDirCacheMap.size()));
            s_lastClearTime = curTime;
            for (auto iter = mDirCacheMap.begin(); iter != mDirCacheMap.end();) {
                if ((NANO_CONVERTING * curTime - iter->second.GetLastModifyTime())
                    > NANO_CONVERTING * INT32_FLAG(polling_dir_timeout)) {
                    iter = mDirCacheMap.erase(iter);
                } else
                    ++iter;
            }
            LOG_INFO(sLogger, ("After clear", mDirCacheMap.size())("Cost time", time(NULL) - s_lastClearTime));
        }

        if (mFileCacheMap.size() > (size_t)INT32_FLAG(polling_file_upperlimit)) {
            LOG_INFO(sLogger, ("start clear file cache", mFileCacheMap.size()));
            s_lastClearTime = curTime;
            for (auto iter = mFileCacheMap.begin(); iter != mFileCacheMap.end();) {
                if ((NANO_CONVERTING * curTime - iter->second.GetLastModifyTime())
                    > NANO_CONVERTING * INT32_FLAG(polling_file_timeout)) {
                    // If the file has been added to PollingModify, remove it here.
                    if (iter->second.HasMatchedConfig() && iter->second.HasEventFlag()) {
                        deleteFileVec.push_back(SplitedFilePath(iter->first));
                        LOG_INFO(sLogger, ("delete file cache", iter->first)("vec size", deleteFileVec.size()));
                    }
                    iter = mFileCacheMap.erase(iter);
                } else
                    ++iter;
            }
            LOG_INFO(sLogger, ("After clear", mFileCacheMap.size())("Cost time", time(NULL) - s_lastClearTime));
        }
    }

    if (!deleteFileVec.empty()) {
        PollingModify::GetInstance()->AddDeleteFile(deleteFileVec);
    }
}

void PollingDirFile::ClearUnavailableFileAndDir() {
    // Collected genereated events.
    // For directory, if its cache item becomes unavailable, a TIMEOUT event will be
    // generated to notify that related directories should be unregistered to release
    // resources (LogInput::ProcessEvent -> EventDispatcher::UnregisterAllDir).
    std::vector<Event*> eventVec;

    {
        ScopedSpinLock lock(mCacheLock);
        for (auto iter = mDirCacheMap.begin(); iter != mDirCacheMap.end();) {
            auto& cacheItem = iter->second;
            if (mCurrentRound - cacheItem.GetLastCheckRound() > (uint64_t)INT32_FLAG(delete_dir_file_round)) {
                if (cacheItem.HasMatchedConfig()) {
                    eventVec.push_back(new Event(iter->first, string(), EVENT_TIMEOUT | EVENT_ISDIR, 0, 0));
                }
                iter = mDirCacheMap.erase(iter);
            } else
                ++iter;
        }

        // Files need not to generate delete event, it is PollingModify's responsibility.
        for (auto iter = mFileCacheMap.begin(); iter != mFileCacheMap.end();) {
            if (mCurrentRound - iter->second.GetLastCheckRound() > (uint64_t)INT32_FLAG(delete_dir_file_round)) {
                iter = mFileCacheMap.erase(iter);
            } else
                ++iter;
        }
    }

    if (!eventVec.empty())
        PollingEventQueue::GetInstance()->PushEvent(eventVec);
}

PollingDirFile::PollingDirFile() {
    mStatCount = 0;
    mCurrentRound = 0;
}

PollingDirFile::~PollingDirFile() {
}

} // namespace logtail
