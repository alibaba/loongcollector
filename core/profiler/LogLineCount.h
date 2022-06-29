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
#include <json/json.h>
#include "sls_logs.pb.h"
#include "common/Lock.h"
#include "util.h"
#include "profile_sender/ProfileSender.h"

namespace logtail {

// forward declaration
struct LoggroupTimeValue;

class LogLineCount {
public:
    static LogLineCount* GetInstance() {
        static LogLineCount* ptr = new LogLineCount();
        return ptr;
    }

public:
    void NotifySuccess(LoggroupTimeValue* data);
    void InsertLineCountDataToLogGroup(sls_logs::LogGroup& logGroup,
                                       const std::string& region,
                                       const std::string& projectName,
                                       const std::string& logStore,
                                       int32_t minuteTime,
                                       int32_t count);
    void SendLineCountData(bool forceSend = false);
    void DumpLineCountDataToLocal();
    bool ReloadLineCountDataFromLocalFile();
    void EraseItemInMap(const std::string& region, const std::string& projectName, const std::string& logstore);

private:
    LogLineCount();
    ~LogLineCount();

private:
    // NOTICE: LogStoreLineCount stands for succeeded line count of log group, when a log group is written to shard successfully,
    // call back function OnSuccess will be called , thus line count information of a certain region, project, log store in a certain minute
    // will update corresponding map below.
    struct LogStoreLineCount {
        LogStoreLineCount() {}
        LogStoreLineCount(const std::string& region,
                          const std::string& projectName,
                          const std::string& bizLogStore,
                          const std::string& cntProjectName,
                          const std::string& cntLogStore)
            : mRegion(region),
              mProjectName(projectName),
              mBizLogStore(bizLogStore),
              mCntProjectName(cntProjectName),
              mCntLogStore(cntLogStore) {}

        // key: time(in minute)
        typedef std::map<int32_t, int32_t> LogCountPerMinuteMap;
        // NotifySuccess() will change this map, insert log lines in a new minute, or add log lines in an existed minute
        // this map may contain log lines in different minutes because in some occasion, the interval we send the log lines result is bigger than one minute
        LogCountPerMinuteMap mLogCountPerMinuteMap;

        std::string mRegion;
        std::string mProjectName;
        std::string mBizLogStore;
        std::string mCntProjectName;
        std::string mCntLogStore;
    };

    void ClearLineCountData();
    void
    FillLineCountDataFromLocalFile(LogStoreLineCount* lineCount, const std::string& name, const Json::Value& value);

private:
    // NOTICE: we will clear the map once we have traversed all the item and sent them
    // key: projectName + "_" + bizLogStore
    typedef std::unordered_map<std::string, LogStoreLineCount*> LogStoreLineCountMap;
    // key: region
    typedef std::unordered_map<std::string, LogStoreLineCountMap*> RegionLineCountMap;
    static RegionLineCountMap mRegionLineCountMap;
    // we need a lock because different threads will read and write line count map
    PTMutex mLineCountMapLock;

    int32_t mLineCountLastSendTime;
    int32_t mLineCountSendInterval;
    std::string mLineCountDumpFileName;
    std::string mBakLineCountDumpFileName;
    ProfileSender mProfileSender;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class DataIntegrityUnittest;
#endif
};

} // namespace logtail
