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
#include <unordered_map>
#include "ConfigManager.h"
#include "sls_logs.pb.h"
#include "common/Lock.h"
#include "profile_sender/ProfileSender.h"

namespace logtail {
// forward declaration
class Config;
struct LogBuffer;
struct LoggroupTimeValue;
struct MergeItem;

struct LogTimeInfo {
    LogTimeInfo() {}
    LogTimeInfo(int64_t seqNum, time_t logTime, int32_t lines, int status)
        : mLogGroupSequenceNum(seqNum), mLogTime(logTime), mSucceededLines(lines), mStatus(status), mInitFlag(true) {}

    static const int LogIntegrityStatus_ParseOK;
    static const int LogIntegrityStatus_ParseFail;
    static const int LogIntegrityStatus_SendOK;
    static const int LogIntegrityStatus_SendFail;

    int64_t mLogGroupSequenceNum;
    time_t mLogTime;
    int32_t mSucceededLines;
    int mStatus;
    bool mInitFlag;
};

struct LogIntegrityInfo {
    LogIntegrityInfo() {}
    LogIntegrityInfo(const std::string& region,
                     const std::string& projectName,
                     const std::string& logstore,
                     const std::string& filename,
                     const std::string& aliuid,
                     const std::string& integrityProject,
                     const std::string& integrityLogstore,
                     time_t lastUpdateTime = -1)
        : mRegion(region),
          mProjectName(projectName),
          mLogstore(logstore),
          mFilename(filename),
          mAliuid(aliuid),
          mIntegrityProject(integrityProject),
          mIntegrityLogstore(integrityLogstore),
          mSendSucceededFlag(true),
          mLastUpdateTime(lastUpdateTime) {}

    void SetStatus(int64_t seqNum, int32_t lines, int status);
    void CalcIntegrityInfo(time_t& integrityTime, int32_t& lines, std::string& status);

    std::string mRegion;
    std::string mProjectName;
    std::string mLogstore;
    std::string mFilename;

    // data integrity
    std::string mAliuid;
    std::string mIntegrityProject;
    std::string mIntegrityLogstore;
    std::list<LogTimeInfo> mLogTimeInfoList;
    bool mSendSucceededFlag;

    time_t mLastUpdateTime;
};

struct LogDest {
    LogDest(const std::string& aliuid,
            const std::string& region,
            const std::string& project,
            const std::string& logstore)
        : mRegion(region), mProjectName(project), mLogstore(logstore) {}

    bool operator<(const LogDest& rhs) const {
        if ((mAliuid < rhs.mAliuid) || (mAliuid == rhs.mAliuid && mRegion < rhs.mRegion)
            || (mAliuid == rhs.mAliuid && mRegion == rhs.mRegion && mProjectName < rhs.mProjectName)
            || (mAliuid == rhs.mAliuid && mRegion == rhs.mRegion && mProjectName == rhs.mProjectName
                && mLogstore < rhs.mLogstore)) {
            return true;
        }
        return false;
    }

    std::string mAliuid;
    std::string mRegion;
    std::string mProjectName;
    std::string mLogstore;
};

struct OutDatedFile {
    OutDatedFile(const std::string& aliuid,
                 const std::string& region,
                 const std::string& project,
                 const std::string& logstore,
                 const std::string& filename,
                 const std::string& integrityProject,
                 const std::string& integrityLogstore,
                 time_t lastUpdateTime = -1)
        : mAliuid(aliuid),
          mRegion(region),
          mProjectName(project),
          mLogstore(logstore),
          mFilename(filename),
          mIntegrityProject(integrityProject),
          mIntegrityLogstore(integrityLogstore),
          mLastUpdateTime(lastUpdateTime) {}

    std::string mAliuid;
    std::string mRegion;
    std::string mProjectName;
    std::string mLogstore;
    std::string mFilename;

    std::string mIntegrityProject;
    std::string mIntegrityLogstore;

    time_t mLastUpdateTime;
};

class LogIntegrity {
public:
    static LogIntegrity* GetInstance() {
        static LogIntegrity* ptr = new LogIntegrity();
        return ptr;
    }

public:
    void RecordIntegrityInfo(MergeItem* item);
    void Notify(LoggroupTimeValue* data, bool flag);
    void SendLogIntegrityInfo();
    void EraseItemInMap(const std::string& region, const std::string& projectName, const std::string& logstore);
    void DumpIntegrityDataToLocal();
    bool ReloadIntegrityDataFromLocalFile();
#ifdef LOGTAIL_DEBUG_FLAG
    void PrintLogTimeInfoListStatus();
#endif
private:
    LogIntegrity();
    ~LogIntegrity();

    std::string
    GetIntegrityInfoKey(const std::string& project, const std::string& logstore, const std::string& filename) {
        return project + "_" + logstore + "_" + filename;
    }
    std::string GetConfigKey(const std::string& integrityProjectName, const std::string& integrityLogstore) {
        return integrityProjectName + "_" + integrityLogstore;
    }
    void EliminateOutDatedData(int32_t gapSeconds);
    void ShrinkLogIntegrityInfoMap(int32_t shrinkSize);
    bool FindLogIntegrityInfo(const std::string& region,
                              const std::string& projectName,
                              const std::string& logstore,
                              const std::string& filename,
                              LogIntegrityInfo*& info);
    void BuildLogGroup(sls_logs::LogGroup& logGroup,
                       const std::string& projectName,
                       const std::string& logstore,
                       const std::string& filename,
                       time_t businessTime,
                       int32_t succeededLines,
                       const std::string& status);
    size_t GetMapItemCount();
    static bool SortByLastUpdateTime(const std::pair<std::string, LogIntegrityInfo*>& lhs,
                                     const std::pair<std::string, LogIntegrityInfo*>& rhs);
    static void SerializeListNodes(const std::list<LogTimeInfo>& logTimeInfoList, Json::Value& value);
    static void DeSerializeListNodes(const Json::Value& value, std::list<LogTimeInfo>& logTimeInfoList);
    void FillLogTimeInfo(LogIntegrityInfo* info, const std::string& name, const Json::Value& value);
    std::string GetLastLogLine(MergeItem* item);
    void InsertItemIntoOutDatedFileMap(LogIntegrityInfo* info);
    time_t EraseItemInOutDatedFileMap(const std::string& region,
                                      const std::string& project,
                                      const std::string& logstore,
                                      const std::string& filename);
    void SendOutDatedFileIntegrityInfo();

private:
    // key: projectName + "_" + bizLogStore + "_" + filename
    typedef std::unordered_map<std::string, LogIntegrityInfo*> LogIntegrityInfoMap;
    // key: region
    typedef std::unordered_map<std::string, LogIntegrityInfoMap*> RegionLogIntegrityInfoMap;
    RegionLogIntegrityInfoMap mRegionLogIntegrityInfoMap;

    // key: projectName + "_" + bizLogStore + "_" + filename
    typedef std::unordered_map<std::string, OutDatedFile*> OutDatedFileMap;
    // key: region
    typedef std::unordered_map<std::string, OutDatedFileMap*> RegionOutDatedFileMap;
    RegionOutDatedFileMap mRegionOutDatedFileMap;

    PTMutex mLogIntegrityMapLock;

    std::string mIntegrityDumpFileName;
    std::string mBakIntegrityDumpFileName;
    ProfileSender mProfileSender;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class DataIntegrityUnittest;
    friend class SenderUnittest;

    void ClearForTest() {
        PTScopedLock lock(mLogIntegrityMapLock);
        mRegionLogIntegrityInfoMap.clear();
        mRegionOutDatedFileMap.clear();
    }
#endif
};

} // namespace logtail
