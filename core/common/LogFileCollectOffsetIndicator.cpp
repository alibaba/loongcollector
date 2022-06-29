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

#include "LogFileCollectOffsetIndicator.h"
#include <vector>
#include <algorithm>
#include "profiler/LogtailAlarm.h"
#include "common/Flags.h"
#include "common/LogstoreSenderQueue.h"
#include "sender/Sender.h"
#include "util.h"
#include "ulogfslib_file.h"

DEFINE_FLAG_INT32(file_eliminate_interval, "time interval for file eliminating, seconds", 86400 * 2);
DEFINE_FLAG_INT32(file_map_shrink_size, "shrink size for file map", 2000);
DEFINE_FLAG_INT32(file_map_max_size, "max size for file map", 10000);
DEFINE_FLAG_INT32(default_file_offset_check_interval, "default file offset check interval, seconds", 1);
DEFINE_FLAG_INT32(default_fuse_file_header_length, "default fuse file header length, bytes", 4096);

namespace logtail {

void LogFileOffsetInfo::SetSendFlag(int64_t seqNum) {
    // set init flag to false
    for (std::deque<LogFileOffsetInfoNode>::iterator iter = mLogFileOffsetInfoQueue.begin();
         iter != mLogFileOffsetInfoQueue.end();
         ++iter) {
        if (seqNum == iter->mSequenceNum) {
            iter->mSendFlag = true;
            LOG_DEBUG(sLogger,
                      ("set status, seq num", seqNum)("project", mLogFileInfo.mProjectName)(
                          "log store", mLogFileInfo.mLogstore)("filename", mLogFileInfo.mFilename));
            return;
        }
    }
    LOG_ERROR(sLogger,
              ("failed to find seq num", seqNum)("project", mLogFileInfo.mProjectName)(
                  "log store", mLogFileInfo.mLogstore)("filename", mLogFileInfo.mFilename));
}

bool LogFileOffsetInfo::CalcOffset(int64_t& offset, int64_t& len, int64_t& filePos, int64_t& readPos, int64_t& size) {
    if (mLogFileOffsetInfoQueue.empty())
        return false;

    std::deque<LogFileOffsetInfoNode>::iterator iter = mLogFileOffsetInfoQueue.begin();
    for (; iter != mLogFileOffsetInfoQueue.end(); ++iter) {
        // succeeded node
        if (iter->mSendFlag) {
            LOG_DEBUG(sLogger,
                      ("find succeeded node, seq num",
                       iter->mSequenceNum)("project", mLogFileInfo.mProjectName)("log store", mLogFileInfo.mLogstore)(
                          "filename", mLogFileInfo.mFilename)("offset", iter->mOffset)("len", iter->mLen));
            offset = iter->mOffset;
            len = iter->mLen;
            filePos = iter->mFileLastPos;
            readPos = iter->mFileReadPos;
            size = iter->mFileSize;
        }
        // meet first failed node or init node(flag false), break
        else {
            LOG_DEBUG(sLogger,
                      ("find failed or init node, seq num",
                       iter->mSequenceNum)("project", mLogFileInfo.mProjectName)("log store", mLogFileInfo.mLogstore)(
                          "filename", mLogFileInfo.mFilename)("offset", iter->mOffset)("len", iter->mLen));
            break;
        }
    }

    // erase nodes from begin to first failed node(or end iterator)
    mLogFileOffsetInfoQueue.erase(mLogFileOffsetInfoQueue.begin(), iter);
    return true;
}

LogFileCollectOffsetIndicator::LogFileCollectOffsetIndicator() {
}

LogFileCollectOffsetIndicator::~LogFileCollectOffsetIndicator() {
    ClearAllFileOffset();
}

void LogFileCollectOffsetIndicator::RecordFileOffset(LoggroupTimeValue* data) {
    const std::string& project = data->mProjectName;
    const std::string& logstore = data->mLogstore;
    const std::string& configName = data->mConfigName;
    const std::string& filename = data->mFilename;
    int64_t seqNum = data->mLogGroupContext.mSeqNum;

    const FileInfoPtr& fileInfoPtr = data->mLogGroupContext.mFileInfoPtr;

    // get all sparse info
    PTScopedLock lock(mLogSparsifierLock);
    int fd = fileInfoPtr->fd;
    const DevInode& devInode = fileInfoPtr->devInode;

    LogFileInfo logFileInfo(project, logstore, configName, filename, devInode, data->mLogGroupContext.mFuseMode);
    LogFileOffsetInfoMap::iterator iter = mLogFileOffsetInfoMap.find(logFileInfo);
    if (iter == mLogFileOffsetInfoMap.end()) {
        LogFileOffsetInfo* logFileOffsetInfo = new LogFileOffsetInfo(project,
                                                                     logstore,
                                                                     configName,
                                                                     filename,
                                                                     devInode,
                                                                     data->mLogGroupContext.mFuseMode,
                                                                     fd,
                                                                     data->mLastUpdateTime);
        iter = mLogFileOffsetInfoMap.insert(std::make_pair(logFileInfo, logFileOffsetInfo)).first;
    }
    LogFileOffsetInfo* logFileOffsetInfo = iter->second;
    logFileOffsetInfo->mLastUpdateTime = data->mLastUpdateTime;

    LogFileOffsetInfoNode node(seqNum,
                               fileInfoPtr->offset,
                               fileInfoPtr->len,
                               fileInfoPtr->filePos,
                               fileInfoPtr->readPos,
                               fileInfoPtr->fileSize);
    logFileOffsetInfo->mLogFileOffsetInfoQueue.push_back(node);

    LOG_DEBUG(sLogger,
              ("insert sparse file info into map, project", project)("log store", logstore)("filename", filename)(
                  "offset", fileInfoPtr->offset)("len", fileInfoPtr->len)("file read pos", fileInfoPtr->readPos)(
                  "file size", fileInfoPtr->fileSize)("sequence num", seqNum));
}

void LogFileCollectOffsetIndicator::NotifySuccess(const LoggroupTimeValue* data) {
    const std::string& project = data->mProjectName;
    const std::string& logstore = data->mLogstore;
    const std::string& configName = data->mConfigName;
    const std::string& filename = data->mFilename;
    int64_t seqNum = data->mLogGroupContext.mSeqNum;

    const FileInfoPtr& fileInfoPtr = data->mLogGroupContext.mFileInfoPtr;
    // empty filename, filter metric data and data-integrity data
    if (filename.empty()) {
        return;
    }

    PTScopedLock lock(mLogSparsifierLock);
    const DevInode& devInode = fileInfoPtr->devInode;

    LogFileOffsetInfo* logFileOffsetInfo = NULL;
    if (FindLogFileOffsetInfo(
            project, logstore, configName, filename, devInode, data->mLogGroupContext.mFuseMode, logFileOffsetInfo)) {
        logFileOffsetInfo->SetSendFlag(data->mLogGroupContext.mSeqNum);
        LOG_DEBUG(sLogger,
                  ("notify success, project", project)("logstore", logstore)("filename", filename)("seq num", seqNum)(
                      "offset", fileInfoPtr->offset)("len", fileInfoPtr->len));
    }
}

void LogFileCollectOffsetIndicator::CalcFileOffset() {
    static int32_t lastOperationTime = 0;
    int32_t curTime = time(NULL);

    if (curTime - lastOperationTime > INT32_FLAG(default_file_offset_check_interval)) {
        lastOperationTime = curTime;

        PTScopedLock lock(mLogSparsifierLock);
        if (mLogFileOffsetInfoMap.empty())
            return;

        for (LogFileOffsetInfoMap::iterator iter = mLogFileOffsetInfoMap.begin();
             iter != mLogFileOffsetInfoMap.end();) {
            LogFileOffsetInfo* logFileOffsetInfo = iter->second;
            // check whether filename and dev inode matches, if not matched, delete this item and continue
            DevInode devInode = GetFileDevInode(logFileOffsetInfo->mLogFileInfo.mFilename);
            if (devInode != logFileOffsetInfo->mLogFileInfo.mDevInode) {
                LOG_ERROR(
                    sLogger,
                    ("file name does not match previous dev inode, file name",
                     logFileOffsetInfo->mLogFileInfo.mFilename)("dev", logFileOffsetInfo->mLogFileInfo.mDevInode.dev)(
                        "inode", logFileOffsetInfo->mLogFileInfo.mDevInode.inode));
                delete logFileOffsetInfo;

                LogFileCollectProgressMap::iterator progressIter = mLogFileCollectProgressMap.find(iter->first);
                if (progressIter != mLogFileCollectProgressMap.end()) {
                    mLogFileCollectProgressMap.erase(progressIter);
                }

                mLogFileOffsetInfoMap.erase(iter++);
                continue;
            }

            int64_t offset = 0, len = 0, filePos = 0, readPos = 0, size = 0;
            if (logFileOffsetInfo->CalcOffset(offset, len, filePos, readPos, size)) {
                LogFileCollectProgress item(offset + len, filePos, readPos, size);
                mLogFileCollectProgressMap[iter->first] = item;
            }
            ++iter;
        }
    }
}

void LogFileCollectOffsetIndicator::ClearAllFileOffset() {
    PTScopedLock lock(mLogSparsifierLock);
    for (LogFileOffsetInfoMap::iterator iter = mLogFileOffsetInfoMap.begin(); iter != mLogFileOffsetInfoMap.end();) {
        delete iter->second;
        mLogFileOffsetInfoMap.erase(iter++);
    }
}

bool LogFileCollectOffsetIndicator::FindLogFileOffsetInfo(const std::string& project,
                                                          const std::string& logstore,
                                                          const std::string& configName,
                                                          const std::string& filename,
                                                          const DevInode& devInode,
                                                          bool fuseMode,
                                                          LogFileOffsetInfo*& logFileOffsetInfo) {
    LogFileInfo fileInfo(project, logstore, configName, filename, devInode, fuseMode);
    LogFileOffsetInfoMap::iterator iter = mLogFileOffsetInfoMap.find(fileInfo);
    if (iter == mLogFileOffsetInfoMap.end()) {
        LOG_ERROR(sLogger,
                  ("failed to find fuse file offset, project",
                   project)("log store", logstore)("filename", filename)("dev", devInode.dev)("inode", devInode.inode));
        return false;
    }

    logFileOffsetInfo = iter->second;
    return true;
}

void LogFileCollectOffsetIndicator::EliminateOutDatedItem() {
    int32_t curTime = time(NULL);
    static int32_t lastEliminateTime = 0;
    if (curTime - lastEliminateTime > INT32_FLAG(file_eliminate_interval)) {
        lastEliminateTime = curTime;

        PTScopedLock lock(mLogSparsifierLock);
        if (mLogFileOffsetInfoMap.empty())
            return;

        for (LogFileOffsetInfoMap::iterator iter = mLogFileOffsetInfoMap.begin();
             iter != mLogFileOffsetInfoMap.end();) {
            LogFileOffsetInfo* logFileOffsetInfo = iter->second;
            // update time too old, file is no longer updated
            if (curTime - logFileOffsetInfo->mLastUpdateTime > INT32_FLAG(file_eliminate_interval)) {
                delete logFileOffsetInfo;
                mLogFileOffsetInfoMap.erase(iter++);
            } else {
                ++iter;
            }
        }
    }
}

void LogFileCollectOffsetIndicator::ShrinkLogFileOffsetInfoMap() {
    PTScopedLock lock(mLogSparsifierLock);

    if (mLogFileOffsetInfoMap.size() >= (size_t)INT32_FLAG(file_map_max_size)) {
        if (INT32_FLAG(file_map_shrink_size) > INT32_FLAG(file_map_max_size)) {
            LOG_ERROR(sLogger,
                      ("invalid parameter, file_map_shrink_size",
                       INT32_FLAG(file_map_shrink_size))("file_map_max_size", INT32_FLAG(file_map_max_size)));
            return;
        }

        // sort by update time
        std::vector<LogFileOffsetInfo*> vec;
        for (LogFileOffsetInfoMap::iterator iter = mLogFileOffsetInfoMap.begin(); iter != mLogFileOffsetInfoMap.end();
             ++iter) {
            vec.push_back(iter->second);
        }
        std::sort(vec.begin(), vec.end(), SortByLastUpdateTime);

        for (int32_t i = 0; i < INT32_FLAG(file_map_shrink_size); ++i) {
            LogFileOffsetInfo* logFileOffsetInfo = vec[i];
            LogFileOffsetInfoMap::iterator iter = mLogFileOffsetInfoMap.find(logFileOffsetInfo->mLogFileInfo);
            if (iter == mLogFileOffsetInfoMap.end()) {
                LOG_ERROR(sLogger,
                          ("failed to find fuse file offset, project", logFileOffsetInfo->mLogFileInfo.mProjectName)(
                              "log store", logFileOffsetInfo->mLogFileInfo.mLogstore)(
                              "filename", logFileOffsetInfo->mLogFileInfo.mFilename));
                continue;
            } else {
                // erase item
                delete iter->second;
                mLogFileOffsetInfoMap.erase(iter);
            }
        }
    }
}

bool LogFileCollectOffsetIndicator::SortByLastUpdateTime(const LogFileOffsetInfo* lhs, const LogFileOffsetInfo* rhs) {
    return lhs->mLastUpdateTime < rhs->mLastUpdateTime;
}

void LogFileCollectOffsetIndicator::DeleteItem(const std::string& filename, const DevInode& devInode) {
    PTScopedLock lock(mLogSparsifierLock);

    for (LogFileOffsetInfoMap::iterator iter = mLogFileOffsetInfoMap.begin(); iter != mLogFileOffsetInfoMap.end();) {
        LogFileOffsetInfo* logFileOffsetInfo = iter->second;
        if (logFileOffsetInfo->mLogFileInfo.mFilename == filename
            && logFileOffsetInfo->mLogFileInfo.mDevInode == devInode) {
            LogFileInfo logFileInfo = iter->first;
            delete logFileOffsetInfo;
            mLogFileOffsetInfoMap.erase(iter++);

            LogFileCollectProgressMap::iterator progressIter = mLogFileCollectProgressMap.find(logFileInfo);
            if (progressIter != mLogFileCollectProgressMap.end()) {
                mLogFileCollectProgressMap.erase(progressIter);
            }
        } else
            ++iter;
    }
}

void LogFileCollectOffsetIndicator::DeleteItem(const std::string& configName, const std::string& filename) {
    PTScopedLock lock(mLogSparsifierLock);

    // stat, get dev inode, if failed to get stat, then we delete item
    struct stat buf;
    if (stat(filename.c_str(), &buf) == 0) {
        LOG_INFO(sLogger, ("get file stat", filename)("new file may be created after old file deleted or moved", ""));
        return;
    }

    LOG_WARNING(sLogger, ("failed to get file stat, delete item", filename));
    for (LogFileOffsetInfoMap::iterator iter = mLogFileOffsetInfoMap.begin(); iter != mLogFileOffsetInfoMap.end();) {
        LogFileOffsetInfo* logFileOffsetInfo = iter->second;
        if (logFileOffsetInfo->mLogFileInfo.mConfigName == configName
            && logFileOffsetInfo->mLogFileInfo.mFilename == filename) {
            LogFileInfo logFileInfo = iter->first;
            delete logFileOffsetInfo;
            mLogFileOffsetInfoMap.erase(iter++);

            LogFileCollectProgressMap::iterator progressIter = mLogFileCollectProgressMap.find(logFileInfo);
            if (progressIter != mLogFileCollectProgressMap.end()) {
                mLogFileCollectProgressMap.erase(progressIter);
            }
        } else
            ++iter;
    }
}

LogFileCollectProgressMap LogFileCollectOffsetIndicator::GetAllFileProgress() {
    PTScopedLock lock(mLogSparsifierLock);

    return mLogFileCollectProgressMap;
}

bool LogFileCollectOffsetIndicator::GetFileProgress(const std::string& filename, LogFileCollectProgressMap& m) {
    PTScopedLock lock(mLogSparsifierLock);

    bool ret = false;
    LogFileCollectProgressMap::iterator iter = mLogFileCollectProgressMap.begin();
    for (; iter != mLogFileCollectProgressMap.end(); ++iter) {
        const LogFileInfo& info = iter->first;
        if (info.mFilename == filename) {
            ret = true;
            m[info] = iter->second;
        }
    }
    return ret;
}

void LogFileCollectOffsetIndicator::MockSparsify(std::map<std::string, int64_t>& origin,
                                                 std::map<std::string, int64_t>& aligned) {
    PTScopedLock lock(mLogSparsifierLock);
    if (mLogFileOffsetInfoMap.empty())
        return;

    for (LogFileOffsetInfoMap::iterator iter = mLogFileOffsetInfoMap.begin(); iter != mLogFileOffsetInfoMap.end();
         ++iter) {
        LogFileOffsetInfo* logFileOffsetInfo = iter->second;
        int64_t offset = 0, len = 0, filePos = 0, readPos = 0, fileSize = 0;
        logFileOffsetInfo->CalcOffset(offset, len, filePos, readPos, fileSize);
        // if offset is greater than default header length(4096), align offset, then sparsify
        // else we cannot sparsify, we should reserve 4096 bytes for header
        const LogFileInfo& info = iter->first;
        origin[info.ToString()] = offset;
        if (offset > INT32_FLAG(default_fuse_file_header_length)) {
            offset = LogFileOffsetInfo::AlignOffset(offset);
        }
        aligned[info.ToString()] = offset;
    }
}

#ifdef APSARA_UNIT_TEST_MAIN
void LogFileCollectOffsetIndicator::CleanEnviroments() {
    ClearAllFileOffset();
}
#endif

} // namespace logtail
