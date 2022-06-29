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

#include "EventHandler.h"
#include <iostream>
#include <string>
#include <vector>
#include "common/util.h"
#include "common/TimeUtil.h"
#include "common/RuntimeUtil.h"
#include "common/FileSystemUtil.h"
#include "common/StringTools.h"
#include "app_config/AppConfig.h"
#include "event/BlockEventManager.h"
#include "controller/EventDispatcher.h"
#include "config_manager/ConfigManager.h"
#include "profiler/LogtailAlarm.h"
#include "processor/LogProcess.h"
#include "logger/Logger.h"
#include "fuse/FuseFileBlacklist.h"
#include "common/LogFileCollectOffsetIndicator.h"
#include "LogInput.h"

using namespace std;
using namespace sls_logs;

DEFINE_FLAG_INT64(read_file_time_slice, "microseconds", 50 * 1000);
DEFINE_FLAG_INT32(logreader_timeout_interval,
                  "reader hasn't updated for a long time will be removed, seconds",
                  86400 * 30);
DEFINE_FLAG_INT32(cookie_timeout_interval,
                  "rotate cookie hasn't updated for a long time will be removed, seconds",
                  1800);
DEFINE_FLAG_INT32(logreader_count_upperlimit,
                  "when reader (in one dir) count exceed limit, reader hasn't updated for 7 * 24 hours will be removed",
                  5000);
DEFINE_FLAG_INT32(logreader_count_maxlimit,
                  "when reader (in one dir) count exceed limit, reader hasn't updated for 24 hours will be removed",
                  10000);

DEFINE_FLAG_INT32(logreader_count_max,
                  "when reader (in one dir) count exceed limit, modify handler will remove old reader ",
                  100000);

DEFINE_FLAG_INT32(logreader_count_max_remove_count, "remove reader count ", 10000);

DEFINE_FLAG_INT32(logreader_filedeleted_remove_interval,
                  "when file is deleted, reader will be removed after seconds",
                  900);
DEFINE_FLAG_INT32(logreader_filerotate_remove_interval,
                  "when file is rotate, reader will be removed after seconds",
                  600);
DEFINE_FLAG_INT32(rotate_overflow_error_interval, "second", 60);

namespace logtail {

void NormalEventHandler::Handle(const Event& event) {
    bool fileCreateModify = false;
    if (event.IsCreate() || event.IsMoveTo()) {
        if (event.IsDir()) {
            mCreateHandlerPtr->Handle(event);
        } else {
            string fullPath = PathJoin(event.GetSource(), event.GetObject());
            fsutil::PathStat buf;
            if (!fsutil::PathStat::stat(fullPath, buf)) {
                // filename before rollback is not exist this moment
                LOG_DEBUG(sLogger, ("get path info error", fullPath));
            } else if (buf.IsDir()) {
                //register consider timeout
                mCreateHandlerPtr->Handle(event);
            } else if (!buf.IsRegFile()) {
                LOG_INFO(sLogger, ("path is not file or directory, ignore it", fullPath)("stat mode", buf.GetMode()));
                LogtailAlarm::GetInstance()->SendAlarm(UNEXPECTED_FILE_TYPE_MODE_ALARM,
                                                       std::string("found unexpected type mode: ")
                                                           + ToString(buf.GetMode()) + ", file path: " + fullPath);
                return;
            } else if (event.IsCreate())
                fileCreateModify = true;
        }
    } else if (event.IsModify())
        fileCreateModify = true;

    if (fileCreateModify) {
        // register a new handler for source
        // match config, then register
        auto const& path = event.GetSource();
        // warning: following case should occur:
        // a/b & a/b/c both in configuration, but requires
        // its descendants being watched timeout
        // while the other not
        const string& name = event.GetObject();

        // the file suffix which will be ignored
        if (!IsValidSuffix(name))
            return;

        Config* config = NULL;
        if (event.GetConfigName().empty()) {
            config = ConfigManager::GetInstance()->FindBestMatch(path, name);
        } else {
            config = ConfigManager::GetInstance()->FindConfigByName(event.GetConfigName());
        }

        if (config != NULL) {
            LOG_DEBUG(
                sLogger,
                ("register dir", PathJoin(path, name))("logstore", config->mCategory)("max depth", config->mMaxDepth));
            EventHandler* newHandler = new CreateModifyHandler(mCreateHandlerPtr);
            EventHandler* handler = newHandler;
            if (EventDispatcher::GetInstance()->RegisterEventHandler(path.c_str(), config, handler)) {
                if (handler != newHandler)
                    delete newHandler;
                else
                    ConfigManager::GetInstance()->AddNewHandler(path, handler);
                handler->Handle(event);
            } else {
                delete handler;
            }
            //from now on, it's ret who is responsible for this directory
            //And when it's timeout configmanager will delete it

            if (ConfigManager::GetInstance()->HaveFuseConfig()) {
                FuseFileBlacklist::GetInstance()->RemoveFromBlackList(PathJoin(path, name));
            }
        } else {
            // cannot find config, add to black list
            if (ConfigManager::GetInstance()->HaveFuseConfig()) {
                FuseFileBlacklist::GetInstance()->AddToBlacklist(PathJoin(path, name));
            }
        }
    }
}

void NormalEventHandler::HandleTimeOut() {
    mCreateHandlerPtr->HandleTimeOut(); // empty call
}

// CreateHandler implementation
// xxx: event.IsDir() true!
void CreateHandler::Handle(const Event& event) {
    // warning: following case should occur:
    // a/b & a/b/c both in configuration, but requires
    // its descendants being watched timeout
    // while the other not
    string object = event.GetObject();
    string path = event.GetSource();
    if (object.size() > 0)
        path += PATH_SEPARATOR + object;
    if (!object.empty() && '.' == object[0]) {
        LOG_INFO(sLogger, ("ignore hidden file or directory", path));
        return;
    }
    Config* config = ConfigManager::GetInstance()->FindBestMatch(path);
    if (config == NULL)
        return;
    else if (event.IsDir())
        ConfigManager::GetInstance()->RegisterHandlersRecursively(path, config, false);
    else {
        // symbolic link
        if (EventDispatcher::GetInstance()->IsDirRegistered(path) == PATH_INODE_NOT_REGISTERED)
            ConfigManager::GetInstance()->RegisterHandlersRecursively(path, config, true);
    }
}


void CreateHandler::HandleTimeOut() {
    // do nothing
}

// TimeoutHandler implementation
void TimeoutHandler::Handle(const Event& ev) {
    const string& dir = ev.GetSource();
    EventDispatcher::GetInstance()->UnregisterEventHandler(dir.c_str());
    ConfigManager::GetInstance()->RemoveHandler(dir);
}


void TimeoutHandler::HandleTimeOut() {
    // do nothing
}

bool CreateModifyHandler::DumpReaderMeta(bool checkConfigFlag) {
    for (ModifyHandlerMap::iterator iter = mModifyHandlerPtrMap.begin(); iter != mModifyHandlerPtrMap.end(); ++iter) {
        iter->second->DumpReaderMeta(checkConfigFlag);
    }
    return true;
}

ModifyHandler* CreateModifyHandler::GetOrCreateModifyHandler(const std::string& configName, Config* pConfig) {
    ModifyHandlerMap::iterator iter = mModifyHandlerPtrMap.find(configName);
    if (iter != mModifyHandlerPtrMap.end()) {
        return iter->second;
    }
    ModifyHandler* pHanlder = new ModifyHandler(configName, pConfig);
    mModifyHandlerPtrMap.insert(std::make_pair(configName, pHanlder));
    return pHanlder;
}

CreateModifyHandler::~CreateModifyHandler() {
    for (ModifyHandlerMap::iterator iter = mModifyHandlerPtrMap.begin(); iter != mModifyHandlerPtrMap.end(); ++iter) {
        delete iter->second;
    }
    mModifyHandlerPtrMap.clear();
}

// CreateModifyHandler implementation
void CreateModifyHandler::Handle(const Event& event) {
    bool isDir = false;
    auto path = std::string(event.GetSource()).append(PATH_SEPARATOR).append(event.GetObject());
    bool fuseFlag = ConfigManager::GetInstance()->HaveFuseConfig();

    if (event.IsDir()) {
        isDir = true;
    } else if (event.IsCreate() || event.IsMoveTo()) {
        fsutil::PathStat buf;
        if (!fsutil::PathStat::stat(path, buf))
            LOG_DEBUG(sLogger, ("get path info error", path));
        else if (buf.IsDir())
            isDir = true;
        else if (!buf.IsRegFile()) {
            LOG_INFO(sLogger, ("path is not file or directory, ignore it", path)("stat mode", buf.GetMode()));
            LogtailAlarm::GetInstance()->SendAlarm(UNEXPECTED_FILE_TYPE_MODE_ALARM,
                                                   std::string("found unexpected type mode: ") + ToString(buf.GetMode())
                                                       + ", file path: " + path);
            return;
        }
    }
    if ((event.IsCreate() || event.IsMoveTo()) && isDir) {
        mCreateHandlerPtr->Handle(event);
    } else if (event.IsCreate() || event.IsModify() || event.IsMoveFrom() || event.IsMoveTo() || event.IsDeleted()
               || event.IsContainerStopped()) {
        if (!event.GetConfigName().empty()) {
            Config* pConfig = ConfigManager::GetInstance()->FindConfigByName(event.GetConfigName());
            if (pConfig != NULL) {
                LOG_DEBUG(
                    sLogger,
                    ("Process event with existed config", event.GetConfigName())(event.GetSource(), event.GetObject()));
                GetOrCreateModifyHandler(pConfig->mConfigName, pConfig)->Handle(event);
            } else {
                // if event is delete
                LOG_WARNING(sLogger, ("can not find config, config may be deleted", event.GetConfigName()));
            }
        } else {
            vector<Config*> pConfigVec;
            if (AppConfig::GetInstance()->IsAcceptMultiConfig()) {
                ConfigManager::GetInstance()->FindAllMatch(pConfigVec, event.GetSource(), event.GetObject());
            } else {
                ConfigManager::GetInstance()->FindMatchWithForceFlag(pConfigVec, event.GetSource(), event.GetObject());
            }

            if (pConfigVec.empty() && fuseFlag) {
                FuseFileBlacklist::GetInstance()->AddToBlacklist(path);
                return;
            }

            for (vector<Config*>::iterator configIter = pConfigVec.begin(); configIter != pConfigVec.end();
                 ++configIter) {
                Config* pConfig = *configIter;
                LOG_DEBUG(sLogger,
                          ("Process event with multi config", pConfigVec.size())(event.GetSource(), event.GetObject()));
                GetOrCreateModifyHandler(pConfig->mConfigName, pConfig)->Handle(event);

                if (pConfig->mIsFuseMode) {
                    FuseFileBlacklist::GetInstance()->RemoveFromBlackList(path);
                }

                // if file is deleted or moved, inode may be wrong
                if ((event.IsMoveFrom() || event.IsDeleted()) && pConfig->mMarkOffsetFlag) {
                    LogFileCollectOffsetIndicator::GetInstance()->DeleteItem(pConfig->mConfigName, path);
                }
            }
        }
    }
}


void CreateModifyHandler::HandleTimeOut() {
    for (ModifyHandlerMap::iterator iter = mModifyHandlerPtrMap.begin(); iter != mModifyHandlerPtrMap.end(); ++iter) {
        iter->second->HandleTimeOut();
    }
    mCreateHandlerPtr->HandleTimeOut(); // empty call
}

// implementation for ModifyHandler
ModifyHandler::ModifyHandler(const std::string& configName, Config* pConfig) : mConfigName(configName) {
    if (pConfig != NULL && pConfig->mPriority > 0 && pConfig->mPriority <= MAX_CONFIG_PRIORITY_LEVEL) {
        mReadFileTimeSlice
            = (1 << (MAX_CONFIG_PRIORITY_LEVEL - pConfig->mPriority + 1)) * INT64_FLAG(read_file_time_slice);
    } else {
        mReadFileTimeSlice = INT64_FLAG(read_file_time_slice);
    }
    mLastOverflowErrorTime = 0;
}

ModifyHandler::~ModifyHandler() {
}

void ModifyHandler::MakeSpaceForNewReader() {
    if (mDevInodeReaderMap.size()
        < (size_t)INT32_FLAG(logreader_count_max) + (size_t)INT32_FLAG(logreader_count_max_remove_count)) {
        return;
    }

    LOG_WARNING(sLogger, ("reader size is up limit, start delete old reader", ""));

    vector<LogFileReader*> sortReaderArray;
    sortReaderArray.resize(mDevInodeReaderMap.size());
    size_t index = 0;
    for (DevInodeLogFileReaderMap::iterator iter = mDevInodeReaderMap.begin(); iter != mDevInodeReaderMap.end();
         ++iter) {
        sortReaderArray[index++] = iter->second.get();
    }

    // little to big
    sort(sortReaderArray.begin(), sortReaderArray.end(), ModifyHandler::CompareReaderByUpdateTime);

    const int32_t deleteCount = (int32_t)mDevInodeReaderMap.size() - INT32_FLAG(logreader_count_max);

    for (int i = 0; i < deleteCount; ++i) {
        LogFileReader* pReader = sortReaderArray[i];
        mDevInodeReaderMap.erase(pReader->GetDevInode());
        LogFileReaderPtrArray& readerArray = *pReader->GetReaderArray();
        for (LogFileReaderPtrArray::iterator iter = readerArray.begin(); iter != readerArray.end(); ++iter) {
            if (iter->get() == pReader) {
                readerArray.erase(iter);
                break;
            }
        }
    }

    LogFileReaderPtr oneReader = mDevInodeReaderMap.begin()->second;

    LOG_WARNING(sLogger,
                ("reader size is up limit, delete old reader", deleteCount)("project", oneReader->GetProjectName())(
                    "logstore", oneReader->GetCategory())("now logreader count", mDevInodeReaderMap.size()));
    LogtailAlarm::GetInstance()->SendAlarm(FILE_READER_EXCEED_ALARM,
                                           string("reader size is up limit, delete old reader, logreader count:")
                                               + ToString(mDevInodeReaderMap.size()),
                                           oneReader->GetProjectName(),
                                           oneReader->GetCategory(),
                                           oneReader->GetRegion());
}


LogFileReaderPtr ModifyHandler::CreateLogFileReaderPtr(
    const string& path, const string& name, Config* pConfig, const DevInode& devInode, bool forceBeginingFlag) {
    LogFileReaderPtrArray& readerArray = mNameReaderMap[name];
    if (readerArray.size() >= pConfig->mAdvancedConfig.mMaxRotateQueueSize) {
        int32_t nowTime = time(NULL);
        if (nowTime - mLastOverflowErrorTime > INT32_FLAG(rotate_overflow_error_interval)) {
            mLastOverflowErrorTime = nowTime;
            LOG_ERROR(
                sLogger,
                ("log file rotator queue size is upper limited, start drop new logfile event", PathJoin(path, name)));
            LogtailAlarm::GetInstance()->SendAlarm(
                DROP_LOG_ALARM,
                string("log file rotator queue size is upper limited, start drop new logfile event, file:")
                    + PathJoin(path, name),
                pConfig->GetProjectName(),
                pConfig->GetCategory(),
                pConfig->mRegion);
        }
        return LogFileReaderPtr();
    }
    LogFileReaderPtr readerPtr(pConfig->CreateLogFileReader(path, name, devInode, forceBeginingFlag));
    if (readerPtr.get() == NULL)
        return LogFileReaderPtr();

    // new log
    bool backFlag = false;
    if (readerPtr->GetRealLogPath().empty() || readerPtr->GetRealLogPath() == readerPtr->GetLogPath()) {
        backFlag = true;
        // if reader is a new file(not from checkpoint), and file is rotate file, reset file pos
        if (readerArray.size() > 0 && !readerPtr->IsFromCheckPoint()) {
            LOG_DEBUG(sLogger, ("file rotate, reset new reader pos", PathJoin(path, name)));
            readerPtr->ResetLastFilePos();
        }
    } else {
        backFlag = false;
        // rotate log, push front
        LOG_DEBUG(sLogger, ("rotator log, push front", readerPtr->GetRealLogPath()));
    }

    // need check skip flag first and if flag is false then open fd
    if (!readerPtr->NeedSkipFirstModify()) {
        if (!readerPtr->UpdateFilePtr()) {
            if (errno != EMFILE) {
                LOG_ERROR(sLogger,
                          ("log file dev inode has been changed in a very short "
                           "time, delete this invalid reader",
                           PathJoin(path, name)));
                return LogFileReaderPtr();
            }
        } else {
            // if first create, we should check and update file signature, because when blocked, new reader will have no chance to update signature
            int64_t fileSize;
            if (!readerPtr->CheckFileSignatureAndOffset(fileSize)) {
                LOG_ERROR(sLogger, ("check file signature fail when create reader", PathJoin(path, name)));
                return LogFileReaderPtr();
            }
        }
    }

    backFlag ? readerArray.push_back(readerPtr) : readerArray.push_front(readerPtr);
    readerPtr->SetReaderArray(&readerArray);
    mDevInodeReaderMap[devInode] = readerPtr;

    LOG_DEBUG(
        sLogger,
        ("create reader ", PathJoin(path, name))(ToString(devInode.inode), readerArray.size())("this", (uint64_t)this));
    return readerPtr;
}


void ModifyHandler::Handle(const Event& event) {
    const string& path = event.GetSource();
    const string& name = event.GetObject();

    if (!IsValidSuffix(name))
        return;

    DevInode devInode(event.GetDev(), event.GetInode());
    string logPath(path);
    logPath.append(PATH_SEPARATOR).append(name);
    if (!devInode.IsValid() && (event.IsModify() || event.IsCreate())) {
        devInode = GetFileDevInode(logPath);
        if (!devInode.IsValid()) {
            // call stat failed, but we should try to find reader because the log file may be moved to another name
            LOG_DEBUG(sLogger, ("get file dev inode error", logPath));
        }
    }

    DevInodeLogFileReaderMap::iterator devInodeIter
        = devInode.IsValid() ? mDevInodeReaderMap.find(devInode) : mDevInodeReaderMap.end();

    // when file is deleted or movefrom, we can't find devinode, so set all log reader's delete flag
    if (event.IsDeleted() || event.IsMoveFrom()) {
        NameLogFileReaderMap::iterator iter = mNameReaderMap.find(name);
        if (iter != mNameReaderMap.end()) {
            LogFileReaderPtrArray& readerArray = iter->second;
            // only set when reader array size is 1
            if (readerArray.size() == (size_t)1) {
                readerArray[0]->SetFileDeleted(true);
                if (readerArray[0]->IsReadToEnd()) {
                    // release fd as quick as possible
                    readerArray[0]->CloseFilePtr();
                }
            }
        }
    } else if (event.IsContainerStopped()) {
        for (auto& pair : mNameReaderMap) {
            LogFileReaderPtrArray& readerArray = pair.second;
            for (auto& reader : readerArray) {
                reader->SetContainerStopped();
                if (reader->IsReadToEnd()) {
                    // release fd as quick as possible
                    reader->CloseFilePtr();
                }
            }
        }
    } else if (event.IsModify()) {
        // devInode cannot be found, this means a rotate file(like a.log.1) has event, and reader for rotate file is moved to mRotatorReaderMap
        if (devInode.IsValid() && devInodeIter == mDevInodeReaderMap.end()) {
            DevInodeLogFileReaderMap::iterator rotateIter = mRotatorReaderMap.find(devInode);
            // the reader for file(whether it's a.log or a.log.1) exists in mDevInodeReaderMap or mRotatorReaderMap
            // if we can find reader in mRotatorReaderMap, it means the file after rotating(a.log.1) also matches config
            if (rotateIter != mRotatorReaderMap.end()) {
                // if dev inode change, just return.
                // if find devinode in rotator map, we should check file signature
                // if sig is different, delete this reader and create new one
                // if sig is same and file size not change, ignore it
                // if sig is same and file size is changed, move reader to normal reader map
                LogFileReaderPtr rotatorReader = rotateIter->second;
                LogFileReader::FileCompareResult cmpRst = rotatorReader->CompareToFile(logPath);
                LOG_DEBUG(sLogger,
                          ("find rotator reader", logPath)("compare result", (int)cmpRst)(
                              rotatorReader->GetLogPath(), rotatorReader->GetRealLogPath())(
                              ToString(rotatorReader->GetLastFilePos()), devInode.inode)("this", (uint64_t)this));
                switch (cmpRst) {
                    case LogFileReader::FileCompareResult_DevInodeChange:
                        return;
                        break;
                    case LogFileReader::FileCompareResult_SigChange:
                        mRotatorReaderMap.erase(rotateIter);
                        break;
                    case LogFileReader::FileCompareResult_SigSameSizeChange: {
                        rotatorReader->UpdateLogPath(logPath);
                        LogFileReaderPtrArray& readerArray = mNameReaderMap[rotatorReader->GetLogPathFile()];
                        // new log
                        if (rotatorReader->GetRealLogPath() == rotatorReader->GetLogPath()) {
                            readerArray.push_back(rotatorReader);
                        } else {
                            // rotate log, push front
                            readerArray.push_front(rotatorReader);
                        }
                        rotatorReader->SetReaderArray(&readerArray);
                        mDevInodeReaderMap[devInode] = rotatorReader;
                        mRotatorReaderMap.erase(rotateIter);
                        devInodeIter = mDevInodeReaderMap.find(devInode);
                    } break;
                    case LogFileReader::FileCompareResult_SigSameSizeSame:
                        return;
                        break;
                    case LogFileReader::FileCompareResult_Error:
                        return;
                        break;
                    default:
                        return;
                }
            }
        }
        uint64_t beginTime = GetCurrentTimeInMicroSeconds();
        LogFileReaderPtrArray* readerArrayPtr = NULL;
        if (!devInode.IsValid()) {
            // call stat failed, but we should try to find reader because the log file may be moved to another name
            NameLogFileReaderMap::iterator iter = mNameReaderMap.find(name);
            if (iter != mNameReaderMap.end()) {
                readerArrayPtr = &(iter->second);
            } else {
                LOG_WARNING(sLogger, ("can not find logreader, may be deleted", logPath));
                return;
            }
        } else if (devInodeIter == mDevInodeReaderMap.end()) {
            Config* config = ConfigManager::GetInstance()->FindConfigByName(mConfigName);
            // double check
            // if event with config name, skip check
            if (config != NULL && (!event.GetConfigName().empty() || config->IsMatch(path, name))) {
                LogFileReaderPtr readerPtr = CreateLogFileReaderPtr(path, name, config, devInode);
                if (readerPtr.get() == NULL) {
                    LogFileReaderPtrArray& readerArray = mNameReaderMap[name];
                    // if rotate queue is full, try read array header
                    if (readerArray.size() >= config->mAdvancedConfig.mMaxRotateQueueSize) {
                        readerPtr = readerArray[0];
                        // push modify event, use head dev inode
                        //Event* ev = new Event(event.GetSource(), event.GetObject(), event.GetType(), event.GetWd(), event.GetCookie(), readerArray[0]->GetDevInode().dev, readerArray[0]->GetDevInode().inode);
                        //LogInput::GetInstance()->PushEventQueue(ev);
                    } else {
                        // other fail, return
                        return;
                    }
                }
                // if we need to skip first modify, reset flag and return
                if (readerPtr->NeedSkipFirstModify()) {
                    readerPtr->DisableSkipFirstModify();
                    return;
                }
                readerArrayPtr = readerPtr->GetReaderArray();
            } else {
                return;
            }
        } else {
            devInodeIter->second->UpdateLogPath(logPath);
            readerArrayPtr = devInodeIter->second->GetReaderArray();
        }
        if (readerArrayPtr->size() == 0) {
            LOG_ERROR(sLogger, ("unknow error, reader array size is 0", logPath));
            return;
        }
        LogFileReaderPtr reader = (*readerArrayPtr)[0];
        // If file modified, it means the file is existed, then we should set fileDeletedFlag to false
        // NOTE: This may override the correct delete flag, which will cause fd close delay!
        // reader->SetFileDeleted(false);

        // make sure file open success, or we just return
        while (!reader->UpdateFilePtr()) {
            if (errno == EMFILE) {
                LOG_WARNING(sLogger,
                            ("too many open files", "skip this read operation")("log path", reader->GetLogPath()));
                return;
            }
            // eg: a.log rotate to a.log1, event sequece : a.log write 2min ago, file ptr closed -> a.log rotate a.log1 -> logtail process new a.log modify -> open file fail -> [old] delete reader -> logtail process a.log1 modify
            //     -> cannot find reader, treat as new file -> read log tail(1MB)
            // so when open file ptr faild, put this reader into rotator map, when process a.log1 modify event, we can find it in rotator map
            LOG_INFO(sLogger,
                     ("open file ptr failed, push into rotate map, inode",
                      reader->GetDevInode().inode)("real path", reader->GetRealLogPath())(
                         "default path", reader->GetLogPath())("now offset", reader->GetLastFilePos()));
            readerArrayPtr->pop_front();
            mDevInodeReaderMap.erase(reader->GetDevInode());
            mRotatorReaderMap[reader->GetDevInode()] = reader;
            if (readerArrayPtr->size() == 0) {
                return;
            }
            reader = (*readerArrayPtr)[0];
            LOG_DEBUG(sLogger, ("read other file", reader->GetDevInode().inode));
        }


        int64_t fileSize = 0;
        bool recreateReaderFlag = false;
        // if dev inode changed, delete this reader and create reader
        if (!reader->CheckDevInode()) {
            LOG_INFO(sLogger,
                     ("file dev inode changed, create new reader. new path", logPath)("old path", reader->GetLogPath())(
                         ToString(readerArrayPtr->size()), mRotatorReaderMap.size())(
                         ToString(reader->GetDevInode().inode), reader->GetLastFilePos())("DevInode map size",
                                                                                          mDevInodeReaderMap.size()));
            recreateReaderFlag = true;
            LogtailAlarm::GetInstance()->SendAlarm(INNER_PROFILE_ALARM,
                                                   string("file dev inode changed, create new reader. new path:")
                                                       + reader->GetLogPath() + " ,project:" + reader->GetProjectName()
                                                       + " ,logstore:" + reader->GetCategory());
        }
        // if signature is different and logpath is different, delete this reader and create reader
        else if (!reader->CheckFileSignatureAndOffset(fileSize) && logPath != reader->GetLogPath()) {
            LOG_INFO(sLogger,
                     ("file sig and name both changed, create new reader. new path", logPath)(
                         "old path", reader->GetLogPath())(ToString(readerArrayPtr->size()), mRotatorReaderMap.size())(
                         ToString(reader->GetDevInode().inode), reader->GetLastFilePos())("DevInode map size",
                                                                                          mDevInodeReaderMap.size()));
            recreateReaderFlag = true;
        }
        if (recreateReaderFlag) {
            readerArrayPtr->pop_front();
            mDevInodeReaderMap.erase(reader->GetDevInode());
            // delete this reader, donot insert into rotator reader map
            // repush this event and wait for create reader
            Event* ev = new Event(event);
            LogInput::GetInstance()->PushEventQueue(ev);
            return;
        }

        bool hasMoreData;
        do {
            if (!LogProcess::GetInstance()->IsValidToReadLog(reader->GetLogstoreKey())) {
                static int32_t s_lastOutPutTime = 0;
                int32_t curTime = time(NULL);
                if (curTime - s_lastOutPutTime > 600) {
                    s_lastOutPutTime = time(NULL);
                    LOG_INFO(sLogger,
                             ("logprocess queue is full, put modify event to event queue again",
                              reader->GetLogPath())(reader->GetProjectName(), reader->GetCategory()));

                    LogtailAlarm::GetInstance()->SendAlarm(
                        PROCESS_QUEUE_BUSY_ALARM,
                        string("logprocess queue is full, put modify event to event queue again, file:")
                            + reader->GetLogPath() + " ,project:" + reader->GetProjectName()
                            + " ,logstore:" + reader->GetCategory());
                }

                BlockedEventManager::GetInstance()->UpdateBlockEvent(
                    reader->GetLogstoreKey(), mConfigName, event, reader->GetDevInode(), curTime);
                return;
            }
            LogBuffer* logBuffer = NULL;
            hasMoreData = reader->ReadLog(logBuffer);
            int32_t pushRetry = 0;
            if (logBuffer != NULL) {
                LogFileProfiler::GetInstance()->AddProfilingReadBytes(reader->GetConfigName(),
                                                                      reader->GetRegion(),
                                                                      reader->GetProjectName(),
                                                                      reader->GetCategory(),
                                                                      reader->GetConvertedPath(),
                                                                      reader->GetDevInode().dev,
                                                                      reader->GetDevInode().inode,
                                                                      reader->GetFileSize(),
                                                                      reader->GetLastFilePos(),
                                                                      time(NULL));
                logBuffer->SetDependecy(reader);
                while (!LogProcess::GetInstance()->PushBuffer(logBuffer)) //10ms
                {
                    ++pushRetry;
                    if (pushRetry % 10 == 0)
                        LogInput::GetInstance()->TryReadEvents(false);
                }
            }

            if (!hasMoreData) {
                if (reader->IsFileDeleted() || reader->IsContainerStopped()) {
                    // release fd as quick as possible
                    reader->CloseFilePtr();
                }
                break;
            }
            if (pushRetry >= 5 || GetCurrentTimeInMicroSeconds() - beginTime > mReadFileTimeSlice) {
                LOG_DEBUG(
                    sLogger,
                    ("read log breakout", "file io cost 1 time slice (50ms) or push blocked")("pushRetry", pushRetry)(
                        "begin time", beginTime)("path", event.GetSource())("file", event.GetObject()));
                Event* ev = new Event(event);
                ev->SetConfigName(mConfigName);
                LogInput::GetInstance()->PushEventQueue(ev);
                break;
            }

            // When loginput thread hold on, we should repush this event back.
            // If we don't repush and this file has no modify event, this reader will never been read.
            if (LogInput::GetInstance()->IsInterupt()) {
                if (hasMoreData) {
                    LOG_INFO(
                        sLogger,
                        ("read log interupt but has more data, reason", "log input thread hold on")(
                            "action", "repush modify event to event queue")("begin time", beginTime)(
                            "path", event.GetSource())("file", event.GetObject())("inode", reader->GetDevInode().inode)(
                            "offset", reader->GetLastFilePos())("size", reader->GetFileSize()));
                } else {
                    LOG_DEBUG(
                        sLogger,
                        ("read log breakout, reason", "log input thread hold on")(
                            "action", "repush modify event to event queue")("begin time", beginTime)(
                            "path", event.GetSource())("file", event.GetObject())("inode", reader->GetDevInode().inode)(
                            "offset", reader->GetLastFilePos())("size", reader->GetFileSize()));
                }
                Event* ev = new Event(event);
                ev->SetConfigName(mConfigName);
                LogInput::GetInstance()->PushEventQueue(ev);
                break;
            }
        } while (true);

        if (!hasMoreData && readerArrayPtr->size() > (size_t)1) {
            LOG_DEBUG(sLogger,
                      ("read head rotate log done, move reader to rotator map",
                       logPath)(ToString(readerArrayPtr->size()), mRotatorReaderMap.size())(
                          ToString(reader->GetDevInode().inode), reader->GetLastFilePos())("DevInode map size",
                                                                                           mDevInodeReaderMap.size()));
            // when a rotated reader finish its reading, it's unlikely that there will be data again
            // so release file fd as quick as possible (open again if new data coming)
            reader->CloseFilePtr();
            readerArrayPtr->pop_front();
            mDevInodeReaderMap.erase(reader->GetDevInode());
            mRotatorReaderMap[reader->GetDevInode()] = reader;
            // need to push modify event again, but without dev inode
            // use head dev + inode
            Event* ev = new Event(event.GetSource(),
                                  event.GetObject(),
                                  event.GetType(),
                                  event.GetWd(),
                                  event.GetCookie(),
                                  (*readerArrayPtr)[0]->GetDevInode().dev,
                                  (*readerArrayPtr)[0]->GetDevInode().inode);
            ev->SetConfigName(mConfigName);
            LogInput::GetInstance()->PushEventQueue(ev);
        }
    }
    // if a file is created, and dev inode cannot found(this means it's a new file), create reader for this file, then insert reader into mDevInodeReaderMap
    else if (event.IsCreate()) {
        if (!devInode.IsValid()) {
            return;
        }
        if (devInodeIter == mDevInodeReaderMap.end()) {
            Config* config = ConfigManager::GetInstance()->FindConfigByName(mConfigName);
            if (config != NULL && (!event.GetConfigName().empty() || config->IsMatch(path, name))) {
                LogFileReaderPtr readerPtr = CreateLogFileReaderPtr(path, name, config, devInode, true);
                if (readerPtr.get() == NULL) {
                    return;
                }
            } else {
                return;
            }
        }
    }
}

void ModifyHandler::HandleTimeOut() {
    MakeSpaceForNewReader();
    DeleteTimeoutReader();
    DeleteRollbackReader();
    // remove deleted reader
    time_t nowTime = time(NULL);
    NameLogFileReaderMap::iterator readerIter = mNameReaderMap.begin();
    size_t closeFilePtrCount = 0;
    size_t maxFilePtrCount = mNameReaderMap.size() > (size_t)100 ? mNameReaderMap.size() / 10 : 100;
    for (; readerIter != mNameReaderMap.end();) {
        bool actioned = false;
        LogFileReaderPtrArray& readerArray = readerIter->second;
        // donot check file delete flag or close ptr when array size > 1
        if (readerArray.size() > 1) {
            ++readerIter;
            actioned = true;
            LOG_DEBUG(sLogger,
                      ("HandleTimeOut filename", readerIter->first)("dir", readerArray[0]->GetLogPath().c_str())(
                          "action", "continue")("reason", "reader array size > 1"));
            continue;
        }

        if (readerArray.size() == 1) {
            LogFileReaderPtrArray::iterator iter = readerArray.begin();
            if ((*iter)->IsFileDeleted()
                && nowTime - (*iter)->GetDeletedTime() > INT32_FLAG(logreader_filedeleted_remove_interval)) {
                actioned = true;
                LOG_DEBUG(sLogger,
                          ("HandleTimeOut filename", readerIter->first)("dir", readerArray[0]->GetLogPath().c_str())(
                              "action", "remove")("reason", "file is deleted"));
                mDevInodeReaderMap.erase((*iter)->GetDevInode());
                readerArray.erase(iter);
            }
        }
        // only close file ptr when readerArray size is 1
        // because when many file is queued, if we close file ptr, maybe we can't find this file again
        if (readerArray.size() == 1) {
            if (readerArray[0]->CloseTimeoutFilePtr(nowTime)) {
                ++closeFilePtrCount;
                actioned = true;
                LOG_DEBUG(sLogger,
                          ("HandleTimeOut filename", readerIter->first)("dir", readerArray[0]->GetLogPath().c_str())(
                              "action", "close")("reason", "file no new data timeout"));
            }
        }
        if (!actioned && readerArray.size() > 0) {
            LOG_DEBUG(sLogger,
                      ("HandleTimeOut filename", readerIter->first)("dir", readerArray[0]->GetLogPath().c_str())(
                          "action", "continue")("reason", "nothing to do"));
        }
        if (readerArray.empty()) {
            LOG_DEBUG(sLogger, ("remove empty reader Array", readerIter->first));
            readerIter = mNameReaderMap.erase(readerIter);
        } else {
            ++readerIter;
        }
        if (closeFilePtrCount > maxFilePtrCount) {
            LOG_DEBUG(sLogger,
                      ("HandleTimeOut action",
                       "break")("closeFilePtrCount", closeFilePtrCount)("reason", "close file count > max file count"));
            break;
        }
    }
}

bool ModifyHandler::DumpReaderMeta(bool checkConfigFlag) {
    DevInodeLogFileReaderMap::iterator it = mDevInodeReaderMap.begin();
    for (; it != mDevInodeReaderMap.end(); ++it) {
        it->second->DumpMetaToMem(checkConfigFlag);
    }
    for (it = mRotatorReaderMap.begin(); it != mRotatorReaderMap.end(); ++it) {
        it->second->DumpMetaToMem(checkConfigFlag);
    }
    return true;
}

void ModifyHandler::DeleteTimeoutReader() {
    if ((int32_t)mDevInodeReaderMap.size() > INT32_FLAG(logreader_count_maxlimit))
        DeleteTimeoutReader(86400);
    else if ((int32_t)mDevInodeReaderMap.size() > INT32_FLAG(logreader_count_upperlimit))
        DeleteTimeoutReader(86400 * 7);
    else
        DeleteTimeoutReader(INT32_FLAG(logreader_timeout_interval));
}

void ModifyHandler::DeleteTimeoutReader(int32_t timeoutInterval) {
    time_t curTime = time(NULL);

    NameLogFileReaderMap::iterator readerIter = mNameReaderMap.begin();
    for (; readerIter != mNameReaderMap.end();) {
        LogFileReaderPtrArray& readerArray = readerIter->second;
        for (LogFileReaderPtrArray::iterator iter = readerArray.begin(); iter != readerArray.end();) {
            int32_t interval = curTime - ((*iter)->GetLastUpdateTime());
            if (interval > timeoutInterval) {
                LOG_DEBUG(sLogger, ("remove reader with timeout flag :", (*iter)->GetLogPath().c_str()));
                mDevInodeReaderMap.erase((*iter)->GetDevInode());
                iter = readerArray.erase(iter);
            }
            // if current reader is not timeout, we should skip check last reader
            // because when there are many readers, we only update first reader's last update time in modify handler
            else
                break;
        }
        if (readerArray.empty()) {
            LOG_DEBUG(sLogger, ("remove empty reader Array", readerIter->first));
            readerIter = mNameReaderMap.erase(readerIter);
        } else {
            ++readerIter;
        }
    }
}

void ModifyHandler::DeleteRollbackReader() {
    int32_t curTime = time(NULL);
    DevInodeLogFileReaderMap::iterator readerIter = mRotatorReaderMap.begin();
    vector<DevInode> deletedReaderKeys;
    for (; readerIter != mRotatorReaderMap.end(); ++readerIter) {
        int32_t interval = curTime - readerIter->second->GetLastUpdateTime();
        readerIter->second->CloseTimeoutFilePtr(curTime);
        if (interval > INT32_FLAG(logreader_filerotate_remove_interval)) {
            deletedReaderKeys.push_back(readerIter->first);
            LOG_DEBUG(sLogger, ("remove reader with rotate flag :", readerIter->second->GetLogPath().c_str()));
        }
    }
    vector<DevInode>::iterator keyIter = deletedReaderKeys.begin();
    for (; keyIter != deletedReaderKeys.end(); ++keyIter)
        mRotatorReaderMap.erase(*keyIter);
}

} // namespace logtail
