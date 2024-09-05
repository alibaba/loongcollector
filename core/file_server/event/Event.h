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
#include <stdint.h>
#include <string>
#include "common/DevInode.h"

namespace logtail {

typedef uint32_t EventType;

#define EVENT_TIMEOUT (1)
#define EVENT_CREATE (2)
#define EVENT_MODIFY (4)
#define EVENT_ISDIR (8)
#define EVENT_OVERFLOW (16)
#define EVENT_MOVE_FROM (32)
#define EVENT_MOVE_TO (64)
#define EVENT_DELETE (128)
#define EVENT_CONTAINER_STOPPED (256)
#define EVENT_READER_FLUSH_TIMEOUT (512)

class Event {
private:
    std::string mSource; // path of file or dir
    std::string mObject; // the object who has changed
    EventType mType;
    int mWd;
    uint32_t mCookie;
    uint64_t mDev;
    uint64_t mInode;
    int64_t mHashKey;
    std::string mConfigName;

    // for read timeout
    int64_t mLastReadPos;
    int64_t mLastFilePos;

public:
    Event(const std::string& source, const std::string& object, EventType type, int wd, uint32_t cookie = 0)
        : mSource(source),
          mObject(object),
          mType(type),
          mWd(wd),
          mCookie(cookie),
          mDev(NO_BLOCK_DEV),
          mInode(NO_BLOCK_INODE) {}
    Event(const std::string& source,
          const std::string& object,
          EventType type,
          int wd,
          uint32_t cookie,
          uint64_t dev,
          uint64_t inode)
        : mSource(source), mObject(object), mType(type), mWd(wd), mCookie(cookie), mDev(dev), mInode(inode) {}

    static bool CompareByFullPath(const Event* lhs, const Event* rhs) {
        std::string lhsPath(lhs->mSource);
        lhsPath.append("/").append(lhs->mObject);
        std::string rhsPath(rhs->mSource);
        rhsPath.append("/").append(rhs->mObject);

        size_t lhsLen = lhsPath.length();
        size_t rhsLen = rhsPath.length();
        if (lhsLen < rhsLen) {
            return false;
        }
        if (lhsLen > rhsLen) {
            return true;
        }
        return lhsPath > rhsPath;
    }

    const std::string& GetSource() const { return mSource; }

    const std::string& GetObject() const { return mObject; }

    // GetObject_ is the same as GetObject, but used to avoid compilation failure on Windows.
    const std::string& GetObject_() const { return mObject; }

    EventType GetType() const { return mType; }

    uint64_t GetDev() const { return mDev; }

    uint64_t GetInode() const { return mInode; }
    
    int GetWd() const { return mWd; }

    const uint32_t GetCookie() const { return mCookie; }

    int64_t GetHashKey() { return mHashKey; }

    const std::string& GetConfigName() const { return mConfigName; }

    int64_t GetLastReadPos() const { return mLastReadPos; }

    int64_t GetLastFilePos() const { return mLastFilePos; }

    void SetSource(const std::string& source) { mSource = source; }  

    void SetDev(uint64_t dev) { mDev = dev; }

    void SetInode(uint64_t inode) { mInode = inode; }

    void SetHashKey(int64_t hashKey) { mHashKey = hashKey; }

    void SetConfigName(const std::string& configName) { mConfigName = configName; }

    void SetLastReadPos(int64_t lastReadPos) { mLastReadPos = lastReadPos; }

    void SetLastFilePos(int64_t lastFilePos) { mLastFilePos = lastFilePos; }
 
    bool IsCreate() const { return mType & EVENT_CREATE; }

    bool IsModify() const { return mType & EVENT_MODIFY; }

    bool IsDir() const { return mType & EVENT_ISDIR; }

    // Check if event buffer is overflow.
    bool IsOverflow() const { return mType & EVENT_OVERFLOW; }

    bool IsTimeout() const { return mType & EVENT_TIMEOUT; }

    bool IsMoveFrom() const { return mType & EVENT_MOVE_FROM; }

    bool IsMoveTo() const { return mType & EVENT_MOVE_TO; }

    bool IsDeleted() const { return mType & EVENT_DELETE; }

    bool IsContainerStopped() const { return mType & EVENT_CONTAINER_STOPPED; }

    bool IsReaderFlushTimeout() const { return mType & EVENT_READER_FLUSH_TIMEOUT; }

    std::string GetTypeString() const {
        std::string type;
        if (IsDir()) {
            type = "ISDIR | ";
        }
        if (IsCreate()) {
            type += "CREATE";
        } else if (IsModify()) {
            type += "MODIFY";
        } else if (IsMoveFrom()) {
            type += "MOVED_FROM";
        } else if (IsMoveTo()) {
            type += "MOVED_TO";
        } else if (IsDeleted()) {
            type += "DELETE";
        } else if (IsTimeout()) {
            type += "DELETE_SELF";
        } else if (IsContainerStopped()) {
            type += "CONTAINER_STOPPED";
        }
        return type;
    }
};

} // namespace logtail
