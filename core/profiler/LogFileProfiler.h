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
#include <mutex>
#include <unordered_map>
#include <map>
#include <json/json.h>
#include "profile_sender/ProfileSender.h"

namespace sls_logs {
class LogGroup;
}

namespace logtail {
// forward declaration
class Config;
struct LoggroupTimeValue;

// Collect the log file's profile such as lines processed.
class LogFileProfiler {
public:
    static LogFileProfiler* GetInstance() {
        static LogFileProfiler* ptr = new LogFileProfiler();
        return ptr;
    }

    void AddProfilingData(const std::string& configName,
                          const std::string& region,
                          const std::string& projectName,
                          const std::string& category,
                          const std::string& filename,
                          uint64_t readBytes,
                          uint64_t skipBytes,
                          uint64_t splitLines,
                          uint64_t parseFailures,
                          uint64_t regexMatchFailures,
                          uint64_t parseTimeFailures,
                          uint64_t historyFailures,
                          uint64_t sendFailures,
                          const std::string& errorLine);
    void AddProfilingSkipBytes(const std::string& configName,
                               const std::string& region,
                               const std::string& projectName,
                               const std::string& category,
                               const std::string& filename,
                               uint64_t skipBytes);

    void AddProfilingReadBytes(const std::string& configName,
                               const std::string& region,
                               const std::string& projectName,
                               const std::string& category,
                               const std::string& filename,
                               uint64_t dev,
                               uint64_t inode,
                               uint64_t fileSize,
                               uint64_t readOffset,
                               int32_t lastReadTime);

    void SendProfileData(bool forceSend = false);

    void SetProfileInterval(int32_t interval) { mSendInterval = interval; }

    int32_t GetProfileInterval() { return mSendInterval; }

    static std::string mHostname;
    static std::string mIpAddr;
    static std::string mOsDetail;
    static std::string mUsername;
    static int32_t mSystemBootTime;

private:
    struct LogStoreStatistic {
        LogStoreStatistic(const std::string& configName,
                          const std::string& projectName,
                          const std::string& category,
                          const std::string& filename,
                          uint64_t readBytes = 0,
                          uint64_t skipBytes = 0,
                          uint64_t splitLines = 0,
                          uint64_t parseFailures = 0,
                          uint64_t regexMatchFailures = 0,
                          uint64_t parseTimeFailures = 0,
                          uint64_t historyFailures = 0,
                          uint64_t sendFailures = 0,
                          const std::string& errorLine = "")
            : mConfigName(configName),
              mProjectName(projectName),
              mCategory(category),
              mFilename(filename),
              mReadBytes(readBytes),
              mSkipBytes(skipBytes),
              mSplitLines(splitLines),
              mParseFailures(parseFailures),
              mRegexMatchFailures(regexMatchFailures),
              mParseTimeFailures(parseTimeFailures),
              mHistoryFailures(historyFailures),
              mSendFailures(sendFailures),
              mErrorLine(errorLine) {
            mLastUpdateTime = time(NULL);
            mFileDev = 0;
            mFileInode = 0;
            mFileSize = 0;
            mReadOffset = 0;
            mLastReadTime = 0;
            mReadCount = 0;
            mReadDelaySum = 0;
        }

        void Reset() {
            mFileDev = 0;
            mFileInode = 0;
            mFileSize = 0;
            mReadOffset = 0;
            mLastReadTime = 0;
            mReadCount = 0;
            mReadDelaySum = 0;
            mReadBytes = 0;
            mSkipBytes = 0;
            mSplitLines = 0;
            mParseFailures = 0;
            mRegexMatchFailures = 0;
            mParseTimeFailures = 0;
            mHistoryFailures = 0;
            mSendFailures = 0;
            mErrorLine.clear();
        }

        void
        UpdateReadInfo(uint64_t dev, uint64_t inode, uint64_t fileSize, uint64_t readOffset, int32_t lastReadTime) {
            mFileDev = dev;
            mFileInode = inode;
            mFileSize = fileSize;
            mReadOffset = readOffset;
            mLastReadTime = lastReadTime;
            ++mReadCount;
            mReadDelaySum += fileSize > readOffset ? fileSize - readOffset : 0;
        }

        std::string mConfigName;
        std::string mProjectName;
        std::string mCategory;
        std::string mFilename;
        // how many bytes processed
        uint64_t mReadBytes;
        // how many bytes skiped
        uint64_t mSkipBytes;
        // how many lines processed: mSplitLines
        // how many lines parse failed: mParseFailures
        // how many lines send failed: mSendFailures
        // how many lines succeed send: mSplitLines - mParseFailures - mSendFailures
        uint64_t mSplitLines;
        // how many lines parse fails (include all failures)
        uint64_t mParseFailures;
        // how many lines regex match fail(include boost crash or not match)
        uint64_t mRegexMatchFailures;
        // how many lines parse timeformat fail
        uint64_t mParseTimeFailures;
        // how many lines history data discarded
        uint64_t mHistoryFailures;
        // how many lines send fails
        uint64_t mSendFailures;
        // one sample error line
        std::string mErrorLine;
        int32_t mLastUpdateTime;

        uint64_t mFileDev;
        uint64_t mFileInode;
        uint64_t mFileSize;
        uint64_t mReadOffset;
        int32_t mLastReadTime;
        // ++mReadCount every call
        uint32_t mReadCount;
        // mReadDelaySum += mFileSize - mReadOffset every call
        // then average delay is mReadDelaySum / mReadCount
        uint64_t mReadDelaySum;
    };

    std::string mDumpFileName;
    std::string mBakDumpFileName;
    int32_t mLastSendTime;
    int32_t mSendInterval;
    // key : "project_name" + "_" + "category" + "_" + "filename"
    typedef std::unordered_map<std::string, LogStoreStatistic*> LogstoreSenderStatisticsMap;
    // key : region, value :unordered_map<std::string, LogStoreStatistic*>
    std::map<std::string, LogstoreSenderStatisticsMap*> mAllStatisticsMap;
    std::mutex mStatisticLock;
    ProfileSender mProfileSender;

    LogFileProfiler();
    ~LogFileProfiler() {}
    void DumpToLocal(int32_t curTime, bool forceSend, Json::Value& detail, Json::Value& logstore);
    void UpdateDumpData(const sls_logs::LogGroup& logGroup, Json::Value& detail, Json::Value& logstore);
    bool GetProfileData(sls_logs::LogGroup& logGroup, LogStoreStatistic* statistic);

    LogstoreSenderStatisticsMap* MakesureRegionStatisticsMapUnlocked(const std::string& region);

#ifdef APSARA_UNIT_TEST_MAIN
    friend class EventDispatcherTest;
    friend class SenderUnittest;

    uint64_t
    GetProfilingLines(const std::string& projectName, const std::string& category, const std::string& filename);
    void CleanEnviroments();
#endif
};

} // namespace logtail
