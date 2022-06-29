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

#include "EventDispatcher.h"
#if defined(__linux__)
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <fnmatch.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <sys/socket.h>
#endif
#include <sys/types.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <vector>
#include <sys/types.h>
#if !defined(LOGTAIL_NO_TC_MALLOC)
#include <gperftools/malloc_extension.h>
#include <gperftools/tcmalloc.h>
#endif
#include "common/version.h"
#include "common/util.h"
#include "common/StringTools.h"
#include "common/HashUtil.h"
#include "common/LogtailCommonFlags.h"
#include "common/RuntimeUtil.h"
#include "common/ErrorUtil.h"
#include "common/GlobalPara.h"
#include "common/FileSystemUtil.h"
#include "common/TimeUtil.h"
#ifdef __linux__
#include "streamlog/StreamLogManager.h"
#endif
#include "app_config/AppConfig.h"
#include "event_handler/EventHandler.h"
#include "event_handler/LogInput.h"
#include "event/Event.h"
#include "processor/LogProcess.h"
#include "sender/Sender.h"
#include "profiler/LogFileProfiler.h"
#include "profiler/LogtailAlarm.h"
#include "profiler/LogIntegrity.h"
#include "profiler/LogLineCount.h"
#include "log_pb/metric.pb.h"
#include "log_pb/sls_logs.pb.h"
#include "checkpoint/CheckPointManager.h"
#include "checkpoint/CheckpointManagerV2.h"
#include "shennong/MetricSender.h"
#include "polling/PollingDirFile.h"
#include "polling/PollingModify.h"
#ifdef APSARA_UNIT_TEST_MAIN
#include "polling/PollingEventQueue.h"
#endif
#include "plugin/LogtailPlugin.h"
#include "config_manager/ConfigManager.h"
#if !defined(_MSC_VER)
#include "LogtailInsightDispatcher.h"
#endif

#ifdef LOGTAIL_RUNTIME_PLUGIN
#include "LogtailRuntimePlugin.h"
#endif

using std::string;
using std::vector;
using namespace sls_logs;

DECLARE_FLAG_INT32(check_point_dump_interval);
DECLARE_FLAG_INT32(ilogtail_max_epoll_events);
DECLARE_FLAG_INT32(ilogtail_epoll_wait_events);
DECLARE_FLAG_INT64(max_logtail_writer_packet_size);
DEFINE_FLAG_INT32(ilogtail_epoll_time_out, "default time out is 1s", 1);
DEFINE_FLAG_INT32(main_loop_check_interval, "seconds", 60);
DEFINE_FLAG_INT32(existed_file_active_timeout,
                  "when first monitor directory, file modified in 120 seconds will be collected",
                  120);
DEFINE_FLAG_INT32(tcmalloc_release_memory_interval, "force release memory held by tcmalloc, seconds", 300);
DEFINE_FLAG_BOOL(merge_shennong_metric, "merge LogGroup into LogPackageList if true", true);
DEFINE_FLAG_BOOL(fs_events_inotify_enable, "", true);
DEFINE_FLAG_INT32(checkpoint_find_max_cache_size, "", 100000);
DEFINE_FLAG_INT32(max_watch_dir_count, "", 100 * 1000);
DEFINE_FLAG_STRING(inotify_watcher_dirs_dump_filename, "", "inotify_watcher_dirs");
DEFINE_FLAG_INT32(exit_flushout_duration, "exit process flushout duration", 20 * 1000);
DEFINE_FLAG_INT32(search_checkpoint_default_dir_depth, "0 means only search current directory", 0);
DEFINE_FLAG_BOOL(enable_polling_discovery, "", true);

#define PBMSG 0

namespace logtail {

typedef struct MessageHdr {
    uint32_t type;
    uint32_t len;
} MessageHdr;

EventDispatcherBase::EventDispatcherBase() : mWatchNum(0), mInotifyWatchNum(0), mStreamLogManagerPtr(NULL) {
    /*
     * May add multiple inotify fd instances in the future,
     * so use epoll here though a little more sophisticated than select
     */
    // NOTE: epoll is used to drive mListenFd and mStreaLogTcpFd, they only work on Linux.
#if defined(__linux__)
    mEpollFd = epoll_create(INT32_FLAG(ilogtail_max_epoll_events));
    mListenFd = -1;
    mStreamLogTcpFd = -1;
#endif
    mEventListener = EventListener::GetInstance();
    if (!AppConfig::GetInstance()->NoInotify()) {
        if (!mEventListener->Init()) {
            LogtailAlarm::GetInstance()->SendAlarm(EPOLL_ERROR_ALARM,
                                                   string("faild to init inotify fd, errno:") + ToString(GetErrno()));
            LOG_ERROR(sLogger, ("faild to init inotify fd, errno:", errno));
        }
    } else {
        LOG_INFO(sLogger, ("do not use inoitfy", ""));
    }


    /** Register a timeout handler.
     * timeoutHandler -> Handle is called whenever no events occur
     * under any path added by AddTimeoutWatch or its offsprings' during timeout seconds.
     * That is to say, if any event occurs under one directory,
     * its time record propagates to its ancestors who also are added to timeout watch.
     * Propagation stops at the ancestor who hasn't been added to timeout watch.
     * Time is accounted when Dispatch is called.
     * mTimeoutHandler -> Handle is called when no events occur under path during timeout seconds.
     */
    mTimeoutHandler = new TimeoutHandler();
    mNonInotifyWd = -1;
}

EventDispatcherBase::~EventDispatcherBase() {
#if defined(__linux__)
    if (mStreamLogManagerPtr != NULL) {
        delete (StreamLogManager*)mStreamLogManagerPtr;
    }
    if (mEpollFd >= 0)
        close(mEpollFd);
    if (mStreamLogTcpFd >= 0)
        close(mStreamLogTcpFd);
    if (mListenFd >= 0)
        close(mListenFd);
#endif
    mEventListener->Destroy();
    if (mTimeoutHandler)
        delete mTimeoutHandler;
}

bool EventDispatcherBase::RegisterEventHandler(const char* path, Config* config, EventHandler*& handler) {
    // @todo
    // if this path belong to many config, if register one config with max_depth 0, then it will register fail
    if (!config->WithinMaxDepth(path)) {
        LOG_DEBUG(sLogger,
                  ("path is out of maxDepth", path)("logstore", config->mCategory)("max depth", config->mMaxDepth));
        return false;
    }
    fsutil::PathStat statBuf;
    // if symbolic link, return the referred inode
    if (!fsutil::PathStat::stat(path, statBuf)) {
        if (errno != EEXIST) {
            LOG_WARNING(sLogger, ("call stat() on path fail", path)("errno", errno));
            LogtailAlarm::GetInstance()->SendAlarm(REGISTER_INOTIFY_FAIL_ALARM,
                                                   "call stat() on path fail" + string(path)
                                                       + ", errno: " + ToString(errno) + ", will not be monitored",
                                                   config->GetProjectName(),
                                                   config->GetCategory(),
                                                   config->mRegion);
        }
        LOG_DEBUG(sLogger, ("call stat() on path fail", path)("errno", errno));
        return false;
    }
    if (!statBuf.IsDir()) {
        LOG_DEBUG(sLogger, ("path is not directory, will not register inotify monitor", path));
        return false;
    }
    uint64_t inode = statBuf.GetDevInode().inode;
    int wd;
    MapType<std::string, int>::Type::iterator pathIter = mPathWdMap.find(path);
    if (pathIter != mPathWdMap.end()) {
        wd = pathIter->second;

        if (inode != mWdDirInfoMap[wd]->mInode) {
            LOG_INFO(sLogger,
                     ("dir's inode was changed", path)("inode_before", mWdDirInfoMap[wd]->mInode)("inode_now", inode));
        }

        if (!AppConfig::GetInstance()->IgnoreDirInodeChanged() && inode != mWdDirInfoMap[wd]->mInode) {
            LOG_INFO(sLogger,
                     ("dir's inode was changed, unregister this path recursively",
                      path)("inode_before", mWdDirInfoMap[wd]->mInode)("inode_now", inode));
            UnregisterAllDir(path);
        } else {
            if (handler == ConfigManager::GetInstance()->GetSharedHandler()) {
                LOG_DEBUG(
                    sLogger,
                    ("ignore replace handler",
                     "both path and inode of dir is registered, handler is sharedHandler")("path", path)("wd", wd));
                return true;
            } else if (ConfigManager::GetInstance()->GetSharedHandler() == mWdDirInfoMap[wd]->mHandler) {
                LOG_DEBUG(sLogger,
                          ("replace handler", "both path and inode of dir is registered, just replace EventHandler")(
                              "path", path)("wd", wd));
                mWdDirInfoMap[wd]->mHandler = handler;
                return true;
            } else {
                LOG_DEBUG(sLogger, ("still use current hander, dir", path)("inode", inode)("wd", wd));
                handler = mWdDirInfoMap[wd]->mHandler;
                return true;
            }
        }
    }

    if (mWatchNum >= INT32_FLAG(max_watch_dir_count)) {
        LOG_WARNING(sLogger,
                    ("fail to monitor dir, max_watch_dir_count", INT32_FLAG(max_watch_dir_count))("dir", path));
        LogtailAlarm::GetInstance()->SendAlarm(DIR_EXCEED_LIMIT_ALARM,
                                               string("dir: ") + path
                                                   + " will not monitored, dir count should less than "
                                                   + ToString(INT32_FLAG(max_watch_dir_count)),
                                               config->GetProjectName(),
                                               config->GetCategory(),
                                               config->mRegion);
        return false;
    }

    wd = -1;
    if (mInotifyWatchNum >= INT32_FLAG(default_max_inotify_watch_num)) {
        LOG_INFO(sLogger,
                 ("failed to add inotify watcher for dir", path)("max allowd inotify watchers",
                                                                 INT32_FLAG(default_max_inotify_watch_num)));
        LogtailAlarm::GetInstance()->SendAlarm(INOTIFY_DIR_NUM_LIMIT_ALARM,
                                               string("failed to register inotify watcher for dir") + path);
    } else {
        // need check mEventListener valid
        if (mEventListener->IsInit() && !AppConfig::GetInstance()->IsInInotifyBlackList(path)) {
            wd = mEventListener->AddWatch(path);
            if (!mEventListener->IsValidID(wd)) {
                string str = ErrnoToString(GetErrno());
                LOG_WARNING(sLogger, ("failed to register dir", path)("reason", str));
#if defined(__linux__)
                // work around bug 13229654
                if (errno == EINVAL || errno == EBADF) {
                    LOG_ERROR(sLogger,
                              ("failed to register dir", path)("errno", errno)("error", str)("force exit",
                                                                                             "wait 10 seconds."));
                    LogtailAlarm::GetInstance()->SendAlarm(LOGTAIL_CRASH_ALARM,
                                                           string("Failed to register dir:  ") + path + ", errno: "
                                                               + ToString(errno) + ", error: " + str + ", force exit");
                    LogtailAlarm::GetInstance()->ForceToSend();
                    sleep(10);
                    _exit(1);
                }
#endif
                if (config->IsTimeout(path))
                    LogtailAlarm::GetInstance()->SendAlarm(REGISTER_INOTIFY_FAIL_ALARM,
                                                           string("Failed to register dir: ") + path + ", reason: "
                                                               + str + ", project: " + config->GetProjectName()
                                                               + ", logstore: " + config->GetCategory());
                else
                    LogtailAlarm::GetInstance()->SendAlarm(REGISTER_INOTIFY_FAIL_ALARM,
                                                           string("Failed to register dir: ") + path
                                                               + ", reason: " + str + ", no timeout");
            } else {
                // recheck inode, wd is relevance to inode
                if (mWdDirInfoMap.find(wd) != mWdDirInfoMap.end()) {
                    // e.g. this path is symbolic link, and it reference to dir which was registered before
                    LOG_DEBUG(sLogger,
                              ("can not register inotify monitor", path)("inode", inode)("wd", wd)(
                                  "reason", "there is already a dir in inotify watch list shard the same inode"));
                    wd = -1;
                } else
                    mInotifyWatchNum++;
            }
        }
    }

    bool dirTimeOutFlag = config->IsTimeout(path);

    if (!mEventListener->IsValidID(wd)) {
        if (dirTimeOutFlag) {
            LOG_DEBUG(sLogger,
                      ("Drop timeout path, source", path)("config, basepath", config->mBasePath)(
                          "preserve", config->mIsPreserve)("preseveDepth", config->mPreserveDepth)("maxDepth",
                                                                                                   config->mMaxDepth));
            return false;
        }
        wd = mNonInotifyWd;
        if (mNonInotifyWd == INT_MIN)
            mNonInotifyWd = -1;
        else
            --mNonInotifyWd;
    }
    fsutil::PathStat lstatBuf;
    bool isSymbolicLink = false;
    if (fsutil::PathStat::lstat(path, lstatBuf) && lstatBuf.IsLink())
        isSymbolicLink = true;

    if (mBrokenLinkSet.find(path) != mBrokenLinkSet.end()) {
        mBrokenLinkSet.erase(path);
    }
    LOG_INFO(sLogger,
             ("add a new watcher for dir", path)("isSymbolicLink", isSymbolicLink)("dir inode", inode)("wd", wd));
    DirInfo* dirInfo = new DirInfo(path, inode, isSymbolicLink, handler);
    AddOneToOneMapEntry(dirInfo, wd);
    ++mWatchNum;
    AddExistedFileEvents(path, wd);
    if (dirTimeOutFlag) {
        AddTimeoutWatch(path);
        LOG_DEBUG(
            sLogger,
            ("AddTimeoutWatch, source", path)("config, basepath", config->mBasePath)("preserve", config->mIsPreserve)(
                "preseveDepth", config->mPreserveDepth)("maxDepth", config->mMaxDepth));
    }

    return true;
}

// read files when add dir inotify watcher at first time
void EventDispatcherBase::AddExistedFileEvents(const char* path, int wd) {
    fsutil::Dir dir(path);
    if (!dir.Open()) {
        auto err = GetErrno();
        if (!fsutil::Dir::IsENOENT(err)) {
            LOG_ERROR(sLogger, ("Open dir fail", path)("error", ErrnoToString(err)));
        }
        return;
    }
    int32_t MAX_TAIL_FILE_COUNT = 50;
    vector<Config*> configs;
    ConfigManager::GetInstance()->GetRelatedConfigs(path, configs);
    for (vector<Config*>::iterator iter = configs.begin(); iter != configs.end(); ++iter) {
        if ((*iter)->mMaxDepth == 0) {
            MAX_TAIL_FILE_COUNT = 1000;
            break;
        }
    }

    fsutil::Entry ent;
    int32_t curTime = time(NULL);
    std::vector<Event*> eventVec;
    int32_t tailFileCount = 0;
    while (ent = dir.ReadNext(false)) {
        ++tailFileCount;
        if (tailFileCount > MAX_TAIL_FILE_COUNT) {
            LOG_DEBUG(sLogger,
                      ("tail existed files for dir", path)("too many files", "ignore the rest")("max file count",
                                                                                                MAX_TAIL_FILE_COUNT));
            break;
        }
        auto entName = ent.Name();
        if (!IsValidSuffix(entName))
            continue;

        string item = PathJoin(path, entName);

        // move to AddExistedCheckPointFileEvents
        //CheckPointPtr checkPointSharePtr;
        //if (CheckPointManager::Instance()->GetCheckPoint(item, checkPointSharePtr))
        //{
        //    if (checkPointSharePtr->mInode == NO_BLOCK_INODE || checkPointSharePtr->mInode == GetFileInode(item))
        //    {
        //        eventVec.push_back(new Event(string(path), string(ent->d_name), EVENT_MODIFY, wd));
        //    }
        //    else
        //    {
        //        CheckPointManager::Instance()->DeleteCheckPoint(item);
        //    }
        //    continue;
        //}
        bool isMatch = false;
        bool tailExisted = false;
        for (vector<Config*>::iterator iter = configs.begin(); iter != configs.end(); ++iter) {
            if (fnmatch((*iter)->mFilePattern.c_str(), entName.c_str(), 0) == 0) {
                isMatch = true;
                if ((*iter)->mTailExisted) {
                    tailExisted = true;
                    break;
                }
            }
        }
        if (!isMatch)
            continue;
        fsutil::PathStat buf;
        if (fsutil::PathStat::stat(item, buf) && buf.IsRegFile()
            && (curTime - int32_t(buf.GetMtime()) < INT32_FLAG(existed_file_active_timeout) || tailExisted)) {
            eventVec.push_back(new Event(string(path), entName, EVENT_MODIFY, wd));
        }
    }
    for (size_t i = 0; i < eventVec.size(); ++i) {
        LOG_DEBUG(sLogger,
                  ("add exist file",
                   eventVec[i]->GetObject())("inode", eventVec[i]->GetInode())("config", eventVec[i]->GetConfigName()));
    }
    if (eventVec.size() > 0)
        LogInput::GetInstance()->PushEventQueue(eventVec);
}

EventDispatcherBase::ValidateCheckpointResult
EventDispatcherBase::validateCheckpoint(CheckPointPtr& checkpoint,
                                        std::map<DevInode, SplitedFilePath>& cachePathDevInodeMap,
                                        std::vector<Event*>& eventVec) {
    static ConfigManager* pConfigMananger = ConfigManager::GetInstance();
    Config* config = pConfigMananger->FindConfigByName(checkpoint->mConfigName);
    if (config == NULL) {
        LOG_INFO(sLogger,
                 ("config", checkpoint->mConfigName)("delete checkpoint without existed config, file path",
                                                     checkpoint->mFileName));
        return ValidateCheckpointResult::kConfigNotFound;
    }

    // Use FileName (logical absolute path) to do config matching.
    const std::string& filePath = checkpoint->mFileName;
    const std::string& realFilePath = checkpoint->mRealFileName.empty() ? filePath : checkpoint->mRealFileName;
    size_t lastSeparator = filePath.find_last_of(PATH_SEPARATOR);
    if (lastSeparator == std::string::npos || lastSeparator == (size_t)0 || lastSeparator >= filePath.size()) {
        LOG_INFO(sLogger, ("delete checkpoint because of invalid file path", filePath)("real path", realFilePath));
        return ValidateCheckpointResult::kInvalidFilePath;
    }
    string path = filePath.substr(0, lastSeparator);
    string fileName = filePath.substr(lastSeparator + 1);

    // Check if the config in checkpoint still matches the file?
    std::vector<Config*> matchedConfigs;
    AppConfig::GetInstance()->IsAcceptMultiConfig()
        ? pConfigMananger->FindAllMatch(matchedConfigs, path, fileName)
        : pConfigMananger->FindMatchWithForceFlag(matchedConfigs, path, fileName);
    bool stillMatch = false;
    for (size_t idx = 0; idx < matchedConfigs.size(); ++idx) {
        if (matchedConfigs[idx]->mConfigName == checkpoint->mConfigName) {
            stillMatch = true;
            break;
        }
    }
    if (!stillMatch) {
        std::string matchList;
        for (size_t idx = 0; idx < matchedConfigs.size(); ++idx) {
            matchList += (!matchList.empty() ? "," : "") + matchedConfigs[idx]->mConfigName;
        }
        LOG_INFO(sLogger,
                 ("delete checkpoint because match list of the file "
                  "has changed",
                  filePath)("real path", realFilePath)("old config name", checkpoint->mConfigName)("new match list",
                                                                                                   matchList));
        return ValidateCheckpointResult::kConfigNotMatched;
    }

    // delete checkpoint if file path is not exist
    MapType<std::string, int>::Type::iterator pathIter = mPathWdMap.find(path);
    if (pathIter == mPathWdMap.end()) {
        LOG_INFO(sLogger, ("delete checkpoint because file path is removed", filePath)("real path", realFilePath));
        return ValidateCheckpointResult::kLogDirNotWatched;
    }

    int wd = pathIter->second;
    DevInode devInode = GetFileDevInode(realFilePath);
    if (devInode.IsValid() && checkpoint->mDevInode == devInode) {
        if (!CheckFileSignature(
                realFilePath, checkpoint->mSignatureHash, checkpoint->mSignatureSize, config->mIsFuseMode)) {
            LOG_INFO(sLogger,
                     ("ignore checkpoint, same dev inode but signature "
                      "has changed",
                      filePath)("real path", realFilePath)("inode", devInode.inode)("sig", checkpoint->mSignatureHash)(
                         "offset", checkpoint->mOffset));
            return ValidateCheckpointResult::kSigChanged;
        }

        LOG_DEBUG(sLogger,
                  ("push check point event", filePath)("real path", realFilePath)("inode", devInode.inode)(
                      "sig", checkpoint->mSignatureHash)("offset", checkpoint->mOffset));
        eventVec.push_back(
            new Event(path, fileName, EVENT_MODIFY, wd, 0, checkpoint->mDevInode.dev, checkpoint->mDevInode.inode));
        eventVec[eventVec.size() - 1]->SetConfigName(checkpoint->mConfigName);
        return ValidateCheckpointResult::kNormal;
    }

    // File path not exist or dev inode mismatch, do search.
    // Can not do search for checkpoint with signature size == 0.
    // See https://aone.alibaba-inc.com/req/29052357.
    if (0 == checkpoint->mSignatureSize) {
        LOG_INFO(sLogger,
                 ("delete checkpoint because signature size is zero",
                  filePath)("real path", realFilePath)("inode", checkpoint->mDevInode.inode));
        return ValidateCheckpointResult::kZeroSigSize;
    }

    // Try to find the real file with dev inode, check cache at first.
    std::map<DevInode, SplitedFilePath>::iterator findIter = cachePathDevInodeMap.find(checkpoint->mDevInode);
    if (findIter != cachePathDevInodeMap.end()) {
        if (findIter->second.mFileDir != path) {
            LOG_INFO(sLogger,
                     ("ignore check point, file dev inode has been used in other dir",
                      filePath)("old real path", realFilePath)(findIter->second.mFileDir, findIter->second.mFileName));
            return ValidateCheckpointResult::kLogDirChanged;
        }

        if (CheckFileSignature(PathJoin(path, findIter->second.mFileName),
                               checkpoint->mSignatureHash,
                               checkpoint->mSignatureSize,
                               config->mIsFuseMode)) {
            LOG_INFO(sLogger,
                     ("find check point, file name has changed in this dir",
                      filePath)("old real path", realFilePath)("new file name", findIter->second.mFileName));
            checkpoint->mRealFileName = PathJoin(findIter->second.mFileDir, findIter->second.mFileName);
            eventVec.push_back(
                new Event(path, fileName, EVENT_MODIFY, wd, 0, checkpoint->mDevInode.dev, checkpoint->mDevInode.inode));
            eventVec[eventVec.size() - 1]->SetConfigName(checkpoint->mConfigName);
            return ValidateCheckpointResult::kRotate;
        }

        if (0 == config->mAdvancedConfig.mExactlyOnceConcurrency) {
            LOG_INFO(sLogger,
                     ("ignore check point, file signature has changed", filePath)("old real path", realFilePath)(
                         findIter->second.mFileDir, findIter->second.mFileName)("inode", checkpoint->mDevInode.inode));
            return ValidateCheckpointResult::kSigChanged;
        }

        LOG_INFO(sLogger,
                 ("exactly once checkpoint", "delete cache and retry search")("old real path", realFilePath)(
                     findIter->second.mFileDir, findIter->second.mFileName)("inode", checkpoint->mDevInode.inode));
        cachePathDevInodeMap.erase(findIter);
    }

    if (cachePathDevInodeMap.size() >= (size_t)INT32_FLAG(checkpoint_find_max_cache_size)) {
        LOG_INFO(sLogger,
                 ("can not find this log (find cache full), delete checkpoint", filePath)("real path", realFilePath));
        LogtailAlarm::GetInstance()->SendAlarm(CHECKPOINT_ALARM,
                                               string("can not find log, delete checkpoint ") + filePath + " real path "
                                                   + realFilePath + " cache devinode size "
                                                   + ToString(cachePathDevInodeMap.size()));
        return ValidateCheckpointResult::kCacheFull;
    }

    LOG_INFO(sLogger,
             ("search file in directory for checkpoint", filePath)("dir", path)("inode", checkpoint->mDevInode.inode)(
                 "config", config->mConfigName)("depth", config->mAdvancedConfig.mSearchCheckpointDirDepth));
    auto const searchResult = SearchFilePathByDevInodeInDirectory(
        path, config->mAdvancedConfig.mSearchCheckpointDirDepth, checkpoint->mDevInode, &cachePathDevInodeMap);
    if (searchResult) {
        const auto& newRealPath = searchResult.value();
        if (CheckFileSignature(
                newRealPath, checkpoint->mSignatureHash, checkpoint->mSignatureSize, config->mIsFuseMode)) {
            LOG_INFO(sLogger,
                     ("find check point, file path has changed", filePath)("old real path", realFilePath)(
                         "new real path", newRealPath)("inode", checkpoint->mDevInode.inode));
            checkpoint->mRealFileName = newRealPath;
            eventVec.push_back(
                new Event(path, fileName, EVENT_MODIFY, wd, 0, checkpoint->mDevInode.dev, checkpoint->mDevInode.inode));
            eventVec[eventVec.size() - 1]->SetConfigName(checkpoint->mConfigName);
            return ValidateCheckpointResult::kRotate;
        }

        LOG_INFO(sLogger,
                 ("ignore check point, file signature has changed", filePath)("old real path", realFilePath)(
                     "new real path", newRealPath)("inode", checkpoint->mDevInode.inode));
        return ValidateCheckpointResult::kSigChanged;
    }

    // Can not find dev inode, delete this checkpoint.
    LOG_INFO(sLogger,
             ("can not find this log, delete checkpoint",
              filePath)("real path", realFilePath)("cache devinode size", cachePathDevInodeMap.size()));
    LogtailAlarm::GetInstance()->SendAlarm(CHECKPOINT_ALARM,
                                           string("can not find log, delete checkpoint ") + filePath + " real path "
                                               + realFilePath + " cache devinode size "
                                               + ToString(cachePathDevInodeMap.size()));
    return ValidateCheckpointResult::kDevInodeNotFound;
}

void EventDispatcherBase::AddExistedCheckPointFileEvents() {
    // All checkpoint will be add into event queue or be deleted
    // This operation will delete not existed file's check point
    std::map<DevInode, SplitedFilePath> cachePathDevInodeMap;
    auto& checkPointMap = CheckPointManager::Instance()->GetAllFileCheckPoint();
    LOG_INFO(sLogger, ("start add existed check point events, size", checkPointMap.size()));
    std::vector<CheckPointManager::CheckPointKey> deleteKeyVec;
    std::vector<Event*> eventVec;
    for (auto iter = checkPointMap.begin(); iter != checkPointMap.end(); ++iter) {
        auto const result = validateCheckpoint(iter->second, cachePathDevInodeMap, eventVec);
        if (!(result == ValidateCheckpointResult::kNormal || result == ValidateCheckpointResult::kRotate)) {
            deleteKeyVec.push_back(iter->first);
        }
    }
    for (size_t i = 0; i < deleteKeyVec.size(); ++i) {
        checkPointMap.erase(deleteKeyVec[i]);
    }
    LOG_INFO(sLogger,
             ("add existed checkpoint events, size", checkPointMap.size())("cache size", cachePathDevInodeMap.size())(
                 "event size", eventVec.size())("delete size", deleteKeyVec.size()));
    auto const v1EventCount = eventVec.size();

    // Load exactly once checkpoints and create events from them.
    // Because they are not in v1 checkpoint manager, no need to delete them.
    auto exactlyOnceConfigs = ConfigManager::GetInstance()->GetMatchedConfigs(
        [](Config* config) { return config->mAdvancedConfig.mExactlyOnceConcurrency > 0; });
    if (!exactlyOnceConfigs.empty()) {
        static auto sCptMV2 = CheckpointManagerV2::GetInstance();
        auto exactlyOnceCpts = sCptMV2->ScanCheckpoints(exactlyOnceConfigs);
        LOG_INFO(sLogger,
                 ("start add exactly once checkpoint events",
                  "")("config size", exactlyOnceConfigs.size())("scanned checkpoint size", exactlyOnceCpts.size()));
        std::vector<std::pair<std::string, PrimaryCheckpointPB>*> batchUpdateCpts;
        std::vector<std::pair<std::string, PrimaryCheckpointPB>*> batchDeleteCpts;
        for (size_t idx = 0; idx < exactlyOnceCpts.size(); ++idx) {
            auto& cptPair = exactlyOnceCpts[idx];
            auto& cpt = cptPair.second;
            auto v1Cpt = std::make_shared<CheckPoint>(cpt.log_path(),
                                                      0,
                                                      cpt.sig_size(),
                                                      cpt.sig_hash(),
                                                      DevInode(cpt.dev(), cpt.inode()),
                                                      cpt.config_name(),
                                                      cpt.real_path(),
                                                      1);
            const auto result = validateCheckpoint(v1Cpt, cachePathDevInodeMap, eventVec);
            switch (result) {
                case ValidateCheckpointResult::kNormal:
                    break;

                case ValidateCheckpointResult::kRotate:
                    LOG_INFO(sLogger,
                             ("exactly once file rotate",
                              cptPair.first)("old checkpoint", cpt.DebugString())("new path", v1Cpt->mRealFileName));
                    cpt.set_real_path(v1Cpt->mRealFileName);
                    batchUpdateCpts.push_back(&cptPair);
                    break;

                default:
                    batchDeleteCpts.push_back(&cptPair);
                    LOG_WARNING(sLogger,
                                ("delete invalid exactly once checkpoint",
                                 static_cast<int>(result))("key", cptPair.first)("checkpoint", cpt.DebugString()));
                    break;
            }
        }
        uint64_t updateUsedTimeInMs = 0;
        if (!batchUpdateCpts.empty()) {
            updateUsedTimeInMs = sCptMV2->UpdatePrimaryCheckpoints(batchUpdateCpts);
        }
        uint64_t deleteUsedTimeInMs = 0;
        if (!batchDeleteCpts.empty()) {
            deleteUsedTimeInMs = sCptMV2->DeletePrimaryCheckpoints(batchDeleteCpts);
        }
        LOG_INFO(sLogger,
                 ("finish add exactly once checkpoint events",
                  "")("cache size", cachePathDevInodeMap.size())("update size", batchUpdateCpts.size())(
                     "update used time", updateUsedTimeInMs)("event size", eventVec.size() - v1EventCount)(
                     "delete size", batchDeleteCpts.size())("delete used time", deleteUsedTimeInMs));
    }

    for (size_t i = 0; i < eventVec.size(); ++i) {
        LOG_DEBUG(sLogger,
                  ("event from checkpoint", i)("file", eventVec[i]->GetObject())("inode", eventVec[i]->GetInode())(
                      "config", eventVec[i]->GetConfigName()));
    }
    if (eventVec.size() > 0) {
        // Sort by Source/Object (length+alphabet) in event to adjust the order of rotating files.
        // eg. /log/a.log.10 -> /log/a.log.9 -> /log/a.log.8 -> ...
        std::sort(eventVec.begin(), eventVec.end(), Event::CompareByFullPath);
        LogInput::GetInstance()->PushEventQueue(eventVec);
    }
}

bool EventDispatcherBase::AddTimeoutWatch(const char* path) {
    MapType<string, int>::Type::iterator itr = mPathWdMap.find(path);
    if (itr != mPathWdMap.end()) {
        mWdUpdateTimeMap[itr->second] = time(NULL);
        return true;
    } else {
        return false;
    }
}

void EventDispatcherBase::AddOneToOneMapEntry(DirInfo* dirInfo, int wd) {
    mPathWdMap[dirInfo->mPath] = wd;
    mWdDirInfoMap[wd] = dirInfo;
}

void EventDispatcherBase::RemoveOneToOneMapEntry(int wd) {
    MapType<int, DirInfo*>::Type::iterator itr = mWdDirInfoMap.find(wd);
    if (itr == mWdDirInfoMap.end())
        return;
    mPathWdMap.erase((itr->second)->mPath);
    delete itr->second;
    mWdDirInfoMap.erase(itr);
}

//add timeout propagation and process logic
bool EventDispatcherBase::Dispatch() {
    mMainThreadRunning = true;
#if defined(_MSC_VER)
    InitWindowsSignalObject();
#endif
#if defined(__linux__)
    if (mEpollFd == -1) {
        LogtailAlarm::GetInstance()->SendAlarm(EPOLL_ERROR_ALARM,
                                               string("Failed to create epoll fd, errno:") + ToString(errno));
        LOG_ERROR(sLogger, ("failed to create epoll fd, errno", errno));
        return false;
    }
#endif

    // Add Domain Socket Listen fd to epoll list
#if defined(__linux__)
    InitShennong();
    if (AppConfig::GetInstance()->GetOpenStreamLog() && AddStreamLogTcpSocketToEpoll()) {
        mStreamLogManagerPtr = new StreamLogManager(AppConfig::GetInstance()->GetStreamLogPoolSizeInMb(),
                                                    AppConfig::GetInstance()->GetStreamLogRcvLenPerCall(),
                                                    mEpollFd);
    }
#endif
    {
        auto pollingModify = PollingModify::GetInstance();
        auto pollingDirFile = PollingDirFile::GetInstance();
        if (BOOL_FLAG(enable_polling_discovery)) {
            pollingModify->Start();
            pollingDirFile->Start();
        } else {
            LOG_INFO(sLogger, ("polling discovery is disabled", ""));
        }
    }

    LogInput::GetInstance()->Start();
    LogProcess::GetInstance()->Start();

    time_t prevTime = time(NULL);
    time_t curTime;
    time_t StreamLogPrevTime = time(NULL);

#if defined(__linux__)
    struct epoll_event event[INT32_FLAG(ilogtail_max_epoll_events)];
#endif
    srand(prevTime);
    time_t lastCheckDir = prevTime - rand() % 60;
    time_t lastTcmallocReleaseMemTime = prevTime - rand() % 60;
    while (mMainThreadRunning) {
#if defined(__linux__)
        if (time(NULL) - StreamLogPrevTime > 10) {
            if (mStreamLogManagerPtr != NULL) {
                ((StreamLogManager*)mStreamLogManagerPtr)->AwakenTimeoutFds();
            }
            StreamLogPrevTime = time(NULL);
        }
#endif

#if defined(_MSC_VER)
        SyncWindowsSignalObject();
#endif

        if (LogtailGlobalPara::Instance()->GetSigtermFlag())
            ExitProcess();

#if defined(__linux__)
        int nfd = epoll_wait(
            mEpollFd, event, INT32_FLAG(ilogtail_epoll_wait_events), INT32_FLAG(ilogtail_epoll_time_out) * 1000);
        int savedErrno = GetErrno();
#elif defined(_MSC_VER)
        sleep(INT32_FLAG(ilogtail_epoll_time_out));
#endif
        UpdateConfig();
#if defined(__linux__)
        if (nfd == -1) {
            if (savedErrno != EINTR) {
                LOG_FATAL(sLogger, ("epoll loop", "exit")(ToString(savedErrno), ErrnoToString(savedErrno)));
                LogtailAlarm::GetInstance()->SendAlarm(EPOLL_ERROR_ALARM, string("epoll loop exit abnormally"));
                return false;
            }
        } else if (nfd > 0) {
            for (int n = 0; n < nfd; ++n) {
                if (IsInterupt())
                    break;

#if defined(__linux__)
                bool isStreamLogTcpFd = mStreamLogManagerPtr == NULL
                    ? false
                    : ((StreamLogManager*)mStreamLogManagerPtr)->AcceptedFdsContains(event[n].data.fd);
#endif
                // handle ERROR (exclude inotify fd to defense)
                if ((event[n].events & EPOLLERR) || (event[n].events & EPOLLHUP)) {
                    int closeFd = event[n].data.fd;
#if defined(__linux__)
                    if (isStreamLogTcpFd) {
                        LOG_INFO(sLogger, ("message", "StreamLog fd error")("fd", ToString(event[n].data.fd)));
                        ((StreamLogManager*)mStreamLogManagerPtr)->DeleteFd(event[n].data.fd);
                    } else
#endif
                    {
                        // clear buffer
                        ErasePacketBuffer(closeFd);

                        // client exception
                        close(closeFd);
                    }
                    // delete fd from epoll
                    struct epoll_event ee;
                    ee.events = 0;
                    ee.data.fd = event[n].data.fd;
                    epoll_ctl(mEpollFd, EPOLL_CTL_DEL, closeFd, &ee);

                    continue;
                }

                if (int32_t(event[n].data.fd) >= 0) {
                    if (event[n].data.fd == mListenFd) {
                        AcceptConnection(mListenFd, mEpollFd);
                    }
#if defined(__linux__)
                    else if (event[n].data.fd == mStreamLogTcpFd) {
                        int StreamLogFd = AcceptTcpConnection(mStreamLogTcpFd, mEpollFd);
                        if (StreamLogFd != -1) {
                            ((StreamLogManager*)mStreamLogManagerPtr)->InsertToAcceptedFds(StreamLogFd);
                        }
                    }
#endif
                    else if (event[n].events & EPOLLIN) {
#if defined(__linux__)
                        if (isStreamLogTcpFd) {
                            struct epoll_event ee;
                            ee.events = 0;
                            ee.data.fd = event[n].data.fd;
                            epoll_ctl(mEpollFd, EPOLL_CTL_DEL, event[n].data.fd, &ee);
                            ((StreamLogManager*)mStreamLogManagerPtr)->AddRcvTask(event[n].data.fd);
                        } else
#endif
                        {
                            ReadMessages(event[n].data.fd);
                        }
                    }
                }
            }
        }
#endif

        ExtraWork();
        curTime = time(NULL);
        DumpCheckPointPeriod(curTime);
        if (curTime - lastCheckDir >= INT32_FLAG(main_loop_check_interval)) {
            LogFileProfiler::GetInstance()->SendProfileData();
#if defined(__linux__)
            CheckShennong();
#endif
            lastCheckDir = curTime;
        }
        if (curTime - lastTcmallocReleaseMemTime >= INT32_FLAG(tcmalloc_release_memory_interval)) {
#ifndef LOGTAIL_NO_TC_MALLOC
            MallocExtension::instance()->ReleaseFreeMemory();
            // tc_malloc_stats();
#endif
            lastTcmallocReleaseMemTime = curTime;
        }
    }
    return true;
}

void EventDispatcherBase::DumpInotifyWatcherDirs() {
    string filename = GetProcessExecutionDir() + STRING_FLAG(inotify_watcher_dirs_dump_filename);
    FILE* pFile = fopen(filename.c_str(), "w");
    if (pFile == NULL) {
        LOG_WARNING(sLogger, ("open file (dump inotify watcher dirs) failed", filename)("errno", errno));
        return;
    }
    string outline = string("WatchNum: ") + ToString(mWatchNum) + ", NotifyNum: " + ToString(mInotifyWatchNum)
        + ", WdUpdateTimeMap: " + ToString(mWdUpdateTimeMap.size()) + ", PathWdMap: " + ToString(mPathWdMap.size())
        + ", WdDirInfoMap: " + ToString(mWdDirInfoMap.size()) + ", BrokenLinkSet: " + ToString(mBrokenLinkSet.size())
        + "\n";
    fwrite(outline.c_str(), 1, outline.size(), pFile);

    string info = "directory\twatch_descriptor\n";
    for (MapType<string, int>::Type::iterator iter = mPathWdMap.begin(); iter != mPathWdMap.end(); ++iter) {
        if (iter->second > 0)
            info.append(iter->first).append("\t").append(ToString(iter->second)).append("\n");
    }
    fwrite(info.c_str(), 1, info.size(), pFile);
    fclose(pFile);
}

void EventDispatcherBase::CheckSymbolicLink() {
    // consider symbolic link like this: a -> b -> c
    // "a" and "b" is symbolic link, "c" is a directory, "a" is registered in inotify
    vector<string> dirToCheck(mBrokenLinkSet.begin(), mBrokenLinkSet.end());
    for (MapType<string, int>::Type::iterator iter = mPathWdMap.begin(); iter != mPathWdMap.end(); ++iter) {
        string path = iter->first;
        int wd = iter->second;
        if (mWdDirInfoMap.find(wd) == mWdDirInfoMap.end())
            LOG_WARNING(sLogger,
                        ("maybe something wrong, path in mPathWdMap", path)("but wd not exist in mWdDirInfoMap", wd));
        else if (mWdDirInfoMap[wd]->mIsSymbolicLink)
            dirToCheck.push_back(path);
    }
    vector<string> dirToAdd;
    for (vector<string>::iterator dirIter = dirToCheck.begin(); dirIter != dirToCheck.end(); ++dirIter) {
        string path = *dirIter;
        fsutil::PathStat lstatBuf;
        if (!fsutil::PathStat::lstat(path, lstatBuf)) //check path itself
        {
            // when "a" was removed, there will be no inotify event
            LOG_WARNING(sLogger, ("path not exist, remove inotify monitor", path));
            if (mBrokenLinkSet.find(path) != mBrokenLinkSet.end()) {
                mBrokenLinkSet.erase(path);
            }
            UnregisterAllDir(path);
        } else {
            fsutil::PathStat statBuf;
            if (!fsutil::PathStat::stat(path, statBuf)) // check path refrence to
            {
                // when "b" was removed, there will be no inotify event
                LOG_WARNING(sLogger, ("existed symbolic link invalid, remove inotify monitor", path));
                UnregisterAllDir(path);
                mBrokenLinkSet.insert(path);
            } else if (statBuf.IsDir()) {
                // when "c" or "b" was removed, a will be put into broken link set
                // then the directory "c" or "b" be created with no IN_CRETATE event, should add inotify monitor for "a" again
                if (mBrokenLinkSet.find(path) != mBrokenLinkSet.end()) {
                    mBrokenLinkSet.erase(path);
                }
                dirToAdd.push_back(path);
            }
        }
    }
    for (vector<string>::iterator iter = dirToAdd.begin(); iter != dirToAdd.end(); ++iter) {
        Config* config = ConfigManager::GetInstance()->FindBestMatch(*iter);
        if (config && IsDirRegistered(*iter) == PATH_INODE_NOT_REGISTERED)
            ConfigManager::GetInstance()->RegisterHandlersRecursively(*iter, config, true);
    }
}

void EventDispatcherBase::ReadInotifyEvents(std::vector<Event*>& eventVec) {
    mEventListener->ReadEvents(eventVec);
}

std::vector<std::pair<std::string, EventHandler*>>
EventDispatcherBase::FindAllSubDirAndHandler(const std::string& baseDir) {
    LOG_DEBUG(sLogger, ("Find all sub dir", baseDir));
    std::vector<std::pair<std::string, EventHandler*>> dirAndHandlers;
    size_t baseDirSize = baseDir.size();
    MapType<std::string, int>::Type::iterator it = mPathWdMap.begin();
    for (; it != mPathWdMap.end(); ++it) {
        const string& pathName = it->first;
        size_t pathNameSize = pathName.size();
        if (baseDirSize > pathNameSize) {
            continue;
        }
        if (memcmp(baseDir.c_str(), pathName.c_str(), baseDirSize) == 0
            && (pathNameSize == baseDirSize || pathName[baseDirSize] == PATH_SEPARATOR[0])) {
            dirAndHandlers.push_back(std::make_pair(it->first, mWdDirInfoMap[it->second]->mHandler));
        }
    }
    return dirAndHandlers;
}

void EventDispatcherBase::UnregisterAllDir(const string& baseDir) {
    LOG_DEBUG(sLogger, ("Remove all sub dir", baseDir));
    auto subDirAndHandlers = FindAllSubDirAndHandler(baseDir);
    for (auto& subDirAndHandler : subDirAndHandlers) {
        mTimeoutHandler->Handle(Event(subDirAndHandler.first.c_str(), "", 0, 0));
    }
}

void EventDispatcherBase::UnregisterEventHandler(const char* path) {
    MapType<std::string, int>::Type::iterator pos = mPathWdMap.find(path);
    if (pos == mPathWdMap.end())
        return;
    int wd = pos->second;
    if (mWdDirInfoMap[wd]->mIsSymbolicLink) {
        fsutil::PathStat lstatBuf;
        if (fsutil::PathStat::lstat(path, lstatBuf)) // TODO: Need review, might be a bug.
        {
            //path(symbolic link) existed, the dir it refrence to is deleted
            mBrokenLinkSet.insert(path);
        }
    }
    RemoveOneToOneMapEntry(wd);
    mWdUpdateTimeMap.erase(wd);
    if (mEventListener->IsValidID(wd) && mEventListener->IsInit()) {
        mEventListener->RemoveWatch(wd);
        mInotifyWatchNum--;
    }
    mWatchNum--;
    LOG_DEBUG(sLogger, ("unregister dir", path));
}

void EventDispatcherBase::StopAllDir(const std::string& baseDir) {
    LOG_DEBUG(sLogger, ("Stop all sub dir", baseDir));
    auto subDirAndHandlers = FindAllSubDirAndHandler(baseDir);
    for (auto& subDirAndHandler : subDirAndHandlers) {
        Event e(subDirAndHandler.first.c_str(), "", EVENT_ISDIR | EVENT_CONTAINER_STOPPED, -1, 0);
        subDirAndHandler.second->Handle(e);
    }
}

DirRegisterStatus EventDispatcherBase::IsDirRegistered(const string& path) {
    fsutil::PathStat statBuf;
    // if symbolic link, return the referred inode
    if (!fsutil::PathStat::stat(path, statBuf)) {
        LOG_WARNING(sLogger, ("call stat() on path fail", path)("errno", GetErrno()));
        return GET_REGISTER_STATUS_ERROR;
    }
    uint64_t inode = statBuf.GetDevInode().inode;

    MapType<string, int>::Type::iterator pathIter = mPathWdMap.find(path);
    if (pathIter != mPathWdMap.end() && inode == mWdDirInfoMap[pathIter->second]->mInode) {
        return PATH_INODE_REGISTERED;
    }
    return PATH_INODE_NOT_REGISTERED;
}

bool EventDispatcherBase::IsRegistered(const char* path) {
    MapType<string, int>::Type::iterator itr = mPathWdMap.find(path);
    if (itr == mPathWdMap.end())
        return false;
    return true;
}

bool EventDispatcherBase::IsRegistered(int wd, std::string& path) {
    MapType<int, DirInfo*>::Type::iterator itr = mWdDirInfoMap.find(wd);
    if (itr == mWdDirInfoMap.end())
        return false;
    else {
        path = (itr->second)->mPath;
        return true;
    }
}

void EventDispatcherBase::HandleTimeout() {
    //increment each watcher's timeout account, if bound meets,
    //call timeout handler
    vector<string*> sources;
    vector<EventHandler*> handlers;

    time_t curTime = time(NULL);
    MapType<int, time_t>::Type::iterator itr = mWdUpdateTimeMap.begin();
    for (; itr != mWdUpdateTimeMap.end(); ++itr) {
        if (curTime - (itr->second) >= INT32_FLAG(timeout_interval)) {
            // add to vector then batch process to avoid possible iterator change problem
            //mHandler may remove what itr points to, thus change the layout of the map container
            //what follows may not work
            //Event ev(source, string(), EVENT_TIMEOUT);
            //mTimoutHandler->Handle(ev);
            sources.push_back(&(mWdDirInfoMap[itr->first]->mPath));
        }
    }
    // when we reach this function, for any dir p and its
    // descendant dir c, we have p is at least as newer as c
    // but in vector sources, p may appear before c, this is
    // not a problem if what we do is irrelevant to the order whose
    // timeout handler is called when both are timeout at the same time.
    vector<string*>::iterator itr1 = sources.begin();
    for (; itr1 != sources.end(); ++itr1) {
        Event ev(*(*itr1), string(), EVENT_TIMEOUT, 0);
        mTimeoutHandler->Handle(ev);
    }
}

void EventDispatcherBase::PropagateTimeout(const char* path) {
    char* tmp = strdup(path);
    MapType<string, int>::Type::iterator pathpos = mPathWdMap.find(tmp);
    if (pathpos == mPathWdMap.end()) {
        // walkarond of bug#5760293, should find the scenarios
        LogtailAlarm::GetInstance()->SendAlarm(
            INVALID_MEMORY_ACCESS_ALARM, "PropagateTimeout access invalid key of mPathWdMap, path : " + string(tmp));
        LOG_ERROR(sLogger, ("PropagateTimeout access invalid key of mPathWdMap, path", string(tmp)));
        free(tmp);
        return;
    }
    MapType<int, time_t>::Type::iterator pos = mWdUpdateTimeMap.find(pathpos->second);
    char* slashpos;
    time_t curTime = time(NULL);
    while (pos != mWdUpdateTimeMap.end()) {
        pos->second = curTime;
        slashpos = strrchr(tmp, '/');
        if (slashpos == NULL)
            break;
        *slashpos = '\0';
        pathpos = mPathWdMap.find(tmp);
        if (pathpos != mPathWdMap.end())
            pos = mWdUpdateTimeMap.find(pathpos->second);
        else
            break;
    }
    free(tmp);
}

void EventDispatcherBase::StartTimeCount() {
    MapType<int, time_t>::Type::iterator itr = mWdUpdateTimeMap.begin();
    time_t cur = time(NULL);
    for (; itr != mWdUpdateTimeMap.end(); ++itr) {
        itr->second = cur;
    }
}
void EventDispatcherBase::DumpAllHandlersMeta(bool remove) {
    MapType<int, DirInfo*>::Type::iterator it;
    vector<int> timeout;
    for (it = mWdDirInfoMap.begin(); it != mWdDirInfoMap.end(); ++it) {
        ((it->second)->mHandler)->DumpReaderMeta(remove);
        timeout.push_back(it->first);
        if (remove)
            ConfigManager::GetInstance()->AddHandlerToDelete((it->second)->mHandler);
    }
    for (size_t i = 0; i < timeout.size(); ++i) {
        int wd = timeout[i];
        string path = mWdDirInfoMap[wd]->mPath;
        if (remove) {
            UnregisterEventHandler(path.c_str());
            ConfigManager::GetInstance()->RemoveHandler(path, false);
            if (ConfigManager::GetInstance()->FindBestMatch(path) == NULL) {
                continue;
            }
        }
        CheckPointManager::Instance()->AddDirCheckPoint(path);
    }
}
void EventDispatcherBase::UpdateConfig() {
    if (ConfigManager::GetInstance()->IsUpdateContainerPaths())
        ConfigManager::GetInstance()->StartUpdateConfig();
    if (ConfigManager::GetInstance()->IsUpdateConfig() == false)
        return;
#if defined(__linux__)
    if (mStreamLogManagerPtr != NULL) {
        ((StreamLogManager*)mStreamLogManagerPtr)->ShutdownConfigUsage();
    }
#endif
    LOG_INFO(sLogger, ("main thread", "start update config"));
    LogInput::GetInstance()->HoldOn();
    LogtailPlugin::GetInstance()->HoldOn(false);
    mBrokenLinkSet.clear();

    PollingDirFile::GetInstance()->ClearCache();
    ConfigManager::GetInstance()->RemoveAllConfigs();
    if (ConfigManager::GetInstance()->LoadAllConfig() == false) {
        LOG_ERROR(sLogger, ("LoadConfig fail", ""));
        ConfigManager::GetInstance()->LoadDockerConfig();
        ConfigManager::GetInstance()->DoUpdateContainerPaths();
        DumpAllHandlersMeta(true);
        // we should add checkpoint events
        LogInput::GetInstance()->Resume(true);
        LogtailPlugin::GetInstance()->Resume();
#if defined(__linux__)
        if (mStreamLogManagerPtr != NULL) {
            ((StreamLogManager*)mStreamLogManagerPtr)->StartupConfigUsage();
        }
#endif
        ConfigManager::GetInstance()->FinishUpdateConfig();
        return;
    }
    ConfigManager::GetInstance()->CleanUnusedUserAK();

    ConfigManager::GetInstance()->LoadDockerConfig();
    ConfigManager::GetInstance()->DoUpdateContainerPaths();
    ConfigManager::GetInstance()->SaveDockerConfig();
    // do not delete check point, when config update too short and we can't create all readers
    // if we remove checkpoint here, logtail will lost checkpoint
    //CheckPointManager::Instance()->RemoveAllCheckPoint();
    DumpAllHandlersMeta(true);
    //CheckPointManager::Instance()->PrintStatus();
    if (ConfigManager::GetInstance()->GetConfigRemoveFlag()) {
        CheckPointManager::Instance()->DumpCheckPointToLocal();
        ConfigManager::GetInstance()->SetConfigRemoveFlag(false);
    }
    // reset last dump time to prevent check point manager to dump check point and delete check point.
    // because this may mix check point when init reader and dump check point happen in same time
    CheckPointManager::Instance()->ResetLastDumpTime();
    // we should add checkpoint events

    LogtailPlugin::GetInstance()->Resume();
    LogInput::GetInstance()->Resume(true);

#if defined(__linux__)
    if (mStreamLogManagerPtr != NULL) {
        ((StreamLogManager*)mStreamLogManagerPtr)->StartupConfigUsage();
    }
#endif

    ConfigManager::GetInstance()->FinishUpdateConfig();
}

void EventDispatcherBase::ExitProcess() {
#if defined(__linux__)
    if (mStreamLogManagerPtr != NULL) {
        ((StreamLogManager*)mStreamLogManagerPtr)->Shutdown();
    }
#endif

    LOG_INFO(sLogger, ("LogInput", "hold on"));
    LogInput::GetInstance()->HoldOn();

    LOG_INFO(sLogger, ("dump checkpoint to local", "start"));
    DumpAllHandlersMeta(false);
    LOG_INFO(sLogger, ("dump checkpoint to local", "30%"));
    if (!(CheckPointManager::Instance()->DumpCheckPointToLocal()))
        LOG_WARNING(sLogger, ("dump checkpoint to local", "fail"));
    else
        LOG_INFO(sLogger, ("dump checkpoint to local", "success"));

    // added by xianzhi(bowen.gbw@antfin.com)
    // should dump line count and integrity data to local file
    LOG_INFO(sLogger, ("dump line count data to local file", "start"));
    LogLineCount::GetInstance()->DumpLineCountDataToLocal();

    LOG_INFO(sLogger, ("dump integrity data to local file", "start"));
    LogIntegrity::GetInstance()->DumpIntegrityDataToLocal();


    LOG_INFO(sLogger, ("flush log process buffer", "start"));

    // resume log process thread to process last buffer
    LogProcess::GetInstance()->Resume();
    Sender::Instance()->SetQueueUrgent();
    // exit logtail plugin
    LogtailPlugin::GetInstance()->HoldOn(true);

    bool logProcessFlushFlag = false;

    for (int i = 0; !logProcessFlushFlag && i < 500; ++i) {
        // deamon send thread may reset flush, so we should set flush every time
        Sender::Instance()->SetFlush();
        logProcessFlushFlag = LogProcess::GetInstance()->FlushOut(10);
    }

    if (!logProcessFlushFlag) {
        LOG_WARNING(sLogger, ("flush log process buffer", "fail"));
    } else {
        LOG_INFO(sLogger, ("flush log process buffer", "success"));
    }
    // hode on again
    LogProcess::GetInstance()->HoldOn();

    LOG_INFO(sLogger, ("flush out sender data", "start"));
    if (!(Sender::Instance()->FlushOut(INT32_FLAG(exit_flushout_duration))))
        LOG_WARNING(sLogger, ("flush out sender data", "fail"));
    else
        LOG_INFO(sLogger, ("flush out sender data", "success"));

#ifdef LOGTAIL_RUNTIME_PLUGIN
    LogtailRuntimePlugin::GetInstance()->UnLoadPluginBase();
#endif

#if defined(_MSC_VER)
    ReleaseWindowsSignalObject();
#endif

    exit(0);
}

void EventDispatcherBase::ProcessHandlerTimeOut() {
    MapType<int, DirInfo*>::Type::iterator mapIter = mWdDirInfoMap.begin();
    for (; mapIter != mWdDirInfoMap.end(); ++mapIter) {
        mapIter->second->mHandler->HandleTimeOut();
    }
    return;
}

void EventDispatcherBase::DumpCheckPointPeriod(int32_t curTime) {
    if (CheckPointManager::Instance()->NeedDump(curTime)) {
        LOG_INFO(sLogger, ("Start dump checkpoint, hold on LogInput", curTime));
        LogInput::GetInstance()->HoldOn();
        DumpAllHandlersMeta(false);

        if (!(CheckPointManager::Instance()->DumpCheckPointToLocal()))
            LOG_WARNING(sLogger, ("dump checkpoint to local", "fail"));
        else
            LOG_DEBUG(sLogger, ("dump checkpoint to local", "success"));
        // after save checkpoint, we should clear all checkpoint
        CheckPointManager::Instance()->RemoveAllCheckPoint();
        LogInput::GetInstance()->Resume(false);
        LOG_INFO(sLogger, ("Finish dump checkpoint, LogInput resumed", curTime));
    }
}

#if defined(__linux__)
int EventDispatcherBase::InitStreamLogTcpSocket() {
    int listenFd;
    if ((listenFd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        LOG_ERROR(sLogger, ("Initialize StreamLog tcp socket", "Failed")(ToString(errno), ErrnoToString(GetErrno())));
        LogtailAlarm::GetInstance()->SendAlarm(STREAMLOG_TCP_SOCKET_BIND_ALARM,
                                               string("Init StreamLog tcp socket fail, errno:") + ToString(errno)
                                                   + ",message:" + ErrnoToString(GetErrno()));
        return -1;
    }
    int reuseAddr = 1;
    if (fcntl(listenFd, F_SETFL, O_NONBLOCK) == -1
        || setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr)) == -1) {
        LOG_ERROR(
            sLogger,
            ("Set Non Blocking or set sock opt to REUSEPORT", "Failed")(ToString(errno), ErrnoToString(GetErrno())));
        LogtailAlarm::GetInstance()->SendAlarm(STREAMLOG_TCP_SOCKET_BIND_ALARM,
                                               string("Set none blocking failed or set sock opt to REUSEPORT, errno:")
                                                   + ToString(errno) + ",message:" + ErrnoToString(GetErrno()));
        return -1;
    }

    sockaddr_in addr_serv;
    memset(&addr_serv, 0, sizeof(addr_serv));
    addr_serv.sin_family = AF_INET;
    addr_serv.sin_port = htons(AppConfig::GetInstance()->GetStreamLogTcpPort());
    in_addr_t addr = inet_addr(AppConfig::GetInstance()->GetStreamLogAddress().c_str());
    addr_serv.sin_addr.s_addr = addr;
    if (addr == (in_addr_t)-1 || bind(listenFd, (const sockaddr*)&addr_serv, sizeof(struct sockaddr)) == -1) {
        LOG_ERROR(sLogger,
                  ("Bind StreamLog tcp socket", "Failed")(ToString(errno),
                                                          ErrnoToString(GetErrno()))("fd", ToString(listenFd)));
        LogtailAlarm::GetInstance()->SendAlarm(STREAMLOG_TCP_SOCKET_BIND_ALARM,
                                               string("bind StreamLog tcp socket failed, errno:") + ToString(errno)
                                                   + ",message:" + ErrnoToString(GetErrno()));
        return -1;
    }

    // 128 represent backlog num
    if (listen(listenFd, 128) == -1) {
        LOG_ERROR(sLogger,
                  ("Listen StreamLog tcp socket",
                   "Failed")(ToString(errno), ErrnoToString(GetErrno()))("File Descriptor", ToString(listenFd)));
        LogtailAlarm::GetInstance()->SendAlarm(STREAMLOG_TCP_SOCKET_BIND_ALARM,
                                               string("listen StreamLog tcp socket failed, errno:") + ToString(errno)
                                                   + ",message:" + ErrnoToString(GetErrno()));
        return -1;
    }

    return listenFd;
}
#endif

#if defined(__linux__)
bool EventDispatcherBase::AcceptConnection(int listenFd, int epollFd) {
    struct sockaddr_un sun;
    socklen_t len = sizeof(sun);
    int eventFd = accept(listenFd, (struct sockaddr*)&sun, &len);

    if (eventFd < 0) {
        LOG_ERROR(sLogger,
                  ("Accept new connections", "Failed")(ToString(errno), ErrnoToString(GetErrno()))("File Path",
                                                                                                   sun.sun_path));
        return false;
    }

    if (fcntl(eventFd, F_SETFL, O_NONBLOCK) == -1) {
        LOG_ERROR(
            sLogger,
            ("Set Non Blocking", "Failed")(ToString(errno), ErrnoToString(GetErrno()))("File Path", sun.sun_path));
        return false;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = eventFd;
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, eventFd, &ev) == -1) {
        LOG_ERROR(sLogger,
                  ("Add epoll event", "Failed")(ToString(errno), ErrnoToString(GetErrno()))("File Path", sun.sun_path));
        return false;
    }

    return true;
}
#endif

#if defined(__linux__)
int EventDispatcherBase::AcceptTcpConnection(int listenFd, int epollFd) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int eventFd = accept(listenFd, (sockaddr*)&addr, &len);

    if (eventFd < 0) {
        LOG_ERROR(sLogger,
                  ("Accept StreamLog tcp new connections", "Failed")(ToString(errno), ErrnoToString(GetErrno())));
        return -1;
    }

    if (fcntl(eventFd, F_SETFL, O_NONBLOCK) == -1) {
        LOG_ERROR(sLogger,
                  ("StreamLog tcp socket set to Non Blocking", "Failed")(ToString(errno), ErrnoToString(GetErrno())));
        close(eventFd);
        return -1;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = eventFd;
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, eventFd, &ev) == -1) {
        LOG_ERROR(sLogger, ("Add epoll event", "Failed")(ToString(errno), ErrnoToString(GetErrno())));
        close(eventFd);
        return -1;
    }
    LOG_INFO(sLogger, ("message", "StreamLog accept tcp connection")("fd", ToString(eventFd)));
    return eventFd;
}
#endif

#if defined(__linux__)
bool EventDispatcherBase::HandleReadException(int eventFd, int length, int hdrLength) {
    ErasePacketBuffer(eventFd);

    // unknown interrupt
    if (length != 0) // 0 means client normally close connection
        LOG_ERROR(sLogger,
                  (ToString(errno), ErrnoToString(GetErrno()))("eventFd", eventFd)("Read Length", ToString(length))(
                      "Head Length", ToString(hdrLength)));

    close(eventFd);

    struct epoll_event ee;
    ee.events = 0;
    ee.data.fd = eventFd;
    epoll_ctl(mEpollFd, EPOLL_CTL_DEL, eventFd, &ee);

    return false;
}
#endif

#if defined(__linux__)
void EventDispatcherBase::ErasePacketBuffer(int eventFd) {
    std::unordered_map<int64_t, SingleDSPacket*>::const_iterator iter = mPacketBuffer.find(eventFd);
    if (iter != mPacketBuffer.end()) {
        iter->second->Reset();
        delete iter->second;
        mPacketBuffer.erase(eventFd);
    }
}
#endif

// true means Read total header
// false means Read part header or occur error
#if defined(__linux__)
bool EventDispatcherBase::ReadDSPacketHdr(int eventFd) {
    SingleDSPacket* singleDSPacketPtr = mPacketBuffer[eventFd];

    if (singleDSPacketPtr->mRecvSize >= MSG_HDR_LEN)
        return true;

    int recvHdrLen = recv(eventFd,
                          singleDSPacketPtr->mPacketHdr + singleDSPacketPtr->mRecvSize,
                          MSG_HDR_LEN - singleDSPacketPtr->mRecvSize,
                          MSG_DONTWAIT);
    int totalRecvLen = recvHdrLen + singleDSPacketPtr->mRecvSize;

    if (recvHdrLen <= 0)
        return HandleReadException(eventFd, recvHdrLen, MSG_HDR_LEN);

    singleDSPacketPtr->mRecvSize = totalRecvLen;
    if (totalRecvLen == MSG_HDR_LEN)
        return true;
    else if (totalRecvLen < MSG_HDR_LEN)
        return false;
    else {
        LOG_ERROR(sLogger,
                  ("RecvLength Exceed total Length", ToString(totalRecvLen))("Header Length", ToString(MSG_HDR_LEN)));
        return HandleReadException(eventFd, recvHdrLen, MSG_HDR_LEN);
    }
}
#endif

#if defined(__linux__)
bool EventDispatcherBase::ReadDSPacket(int eventFd) {
    SingleDSPacket* singleDSPacketPtr = mPacketBuffer[eventFd];
    if (singleDSPacketPtr->mRecvSize < MSG_HDR_LEN)
        return false;

    if (singleDSPacketPtr->mRecvSize == MSG_HDR_LEN) // already get the whole head
    {
        MessageHdr* msgHdrPtr;
        msgHdrPtr = (MessageHdr*)(singleDSPacketPtr->mPacketHdr);

        if ((msgHdrPtr->len > 0) && (msgHdrPtr->len < INT64_FLAG(max_logtail_writer_packet_size))) {
            singleDSPacketPtr->mPacket = new char[msgHdrPtr->len];
            singleDSPacketPtr->mPacketSize = msgHdrPtr->len;
            singleDSPacketPtr->mPacketType = msgHdrPtr->type;
        } else {
            LOG_ERROR(sLogger, ("Wrong Packet Message Size", ToString(msgHdrPtr->len)));
            return HandleReadException(eventFd, 0, msgHdrPtr->len);
        }
    }

    int recvLen = recv(eventFd,
                       singleDSPacketPtr->mPacket + singleDSPacketPtr->mRecvSize - MSG_HDR_LEN,
                       singleDSPacketPtr->mPacketSize - singleDSPacketPtr->mRecvSize + MSG_HDR_LEN,
                       MSG_DONTWAIT);
    int totalRecvLen = recvLen + singleDSPacketPtr->mRecvSize;
    if (recvLen <= 0)
        return HandleReadException(eventFd, recvLen, singleDSPacketPtr->mPacketSize);

    if (totalRecvLen == (int)(singleDSPacketPtr->mPacketSize) + MSG_HDR_LEN) {
        if (singleDSPacketPtr->mPacketType == PBMSG) {
            // send the message
            oas::MetricGroup metricGroup;
            string msgStr;
            msgStr.assign(singleDSPacketPtr->mPacket, singleDSPacketPtr->mPacketSize);
            if (metricGroup.ParseFromString(msgStr)) {
                if (metricGroup.metrics_size() > 0) // directly ignore empty metricGroup
                {
                    LogGroup logGroup;
                    logGroup.set_source(metricGroup.metrics(0).source());
                    logGroup.set_category(metricGroup.metricname());
                    logGroup.set_topic(metricGroup.key());
                    logGroup.set_machineuuid(ConfigManager::GetInstance()->GetUUID());
                    Log* logPtr;
                    for (int i = 0; i < metricGroup.metrics_size(); i++) {
                        const oas::Metric& metric = metricGroup.metrics(i);
                        logPtr = logGroup.add_logs();
                        logPtr->set_time(metric.time());
                        for (int j = 0; j < metric.contextgroup_size(); ++j) {
                            Log_Content* logContentPtr = logPtr->add_contents();
                            logContentPtr->set_key(metric.contextgroup(j).key());
                            logContentPtr->set_value(metric.contextgroup(j).value());
                        }
                    }
                    Config* config = ConfigManager::GetInstance()->FindDSConfigByCategory(logGroup.category());
                    MetricSender::SendMetric(logGroup); //Sender::Send() will erase log group
                    if (config != NULL) {
                        LogFileProfiler::GetInstance()->AddProfilingData(config->mConfigName,
                                                                         config->mRegion,
                                                                         config->mProjectName,
                                                                         config->mCategory,
                                                                         "",
                                                                         msgStr.size(),
                                                                         0,
                                                                         logGroup.logs_size(),
                                                                         0,
                                                                         0,
                                                                         0,
                                                                         0,
                                                                         0,
                                                                         "");
                        // send to SLS project
                        if (!Sender::Instance()->Send(
                                config->mProjectName,
                                "",
                                logGroup,
                                config,
                                BOOL_FLAG(merge_shennong_metric) ? MERGE_BY_LOGSTORE : MERGE_BY_TOPIC,
                                (uint32_t)(msgStr.size() * DOUBLE_FLAG(loggroup_bytes_inflation)))) {
                            LogtailAlarm::GetInstance()->SendAlarm(DISCARD_DATA_ALARM,
                                                                   "push metric data into batch map fail",
                                                                   config->mProjectName,
                                                                   config->mCategory,
                                                                   config->mRegion);
                            LOG_ERROR(sLogger,
                                      ("push metric data into batch map fail, discard logs", logGroup.logs_size())(
                                          "project", config->mProjectName)("logstore", config->mCategory));
                        }
                    }
                }
            } else {
                LOG_ERROR(sLogger, ("Parse Protobuffer Message", "Failed"));
                LogtailAlarm::GetInstance()->SendAlarm(METRIC_GROUP_PARSE_FAIL_ALARM,
                                                       "desearlize from metricgroup fail");
            }
        } else if (singleDSPacketPtr->mPacketType == INSIGHT_CMD_TYPE) {
            static LogtailInsightDispatcher* insightDispatcher = LogtailInsightDispatcher::GetInstance();
            int32_t ret = insightDispatcher->ExecuteCommand(
                eventFd, singleDSPacketPtr->mPacket, singleDSPacketPtr->mPacketSize);
            if (ret == -2) {
                // ret is -2, meet send error, we should close fd
                close(eventFd);

                struct epoll_event ee;
                ee.events = 0;
                ee.data.fd = eventFd;
                epoll_ctl(mEpollFd, EPOLL_CTL_DEL, eventFd, &ee);

                LOG_ERROR(sLogger,
                          ("error while sending log group to logtail insight", "close fd and remove fd from epoll"));
                return false;
            }
        } else {
            LOG_WARNING(sLogger, ("Data Type is not support", singleDSPacketPtr->mPacketType));
        }

        singleDSPacketPtr->Reset(); // reset packet buffer state
    } else if (totalRecvLen < (int)(singleDSPacketPtr->mPacketSize) + MSG_HDR_LEN) {
        singleDSPacketPtr->mRecvSize = totalRecvLen;
    }

    return true;
}
#endif

#if defined(__linux__)
bool EventDispatcherBase::ReadMessages(int eventFd) {
    // determine eventFd exist
    if (mPacketBuffer.find(eventFd) == mPacketBuffer.end()) {
        SingleDSPacket* singleDSPacketPtr = new SingleDSPacket();
        mPacketBuffer[eventFd] = singleDSPacketPtr;
    }

    if (ReadDSPacketHdr(eventFd))
        return ReadDSPacket(eventFd);
    else
        return false;
}
#endif

#if defined(__linux__)
bool EventDispatcherBase::AddStreamLogTcpSocketToEpoll() {
    // remove old listenfd from epoll, e.g : socket file was removed
    if (mStreamLogTcpFd != -1) {
        close(mStreamLogTcpFd);
        struct epoll_event ev;
        epoll_ctl(mEpollFd, EPOLL_CTL_DEL, mStreamLogTcpFd, &ev);
    }
    // add new listenfd for epoll
    int listenFd = InitStreamLogTcpSocket();
    if (listenFd != -1) {
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = listenFd;
        if (epoll_ctl(mEpollFd, EPOLL_CTL_ADD, listenFd, &ev) == -1) {
            LogtailAlarm::GetInstance()->SendAlarm(
                EPOLL_ERROR_ALARM, string("Failed to add StreamLog tcp fd to epoll, errno:" + ToString(errno)));
            LOG_ERROR(sLogger, ("add StreamLog tcp socket fd to epoll fail, errno", errno));
            close(listenFd);
            mStreamLogTcpFd = -1;
            return false;
        }
        mStreamLogTcpFd = listenFd;
        return true;
    }
    mStreamLogTcpFd = -1;
    return false;
}
#endif

#ifdef APSARA_UNIT_TEST_MAIN
void EventDispatcherBase::CleanEnviroments() {
    mMainThreadRunning = false;
    sleep(2); //INT32_FLAG(ilogtail_epoll_time_out) + 1
    mPathWdMap.clear();
    for (MapType<int, DirInfo*>::Type::iterator iter = mWdDirInfoMap.begin(); iter != mWdDirInfoMap.end(); ++iter)
        delete iter->second;
    mWdDirInfoMap.clear();
    mBrokenLinkSet.clear();
    mWdUpdateTimeMap.clear();
    for (std::unordered_map<int64_t, SingleDSPacket*>::iterator iter = mPacketBuffer.begin();
         iter != mPacketBuffer.end();
         ++iter)
        delete iter->second;
    mPacketBuffer.clear();
    mEventListener->Destroy();
    mEventListener->Init();
    PollingDirFile::GetInstance()->Stop();
    PollingModify::GetInstance()->Stop();
    PollingEventQueue::GetInstance()->Clear();
    CheckPointManager::Instance()->RemoveAllCheckPoint();
}

int32_t EventDispatcherBase::GetInotifyWatcherCount() {
    int32_t inotifyWatcherCount = 0;
    for (MapType<int, DirInfo*>::Type::iterator iter = mWdDirInfoMap.begin(); iter != mWdDirInfoMap.end(); ++iter) {
        if (iter->first >= 0)
            ++inotifyWatcherCount;
    }
    return inotifyWatcherCount;
}
#endif

} // namespace logtail
