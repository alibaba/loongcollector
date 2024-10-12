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
#include "Flags.h"
#if defined(__linux__)
#include <fnmatch.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>

#include <vector>

#include "app_config/AppConfig.h"
#include "checkpoint/CheckPointManager.h"
#include "checkpoint/CheckpointManagerV2.h"
#include "common/ErrorUtil.h"
#include "common/FileSystemUtil.h"
#include "common/HashUtil.h"
#include "common/LogtailCommonFlags.h"
#include "common/RuntimeUtil.h"
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "common/version.h"
#include "file_server/event/Event.h"
#include "file_server/event_handler/EventHandler.h"
#include "file_server/event_handler/LogInput.h"
#include "file_server/polling/PollingDirFile.h"
#include "file_server/polling/PollingModify.h"
#include "monitor/LogFileProfiler.h"
#include "monitor/LogtailAlarm.h"
#include "monitor/MetricExportor.h"
#include "protobuf/sls/metric.pb.h"
#include "protobuf/sls/sls_logs.pb.h"
#ifdef APSARA_UNIT_TEST_MAIN
#include "file_server/polling/PollingEventQueue.h"
#endif
#include "application/Application.h"
#include "file_server/ConfigManager.h"
#include "file_server/FileServer.h"
#include "go_pipeline/LogtailPlugin.h"
#include "pipeline/PipelineManager.h"
#include "pipeline/plugin/PluginRegistry.h"
#include "plugin/input/InputContainerStdio.h"
#include "plugin/input/InputFile.h"

using namespace std;
using namespace sls_logs;

// 商业版
// DEFINE_FLAG_BOOL(merge_shennong_metric, "merge LogGroup into LogPackageList if true", true);

// 废弃
// DEFINE_FLAG_INT32(main_loop_check_interval, "seconds", 60);

// 移动
// DEFINE_FLAG_INT32(ilogtail_epoll_time_out, "default time out is 1s", 1);
// DEFINE_FLAG_INT32(tcmalloc_release_memory_interval, "force release memory held by tcmalloc, seconds", 300);
// DEFINE_FLAG_BOOL(fs_events_inotify_enable, "", true);
// DEFINE_FLAG_INT32(exit_flushout_duration, "exit process flushout duration", 20 * 1000);
// DEFINE_FLAG_INT32(search_checkpoint_default_dir_depth, "0 means only search current directory", 0);
// DEFINE_FLAG_BOOL(enable_polling_discovery, "", true);

DEFINE_FLAG_INT32(existed_file_active_timeout,
                  "when first monitor directory, file modified in 120 seconds will be collected",
                  120);
DEFINE_FLAG_INT32(checkpoint_find_max_cache_size, "", 100000);
DEFINE_FLAG_INT32(max_watch_dir_count, "", 100 * 1000);
DEFINE_FLAG_INT32(default_max_inotify_watch_num, "the max allowed inotify watch dir number", 3000);

namespace logtail {

EventDispatcher::EventDispatcher() : mWatchNum(0), mInotifyWatchNum(0) {
    /*
     * May add multiple inotify fd instances in the future,
     * so use epoll here though a little more sophisticated than select
     */
    // NOTE: epoll is used to drive mListenFd and mStreaLogTcpFd, they only work on Linux.
    // #if defined(__linux__)
    //     mEpollFd = epoll_create(INT32_FLAG(ilogtail_max_epoll_events));
    //     mListenFd = -1;
    //     mStreamLogTcpFd = -1;
    // #endif
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

EventDispatcher::~EventDispatcher() {
    // #if defined(__linux__)
    //     if (mStreamLogManagerPtr != NULL) {
    //         delete (StreamLogManager*)mStreamLogManagerPtr;
    //     }
    //     if (mEpollFd >= 0)
    //         close(mEpollFd);
    //     if (mStreamLogTcpFd >= 0)
    //         close(mStreamLogTcpFd);
    //     if (mListenFd >= 0)
    //         close(mListenFd);
    // #endif
    mEventListener->Destroy();
    if (mTimeoutHandler)
        delete mTimeoutHandler;
}

bool EventDispatcher::RegisterEventHandler(const char* path,
                                           const FileDiscoveryConfig& config,
                                           EventHandler*& handler) {
    if (AppConfig::GetInstance()->IsHostPathMatchBlacklist(path)) {
        LOG_INFO(sLogger, ("ignore path matching host path blacklist", path));
        return false;
    }
    // @todo
    // if this path belong to many config, if register one config with max_depth 0, then it will register fail
    if (!config.first->WithinMaxDepth(path)) {
        LOG_DEBUG(sLogger,
                  ("path is out of maxDepth",
                   path)("logstore", config.second->GetLogstoreName())("max depth", config.first->mMaxDirSearchDepth));
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
                                                   config.second->GetProjectName(),
                                                   config.second->GetLogstoreName(),
                                                   config.second->GetRegion());
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
    MapType<string, int>::Type::iterator pathIter = mPathWdMap.find(path);
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
                                               config.second->GetProjectName(),
                                               config.second->GetLogstoreName(),
                                               config.second->GetRegion());
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
                if (config.first->IsTimeout(path))
                    LogtailAlarm::GetInstance()->SendAlarm(REGISTER_INOTIFY_FAIL_ALARM,
                                                           string("Failed to register dir: ") + path + ", reason: "
                                                               + str + ", project: " + config.second->GetProjectName()
                                                               + ", logstore: " + config.second->GetLogstoreName());
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

    bool dirTimeOutFlag = config.first->IsTimeout(path);

    if (!mEventListener->IsValidID(wd)) {
        if (dirTimeOutFlag) {
            LOG_DEBUG(
                sLogger,
                ("Drop timeout path, source", path)("config, basepath", config.first->GetBasePath())(
                    "preseveDepth", config.first->mPreservedDirDepth)("maxDepth", config.first->mMaxDirSearchDepth));
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
             ("add a new watcher for dir", path)("wd", wd)("dir inode", inode)("isSymbolicLink", isSymbolicLink));
    DirInfo* dirInfo = new DirInfo(path, inode, isSymbolicLink, handler);
    AddOneToOneMapEntry(dirInfo, wd);
    ++mWatchNum;
    AddExistedFileEvents(path, wd);
    if (dirTimeOutFlag) {
        AddTimeoutWatch(path);
        LOG_DEBUG(sLogger,
                  ("AddTimeoutWatch, source", path)("config, basepath", config.first->GetBasePath())(
                      "preseveDepth", config.first->mPreservedDirDepth)("maxDepth", config.first->mMaxDirSearchDepth));
    }

    return true;
}

// read files when add dir inotify watcher at first time
void EventDispatcher::AddExistedFileEvents(const char* path, int wd) {
    fsutil::Dir dir(path);
    if (!dir.Open()) {
        auto err = GetErrno();
        if (!fsutil::Dir::IsENOENT(err)) {
            LOG_ERROR(sLogger, ("Open dir fail", path)("error", ErrnoToString(err)));
        }
        return;
    }
    int32_t MAX_TAIL_FILE_COUNT = 50;
    vector<FileDiscoveryConfig> configs;
    ConfigManager::GetInstance()->GetRelatedConfigs(path, configs);
    for (auto iter = configs.begin(); iter != configs.end(); ++iter) {
        if ((*iter).first->mMaxDirSearchDepth == 0) {
            MAX_TAIL_FILE_COUNT = 1000;
            break;
        }
    }

    fsutil::Entry ent;
    int32_t curTime = time(NULL);
    vector<Event*> eventVec;
    int32_t tailFileCount = 0;
    while ((ent = dir.ReadNext(false))) {
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
        // CheckPointPtr checkPointSharePtr;
        // if (CheckPointManager::Instance()->GetCheckPoint(item, checkPointSharePtr))
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
        for (auto iter = configs.begin(); iter != configs.end(); ++iter) {
            if (fnmatch((*iter).first->GetFilePattern().c_str(), entName.c_str(), 0) == 0) {
                isMatch = true;
                if ((*iter).first->IsTailingAllMatchedFiles()) {
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
        LOG_INFO(sLogger,
                 ("generate MODIFY event for the recently updated existing file",
                  PathJoin(eventVec[i]->GetSource(), eventVec[i]->GetObject())));
    }
    if (eventVec.size() > 0)
        LogInput::GetInstance()->PushEventQueue(eventVec);
}

EventDispatcher::ValidateCheckpointResult EventDispatcher::validateCheckpoint(
    CheckPointPtr& checkpoint, map<DevInode, SplitedFilePath>& cachePathDevInodeMap, vector<Event*>& eventVec) {
    shared_ptr<Pipeline> config = PipelineManager::GetInstance()->FindConfigByName(checkpoint->mConfigName);
    if (config == NULL) {
        LOG_INFO(sLogger,
                 ("delete checkpoint", "the corresponding config is deleted")("config", checkpoint->mConfigName)(
                     "log reader queue name", checkpoint->mFileName)("real file path", checkpoint->mRealFileName)(
                     "file device", checkpoint->mDevInode.inode)("file inode", checkpoint->mDevInode.inode));
        return ValidateCheckpointResult::kConfigNotFound;
    }

    // Use FileName (logical absolute path) to do config matching.
    const string& filePath = checkpoint->mFileName;
    const string realFilePath = checkpoint->mRealFileName.empty() ? filePath : checkpoint->mRealFileName;
    size_t lastSeparator = filePath.find_last_of(PATH_SEPARATOR);
    if (lastSeparator == string::npos || lastSeparator == (size_t)0 || lastSeparator >= filePath.size()) {
        LOG_INFO(sLogger,
                 ("delete checkpoint", "invalid log reader queue name")("config", checkpoint->mConfigName)(
                     "log reader queue name", checkpoint->mFileName)("real file path", checkpoint->mRealFileName)(
                     "file device", checkpoint->mDevInode.inode)("file inode", checkpoint->mDevInode.inode));
        return ValidateCheckpointResult::kInvalidFilePath;
    }
    string path = filePath.substr(0, lastSeparator);
    string fileName = filePath.substr(lastSeparator + 1);

    // Check if the config in checkpoint still matches the file?
    vector<FileDiscoveryConfig> matchedConfigs;
    AppConfig::GetInstance()->IsAcceptMultiConfig()
        ? ConfigManager::GetInstance()->FindAllMatch(matchedConfigs, path, fileName)
        : ConfigManager::GetInstance()->FindMatchWithForceFlag(matchedConfigs, path, fileName);
    bool stillMatch = false;
    for (size_t idx = 0; idx < matchedConfigs.size(); ++idx) {
        if (matchedConfigs[idx].second->GetConfigName() == checkpoint->mConfigName) {
            stillMatch = true;
            break;
        }
    }
    if (!stillMatch) {
        LOG_INFO(
            sLogger,
            ("delete checkpoint", "original config no more matches the file path")("config", checkpoint->mConfigName)(
                "log reader queue name", checkpoint->mFileName)("real file path", checkpoint->mRealFileName)(
                "file device", checkpoint->mDevInode.inode)("file inode", checkpoint->mDevInode.inode));
        return ValidateCheckpointResult::kConfigNotMatched;
    }
    // Determine the type of input plugin.
    string name = config->GetInputs()[0]->GetPlugin()->Name();
    const InputFile* inputFile = nullptr;
    const InputContainerStdio* inputContainerStdio = nullptr;
    if (name == InputFile::sName) {
        inputFile = static_cast<const InputFile*>(config->GetInputs()[0]->GetPlugin());
    } else if (name == InputContainerStdio::sName) {
        inputContainerStdio = static_cast<const InputContainerStdio*>(config->GetInputs()[0]->GetPlugin());
    }

    // delete checkpoint if file path is not exist
    MapType<string, int>::Type::iterator pathIter = mPathWdMap.find(path);
    if (pathIter == mPathWdMap.end()) {
        LOG_INFO(sLogger,
                 ("delete checkpoint", "file path no longer exists")("config", checkpoint->mConfigName)(
                     "log reader queue name", checkpoint->mFileName)("real file path", checkpoint->mRealFileName)(
                     "file device", checkpoint->mDevInode.inode)("file inode", checkpoint->mDevInode.inode));
        return ValidateCheckpointResult::kLogDirNotWatched;
    }

    int wd = pathIter->second;
    DevInode devInode = GetFileDevInode(realFilePath);
    if (devInode.IsValid() && checkpoint->mDevInode.inode == devInode.inode) {
        if (!CheckFileSignature(realFilePath, checkpoint->mSignatureHash, checkpoint->mSignatureSize)) {
            LOG_INFO(sLogger,
                     ("delete checkpoint", "file device & inode remains the same but signature has changed")(
                         "config", checkpoint->mConfigName)("log reader queue name", checkpoint->mFileName)(
                         "real file path", checkpoint->mRealFileName)("file device", checkpoint->mDevInode.inode)(
                         "file inode", checkpoint->mDevInode.inode));
            return ValidateCheckpointResult::kSigChanged;
        }
        if (checkpoint->mDevInode.dev != devInode.dev) {
            // all other checks passed. dev may be a statefulset pv remounted on another node
            checkpoint->mDevInode.dev = devInode.dev;
        }

        LOG_INFO(sLogger,
                 ("generate MODIFY event for file with checkpoint",
                  "nothing changed on the file")("config", checkpoint->mConfigName)("log reader queue name", filePath)(
                     "real file path", realFilePath)("file device", checkpoint->mDevInode.dev)(
                     "file inode", checkpoint->mDevInode.inode)("signature", checkpoint->mSignatureHash)(
                     "last file position", checkpoint->mOffset)("is file open when dumped",
                                                                ToString(checkpoint->mFileOpenFlag))(
                     "is container stopped when dumped", ToString(checkpoint->mContainerStopped)));
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
                 ("delete checkpoint", "file signature size is zero")("config", checkpoint->mConfigName)(
                     "log reader queue name", checkpoint->mFileName)("real file path", checkpoint->mRealFileName)(
                     "file device", checkpoint->mDevInode.inode)("file inode", checkpoint->mDevInode.inode));
        return ValidateCheckpointResult::kZeroSigSize;
    }

    // Try to find the real file with dev inode, check cache at first.
    map<DevInode, SplitedFilePath>::iterator findIter = cachePathDevInodeMap.find(checkpoint->mDevInode);
    if (findIter != cachePathDevInodeMap.end()) {
        if (findIter->second.mFileDir != path) {
            LOG_INFO(sLogger,
                     ("delete checkpoint", "file has been moved to other dir")("config", checkpoint->mConfigName)(
                         "log reader queue name", checkpoint->mFileName)("original real file path", realFilePath)(
                         "new real file path", PathJoin(findIter->second.mFileDir, findIter->second.mFileName))(
                         "file device", checkpoint->mDevInode.inode)("file inode", checkpoint->mDevInode.inode));
            return ValidateCheckpointResult::kLogDirChanged;
        }

        if (CheckFileSignature(
                PathJoin(path, findIter->second.mFileName), checkpoint->mSignatureHash, checkpoint->mSignatureSize)) {
            checkpoint->mRealFileName = PathJoin(findIter->second.mFileDir, findIter->second.mFileName);
            LOG_INFO(sLogger,
                     ("generate MODIFY event for file with checkpoint",
                      "file has been renamed, but still in the same dir")("config", checkpoint->mConfigName)(
                         "log reader queue name", filePath)("original real file path", realFilePath)(
                         "new real file path", checkpoint->mRealFileName)("file device", checkpoint->mDevInode.dev)(
                         "file inode", checkpoint->mDevInode.inode)("signature", checkpoint->mSignatureHash)(
                         "last file position", checkpoint->mOffset)("is file open when dumped",
                                                                    ToString(checkpoint->mFileOpenFlag))(
                         "is container stopped when dumped", ToString(checkpoint->mContainerStopped)));
            eventVec.push_back(
                new Event(path, fileName, EVENT_MODIFY, wd, 0, checkpoint->mDevInode.dev, checkpoint->mDevInode.inode));
            eventVec[eventVec.size() - 1]->SetConfigName(checkpoint->mConfigName);
            return ValidateCheckpointResult::kRotate;
        }

        if ((inputFile && 0 == inputFile->mExactlyOnceConcurrency) || inputContainerStdio) {
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
        LOG_WARNING(
            sLogger,
            ("delete checkpoint", "cannot find the file because of full find cache")("config", checkpoint->mConfigName)(
                "log reader queue name", checkpoint->mFileName)("real file path", checkpoint->mRealFileName)(
                "file device", checkpoint->mDevInode.inode)("file inode", checkpoint->mDevInode.inode));
        LogtailAlarm::GetInstance()->SendAlarm(
            CHECKPOINT_ALARM,
            string("cannot find the file because of full find cache, delete the checkpoint, log reader queue name: ")
                + filePath + ", real file path: " + realFilePath);
        return ValidateCheckpointResult::kCacheFull;
    }
    uint16_t searchDepth = 0;
    if (inputFile) {
        searchDepth = inputFile->mMaxCheckpointDirSearchDepth;
    }
    auto const searchResult
        = SearchFilePathByDevInodeInDirectory(path, searchDepth, checkpoint->mDevInode, &cachePathDevInodeMap);
    if (searchResult) {
        const auto& newRealPath = searchResult.value();
        if (CheckFileSignature(newRealPath, checkpoint->mSignatureHash, checkpoint->mSignatureSize)) {
            checkpoint->mRealFileName = newRealPath;
            LOG_INFO(sLogger,
                     ("generate MODIFY event for file with checkpoint",
                      "file has been renamed, but still in the same dir")("config", checkpoint->mConfigName)(
                         "log reader queue name", filePath)("original real file path", realFilePath)(
                         "new real file path", checkpoint->mRealFileName)("file device", checkpoint->mDevInode.dev)(
                         "file inode", checkpoint->mDevInode.inode)("signature", checkpoint->mSignatureHash)(
                         "last file position", checkpoint->mOffset)("is file open when dumped",
                                                                    ToString(checkpoint->mFileOpenFlag))(
                         "is container stopped when dumped", ToString(checkpoint->mContainerStopped)));
            eventVec.push_back(
                new Event(path, fileName, EVENT_MODIFY, wd, 0, checkpoint->mDevInode.dev, checkpoint->mDevInode.inode));
            eventVec[eventVec.size() - 1]->SetConfigName(checkpoint->mConfigName);
            return ValidateCheckpointResult::kRotate;
        }

        LOG_INFO(sLogger,
                 ("delete checkpoint", "file has been renamed but signature has changed")(
                     "config", checkpoint->mConfigName)("log reader queue name", checkpoint->mFileName)(
                     "original real file path", realFilePath)("new real file path", newRealPath)(
                     "file device", checkpoint->mDevInode.inode)("file inode", checkpoint->mDevInode.inode));
        return ValidateCheckpointResult::kSigChanged;
    }

    // Can not find dev inode, delete this checkpoint.
    LOG_INFO(sLogger,
             ("delete checkpoint", "cannot find the file any more")("config", checkpoint->mConfigName)(
                 "log reader queue name", checkpoint->mFileName)("real file path", checkpoint->mRealFileName)(
                 "file device", checkpoint->mDevInode.inode)("file inode", checkpoint->mDevInode.inode));
    return ValidateCheckpointResult::kDevInodeNotFound;
}

void EventDispatcher::AddExistedCheckPointFileEvents() {
    // All checkpoint will be add into event queue or be deleted
    // This operation will delete not existed file's check point
    map<DevInode, SplitedFilePath> cachePathDevInodeMap;
    auto& checkPointMap = CheckPointManager::Instance()->GetAllFileCheckPoint();
    LOG_INFO(sLogger, ("start to verify existed checkpoints, total checkpoint count", checkPointMap.size()));
    vector<CheckPointManager::CheckPointKey> deleteKeyVec;
    vector<Event*> eventVec;
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
             ("checkpoint verification ends, generated event count", eventVec.size())("checkpoint deletion count",
                                                                                      deleteKeyVec.size()));
    auto const v1EventCount = eventVec.size();

    // Load exactly once checkpoints and create events from them.
    // Because they are not in v1 checkpoint manager, no need to delete them.
    auto exactlyOnceConfigs = FileServer::GetInstance()->GetExactlyOnceConfigs();
    if (!exactlyOnceConfigs.empty()) {
        static auto sCptMV2 = CheckpointManagerV2::GetInstance();
        auto exactlyOnceCpts = sCptMV2->ScanCheckpoints(exactlyOnceConfigs);
        LOG_INFO(sLogger,
                 ("start add exactly once checkpoint events",
                  "")("config size", exactlyOnceConfigs.size())("scanned checkpoint size", exactlyOnceCpts.size()));
        vector<pair<string, PrimaryCheckpointPB>*> batchUpdateCpts;
        vector<pair<string, PrimaryCheckpointPB>*> batchDeleteCpts;
        for (size_t idx = 0; idx < exactlyOnceCpts.size(); ++idx) {
            auto& cptPair = exactlyOnceCpts[idx];
            auto& cpt = cptPair.second;
            auto v1Cpt = make_shared<CheckPoint>(cpt.log_path(),
                                                 0,
                                                 cpt.sig_size(),
                                                 cpt.sig_hash(),
                                                 DevInode(cpt.dev(), cpt.inode()),
                                                 cpt.config_name(),
                                                 cpt.real_path(),
                                                 1,
                                                 0,
                                                 0);
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
        sort(eventVec.begin(), eventVec.end(), Event::CompareByFullPath);
        LogInput::GetInstance()->PushEventQueue(eventVec);
    }
}

bool EventDispatcher::AddTimeoutWatch(const char* path) {
    MapType<string, int>::Type::iterator itr = mPathWdMap.find(path);
    if (itr != mPathWdMap.end()) {
        mWdUpdateTimeMap[itr->second] = time(NULL);
        return true;
    } else {
        return false;
    }
}

void EventDispatcher::AddOneToOneMapEntry(DirInfo* dirInfo, int wd) {
    mPathWdMap[dirInfo->mPath] = wd;
    mWdDirInfoMap[wd] = dirInfo;
}

void EventDispatcher::RemoveOneToOneMapEntry(int wd) {
    MapType<int, DirInfo*>::Type::iterator itr = mWdDirInfoMap.find(wd);
    if (itr == mWdDirInfoMap.end())
        return;
    mPathWdMap.erase((itr->second)->mPath);
    delete itr->second;
    mWdDirInfoMap.erase(itr);
}

void EventDispatcher::DumpInotifyWatcherDirs() {
    string filename = GetInotifyWatcherDirsDumpFileName();
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

void EventDispatcher::CheckSymbolicLink() {
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
        if (!fsutil::PathStat::lstat(path, lstatBuf)) // check path itself
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
                // then the directory "c" or "b" be created with no IN_CRETATE event, should add inotify monitor for "a"
                // again
                if (mBrokenLinkSet.find(path) != mBrokenLinkSet.end()) {
                    mBrokenLinkSet.erase(path);
                }
                dirToAdd.push_back(path);
            }
        }
    }
    for (vector<string>::iterator iter = dirToAdd.begin(); iter != dirToAdd.end(); ++iter) {
        FileDiscoveryConfig config = ConfigManager::GetInstance()->FindBestMatch(*iter);
        if (config.first && IsDirRegistered(*iter) == PATH_INODE_NOT_REGISTERED)
            ConfigManager::GetInstance()->RegisterHandlersRecursively(*iter, config, true);
    }
}

void EventDispatcher::ReadInotifyEvents(vector<Event*>& eventVec) {
    mEventListener->ReadEvents(eventVec);
}

vector<pair<string, EventHandler*>> EventDispatcher::FindAllSubDirAndHandler(const string& baseDir) {
    LOG_DEBUG(sLogger, ("Find all sub dir", baseDir));
    vector<pair<string, EventHandler*>> dirAndHandlers;
    size_t baseDirSize = baseDir.size();
    MapType<string, int>::Type::iterator it = mPathWdMap.begin();
    for (; it != mPathWdMap.end(); ++it) {
        const string& pathName = it->first;
        size_t pathNameSize = pathName.size();
        if (baseDirSize > pathNameSize) {
            continue;
        }
        if (memcmp(baseDir.c_str(), pathName.c_str(), baseDirSize) == 0
            && (pathNameSize == baseDirSize || pathName[baseDirSize] == PATH_SEPARATOR[0])) {
            dirAndHandlers.push_back(make_pair(it->first, mWdDirInfoMap[it->second]->mHandler));
        }
    }
    return dirAndHandlers;
}

void EventDispatcher::UnregisterAllDir(const string& baseDir) {
    LOG_DEBUG(sLogger, ("Remove all sub dir", baseDir));
    auto subDirAndHandlers = FindAllSubDirAndHandler(baseDir);
    for (auto& subDirAndHandler : subDirAndHandlers) {
        mTimeoutHandler->Handle(Event(subDirAndHandler.first.c_str(), "", 0, 0));
    }
}

void EventDispatcher::UnregisterEventHandler(const char* path) {
    MapType<string, int>::Type::iterator pos = mPathWdMap.find(path);
    if (pos == mPathWdMap.end())
        return;
    int wd = pos->second;
    if (mWdDirInfoMap[wd]->mIsSymbolicLink) {
        fsutil::PathStat lstatBuf;
        if (fsutil::PathStat::lstat(path, lstatBuf)) // TODO: Need review, might be a bug.
        {
            // path(symbolic link) existed, the dir it refrence to is deleted
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
    LOG_INFO(sLogger, ("remove the watcher for dir", path)("wd", wd));
}

void EventDispatcher::StopAllDir(const string& baseDir) {
    LOG_DEBUG(sLogger, ("Stop all sub dir", baseDir));
    auto subDirAndHandlers = FindAllSubDirAndHandler(baseDir);
    for (auto& subDirAndHandler : subDirAndHandlers) {
        Event e(subDirAndHandler.first.c_str(), "", EVENT_ISDIR | EVENT_CONTAINER_STOPPED, -1, 0);
        subDirAndHandler.second->Handle(e);
    }
}

DirRegisterStatus EventDispatcher::IsDirRegistered(const string& path) {
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

bool EventDispatcher::IsRegistered(const char* path) {
    MapType<string, int>::Type::iterator itr = mPathWdMap.find(path);
    if (itr == mPathWdMap.end())
        return false;
    return true;
}

bool EventDispatcher::IsRegistered(int wd, string& path) {
    MapType<int, DirInfo*>::Type::iterator itr = mWdDirInfoMap.find(wd);
    if (itr == mWdDirInfoMap.end())
        return false;
    else {
        path = (itr->second)->mPath;
        return true;
    }
}

void EventDispatcher::HandleTimeout() {
    // increment each watcher's timeout account, if bound meets,
    // call timeout handler
    vector<string*> sources;
    vector<EventHandler*> handlers;

    time_t curTime = time(NULL);
    MapType<int, time_t>::Type::iterator itr = mWdUpdateTimeMap.begin();
    for (; itr != mWdUpdateTimeMap.end(); ++itr) {
        if (curTime - (itr->second) >= INT32_FLAG(timeout_interval)) {
            // add to vector then batch process to avoid possible iterator change problem
            // mHandler may remove what itr points to, thus change the layout of the map container
            // what follows may not work
            // Event ev(source, string(), EVENT_TIMEOUT);
            // mTimoutHandler->Handle(ev);
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

void EventDispatcher::PropagateTimeout(const char* path) {
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

void EventDispatcher::StartTimeCount() {
    MapType<int, time_t>::Type::iterator itr = mWdUpdateTimeMap.begin();
    time_t cur = time(NULL);
    for (; itr != mWdUpdateTimeMap.end(); ++itr) {
        itr->second = cur;
    }
}
void EventDispatcher::DumpAllHandlersMeta(bool remove) {
    MapType<int, DirInfo*>::Type::iterator it;
    vector<int> timeout;
    for (it = mWdDirInfoMap.begin(); it != mWdDirInfoMap.end(); ++it) {
        ((it->second)->mHandler)->DumpReaderMeta(true, remove);
    }
    for (it = mWdDirInfoMap.begin(); it != mWdDirInfoMap.end(); ++it) {
        ((it->second)->mHandler)->DumpReaderMeta(false, remove);
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
            if (ConfigManager::GetInstance()->FindBestMatch(path).first == NULL) {
                continue;
            }
        }
        CheckPointManager::Instance()->AddDirCheckPoint(path);
    }
    LOG_INFO(sLogger, ("save log reader status", "succeeded"));
}

void EventDispatcher::ProcessHandlerTimeOut() {
    MapType<int, DirInfo*>::Type::iterator mapIter = mWdDirInfoMap.begin();
    for (; mapIter != mWdDirInfoMap.end(); ++mapIter) {
        mapIter->second->mHandler->HandleTimeOut();
    }
    return;
}

void EventDispatcher::DumpCheckPointPeriod(int32_t curTime) {
    if (CheckPointManager::Instance()->NeedDump(curTime)) {
        LOG_INFO(sLogger, ("checkpoint dump", "starts"));
        FileServer::GetInstance()->Pause(false);
        DumpAllHandlersMeta(false);

        if (!(CheckPointManager::Instance()->DumpCheckPointToLocal()))
            LOG_WARNING(sLogger, ("dump checkpoint to local", "failed"));
        else
            LOG_DEBUG(sLogger, ("dump checkpoint to local", "succeeded"));
        // after save checkpoint, we should clear all checkpoint
        CheckPointManager::Instance()->RemoveAllCheckPoint();
        FileServer::GetInstance()->Resume(false);
        LOG_INFO(sLogger, ("checkpoint dump", "succeeded"));
    }
}

bool EventDispatcher::IsAllFileRead() {
    for (auto it = mWdDirInfoMap.begin(); it != mWdDirInfoMap.end(); ++it) {
        if (!((it->second)->mHandler)->IsAllFileRead()) {
            return false;
        }
    }
    return true;
}

#ifdef APSARA_UNIT_TEST_MAIN
void EventDispatcher::CleanEnviroments() {
    // mMainThreadRunning = false;
    sleep(2); // INT32_FLAG(ilogtail_epoll_time_out) + 1
    mPathWdMap.clear();
    for (MapType<int, DirInfo*>::Type::iterator iter = mWdDirInfoMap.begin(); iter != mWdDirInfoMap.end(); ++iter)
        delete iter->second;
    mWdDirInfoMap.clear();
    mBrokenLinkSet.clear();
    mWdUpdateTimeMap.clear();
    // for (unordered_map<int64_t, SingleDSPacket*>::iterator iter = mPacketBuffer.begin();
    //      iter != mPacketBuffer.end();
    //      ++iter)
    //     delete iter->second;
    // mPacketBuffer.clear();
    mEventListener->Destroy();
    mEventListener->Init();
    PollingDirFile::GetInstance()->Stop();
    PollingModify::GetInstance()->Stop();
    PollingEventQueue::GetInstance()->Clear();
    CheckPointManager::Instance()->RemoveAllCheckPoint();
}

int32_t EventDispatcher::GetInotifyWatcherCount() {
    int32_t inotifyWatcherCount = 0;
    for (MapType<int, DirInfo*>::Type::iterator iter = mWdDirInfoMap.begin(); iter != mWdDirInfoMap.end(); ++iter) {
        if (iter->first >= 0)
            ++inotifyWatcherCount;
    }
    return inotifyWatcherCount;
}
#endif

} // namespace logtail
