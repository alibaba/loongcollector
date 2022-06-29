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

#include "LogFileProfiler.h"
#include <string>
#include "common/version.h"
#include "common/Constants.h"
#include "common/StringTools.h"
#include "common/MachineInfoUtil.h"
#include "common/LogtailCommonFlags.h"
#include "common/RuntimeUtil.h"
#include "common/TimeUtil.h"
#include "common/ErrorUtil.h"
#include "log_pb/sls_logs.pb.h"
#include "logger/Logger.h"
#include "sender/Sender.h"
#include "config_manager/ConfigManager.h"
#include "app_config/AppConfig.h"

using namespace std;
using namespace sls_logs;

namespace logtail {

string LogFileProfiler::mHostname;
string LogFileProfiler::mIpAddr;
string LogFileProfiler::mOsDetail;
string LogFileProfiler::mUsername;
int32_t LogFileProfiler::mSystemBootTime = -1;

LogFileProfiler::LogFileProfiler() {
    srand(time(NULL));
    mSendInterval = INT32_FLAG(profile_data_send_interval);
    mLastSendTime = time(NULL) - (rand() % (mSendInterval / 10)) * 10;
    mDumpFileName = GetProcessExecutionDir() + STRING_FLAG(logtail_profile_snapshot);
    mBakDumpFileName = GetProcessExecutionDir() + STRING_FLAG(logtail_profile_snapshot) + "_bak";

    mHostname = GetHostName();
#if defined(_MSC_VER)
    mHostname = EncodingConverter::GetInstance()->FromACPToUTF8(mHostname);
#endif
    mIpAddr = GetHostIp();
    mOsDetail = GetOsDetail();
    mUsername = GetUsername();
}

bool LogFileProfiler::GetProfileData(LogGroup& logGroup, LogStoreStatistic* statistic) {
    if (statistic->mReadBytes + statistic->mSkipBytes == 0)
        return false;

    Log* logPtr = logGroup.add_logs();
    logPtr->set_time(AppConfig::GetInstance()->EnableLogTimeAutoAdjust() ? time(NULL) + GetTimeDelta() : time(NULL));
    Log_Content* contentPtr = logPtr->add_contents();
    contentPtr->set_key("logreader_project_name");
    contentPtr->set_value(statistic->mProjectName);
    contentPtr = logPtr->add_contents();
    contentPtr->set_key("category");
    contentPtr->set_value(statistic->mCategory);
    contentPtr = logPtr->add_contents();
    contentPtr->set_key("config_name");
    contentPtr->set_value(statistic->mConfigName);
    contentPtr = logPtr->add_contents();
    contentPtr->set_key("file_name");
    contentPtr->set_value(statistic->mFilename.empty() ? "logstore_statistics" : statistic->mFilename);
    contentPtr = logPtr->add_contents();
    contentPtr->set_key("logtail_version");
    contentPtr->set_value(ILOGTAIL_VERSION);
    contentPtr = logPtr->add_contents();
    contentPtr->set_key("source_ip");
    contentPtr->set_value(mIpAddr);
    contentPtr = logPtr->add_contents();
    contentPtr->set_key("os");
    contentPtr->set_value(OS_NAME);
    contentPtr = logPtr->add_contents();
    contentPtr->set_key("total_bytes");
    contentPtr->set_value(ToString(statistic->mReadBytes));
    contentPtr = logPtr->add_contents();
    contentPtr->set_key("skip_bytes");
    contentPtr->set_value(ToString(statistic->mSkipBytes));
    contentPtr = logPtr->add_contents();
    contentPtr->set_key("succeed_lines");
    contentPtr->set_value(ToString(statistic->mSplitLines - statistic->mParseFailures - statistic->mSendFailures));
    contentPtr = logPtr->add_contents();
    contentPtr->set_key("parse_failures");
    contentPtr->set_value(ToString(statistic->mParseFailures));
    contentPtr = logPtr->add_contents();
    contentPtr->set_key("send_failures");
    contentPtr->set_value(ToString(statistic->mSendFailures));
    contentPtr = logPtr->add_contents();
    contentPtr->set_key("regex_match_failures");
    contentPtr->set_value(ToString(statistic->mRegexMatchFailures));
    contentPtr = logPtr->add_contents();
    contentPtr->set_key("history_data_failures");
    contentPtr->set_value(ToString(statistic->mHistoryFailures));
    contentPtr = logPtr->add_contents();
    contentPtr->set_key("time_format_failures");
    contentPtr->set_value(ToString(statistic->mParseTimeFailures));
    contentPtr = logPtr->add_contents();
    contentPtr->set_key("file_dev");
    contentPtr->set_value(ToString(statistic->mFileDev));
    contentPtr = logPtr->add_contents();
    contentPtr->set_key("file_inode");
    contentPtr->set_value(ToString(statistic->mFileInode));
    contentPtr = logPtr->add_contents();
    contentPtr->set_key("last_read_time");
    contentPtr->set_value(ToString(statistic->mLastReadTime));
    contentPtr = logPtr->add_contents();
    contentPtr->set_key("read_count");
    contentPtr->set_value(ToString(statistic->mReadCount));
    contentPtr = logPtr->add_contents();
    contentPtr->set_key("file_size");
    contentPtr->set_value(ToString(statistic->mFileSize));
    contentPtr = logPtr->add_contents();
    contentPtr->set_key("read_offset");
    contentPtr->set_value(ToString(statistic->mReadOffset));
    contentPtr = logPtr->add_contents();
    contentPtr->set_key("read_avg_delay");
    contentPtr->set_value(ToString(statistic->mReadCount == 0 ? 0 : statistic->mReadDelaySum / statistic->mReadCount));

    if (!statistic->mErrorLine.empty()) {
        contentPtr = logPtr->add_contents();
        contentPtr->set_key("error_line");
        contentPtr->set_value(statistic->mErrorLine);
    }

    // get logstore send info
    if (statistic->mFilename.empty()) {
        LogstoreFeedBackKey fbKey = GenerateLogstoreFeedBackKey(statistic->mProjectName, statistic->mCategory);
        LogstoreSenderStatistics senderStatistics = Sender::Instance()->GetSenderStatistics(fbKey);

        contentPtr = logPtr->add_contents();
        contentPtr->set_key("max_unsend_time");
        contentPtr->set_value(ToString(senderStatistics.mMaxUnsendTime));
        contentPtr = logPtr->add_contents();
        contentPtr->set_key("min_unsend_time");
        contentPtr->set_value(ToString(senderStatistics.mMinUnsendTime));
        contentPtr = logPtr->add_contents();
        contentPtr->set_key("max_send_success_time");
        contentPtr->set_value(ToString(senderStatistics.mMaxSendSuccessTime));
        contentPtr = logPtr->add_contents();
        contentPtr->set_key("send_queue_size");
        contentPtr->set_value(ToString(senderStatistics.mSendQueueSize));
        contentPtr = logPtr->add_contents();
        contentPtr->set_key("send_network_error");
        contentPtr->set_value(ToString(senderStatistics.mSendNetWorkErrorCount));
        contentPtr = logPtr->add_contents();
        contentPtr->set_key("send_quota_error");
        contentPtr->set_value(ToString(senderStatistics.mSendQuotaErrorCount));
        contentPtr = logPtr->add_contents();
        contentPtr->set_key("send_discard_error");
        contentPtr->set_value(ToString(senderStatistics.mSendDiscardErrorCount));
        contentPtr = logPtr->add_contents();
        contentPtr->set_key("send_success_count");
        contentPtr->set_value(ToString(senderStatistics.mSendSuccessCount));
        contentPtr = logPtr->add_contents();
        contentPtr->set_key("send_block_flag");
        contentPtr->set_value(ToString(senderStatistics.mSendBlockFlag));
        contentPtr = logPtr->add_contents();
        contentPtr->set_key("sender_valid_flag");
        contentPtr->set_value(ToString(senderStatistics.mValidToSendFlag));
    }

    return true;
}

LogFileProfiler::LogstoreSenderStatisticsMap*
LogFileProfiler::MakesureRegionStatisticsMapUnlocked(const string& region) {
    // @todo
    //string region;
    std::map<std::string, LogstoreSenderStatisticsMap*>::iterator iter = mAllStatisticsMap.find(region);
    if (iter != mAllStatisticsMap.end()) {
        return iter->second;
    }
    LogstoreSenderStatisticsMap* pMap = new LogstoreSenderStatisticsMap;
    mAllStatisticsMap.insert(std::pair<std::string, LogstoreSenderStatisticsMap*>(region, pMap));
    return pMap;
}

void LogFileProfiler::SendProfileData(bool forceSend) {
    int32_t curTime = time(NULL);
    if (!forceSend && (curTime - mLastSendTime < mSendInterval))
        return;
    size_t sendRegionIndex = 0;
    Json::Value detail;
    Json::Value logstore;
    do {
        LogGroup logGroup;
        logGroup.set_category("shennong_log_profile");
        logGroup.set_source(LogFileProfiler::mIpAddr);
        string region;
        {
            // only lock statisticsMap, not sender or dump
            std::lock_guard<std::mutex> lock(mStatisticLock);
            if (mAllStatisticsMap.size() <= sendRegionIndex) {
                break;
            }

            size_t iterIndex = 0;
            std::map<std::string, LogstoreSenderStatisticsMap*>::iterator iter = mAllStatisticsMap.begin();
            while (iterIndex != sendRegionIndex) {
                ++iterIndex;
                ++iter;
            }

            ++sendRegionIndex;
            region = iter->first;
            LogstoreSenderStatisticsMap& statisticsMap = *(iter->second);
            if (statisticsMap.size() > (size_t)0) {
                std::unordered_map<string, LogStoreStatistic*>::iterator iter = statisticsMap.begin();
                for (; iter != statisticsMap.end();) {
                    GetProfileData(logGroup, iter->second);
                    if ((curTime - iter->second->mLastUpdateTime) > mSendInterval * 3) {
                        delete iter->second;
                        iter = statisticsMap.erase(iter);
                    } else {
                        iter->second->Reset();
                        iter++;
                    }
                }
            }
        }
        UpdateDumpData(logGroup, detail, logstore);
        mProfileSender.SendToProfileProject(region, logGroup);
    } while (true);
    DumpToLocal(curTime, forceSend, detail, logstore);
    mLastSendTime = curTime;
}

void LogFileProfiler::AddProfilingData(const std::string& configName,
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
                                       const std::string& errorLine) {
    if (filename.size() > (size_t)0) {
        // logstore statistics
        AddProfilingData(configName,
                         region,
                         projectName,
                         category,
                         "",
                         readBytes,
                         skipBytes,
                         splitLines,
                         parseFailures,
                         regexMatchFailures,
                         parseTimeFailures,
                         historyFailures,
                         sendFailures,
                         "");
    }
    string key = projectName + "_" + category + "_" + filename;
    std::lock_guard<std::mutex> lock(mStatisticLock);
    LogstoreSenderStatisticsMap& statisticsMap = *MakesureRegionStatisticsMapUnlocked(region);
    std::unordered_map<string, LogStoreStatistic*>::iterator iter = statisticsMap.find(key);
    if (iter != statisticsMap.end()) {
        (iter->second)->mReadBytes += readBytes;
        (iter->second)->mSkipBytes += skipBytes;
        (iter->second)->mSplitLines += splitLines;
        (iter->second)->mParseFailures += parseFailures;
        (iter->second)->mRegexMatchFailures += regexMatchFailures;
        (iter->second)->mParseTimeFailures += parseTimeFailures;
        (iter->second)->mHistoryFailures += historyFailures;
        (iter->second)->mSendFailures += sendFailures;
        if ((iter->second)->mErrorLine.empty())
            (iter->second)->mErrorLine = errorLine;
        (iter->second)->mLastUpdateTime = time(NULL);
    } else
        statisticsMap.insert(std::pair<string, LogStoreStatistic*>(key,
                                                                   new LogStoreStatistic(configName,
                                                                                         projectName,
                                                                                         category,
                                                                                         filename,
                                                                                         readBytes,
                                                                                         skipBytes,
                                                                                         splitLines,
                                                                                         parseFailures,
                                                                                         regexMatchFailures,
                                                                                         parseTimeFailures,
                                                                                         historyFailures,
                                                                                         sendFailures,
                                                                                         errorLine)));
}

void LogFileProfiler::AddProfilingSkipBytes(const std::string& configName,
                                            const std::string& region,
                                            const std::string& projectName,
                                            const std::string& category,
                                            const std::string& filename,
                                            uint64_t skipBytes) {
    if (filename.size() > (size_t)0) {
        // logstore statistics
        AddProfilingSkipBytes(configName, region, projectName, category, "", skipBytes);
    }
    string key = projectName + "_" + category + "_" + filename;
    std::lock_guard<std::mutex> lock(mStatisticLock);
    LogstoreSenderStatisticsMap& statisticsMap = *MakesureRegionStatisticsMapUnlocked(region);
    std::unordered_map<string, LogStoreStatistic*>::iterator iter = statisticsMap.find(key);
    if (iter != statisticsMap.end()) {
        (iter->second)->mSkipBytes += skipBytes;
        (iter->second)->mLastUpdateTime = time(NULL);
    } else {
        LogStoreStatistic* statistic = new LogStoreStatistic(configName, projectName, category, filename);
        statistic->mSkipBytes += skipBytes;
        statisticsMap.insert(std::pair<string, LogStoreStatistic*>(key, statistic));
    }
}

void LogFileProfiler::AddProfilingReadBytes(const std::string& configName,
                                            const std::string& region,
                                            const std::string& projectName,
                                            const std::string& category,
                                            const std::string& filename,
                                            uint64_t dev,
                                            uint64_t inode,
                                            uint64_t fileSize,
                                            uint64_t readOffset,
                                            int32_t lastReadTime) {
    if (filename.size() > (size_t)0) {
        // logstore statistics
        AddProfilingReadBytes(
            configName, region, projectName, category, "", dev, inode, fileSize, readOffset, lastReadTime);
    }
    string key = projectName + "_" + category + "_" + filename;
    std::lock_guard<std::mutex> lock(mStatisticLock);
    LogstoreSenderStatisticsMap& statisticsMap = *MakesureRegionStatisticsMapUnlocked(region);
    std::unordered_map<string, LogStoreStatistic*>::iterator iter = statisticsMap.find(key);
    if (iter != statisticsMap.end()) {
        (iter->second)->UpdateReadInfo(dev, inode, fileSize, readOffset, lastReadTime);
    } else {
        LogStoreStatistic* statistic = new LogStoreStatistic(configName, projectName, category, filename);
        statistic->UpdateReadInfo(dev, inode, fileSize, readOffset, lastReadTime);
        statisticsMap.insert(std::pair<string, LogStoreStatistic*>(key, statistic));
    }
}

void LogFileProfiler::DumpToLocal(int32_t curTime, bool forceSend, Json::Value& detail, Json::Value& logstore) {
    Json::Value root;
    root["version"] = ILOGTAIL_VERSION;
    root["ip"] = mIpAddr;
    root["begin_time"] = (Json::UInt64)mLastSendTime;
    root["begin_time_readable"] = GetTimeStamp(mLastSendTime, "%Y-%m-%d %H:%M:%S");
    root["end_time"] = (Json::UInt64)curTime;
    root["end_time_readable"] = GetTimeStamp(curTime, "%Y-%m-%d %H:%M:%S");


    root["detail"] = detail;
    root["logstore"] = logstore;
    string styledRoot = root.toStyledString();
    if (forceSend) {
        FILE* pFile = fopen(mBakDumpFileName.c_str(), "w");
        if (pFile == NULL) {
            LOG_ERROR(sLogger, ("open file failed", mBakDumpFileName)("errno", errno));
            return;
        }
        fwrite(styledRoot.c_str(), 1, styledRoot.size(), pFile);
        fclose(pFile);
#if defined(_MSC_VER)
        remove(mDumpFileName.c_str());
#endif
        if (rename(mBakDumpFileName.c_str(), mDumpFileName.c_str()) == -1)
            LOG_INFO(sLogger,
                     ("rename profile snapshot fail, file", mDumpFileName)("error", ErrnoToString(GetErrno())));
    }

    static auto gProfileLogger = Logger::Instance().GetLogger("/apsara/sls/ilogtail/profile");
    LOG_INFO(gProfileLogger, ("\n", styledRoot));
}

void LogFileProfiler::UpdateDumpData(const sls_logs::LogGroup& logGroup, Json::Value& detail, Json::Value& logstore) {
    for (int32_t logIdx = 0; logIdx < logGroup.logs_size(); ++logIdx) {
        Json::Value category;
        const Log& log = logGroup.logs(logIdx);
        bool logstoreFlag = false;
        for (int32_t conIdx = 0; conIdx < log.contents_size(); ++conIdx) {
            const Log_Content& content = log.contents(conIdx);
            const string& key = content.key();
            const string& value = content.value();
            if (key == "logreader_project_name")
                category["project"] = value;
            else if (key == "category")
                category["logstore"] = value;
            else if (key == "config_name")
                category["config_name"] = value;
            else if (key == "file_name") {
                category["file"] = value;
                if (value == "logstore_statistics") {
                    logstoreFlag = true;
                }
            } else if (key == "total_bytes")
                category["read_bytes"] = value;
            else if (key == "skip_bytes")
                category["skip_bytes"] = value;
            else if (key == "succeed_lines")
                category["split_lines"] = value;
            else if (key == "parse_failures")
                category["parse_fail_lines"] = value;
            else if (key == "file_dev")
                category["file_dev"] = value;
            else if (key == "file_inode")
                category["file_inode"] = value;
            else if (key == "last_read_time")
                category["last_read_time"] = value;
            else if (key == "read_count")
                category["read_count"] = value;
            else if (key == "file_size")
                category["file_size"] = value;
            else if (key == "read_offset")
                category["read_offset"] = value;
            else if (key == "read_avg_delay")
                category["read_avg_delay"] = value;
            else if (key == "max_unsend_time")
                category["max_unsend_time"] = value;
            else if (key == "min_unsend_time")
                category["min_unsend_time"] = value;
            else if (key == "max_send_success_time")
                category["max_send_success_time"] = value;
            else if (key == "send_queue_size")
                category["send_queue_size"] = value;
            else if (key == "send_network_error")
                category["send_network_error"] = value;
            else if (key == "send_quota_error")
                category["send_quota_error"] = value;
            else if (key == "send_discard_error")
                category["send_discard_error"] = value;
            else if (key == "send_success_count")
                category["send_success_count"] = value;
            else if (key == "sender_valid_flag")
                category["sender_valid_flag"] = value;
            else if (key == "send_block_flag")
                category["send_block_flag"] = value;
        }
        if (logstoreFlag) {
            logstore.append(category);
        } else {
            detail.append(category);
        }
    }
}

#ifdef APSARA_UNIT_TEST_MAIN
uint64_t LogFileProfiler::GetProfilingLines(const std::string& projectName,
                                            const std::string& category,
                                            const std::string& filename) {
    std::string key = projectName + "_" + category + "_" + filename;
    std::lock_guard<std::mutex> lock(mStatisticLock);
    if (mAllStatisticsMap.size() != (size_t)1) {
        return 0;
    }
    LogstoreSenderStatisticsMap statisticMap = *(mAllStatisticsMap.begin()->second);
    std::unordered_map<std::string, LogStoreStatistic*>::iterator iter = statisticMap.find(key);
    if (iter == statisticMap.end())
        return 0;
    else
        return (iter->second->mSplitLines - iter->second->mParseFailures);
}

void LogFileProfiler::CleanEnviroments() {
    std::lock_guard<std::mutex> lock(mStatisticLock);
    // just for test, memory leaks
    mAllStatisticsMap.clear();
}
#endif

} // namespace logtail
