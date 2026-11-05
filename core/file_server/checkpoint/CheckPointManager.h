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
#include <ctime>

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "boost/optional.hpp"
#include "json/json.h"

#include "common/DevInode.h"
#include "common/EncodingConverter.h"
#include "common/SplitedFilePath.h"
#include "file_server/reader/LogFileReader.h"

#ifdef APSARA_UNIT_TEST_MAIN
#include "AppConfig.h"
#endif

namespace logtail {

#define NO_CHECKPOINT_VERSION 0
class CheckPoint {
public:
    DevInode mDevInode;
    // Filesystem UUID of the block device that held the file. Empty for checkpoints
    // written before this field existed, and for filesystems without a UUID.
    std::string mFsUuid;
    int64_t mOffset = 0;
    uint64_t mSignatureHash = 0;
    uint32_t mSignatureSize = 0;
    int32_t mLastUpdateTime = 0;
    bool mFileOpenFlag = false;
    bool mContainerStopped = false;
    std::string mContainerID;
    bool mLastForceRead = false;
    std::string mCache;
    std::string mConfigName;
    std::string mFileName;
    std::string mResolvedFileName;
    std::string mRealFileName;
    int32_t mIdxInReaderArray = LogFileReader::CHECKPOINT_IDX_UNDEFINED;
    // In-memory residency start. Set on AddCheckPoint (reset on overwrite and on rescue
    // in AddExistedCheckPointFileEvents), never serialized. Only used by
    // CheckTimeoutCheckPoint to evict pending entries that nobody consumes.
    int32_t mMemInsertTime = 0;

    CheckPoint() {}

    CheckPoint(const std::string& filename,
               const std::string& resolvedFileName,
               int64_t offset,
               uint32_t signatureSize,
               uint64_t signatureHash,
               DevInode devInode,
               const std::string& configName,
               const std::string& realFileName,
               bool fileOpenFlag,
               bool containerStopped,
               std::string containerID,
               bool lastForceRead)
        : mDevInode(devInode),
          mOffset(offset),
          mSignatureHash(signatureHash),
          mSignatureSize(signatureSize),
          mLastUpdateTime(0),
          mFileOpenFlag(fileOpenFlag),
          mContainerStopped(containerStopped),
          mContainerID(containerID),
          mLastForceRead(lastForceRead),
          mConfigName(configName),
          mFileName(filename),
          mResolvedFileName(resolvedFileName),
          mRealFileName(realFileName) {}
};

class DirCheckPoint {
public:
    std::string mParentName;
    std::set<std::string> mSubDir;
    int32_t mUpdateTime;
    DirCheckPoint() {}
    DirCheckPoint(const std::string& dir) : mParentName(dir), mUpdateTime(time(NULL)) {}
};

typedef std::shared_ptr<DirCheckPoint> DirCheckPointPtr;
typedef std::shared_ptr<CheckPoint> CheckPointPtr;

class CheckPointManager {
public:
    struct CheckPointKey {
        CheckPointKey() {}
        CheckPointKey(const DevInode& devInode, const std::string& configName)
            : mDevInode(devInode), mConfigName(configName) {}
        DevInode mDevInode;
        std::string mConfigName;

        bool operator<(const CheckPointKey& o) const {
            if (mDevInode < o.mDevInode) {
                return true;
            }
            if (mDevInode == o.mDevInode) {
                return mConfigName < o.mConfigName;
            }
            return false;
        }

        bool operator==(const CheckPointKey& o) const {
            return mDevInode == o.mDevInode && mConfigName == o.mConfigName;
        }
    };

    struct FsUuidCheckPointKey {
        std::string mFsUuid;
        uint64_t mInode = 0;
        std::string mConfigName;

        bool operator==(const FsUuidCheckPointKey& o) const {
            return mInode == o.mInode && mFsUuid == o.mFsUuid && mConfigName == o.mConfigName;
        }
    };

    struct FsUuidCheckPointKeyHash {
        size_t operator()(const FsUuidCheckPointKey& key) const {
            size_t seed = std::hash<std::string>()(key.mFsUuid);
            seed ^= std::hash<uint64_t>()(key.mInode) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= std::hash<std::string>()(key.mConfigName) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

    typedef std::map<CheckPointKey, CheckPointPtr> DevInodeCheckPointHashMap;

private:
    void IndexFsUuid(const CheckPointPtr& checkPoint);
    void UnindexFsUuid(const CheckPointPtr& checkPoint);

    DevInodeCheckPointHashMap mDevInodeCheckPointPtrMap;
    std::unordered_map<FsUuidCheckPointKey, CheckPointPtr, FsUuidCheckPointKeyHash> mFsUuidCheckPointPtrMap;
    std::unordered_map<std::string, DirCheckPointPtr> mDirNameMap;
    // Keys written by AddCheckPoint during the current dump round (i.e. snapshots of
    // live readers from DumpAllHandlersMeta). EndDumpRound erases exactly these, so
    // pending handoff entries survive periodic dumps.
    std::vector<CheckPointKey> mDumpRoundKeys;
    bool mCollectingDumpRound = false;
    int32_t mLastCheckTime;
    int32_t mLastDumpTime;
    int32_t mLoadVersion;
    int32_t mReaderCount;
    CheckPointManager()
        : mLastCheckTime(time(NULL)), mLastDumpTime(time(NULL)), mLoadVersion(NO_CHECKPOINT_VERSION), mReaderCount(0) {}

    // A checkpoint's physical file still exists when its path currently resolves to the
    // same (dev, inode). After a fake rotation the deleted file's path is gone or points
    // to a different inode, so such a checkpoint is treated as stale. Host / real /
    // resolved paths are probed so a rotated-but-present file is not misjudged.
    static bool CheckPointFileStillExists(const CheckPoint& checkPoint);
    static bool getCheckPointSearchDepth(const CheckPoint& checkPoint, uint16_t& searchDepth);

public:
    bool CheckVersion();
    void AddCheckPoint(CheckPoint* checkPointPtr);
    void AddDirCheckPoint(const std::string& dirname);
    void DeleteCheckPoint(DevInode devInode, const std::string& configName);
    void DeleteDirCheckPoint(const std::string& dirname);
    void LoadCheckPoint();
    void LoadDirCheckPoint(const Json::Value& root);
    void LoadFileCheckPoint(const Json::Value& root);
    bool DumpCheckPointToLocal();
    int32_t GetReaderCount();
    bool GetCheckPoint(DevInode devInode, const std::string& configName, CheckPointPtr& checkPointPtr);
    bool GetCheckPointByFsUuid(const std::string& fsUuid,
                               uint64_t inode,
                               const std::string& configName,
                               CheckPointPtr& checkPointPtr);
    // Moves the checkpoint off oldKey onto newDevInode and refreshes mFsUuid.
    // Does not call AddCheckPoint, and does not register the new key with the dump round.
    bool RekeyCheckPoint(const CheckPointKey& oldKey, const DevInode& newDevInode, const std::string& fsUuid);
    bool GetDirCheckPoint(const std::string& filename, DirCheckPointPtr& checkPointPtr);
    void RemoveAllCheckPoint();
    // Dump-round bracket for periodic dump: keys added between Begin and End (live
    // reader snapshots) are erased by EndDumpRound; other entries stay pending.
    void BeginDumpRound();
    void EndDumpRound();
    void CheckTimeoutCheckPoint();
    bool NeedDump(int32_t curTime);
    void ResetLastDumpTime();
    DevInodeCheckPointHashMap& GetAllFileCheckPoint();

    static CheckPointManager* Instance() {
        static CheckPointManager checkPointManager;
        return &checkPointManager;
    }

    static bool CheckPointCmpByUpdateTime(const CheckPoint* left, const CheckPoint* right) {
        return left->mLastUpdateTime > right->mLastUpdateTime;
    }

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ConfigUpdatorUnittest;
    void RemoveLocalCheckPoint();
    void PrintStatus();
#endif
};

// Iterate files in dirPath, find the file with devInode.
//
// It will only iterate first N files, which is controlled by flag
//  checkpoint_find_max_file_count.
//
// @cache [out]: cache iterated files if not nullptr.
// @searchTruncated [out]: set to true if the file-count limit stops the scan.
//
// @return the path of the file with devInode if find.
boost::optional<std::string> SearchFilePathByDevInodeInDirectory(const std::string& dirPath,
                                                                 const uint16_t searchDepth,
                                                                 const DevInode& devInode,
                                                                 std::map<DevInode, SplitedFilePath>* cache);
boost::optional<std::string> SearchFilePathByDevInodeInDirectory(const std::string& dirPath,
                                                                 const uint16_t searchDepth,
                                                                 const DevInode& devInode,
                                                                 std::map<DevInode, SplitedFilePath>* cache,
                                                                 bool* searchTruncated);

// Find a regular file under dirPath whose inode matches and whose filesystem UUID
// equals fsUuid. Used when the block device number changed after a reboot.
boost::optional<std::string> SearchFilePathByInodeAndFsUuid(const std::string& dirPath,
                                                            uint16_t searchDepth,
                                                            uint64_t inode,
                                                            const std::string& fsUuid,
                                                            DevInode* matchedDevInode);

} // namespace logtail
