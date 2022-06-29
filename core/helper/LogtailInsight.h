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
#include <vector>
#include <map>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sstream>
#include <iostream>
#include <sys/types.h>
#include <math.h>
#include <algorithm>
#include <json/json.h>
#include <linux/limits.h>
#include "common/FileSystemUtil.h"
#include "common/RuntimeUtil.h"

using namespace std;

namespace logtail {

struct LogStoreStatistic {
    LogStoreStatistic(uint64_t readBytes = 0,
                      uint64_t skipBytes = 0,
                      uint64_t splitLines = 0,
                      uint64_t parseFailures = 0,
                      uint64_t regexMatchFailures = 0,
                      uint64_t parseTimeFailures = 0,
                      uint64_t historyFailures = 0,
                      uint64_t sendFailures = 0,
                      const std::string& errorLine = "")
        : mReadBytes(readBytes),
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
        mReadDelayAvg = 0;
        mMaxUnsendTime = 0;
        mMinUnsendTime = 0;
        mMaxSendSuccessTime = 0;
        mSendQueueSize = 0;
        mSendNetWorkErrorCount = 0;
        mSendQuotaErrorCount = 0;
        mSendDiscardErrorCount = 0;
        mSendSuccessCount = 0;
        mValidToSendFlag = false;
        mSendBlockFlag = false;
    }

    void Reset() {
        mFileDev = 0;
        mFileInode = 0;
        mFileSize = 0;
        mReadOffset = 0;
        mLastReadTime = 0;
        mReadCount = 0;
        mReadDelayAvg = 0;
        mReadBytes = 0;
        mSkipBytes = 0;
        mSplitLines = 0;
        mParseFailures = 0;
        mRegexMatchFailures = 0;
        mParseTimeFailures = 0;
        mHistoryFailures = 0;
        mSendFailures = 0;
        mErrorLine.clear();

        mMaxUnsendTime = 0;
        mMinUnsendTime = 0;
        mMaxSendSuccessTime = 0;
        mSendQueueSize = 0;
        mSendNetWorkErrorCount = 0;
        mSendQuotaErrorCount = 0;
        mSendDiscardErrorCount = 0;
        mSendSuccessCount = 0;
        mValidToSendFlag = false;
        mSendBlockFlag = false;
    }

    void UpdateReadInfo(uint64_t dev, uint64_t inode, uint64_t fileSize, uint64_t readOffset, int32_t lastReadTime) {
        mFileDev = dev;
        mFileInode = inode;
        mFileSize = fileSize;
        mReadOffset = readOffset;
        mLastReadTime = lastReadTime;
        ++mReadCount;
        mReadDelayAvg += fileSize > readOffset ? fileSize - readOffset : 0;
    }

    std::string mProjectName;
    std::string mCategory;
    std::string mConfigName;
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
    uint64_t mReadDelayAvg;

    string mStatus;

    int32_t mMaxUnsendTime;
    int32_t mMinUnsendTime;
    int32_t mMaxSendSuccessTime;
    uint32_t mSendQueueSize;
    uint32_t mSendNetWorkErrorCount;
    uint32_t mSendQuotaErrorCount;
    uint32_t mSendDiscardErrorCount;
    uint32_t mSendSuccessCount;
    bool mSendBlockFlag;
    bool mValidToSendFlag;
};

template <typename T>
string ToString(const T& val) {
    stringstream ss;
    ss << val;
    return ss.str();
}

template <>
string ToString(const bool& val) {
    return val ? "true" : "false";
}

int min(int l, int r) {
    return l <= r ? l : r;
}

string ToWithString(string str, int with) {
    int deltaSize = with - str.size();
    if (deltaSize > 0) {
        string rst;
        rst.append((size_t)deltaSize, ' ');
        rst.append(str);
        return rst;
    }
    return str;
}

string ToHumanReadableByteCount(int64_t bytes) {
    // Static lookup table of byte-based SI units
    static const char* suffix[][2] = {{"B", "B"},
                                      {"kB", "KiB"},
                                      {"MB", "MiB"},
                                      {"GB", "GiB"},
                                      {"TB", "TiB"},
                                      {"EB", "EiB"},
                                      {"ZB", "ZiB"},
                                      {"YB", "YiB"}};
    int unit = 1024;
    double coeff = 0.f;
    int exp = 0;
    char rst[32];
    if (bytes > 0) {
        exp = min((int)(log(bytes) / log(unit)), (int)sizeof(suffix) / sizeof(suffix[0]) - 1);
    }
    coeff = bytes / pow(unit, exp);
    if (exp == 0) {
        sprintf(rst, "%d%s", (int)bytes, suffix[exp][0]);
    } else {
        sprintf(rst, "%.2f%s", coeff, suffix[exp][0]);
    }
    return rst;
}

// Create by david zhang. 2017/08/08 10:28:31
class LogtailInsight {
public:
    LogtailInsight();
    ~LogtailInsight();

public:
    static LogtailInsight* GetInstance() {
        if (s_instance == NULL) {
            s_instance = new LogtailInsight;
        }
        return s_instance;
    }

    static void FinalizeInstance() {
        if (s_instance != NULL) {
            delete s_instance;
            s_instance = NULL;
        }
    }

    int ExecuteCommand(const string& serverAddress,
                       const string& cmdType,
                       const std::vector<string>& paramList,
                       bool jsonOutPut,
                       bool detail);
    static void* ForcedExitTimer(void* arg);

    int64_t JsonValueToInt64(const Json::Value& category) {
        int64_t val = 0;
        stringstream ss;
        ss << category.asString();
        ss >> val;
        return val;
    }


    void JsonToLogStoreStatistic(Json::Value& category, bool logstoreFlag) {
        LogStoreStatistic* pStatistics = new LogStoreStatistic();
        if (category.isMember("project"))
            pStatistics->mProjectName = category["project"].asString();
        if (category.isMember("logstore"))
            pStatistics->mCategory = category["logstore"].asString();
        if (category.isMember("config_name"))
            pStatistics->mConfigName = category["config_name"].asString();
        if (category.isMember("file")) {
            pStatistics->mFilename = category["file"].asString();
        }
        if (category.isMember("read_bytes"))
            pStatistics->mReadBytes = JsonValueToInt64(category["read_bytes"]);
        if (category.isMember("skip_bytes"))
            pStatistics->mSkipBytes = JsonValueToInt64(category["skip_bytes"]);
        if (category.isMember("split_lines"))
            pStatistics->mSplitLines = JsonValueToInt64(category["split_lines"]);
        if (category.isMember("parse_fail_lines"))
            pStatistics->mParseFailures = JsonValueToInt64(category["parse_fail_lines"]);
        if (category.isMember("file_dev"))
            pStatistics->mFileDev = JsonValueToInt64(category["file_dev"]);
        if (category.isMember("file_inode"))
            pStatistics->mFileInode = JsonValueToInt64(category["file_inode"]);
        if (category.isMember("last_read_time"))
            pStatistics->mLastReadTime = JsonValueToInt64(category["last_read_time"]);
        if (category.isMember("read_count"))
            pStatistics->mReadCount = JsonValueToInt64(category["read_count"]);
        if (category.isMember("file_size"))
            pStatistics->mFileSize = JsonValueToInt64(category["file_size"]);
        if (category.isMember("read_offset"))
            pStatistics->mReadOffset = JsonValueToInt64(category["read_offset"]);
        if (category.isMember("read_avg_delay"))
            pStatistics->mReadDelayAvg = JsonValueToInt64(category["read_avg_delay"]);
        if (category.isMember("max_unsend_time"))
            pStatistics->mMaxUnsendTime = JsonValueToInt64(category["max_unsend_time"]);
        if (category.isMember("min_unsend_time"))
            pStatistics->mMinUnsendTime = JsonValueToInt64(category["min_unsend_time"]);
        if (category.isMember("max_send_success_time"))
            pStatistics->mMaxSendSuccessTime = JsonValueToInt64(category["max_send_success_time"]);
        if (category.isMember("send_queue_size"))
            pStatistics->mSendQueueSize = JsonValueToInt64(category["send_queue_size"]);
        if (category.isMember("send_network_error"))
            pStatistics->mSendNetWorkErrorCount = JsonValueToInt64(category["send_network_error"]);
        if (category.isMember("send_quota_error"))
            pStatistics->mSendQuotaErrorCount = JsonValueToInt64(category["send_quota_error"]);
        if (category.isMember("send_discard_error"))
            pStatistics->mSendDiscardErrorCount = JsonValueToInt64(category["send_discard_error"]);
        if (category.isMember("send_success_count"))
            pStatistics->mSendSuccessCount = JsonValueToInt64(category["send_success_count"]);
        if (category.isMember("send_block_flag"))
            pStatistics->mSendBlockFlag = category["send_block_flag"].asString() == "true";
        if (category.isMember("sender_valid_flag"))
            pStatistics->mValidToSendFlag = category["sender_valid_flag"].asString() == "true";

        pStatistics->mStatus = "ok";

        if (logstoreFlag) {
            if (pStatistics->mReadDelayAvg > 10 * 1024 * 104) {
                pStatistics->mStatus = "process_block";
            }
            if (pStatistics->mParseFailures > 0) {
                pStatistics->mStatus = "parse_fail";
            }
            if (pStatistics->mMinUnsendTime > 0 && mTimeValue - pStatistics->mMinUnsendTime > 10) {
                pStatistics->mStatus = "send_block";
            }
            if (pStatistics->mMinUnsendTime > 0 && mTimeValue - pStatistics->mMinUnsendTime > 60) {
                pStatistics->mStatus = "sender_invalid";
            }
            if (pStatistics->mValidToSendFlag == false) {
                pStatistics->mStatus = "sender_invalid";
            }

            //cout << "add, project :" << pStatistics->mProjectName << " logstore :" << pStatistics->mCategory << endl;
            mLogstoreProfileMap[pStatistics->mProjectName + '#' + pStatistics->mCategory] = pStatistics;
        } else {
            if (pStatistics->mReadDelayAvg > 10 * 1024 * 104) {
                pStatistics->mStatus = "process_block";
            }
            if (pStatistics->mParseFailures > 0) {
                pStatistics->mStatus = "parse_fail";
            }
            //cout << "add, project :" << pStatistics->mProjectName << " logstore :" << pStatistics->mCategory << " file :" << pStatistics->mFilename << endl;
            mFileProfileMap[pStatistics->mProjectName + '#' + pStatistics->mCategory + '#' + pStatistics->mFilename]
                = pStatistics;
        }
    }

    bool StringToJson(const string& fileContent, Json::Value& valueJson) {
        Json::Reader jsonReader;
        return jsonReader.parse(fileContent, valueJson);
    }

    string GetProcessExecutionDir(void) {
        char exePath[PATH_MAX + 1] = {0};
        readlink("/proc/self/exe", exePath, sizeof(exePath));
        string fullPath(exePath);
        size_t index = fullPath.rfind("/");
        if (index == string::npos) {
            return string();
        }
        return fullPath.substr(0, index + 1);
    }

    time_t StringToDatetime(string str) {
        char* cha = (char*)str.data();
        tm tm_;
        int year = 1900;
        int month = 1;
        int day = 0;
        int hour = 0;
        int minute = 0;
        int second = 0;
        if (6 != sscanf(cha, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second)) {
            cout << string("cast time fail, " + str) << endl;
            return 0;
        }
        tm_.tm_year = year - 1900;
        tm_.tm_mon = month - 1;
        tm_.tm_mday = day;
        tm_.tm_hour = hour;
        tm_.tm_min = minute;
        tm_.tm_sec = second;
        tm_.tm_isdst = 0;
        time_t t_ = mktime(&tm_);
        return t_;
    }

    string DateTimeToString(time_t timeVal) {
        struct tm time_info;
        char timeString[32]; // space for "HH:MM:SS\0"

        localtime_r(&timeVal, &time_info);

        strftime(timeString, sizeof(timeString), "%y-%m-%d %H:%M:%S", &time_info);
        return timeString;
    }

    int StringToStatus(const string& fileContent) {
        stringstream ss(fileContent);
        while (!ss.eof()) {
            string line;
            string key;
            string val;
            getline(ss, line);
            if (line.size() == 0)
                continue;
            if (line[0] == '#') {
                continue;
            }
            size_t sept = line.find(':');
            if (sept == string::npos || sept == line.size() - 1) {
                continue;
            }

            key = line.substr(0, sept);
            val = line.substr(sept + 1);
            mStatusMap[key] = val;
            //cout << "key value : " << key << val << endl;
        }
        return 0;
    }

    string GetLogtailStatus() {
        return mStatusMap.find("status") != mStatusMap.end() ? mStatusMap["status"] : string("UnknowError");
    }


    string GetLogtailDetail() {
        string rstStr;
        for (map<string, string>::iterator iter = mStatusMap.begin(); iter != mStatusMap.end(); ++iter) {
            rstStr += iter->first + " : " + iter->second + "\n";
        }
        rstStr += string("time_readable") + " : " + mTimeString + "\n";
        rstStr += string("time") + " : " + ToString(mTimeValue);
        return rstStr;
    }

    string ListAllLogstore() {
        string rst;
        map<string, LogStoreStatistic*>::iterator iter = mLogstoreProfileMap.begin();
        for (; iter != mLogstoreProfileMap.end(); ++iter) {
            rst += iter->second->mProjectName + " : " + iter->second->mCategory + "\n";
        }
        return rst;
    }

    string listAllFiles(string project, string logstore) {
        string rst;
        map<string, LogStoreStatistic*>::iterator iter = mFileProfileMap.begin();
        for (; iter != mFileProfileMap.end(); ++iter) {
            if (iter->second->mProjectName == project && iter->second->mCategory == logstore) {
                rst += iter->second->mFilename + "\n";
            }
        }
        return rst;
    }

    string GetLogStoreProfileJson(string project, string logstore) {
        Json::Value rstValue;
        string key = project + '#' + logstore;
        map<string, LogStoreStatistic*>::iterator iter = mLogstoreProfileMap.find(key);
        if (iter == mLogstoreProfileMap.end()) {
            //rstValue["find"] = Json::Value(false);
            return rstValue.toStyledString();
        }
        LogStoreStatistic* pStatistics = iter->second;
        //rstValue["find"] = Json::Value(true);
        rstValue["time_begin_readable"] = Json::Value(DateTimeToString(mTimeValue - 600));
        rstValue["time_end_readable"] = Json::Value(DateTimeToString(mTimeValue));
        rstValue["time_begin"] = Json::Int(mTimeValue - 600);
        rstValue["time_end"] = Json::Value(mTimeValue);

        rstValue["project"] = Json::Value(pStatistics->mProjectName);
        rstValue["logstore"] = Json::Value(pStatistics->mCategory);
        rstValue["config"] = Json::Value(pStatistics->mConfigName);
        rstValue["read_bytes"] = Json::Int64(pStatistics->mReadBytes);
        rstValue["parse_success_lines"] = Json::Int64(pStatistics->mSplitLines);
        rstValue["parse_fail_lines"] = Json::Int64(pStatistics->mParseFailures);
        rstValue["last_read_time"] = Json::Int64(pStatistics->mLastReadTime);
        rstValue["read_count"] = Json::Int64(pStatistics->mReadCount);
        rstValue["avg_delay_bytes"] = Json::Int64(pStatistics->mReadDelayAvg);

        rstValue["max_unsend_time"] = Json::Int64(pStatistics->mMaxUnsendTime);
        rstValue["min_unsend_time"] = Json::Int64(pStatistics->mMinUnsendTime);
        rstValue["max_send_success_time"] = Json::Int64(pStatistics->mMaxSendSuccessTime);
        rstValue["send_queue_size"] = Json::Int64(pStatistics->mSendQueueSize);
        rstValue["send_network_error_count"] = Json::Int64(pStatistics->mSendNetWorkErrorCount);
        rstValue["send_network_quota_count"] = Json::Int64(pStatistics->mSendQuotaErrorCount);
        rstValue["send_network_discard_count"] = Json::Int64(pStatistics->mSendDiscardErrorCount);
        rstValue["send_success_count"] = Json::Int64(pStatistics->mSendSuccessCount);
        rstValue["send_block_flag"] = Json::Value(pStatistics->mSendBlockFlag);
        rstValue["sender_valid_flag"] = Json::Value(pStatistics->mValidToSendFlag);
        rstValue["status"] = Json::Value(pStatistics->mStatus);


        return rstValue.toStyledString();
    }

    string GetLogStoreProfileSimple(string project, string logstore) {
        string key = project + '#' + logstore;
        map<string, LogStoreStatistic*>::iterator iter = mLogstoreProfileMap.find(key);
        if (iter == mLogstoreProfileMap.end()) {
            return "";
            //return "find : false";
        }
        LogStoreStatistic* pStatistics = iter->second;
        //string rstStr = "find : true";
        string rstStr = "time_begin_readable : " + DateTimeToString(mTimeValue - 600);
        rstStr += "\ntime_end_readable : " + DateTimeToString(mTimeValue);
        rstStr += "\ntime_begin : " + ToString(mTimeValue - 600);
        rstStr += "\ntime_end : " + ToString(mTimeValue);
        rstStr += "\nproject : " + ToString(pStatistics->mProjectName);
        rstStr += "\nlogstore : " + ToString(pStatistics->mCategory);
        rstStr += "\nstatus : " + ToString(pStatistics->mStatus);
        rstStr += "\nconfig : " + ToString(pStatistics->mConfigName);
        rstStr += "\nread_bytes : " + ToString(pStatistics->mReadBytes);
        rstStr += "\nparse_success_lines : " + ToString(pStatistics->mSplitLines);
        rstStr += "\nparse_fail_lines : " + ToString(pStatistics->mParseFailures);
        rstStr += "\nlast_read_time : " + ToString(pStatistics->mLastReadTime);
        rstStr += "\nread_count : " + ToString(pStatistics->mReadCount);
        rstStr += "\navg_delay_bytes : " + ToString(pStatistics->mReadDelayAvg);
        rstStr += "\nmax_unsend_time : " + ToString(pStatistics->mMaxUnsendTime);
        rstStr += "\nmin_unsend_time : " + ToString(pStatistics->mMinUnsendTime);
        rstStr += "\nmax_send_success_time : " + ToString(pStatistics->mMaxSendSuccessTime);
        rstStr += "\nsend_queue_size : " + ToString(pStatistics->mSendQueueSize);
        rstStr += "\nsend_network_error_count : " + ToString(pStatistics->mSendNetWorkErrorCount);
        rstStr += "\nsend_network_quota_count : " + ToString(pStatistics->mSendQuotaErrorCount);
        rstStr += "\nsend_network_discard_count : " + ToString(pStatistics->mSendDiscardErrorCount);
        rstStr += "\nsend_success_count : " + ToString(pStatistics->mSendSuccessCount);
        rstStr += "\nsend_block_flag : " + ToString(pStatistics->mSendBlockFlag);
        rstStr += "\nsender_valid_flag : " + ToString(pStatistics->mValidToSendFlag);

        return rstStr;
    }

    string GetLogStoreProfileOneLine(string project, string logstore) {
        string key = project + '#' + logstore;
        map<string, LogStoreStatistic*>::iterator iter = mLogstoreProfileMap.find(key);
        if (iter == mLogstoreProfileMap.end()) {
            return "";
            //return "find : false";
        }
        LogStoreStatistic* pStatistics = iter->second;
        //string rstStr = "find : true";
        string rstStr = ToWithString(DateTimeToString(mTimeValue - 600), 18);
        rstStr += "  " + ToWithString(pStatistics->mStatus, 14);
        rstStr += "  " + ToWithString(ToHumanReadableByteCount(pStatistics->mReadBytes), 8);
        rstStr += "  " + ToWithString(ToString(pStatistics->mSplitLines), 13);
        rstStr += "  " + ToWithString(ToString(pStatistics->mParseFailures), 10);
        rstStr += "  " + ToWithString(DateTimeToString(pStatistics->mLastReadTime), 18);
        rstStr += "  " + ToWithString(ToString(pStatistics->mReadCount), 10);
        rstStr += "  " + ToWithString(ToHumanReadableByteCount(pStatistics->mReadDelayAvg), 9);
        rstStr += "  " + ToWithString(ToString(pStatistics->mSendQueueSize), 10);
        rstStr += "  " + ToWithString(ToString(pStatistics->mSendNetWorkErrorCount), 13);
        rstStr += "  " + ToWithString(ToString(pStatistics->mSendQuotaErrorCount), 11);
        rstStr += "  " + ToWithString(ToString(pStatistics->mSendDiscardErrorCount), 13);
        rstStr += "  " + ToWithString(ToString(pStatistics->mSendSuccessCount), 12);
        rstStr += "  " + ToWithString(ToString(pStatistics->mSendBlockFlag), 10);
        rstStr += "  " + ToWithString(ToString(pStatistics->mValidToSendFlag), 10);
        rstStr += "  " + ToWithString(DateTimeToString(pStatistics->mMaxUnsendTime), 18);
        rstStr += "  " + ToWithString(DateTimeToString(pStatistics->mMinUnsendTime), 18);
        rstStr += "  " + ToWithString(DateTimeToString(pStatistics->mMaxSendSuccessTime), 18) + "\n";

        return rstStr;
    }


    string GetLogStoreProfileHeader() {
        string rstStr = "        begin_time"; //18
        rstStr += "          status"; // 12
        rstStr += "      read"; //8
        rstStr += "  parse_success"; //13
        rstStr += "  parse_fail"; //10
        rstStr += "      last_read_time"; //18
        rstStr += "  read_count"; //10
        rstStr += "  avg_delay"; //9
        rstStr += "  send_queue"; //10
        rstStr += "  network_error"; //13
        rstStr += "  quota_error"; //11
        rstStr += "  discard_error"; //13
        rstStr += "  send_success"; //12
        rstStr += "  send_block"; //10
        rstStr += "  send_valid"; //10
        rstStr += "          max_unsend"; //18
        rstStr += "          min_unsend"; //18
        rstStr += "    max_send_success\n"; //18
        return rstStr;
    }

    string GetFileProfileJson(string project, string logstore, string fileName) {
        Json::Value rstValue;
        string key = project + '#' + logstore + '#' + fileName;
        map<string, LogStoreStatistic*>::iterator iter = mFileProfileMap.find(key);
        if (iter == mFileProfileMap.end()) {
            //rstValue["find"] = Json::Value(false);
            return rstValue.toStyledString();
        }
        LogStoreStatistic* pStatistics = iter->second;
        //rstValue["find"] = Json::Value(true);
        rstValue["time_begin_readable"] = Json::Value(DateTimeToString(mTimeValue - 600));
        rstValue["time_end_readable"] = Json::Value(DateTimeToString(mTimeValue));
        rstValue["time_begin"] = Json::Int(mTimeValue - 600);
        rstValue["time_end"] = Json::Value(mTimeValue);

        rstValue["project"] = Json::Value(pStatistics->mProjectName);
        rstValue["logstore"] = Json::Value(pStatistics->mCategory);
        rstValue["status"] = Json::Value(pStatistics->mStatus);
        rstValue["config"] = Json::Value(pStatistics->mConfigName);
        rstValue["file_path"] = Json::Value(pStatistics->mFilename);
        rstValue["read_bytes"] = Json::Int64(pStatistics->mReadBytes);
        rstValue["parse_success_lines"] = Json::Int64(pStatistics->mSplitLines);
        rstValue["parse_fail_lines"] = Json::Int64(pStatistics->mParseFailures);
        rstValue["last_read_time"] = Json::Int64(pStatistics->mLastReadTime);
        rstValue["file_dev"] = Json::Int64(pStatistics->mFileDev);
        rstValue["file_inode"] = Json::Int64(pStatistics->mFileInode);
        rstValue["file_size_bytes"] = Json::Int64(pStatistics->mFileSize);
        rstValue["read_offset_bytes"] = Json::Int64(pStatistics->mReadOffset);
        rstValue["read_count"] = Json::Int64(pStatistics->mReadCount);
        rstValue["avg_delay_bytes"] = Json::Int64(pStatistics->mReadDelayAvg);

        return rstValue.toStyledString();
    }

    string GetFileProfileSimple(string project, string logstore, string fileName) {
        string key = project + '#' + logstore + '#' + fileName;
        map<string, LogStoreStatistic*>::iterator iter = mFileProfileMap.find(key);
        if (iter == mFileProfileMap.end()) {
            return "";
            //return "find : false";
        }
        LogStoreStatistic* pStatistics = iter->second;
        //string rstStr = "find : true";

        string rstStr = "time_begin_readable : " + DateTimeToString(mTimeValue - 600);
        rstStr += "\ntime_end_readable : " + DateTimeToString(mTimeValue);
        rstStr += "\ntime_begin : " + ToString(mTimeValue - 600);
        rstStr += "\ntime_end : " + ToString(mTimeValue);
        rstStr += "\nproject : " + ToString(pStatistics->mProjectName);
        rstStr += "\nlogstore : " + ToString(pStatistics->mCategory);
        rstStr += "\nstatus : " + ToString(pStatistics->mStatus);
        rstStr += "\nconfig : " + ToString(pStatistics->mConfigName);
        rstStr += "\nfile_path : " + ToString(pStatistics->mFilename);
        rstStr += "\nfile_dev : " + ToString(pStatistics->mFileDev);
        rstStr += "\nfile_inode : " + ToString(pStatistics->mFileInode);
        rstStr += "\nfile_size_bytes : " + ToString(pStatistics->mFileSize);
        rstStr += "\nfile_offset_bytes : " + ToString(pStatistics->mReadOffset);
        rstStr += "\nread_bytes : " + ToString(pStatistics->mReadBytes);
        rstStr += "\nparse_success_lines : " + ToString(pStatistics->mSplitLines);
        rstStr += "\nparse_fail_lines : " + ToString(pStatistics->mParseFailures);
        rstStr += "\nlast_read_time : " + ToString(pStatistics->mLastReadTime);
        rstStr += "\nread_count : " + ToString(pStatistics->mReadCount);
        rstStr += "\navg_delay_bytes : " + ToString(pStatistics->mReadDelayAvg);
        rstStr += "\nread_offset_bytes : " + ToString(pStatistics->mReadOffset);


        return rstStr;
    }

    string GetFileProfileOneLine(string project, string logstore, string fileName) {
        string key = project + '#' + logstore + '#' + fileName;
        map<string, LogStoreStatistic*>::iterator iter = mFileProfileMap.find(key);
        if (iter == mFileProfileMap.end()) {
            return "";
            //return "find : false";
        }
        LogStoreStatistic* pStatistics = iter->second;
        //string rstStr = "find : true";
        string rstStr = ToWithString(DateTimeToString(mTimeValue - 600), 18);
        rstStr += "  " + ToWithString(pStatistics->mStatus, 14);
        rstStr += "  " + ToWithString(ToHumanReadableByteCount(pStatistics->mReadBytes), 8);
        rstStr += "  " + ToWithString(ToString(pStatistics->mSplitLines), 13);
        rstStr += "  " + ToWithString(ToString(pStatistics->mParseFailures), 10);
        rstStr += "  " + ToWithString(DateTimeToString(pStatistics->mLastReadTime), 18);
        rstStr += "  " + ToWithString(ToString(pStatistics->mReadCount), 10);
        rstStr += "  " + ToWithString(ToHumanReadableByteCount(pStatistics->mReadDelayAvg), 9);
        rstStr += "  " + ToWithString(ToString(pStatistics->mFileDev), 8);
        rstStr += "  " + ToWithString(ToString(pStatistics->mFileInode), 8);
        rstStr += "  " + ToWithString(ToHumanReadableByteCount(pStatistics->mFileSize), 9);
        rstStr += "  " + ToWithString(ToHumanReadableByteCount(pStatistics->mReadOffset), 11) + "\n";

        return rstStr;
    }

    string GetFileProfileHeader() {
        string rstStr = "        begin_time"; //18
        rstStr += "          status"; // 12
        rstStr += "      read"; //8
        rstStr += "  parse_success"; //13
        rstStr += "  parse_fail"; //10
        rstStr += "      last_read_time"; //18
        rstStr += "  read_count"; //10
        rstStr += "  avg_delay"; //9
        rstStr += "    device"; //8
        rstStr += "     inode"; //8
        rstStr += "  file_size"; //9
        rstStr += "  read_offset\n"; //11
        return rstStr;
    }

    int ReadData(string filePath, string& fileContent, string& timeStr) {
        filePath = GetProcessExecutionDir() + filePath;
        FILE* pFile = fopen(filePath.c_str(), "r");
        //cout << "open file" << filePath << endl;
        if (pFile == NULL) {
            cout << string("query fail, error: ") + strerror(errno) << endl;
            return errno;
        }
        fseek(pFile, 0, SEEK_END);
        int fileSize = ftell(pFile);
        fseek(pFile, 0, SEEK_SET);

        if (fileSize > 0) {
            int rst = 0;
            char* readBuf = (char*)malloc(fileSize + 1);
            memset(readBuf, 0, fileSize + 1);
            fread(readBuf, 1, fileSize, pFile);

            string allStr(readBuf);

            GetTimeStr(allStr, timeStr);
            //cout << "time : " << timeStr << endl;

            size_t contentPos = allStr.find("\n:");
            if (contentPos == string::npos || contentPos == allStr.size() - 2) {
                cout << (string("file format error, " + filePath).c_str()) << endl;
                rst = 1;
                goto err;
            }

            fileContent = allStr.substr(contentPos + 2);
            //cout << "fileContent : " << fileContent.substr(0, 100) << endl;
        err:
            free(readBuf);
            fclose(pFile);
            return rst;
        }
        cout << string("invalid profile, maybe logtail restart") << endl;

        fclose(pFile);
        return 2;
    }

    static std::string GetFilePath(const std::string& fileType, const std::string& delta) {
        const std::string execDir = logtail::GetProcessExecutionDir();
        const std::string pathPrefix = "snapshot" + PATH_SEPARATOR + fileType + ".";
        std::string v0FilePath = pathPrefix + "LOG." + delta;
        std::string v1FilePath = pathPrefix + delta + ".LOG";

        fsutil::PathStat v0Stat, v1Stat;
        bool foundV0 = fsutil::PathStat::stat(execDir + v0FilePath, v0Stat);
        bool foundV1 = fsutil::PathStat::stat(execDir + v1FilePath, v1Stat);

        if (foundV0 && !foundV1) {
            return v0FilePath;
        }
        if (!foundV0 && foundV1) {
            return v1FilePath;
        }
        // Both exist, return latest one.
        return v1Stat.GetMtime() >= v0Stat.GetMtime() ? v1FilePath : v0FilePath;
    }

    int InitStatus(string delta = "1", bool checkFlag = false) {
        int val = atoi(delta.c_str());
        if (val < 1 || val > 60) {
            cout << "invalid query interval" << endl;
            return 1;
        }
        string contentStr;
        string timeStr;
        if (ReadData(GetFilePath("ilogtail_status", delta), contentStr, timeStr) != 0) {
            return 1;
        }
        int32_t queryTime = time(NULL) - val * 60;
        if (checkFlag) {
            if (mTimeValue - queryTime > 60 || queryTime - mTimeValue > 60) {
                cout << "no match time interval, please check logtail status" << endl;
                return 1;
            }
        }
        StringToStatus(contentStr);
        return 0;
    }

    int InitProfile(string delta = "1", bool checkFlag = false) {
        int val = atoi(delta.c_str());
        if (val < 1 || val > 60) {
            cout << "invalid query interval" << endl;
            return 1;
        }
        string contentStr;
        string timeStr;
        if (ReadData(GetFilePath("ilogtail_profile", delta), contentStr, timeStr) != 0) {
            return 1;
        }
        int32_t queryTime = time(NULL) - val * 600;
        if (checkFlag) {
            if (mTimeValue - queryTime > 600 || queryTime - mTimeValue > 600) {
                cout << "no match time interval, please check logtail status" << endl;
                return 1;
            }
        }
        Json::Value rootValue;
        StringToJson(contentStr, rootValue);
        if (rootValue.isMember("detail") && rootValue["detail"].isArray()) {
            Json::Value& values = rootValue["detail"];
            for (Json::ValueIterator iter = values.begin(); iter != values.end(); ++iter) {
                JsonToLogStoreStatistic(*iter, false);
            }
        }
        if (rootValue.isMember("logstore") && rootValue["logstore"].isArray()) {
            Json::Value& values = rootValue["logstore"];
            for (Json::ValueIterator iter = values.begin(); iter != values.end(); ++iter) {
                JsonToLogStoreStatistic(*iter, true);
            }
        }
        return 0;
    }

    int QueryLogstoreRange(string project, string logstore, string beginIndex, string endIndex) {
        int bIndex = atoi(beginIndex.c_str());
        int eIndex = atoi(endIndex.c_str());
        if (bIndex < 1 || bIndex > 60 || eIndex < 1 || eIndex > 60 || bIndex > eIndex) {
            cout << "invalid query interval" << endl;
            return 1;
        }

        cout << GetLogStoreProfileHeader();
        int rst = 0;
        for (int i = bIndex; i <= eIndex; ++i) {
            rst = InitProfile(ToString(i));
            if (rst != 0) {
                continue;
            }
            cout << GetLogStoreProfileOneLine(project, logstore);
            mLogstoreProfileMap.clear();
            mFileProfileMap.clear();
        }
        return 0;
    }

    int QueryLogfileRange(string project, string logstore, string file, string beginIndex, string endIndex) {
        int bIndex = atoi(beginIndex.c_str());
        int eIndex = atoi(endIndex.c_str());
        if (bIndex < 1 || bIndex > 60 || eIndex < 1 || eIndex > 60 || bIndex > eIndex) {
            cout << "invalid query interval" << endl;
            return 1;
        }

        cout << GetFileProfileHeader();
        int rst = 0;
        for (int i = bIndex; i <= eIndex; ++i) {
            rst = InitProfile(ToString(i));
            if (rst != 0) {
                continue;
            }
            cout << GetFileProfileOneLine(project, logstore, file);
            mLogstoreProfileMap.clear();
            mFileProfileMap.clear();
        }
        return 0;
    }

private:
    static LogtailInsight* s_instance;

    void GetTimeStr(const string& allStr, string& timeStr);

protected:
    map<string, string> mStatusMap;

    Json::Value mProfileJson;
    string mTimeString;
    int32_t mTimeValue;
    // project + logstore
    map<string, LogStoreStatistic*> mLogstoreProfileMap;
    // project + logstore + filename
    map<string, LogStoreStatistic*> mFileProfileMap;

private:
};

} // namespace logtail
