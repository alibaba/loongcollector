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
#include <deque>
#include <string>
#include <utility>
#include <map>
#include <unordered_map>
#include "common/Lock.h"
#include "common/DevInode.h"
#include "common/StringTools.h"
#include "FileInfo.h"
#include "util.h"

namespace logtail {

// forward declaration
struct LoggroupTimeValue;

struct LogFileOffsetInfoNode {
    // default send status is false
    LogFileOffsetInfoNode(int64_t seq, int64_t offset, int64_t len, int64_t filePos, int64_t readPos, int64_t size)
        : mSendFlag(false),
          mSequenceNum(seq),
          mOffset(offset),
          mLen(len),
          mFileLastPos(filePos),
          mFileReadPos(readPos),
          mFileSize(size) {}

    bool mSendFlag;
    int64_t mSequenceNum;
    int64_t mOffset;
    int64_t mLen;
    int64_t mFileLastPos;
    int64_t mFileReadPos;
    int64_t mFileSize;
};

struct LogFileInfo {
    LogFileInfo(const std::string& project,
                const std::string& logstore,
                const std::string& configName,
                const std::string& filename,
                DevInode devInode,
                bool fuseMode)
        : mProjectName(project),
          mLogstore(logstore),
          mConfigName(configName),
          mFilename(filename),
          mDevInode(devInode),
          mFuseMode(fuseMode) {}

    LogFileInfo(const LogFileInfo& rhs)
        : mProjectName(rhs.mProjectName),
          mLogstore(rhs.mLogstore),
          mConfigName(rhs.mConfigName),
          mFilename(rhs.mFilename),
          mDevInode(rhs.mDevInode),
          mFuseMode(rhs.mFuseMode) {}

    LogFileInfo& operator=(const LogFileInfo& rhs) {
        if (this != &rhs) {
            mProjectName = rhs.mProjectName;
            mLogstore = rhs.mLogstore;
            mConfigName = rhs.mConfigName;
            mFilename = rhs.mFilename;
            mDevInode = rhs.mDevInode;
            mFuseMode = rhs.mFuseMode;
        }
        return *this;
    }

    bool operator==(const LogFileInfo& rhs) const {
        return mProjectName == rhs.mProjectName && mLogstore == rhs.mLogstore && mConfigName == rhs.mConfigName
            && mFilename == rhs.mFilename;
    }

    struct LogFileInfoHash {
        size_t operator()(const LogFileInfo& info) const {
            size_t seed = 0;
            boost::hash_combine(seed, boost::hash_value(info.mProjectName));
            boost::hash_combine(seed, boost::hash_value(info.mLogstore));
            boost::hash_combine(seed, boost::hash_value(info.mConfigName));
            boost::hash_combine(seed, boost::hash_value(info.mFilename));
            return seed;
        }
    };

    std::string ToString() const {
        std::string str;
        return str.append(mProjectName)
            .append("_")
            .append(mLogstore)
            .append("_")
            .append(mConfigName)
            .append("_")
            .append(mFilename)
            .append("_")
            .append(logtail::ToString(mDevInode.dev))
            .append("_")
            .append(logtail::ToString(mDevInode.inode));
    }

    std::string mProjectName;
    std::string mLogstore;
    std::string mConfigName;
    std::string mFilename;

    DevInode mDevInode;
    bool mFuseMode;
};

// record all sparse info, erase item after calc sparse offset and length
class LogFileOffsetInfo {
public:
    LogFileOffsetInfo(const std::string& project,
                      const std::string& logstore,
                      const std::string configName,
                      const std::string& filename,
                      const DevInode& devInode,
                      bool fuseMode,
                      int fd,
                      int32_t curTime)
        : mLogFileInfo(project, logstore, configName, filename, devInode, fuseMode), mFd(fd), mLastUpdateTime(-1) {}

    void SetSendFlag(int64_t seqNum); // send send flag to true
    bool CalcOffset(int64_t& offset, int64_t& len, int64_t& filePos, int64_t& readPos, int64_t& size);

    bool operator<(const LogFileOffsetInfo& rhs) const { return mLastUpdateTime < rhs.mLastUpdateTime; }

    static inline int64_t AlignOffset(int64_t originOffset) {
        if (originOffset == 0)
            return 0;
        // align to 4k, 2^12
        // reserve at least 1 byte at the end of file
        return ((originOffset - 1) >> 12) << 12;
    }

public:
    LogFileInfo mLogFileInfo;
    int mFd;

    std::deque<LogFileOffsetInfoNode> mLogFileOffsetInfoQueue;
    int32_t mLastUpdateTime;
};

struct LogFileCollectProgress {
    LogFileCollectProgress(int64_t offset = 0, int64_t filePos = 0, int64_t readPos = 0, int64_t size = 0)
        : mSendOffset(offset), mFileLastPos(filePos), mFileReadPos(readPos), mFileSize(size) {}

    LogFileCollectProgress(const LogFileCollectProgress& rhs)
        : mSendOffset(rhs.mSendOffset),
          mFileLastPos(rhs.mFileLastPos),
          mFileReadPos(rhs.mFileReadPos),
          mFileSize(rhs.mFileSize) {}

    LogFileCollectProgress& operator=(const LogFileCollectProgress& rhs) {
        if (this != &rhs) {
            mSendOffset = rhs.mSendOffset;
            mFileLastPos = rhs.mFileLastPos;
            mFileReadPos = rhs.mFileReadPos;
            mFileSize = rhs.mFileSize;
        }
        return *this;
    }

    bool IsValid() const {
        return mSendOffset >= 0 && mFileLastPos >= 0 && mFileReadPos >= 0 && mFileSize > 0 && mSendOffset <= mFileSize
            && mSendOffset <= mFileLastPos && mFileLastPos <= mFileReadPos;
    }

    bool IsFinished() const { return IsValid() && mSendOffset == mFileLastPos && mFileReadPos == mFileSize; }

    int64_t mSendOffset;
    int64_t mFileLastPos;
    int64_t mFileReadPos;
    int64_t mFileSize;
};

typedef std::unordered_map<LogFileInfo, LogFileCollectProgress, LogFileInfo::LogFileInfoHash> LogFileCollectProgressMap;

class LogFileCollectOffsetIndicator {
public:
    static LogFileCollectOffsetIndicator* GetInstance() {
        static LogFileCollectOffsetIndicator* ptr = new LogFileCollectOffsetIndicator();
        return ptr;
    }

public:
    void RecordFileOffset(LoggroupTimeValue* data);
    void NotifySuccess(const LoggroupTimeValue* data);
    void CalcFileOffset();
    void EliminateOutDatedItem(); // eliminate out-dated file, no longer updated since 2 days ago
    void ShrinkLogFileOffsetInfoMap(); // if map size > 10000, then erase 2000 item
    void DeleteItem(const std::string& filename, const DevInode& devInode);
    void DeleteItem(const std::string& configName, const std::string& filename);
    LogFileCollectProgressMap GetAllFileProgress();
    bool GetFileProgress(const std::string& filename, LogFileCollectProgressMap& m);

private:
    LogFileCollectOffsetIndicator();
    ~LogFileCollectOffsetIndicator();

    void ClearAllFileOffset();
    bool FindLogFileOffsetInfo(const std::string& project,
                               const std::string& logstore,
                               const std::string& configName,
                               const std::string& filename,
                               const DevInode& devInode,
                               bool fuseMode,
                               LogFileOffsetInfo*& logFileOffsetInfo);
    static bool SortByLastUpdateTime(const LogFileOffsetInfo* lhs, const LogFileOffsetInfo* rhs);

    // for unit test
    void MockSparsify(std::map<std::string, int64_t>& origin, std::map<std::string, int64_t>& aligned);

private:
    PTMutex mLogSparsifierLock;

    typedef std::unordered_map<LogFileInfo, LogFileOffsetInfo*, LogFileInfo::LogFileInfoHash> LogFileOffsetInfoMap;
    LogFileOffsetInfoMap mLogFileOffsetInfoMap;

    LogFileCollectProgressMap mLogFileCollectProgressMap;

#ifdef APSARA_UNIT_TEST_MAIN
    void CleanEnviroments();
    friend class FuseFileUnittest;
    friend class SenderUnittest;
#endif
};

} // namespace logtail
