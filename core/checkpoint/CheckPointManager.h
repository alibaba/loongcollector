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
#include <string>
#include <memory>
#include <unordered_map>
#include <set>
#include <ctime>
#include <json/json.h>
#include <boost/optional.hpp>
#include "common/DevInode.h"
#include "common/EncodingConverter.h"
#include "common/SplitedFilePath.h"

#ifdef APSARA_UNIT_TEST_MAIN
#include "AppConfig.h"
#endif

namespace logtail {

#define NO_CHECKPOINT_VERSION 0
class CheckPoint {
public:
    std::string mFileName;
    std::string mRealFileName;
    int64_t mOffset;
    uint32_t mSignatureSize;
    uint64_t mSignatureHash;
    int32_t mLastUpdateTime;
    DevInode mDevInode;
    int32_t mFileOpenFlag;
    std::string mConfigName;

    CheckPoint() {}

    CheckPoint(const std::string& filename,
               int64_t offset,
               uint32_t signatureSize,
               uint64_t signatureHash,
               DevInode devInode,
               const std::string& configName = std::string(),
               const std::string& realFileName = std::string(),
               int32_t fileOpenFlag = 0)
        : mFileName(filename),
          mRealFileName(realFileName),
          mOffset(offset),
          mSignatureSize(signatureSize),
          mSignatureHash(signatureHash),
          mLastUpdateTime(0),
          mDevInode(devInode),
          mFileOpenFlag(fileOpenFlag),
          mConfigName(configName) {}
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
    };

    typedef std::map<CheckPointKey, CheckPointPtr> DevInodeCheckPointHashMap;

private:
    DevInodeCheckPointHashMap mDevInodeCheckPointPtrMap;
    std::unordered_map<std::string, DirCheckPointPtr> mDirNameMap;
    int32_t mLastCheckTime;
    int32_t mLastDumpTime;
    int32_t mLoadVersion;
    int32_t mReaderCount;
    CheckPointManager()
        : mLastCheckTime(time(NULL)), mLastDumpTime(time(NULL)), mLoadVersion(NO_CHECKPOINT_VERSION), mReaderCount(0) {}

public:
    bool CheckVersion();
    void AddCheckPoint(CheckPoint* checkPointPtr);
    void AddDirCheckPoint(const std::string& dirname);
    void DeleteCheckPoint(DevInode devInode, const std::string& configName);
    void DeleteDirCheckPoint(const std::string& filename);
    void LoadCheckPoint();
    void LoadDirCheckPoint(const Json::Value& root);
    void LoadFileCheckPoint(const Json::Value& root);
    bool DumpCheckPointToLocal();
    int32_t GetReaderCount();
    bool GetCheckPoint(DevInode devInode, const std::string& configName, CheckPointPtr& checkPointPtr);
    bool GetDirCheckPoint(const std::string& filename, DirCheckPointPtr& checkPointPtr);
    void RemoveAllCheckPoint();
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
//
// @return the path of the file with devInode if find.
boost::optional<std::string> SearchFilePathByDevInodeInDirectory(const std::string& dirPath,
                                                                 const uint16_t searchDepth,
                                                                 const DevInode& devInode,
                                                                 std::map<DevInode, SplitedFilePath>* cache);

} // namespace logtail
