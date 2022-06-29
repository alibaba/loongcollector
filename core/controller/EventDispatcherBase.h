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
#include <sys/types.h>
#if defined(__linux__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif
#include <stddef.h>
#include <time.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include "profiler/LogFileProfiler.h"
#include "polling/PollingModify.h"
#include "polling/PollingDirFile.h"
#include "event_listener/EventListener.h"
#include "checkpoint/CheckPointManager.h"

namespace logtail {

class Config;
class TimeoutHandler;
class EventHandler;
class Event;

const int MSG_HDR_LEN = 8;

typedef struct SingleDSPacket {
public:
    int64_t mPacketSize;
    int64_t mRecvSize;
    char mPacketHdr[MSG_HDR_LEN];
    char* mPacket;
    int8_t mPacketType;

    SingleDSPacket() : mPacketSize(0), mRecvSize(0), mPacket(NULL), mPacketType(0) {}

    void Reset() {
        memset(mPacketHdr, 0, MSG_HDR_LEN);
        mPacketSize = mRecvSize = mPacketType = 0;
        if (mPacket) {
            delete[] mPacket;
            mPacket = NULL;
        }
    }
} SingleDSPacket;

enum DirRegisterStatus { GET_REGISTER_STATUS_ERROR = 0, PATH_INODE_REGISTERED = 1, PATH_INODE_NOT_REGISTERED = 2 };

struct DirInfo {
    std::string mPath;
    uint64_t mInode;
    bool mIsSymbolicLink;
    EventHandler* mHandler;

    DirInfo() : mPath(std::string()), mInode(0), mIsSymbolicLink(false), mHandler(NULL) {}
    DirInfo(const std::string& path, uint64_t inode, bool isSymbolicLink, EventHandler* handler)
        : mPath(path), mInode(inode), mIsSymbolicLink(isSymbolicLink), mHandler(handler) {}
};

class EventDispatcherBase {
public:
    /** Add event handler to a directory. Whenever the any event
     * happens in this directory, handler->Handle(event) is called.
     * Previously registered handler is overwritten silently;
     * To prevent resource leak, one may first call GetHandler to do some cleanup work on that handler.
     * Currently supported events:
     *     creation of file or directory under path
     *     whether an file or directory is created
     *     modification of file under path
     *
     * @param path the path of directory to be watched;
     * should not be terminated by '/'; should not contain "//"
     *
     * @param handler for the events occur on the path
     * @param reason if not null and the call fail, the reason for failure is stored
     *
     * @return true on success; on error false is returned
     */
    bool RegisterEventHandler(const char* path, Config* config, EventHandler*& handler);

    /** Unregister handler for path; If no handler registered for path, do nothing but return.
     * After this call, no event watched on this path any more.
     * Timeout watch on path, if added, is also removed.
     * Pending events on this path are discarded.
     *
     * @param path for whom event handler will be removed.
     */
    //TODO see whether report errors
    void UnregisterEventHandler(const char* path);

    /** Close handlers for path; If no handler registered for path, do nothing but return.
     *
     * @param path for whom event handler will be removed.
     */
    void StopAllDir(const std::string& baseDir);

    EventHandler* GetHandler(const char* path) {
        MapType<std::string, int>::Type::iterator itr = mPathWdMap.find(path);
        if (itr == mPathWdMap.end())
            return NULL;
        else
            return mWdDirInfoMap[itr->second]->mHandler;
    }

    size_t GetHandlerCount() { return mPathWdMap.size(); }

    /** Test whether a directory is registered.
     *
     * @param path the directory
     * should not be terminated by '/'; should not contain "//"
     *
     * @return true if registered, false if not
     */
    bool IsRegistered(const char* path);

    /** Test whether a directory is registered.
     *
     * @param path the directory
     * should not be terminated by '/'; should not contain "//"
     * @return DIR_REGISTER_STATUS
     */
    DirRegisterStatus IsDirRegistered(const std::string& path);

    int InitStreamLogTcpSocket();


    /** Accept Connection on domain socket
     *
     * @param listenFd listen socket fd
     * @param epollFd
     *
     * @return true if success, false if failure
     */
    bool AcceptConnection(int listenFd, int epollFd);
    int AcceptTcpConnection(int listenFd, int epollFd);

    /** Handle Read Message ERROR
     *
     * @param eventFd
     * @param length
     *
     * @return true if quit normal, false if unknown exception
     */
    bool HandleReadException(int eventFd, int length, int hdrLength);

    /** Read Messsage on domain socket
     *
     * @param eventFd
     *
     * @return false if failure, true if success
     */
    bool ReadMessages(int eventFd);

    void ErasePacketBuffer(int eventFd);
    bool ReadDSPacketHdr(int eventFd);
    bool ReadDSPacket(int eventFd);

    /** Enter the event loop, dispatch to the approiate handler when an event occurs.
     * Propagate timeout.
     *
     * @return true on success; false on failure
     */
    bool Dispatch();
#if defined(_MSC_VER)
    virtual void InitWindowsSignalObject() = 0;
    virtual void SyncWindowsSignalObject() = 0;
    virtual void ReleaseWindowsSignalObject() = 0;
#endif
#if defined(__linux__)
    virtual void InitShennong() = 0;
    virtual void CheckShennong() = 0;
#endif
    virtual void ExtraWork() = 0;
    void DumpAllHandlersMeta(bool);
    std::vector<std::pair<std::string, EventHandler*> > FindAllSubDirAndHandler(const std::string& baseDir);
    void UnregisterAllDir(const std::string& basePath);
    bool IsRegistered(int wd, std::string& path);
    void CheckSymbolicLink();

    void DumpCheckPointPeriod(int32_t curTime);

    void StartTimeCount();
    void PropagateTimeout(const char* path);
    void HandleTimeout();

    void ReadInotifyEvents(std::vector<Event*>& eventVec);

    void ProcessHandlerTimeOut();
    void AddExistedCheckPointFileEvents();

    bool FindCheckPointInPath(const std::string& path,
                              const DevInode& checkPointDevInode,
                              std::map<DevInode, SplitedFilePath>& cachePathDevInodeMap,
                              std::string& realFileName);

    void DumpInotifyWatcherDirs();
    virtual bool IsInterupt() = 0;

protected:
    EventDispatcherBase();
    ~EventDispatcherBase();
    bool AddStreamLogTcpSocketToEpoll();
    void AddOneToOneMapEntry(DirInfo* dirInfo, int wd);
    void RemoveOneToOneMapEntry(int wd);
    void UpdateConfig();
    void RemoveDSProfilers();
    void SendDSProfileData();
    void ExitProcess();
    /**
     * @return true on success; false if path isn't registered by RegisterEventHandler.
     */
    bool AddTimeoutWatch(const char* path);
    void AddExistedFileEvents(const char* path, int wd);

    enum class ValidateCheckpointResult {
        kNormal,
        kConfigNotFound,
        kInvalidFilePath,
        kConfigNotMatched,
        kLogDirNotWatched,
        kRotate,
        kZeroSigSize,
        kSigChanged,
        kLogDirChanged,
        kCacheFull,
        kDevInodeNotFound
    };
    ValidateCheckpointResult validateCheckpoint(CheckPointPtr& checkpoint,
                                                std::map<DevInode, SplitedFilePath>& cachePathDevInodeMap,
                                                std::vector<Event*>& eventVec);

    int mListenFd;
    int mWatchNum;
    int mInotifyWatchNum;
    int mEpollFd;
    int mStreamLogTcpFd;
    int mNonInotifyWd;
    EventHandler* mTimeoutHandler;
    // work around due to c++'s lack of typedef for template
    // substitute by std::map if stl's map is preferred
    template <typename KeyType, typename ValueType>
    struct MapType {
        typedef std::unordered_map<KeyType, ValueType> Type;
    };

    MapType<std::string, int>::Type mPathWdMap;
    MapType<int, DirInfo*>::Type mWdDirInfoMap;
    std::set<std::string> mBrokenLinkSet;
    // for timeout issue
    MapType<int, time_t>::Type mWdUpdateTimeMap;
    std::unordered_map<int64_t, SingleDSPacket*> mPacketBuffer;
    void* mStreamLogManagerPtr;
    volatile bool mMainThreadRunning;
    EventListener* mEventListener;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class EventDispatcherTest;
    friend class ConfigUpdatorUnittest;
    friend class ConfigMatchUnittest;
    friend class FuxiSceneUnittest;
    friend class LogtailWriterTest;
    friend class SymlinkInotifyTest;
    friend class SenderUnittest;
    friend class FuseFileUnittest;
    friend class MultiServerConfigUpdatorUnitest;

    void CleanEnviroments();
    int32_t GetInotifyWatcherCount();
#endif
};

} //namespace logtail
