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

#include <vector>
#include <utility>
#include <algorithm>
#include "LogIntegrity.h"
#include "LogstoreSenderQueue.h"
#include "LogFileReader.h"
#include "ConfigManager.h"
#include "common/LogtailCommonFlags.h"
#include "common/RuntimeUtil.h"
#include "common/JsonUtil.h"
#include "common/ExceptionBase.h"
#include "common/Constants.h"
#include "Sender.h"
#include "LogtailAlarm.h"
#include "LogFileProfiler.h"

using namespace sls_logs;
using namespace std;

DEFINE_FLAG_INT32(log_integrity_send_interval, "log integrity send interval, seconds", 3);
DEFINE_FLAG_INT32(eliminated_file_heart_beat_send_interval, "eliminated file heart beat send interval", 6);
DEFINE_FLAG_INT32(data_integrity_regex_time_pos,
                  "data integrity regex time pos",
                  -1); // if pos is -1, we should use regex match to get log time
DECLARE_FLAG_INT32(file_map_shrink_size);
DECLARE_FLAG_INT32(file_eliminate_interval);
DECLARE_FLAG_INT32(file_map_max_size);

namespace logtail {
const int LogTimeInfo::LogIntegrityStatus_ParseOK = 2;
const int LogTimeInfo::LogIntegrityStatus_ParseFail = 0;
const int LogTimeInfo::LogIntegrityStatus_SendOK = 1;
const int LogTimeInfo::LogIntegrityStatus_SendFail = 0;

void LogIntegrityInfo::SetStatus(int64_t seqNum, int32_t lines, int status) {
    // set status, set init flag to false
    for (std::list<LogTimeInfo>::iterator iter = mLogTimeInfoList.begin(); iter != mLogTimeInfoList.end(); ++iter) {
        if (seqNum == iter->mLogGroupSequenceNum) {
            iter->mSucceededLines = lines;
            iter->mStatus |= status;
            iter->mInitFlag = false;
            LOG_DEBUG(sLogger,
                      ("set status, seq num", seqNum)("project", mProjectName)("log store", mLogstore)(
                          "filename", mFilename)("lines", lines)("status", status));
            return;
        }
    }
    LOG_ERROR(sLogger,
              ("failed to find seq num",
               seqNum)("project", mProjectName)("log store", mLogstore)("filename", mFilename)("status", status));
}

void LogIntegrityInfo::CalcIntegrityInfo(time_t& integrityTime, int32_t& lines, std::string& status) {
    // list is empty, no new log
    if (mLogTimeInfoList.empty()) {
        status = "NoNewLog";
        return;
    }

    // list has send failed status, move to first failed node or init node
    if (!mSendSucceededFlag) {
        status = "SendFailed";
        for (std::list<LogTimeInfo>::iterator iter = mLogTimeInfoList.begin(); iter != mLogTimeInfoList.end();) {
            // not init node, send succeeded
            if (!iter->mInitFlag && iter->mStatus % 2 == LogTimeInfo::LogIntegrityStatus_SendOK) {
                LOG_DEBUG(sLogger,
                          ("log time", iter->mLogTime)("lines", iter->mSucceededLines)("project", mProjectName)(
                              "log store", mLogstore)("filename", mFilename)("status", iter->mStatus));
                integrityTime = std::max(integrityTime, iter->mLogTime);
                lines += iter->mSucceededLines;
                iter = mLogTimeInfoList.erase(iter);
            }
            // first init node, check status then return
            else if (iter->mInitFlag) {
                status = integrityTime == -1 ? "ParseFailed" : "OK";
                return;
            }
            // send failed, just return
            else
                return;
        }

        // all nodes are erased, or lefts nodes are all init nodes, set mSendSucceededFlag to true
        status = integrityTime == -1 ? "ParseFailed" : "OK";
        mSendSucceededFlag = true;
        return;
    } else {
        // all nodes should be erased
        for (std::list<LogTimeInfo>::iterator iter = mLogTimeInfoList.begin(); iter != mLogTimeInfoList.end();) {
            // not init node, send succeeded
            if (!iter->mInitFlag && iter->mStatus % 2 == LogTimeInfo::LogIntegrityStatus_SendOK) {
                LOG_DEBUG(sLogger,
                          ("log time", iter->mLogTime)("lines", iter->mSucceededLines)("project", mProjectName)(
                              "log store", mLogstore)("filename", mFilename)("status", iter->mStatus));
                integrityTime = std::max(integrityTime, iter->mLogTime);
                lines += iter->mSucceededLines;
                iter = mLogTimeInfoList.erase(iter);
            }
            // first init node, break
            else if (iter->mInitFlag) {
                break;
            } else {
                LOG_ERROR(sLogger,
                          ("unexpected send failed node occurs, region", mRegion)("project", mProjectName)(
                              "log store", mLogstore)("filename", mFilename)("log time",
                                                                             iter->mLogTime)("status", iter->mStatus));
                ++iter;
            }
        }
        status = integrityTime == -1 ? "ParseFailed" : "OK";
    }
}

LogIntegrity::LogIntegrity() {
    mIntegrityDumpFileName = GetProcessExecutionDir() + STRING_FLAG(logtail_integrity_snapshot);
    mBakIntegrityDumpFileName = GetProcessExecutionDir() + STRING_FLAG(logtail_integrity_snapshot) + "_bak";
}

LogIntegrity::~LogIntegrity() {
    PTScopedLock lock(mLogIntegrityMapLock);
    for (RegionLogIntegrityInfoMap::iterator regionIter = mRegionLogIntegrityInfoMap.begin();
         regionIter != mRegionLogIntegrityInfoMap.end();
         ++regionIter) {
        LogIntegrityInfoMap* logIntegrityInfoMap = regionIter->second;
        for (LogIntegrityInfoMap::iterator projectLogStoreFilenameIter = logIntegrityInfoMap->begin();
             projectLogStoreFilenameIter != logIntegrityInfoMap->end();
             ++projectLogStoreFilenameIter) {
            LogIntegrityInfo* logIntegrityInfo = projectLogStoreFilenameIter->second;
            delete logIntegrityInfo;
        }
        logIntegrityInfoMap->clear();
        delete logIntegrityInfoMap;
    }
    mRegionLogIntegrityInfoMap.clear();

    for (RegionOutDatedFileMap::iterator regionIter = mRegionOutDatedFileMap.begin();
         regionIter != mRegionOutDatedFileMap.end();
         ++regionIter) {
        OutDatedFileMap* outDatedFileMap = regionIter->second;
        for (OutDatedFileMap::iterator fileIter = outDatedFileMap->begin(); fileIter != outDatedFileMap->end();
             ++fileIter) {
            OutDatedFile* outDatedFile = fileIter->second;
            delete outDatedFile;
        }
        outDatedFileMap->clear();
        delete outDatedFileMap;
    }
    mRegionOutDatedFileMap.clear();
}

void LogIntegrity::RecordIntegrityInfo(MergeItem* item) {
    IntegrityConfigPtr integrityConfigPtr = item->mLogGroupContext.mIntegrityConfigPtr;
    if (integrityConfigPtr.get() == NULL || !integrityConfigPtr->mIntegritySwitch)
        return;

    const string& region = item->mRegion;
    const string& projectName = item->mProjectName;
    const string& logstore = item->mLogGroup.category();
    const string& filename = item->mFilename;
    int64_t seqNum = item->mLogGroupContext.mSeqNum;

    // lock
    PTScopedLock lock(mLogIntegrityMapLock);
    // find region
    RegionLogIntegrityInfoMap::iterator regionIter = mRegionLogIntegrityInfoMap.find(region);
    if (regionIter == mRegionLogIntegrityInfoMap.end()) {
        LogIntegrityInfoMap* logIntegrityInfoMap = new LogIntegrityInfoMap;
        regionIter = mRegionLogIntegrityInfoMap.insert(std::make_pair(region, logIntegrityInfoMap)).first;
    }
    LogIntegrityInfoMap* logIntegrityInfoMap = regionIter->second;

    // parse log time
    time_t logTime = -1;
    bool parseSucceeded = false;
    // if time pos is -1, use regex
    // if config->mTimePos is invalid, use regex
    std::string lastLogLine = GetLastLogLine(item);
    if (lastLogLine.empty()) {
        LOG_DEBUG(
            sLogger,
            ("empty log line, region", region)("project", projectName)("log store", logstore)("filename", filename));
        return;
    }

    if (integrityConfigPtr->mTimePos <= INT32_FLAG(data_integrity_regex_time_pos)
        || integrityConfigPtr->mTimePos >= (int32_t)lastLogLine.size())
        parseSucceeded = LogFileReader::ParseLogTime(lastLogLine.c_str(),
                                                     integrityConfigPtr->mCompiledTimeReg,
                                                     logTime,
                                                     integrityConfigPtr->mTimeFormat,
                                                     region,
                                                     projectName,
                                                     logstore,
                                                     filename);
    else
        parseSucceeded = LogFileReader::GetLogTimeByOffset(lastLogLine.c_str(),
                                                           integrityConfigPtr->mTimePos,
                                                           logTime,
                                                           integrityConfigPtr->mTimeFormat,
                                                           region,
                                                           projectName,
                                                           logstore,
                                                           filename);

    // find log time info
    string key = GetIntegrityInfoKey(projectName, logstore, filename);
    LogIntegrityInfoMap::iterator projectLogStoreFilenameIter = logIntegrityInfoMap->find(key);
    if (projectLogStoreFilenameIter == logIntegrityInfoMap->end()) {
        // erase item in out-dated file map
        // get last update time from out-dated map
        time_t lastUpdateTime = EraseItemInOutDatedFileMap(region, projectName, logstore, filename);
        // if file existed in out-dated map, get last update time from the map
        // if it's a new file, last update time is -1
        LogIntegrityInfo* logIntegrityInfo = new LogIntegrityInfo(region,
                                                                  projectName,
                                                                  logstore,
                                                                  filename,
                                                                  integrityConfigPtr->mAliuid,
                                                                  integrityConfigPtr->mIntegrityProjectName,
                                                                  integrityConfigPtr->mIntegrityLogstore,
                                                                  lastUpdateTime);
        projectLogStoreFilenameIter = logIntegrityInfoMap->insert(std::make_pair(key, logIntegrityInfo)).first;
    }
    LogIntegrityInfo* logIntegrityInfo = projectLogStoreFilenameIter->second;
    // initial succeeded lines is 0, will modify afterwards
    LogTimeInfo logTimeInfo(seqNum,
                            parseSucceeded ? logTime : -1,
                            0,
                            parseSucceeded ? LogTimeInfo::LogIntegrityStatus_ParseFail
                                           : LogTimeInfo::LogIntegrityStatus_ParseOK);
    // append log time info in list
    logIntegrityInfo->mLogTimeInfoList.push_back(logTimeInfo);

    LOG_DEBUG(sLogger,
              ("insert log time info into map, region", region)("project", projectName)("log store", logstore)(
                  "filename", filename)("log time", logTime)("sequence num", seqNum));
}

void LogIntegrity::Notify(LoggroupTimeValue* data, bool flag) {
    const string& region = data->mRegion;
    const string& projectName = data->mProjectName;
    const string& logstore = data->mLogstore;
    const string& filename = data->mFilename;

    // empty filename, filter metric data and data-integrity data
    if (filename.empty()) {
        LOG_DEBUG(sLogger,
                  ("successfully send metric data or integrity data, region",
                   region)("project", projectName)("logstore", logstore));
        return;
    }
    LOG_DEBUG(sLogger,
              (flag ? "notify success, region" : "notify fail, region",
               region)("project_name", projectName)("logstore", logstore)("filename", filename)(
                  "seq num", data->mLogGroupContext.mSeqNum)("lines", data->mLogLines));

    // lock
    PTScopedLock lock(mLogIntegrityMapLock);
    LogIntegrityInfo* info = NULL;
    if (FindLogIntegrityInfo(region, projectName, logstore, filename, info)) {
        info->mLastUpdateTime = data->mLastUpdateTime;
        info->SetStatus(data->mLogGroupContext.mSeqNum,
                        data->mLogLines,
                        flag ? LogTimeInfo::LogIntegrityStatus_SendOK : LogTimeInfo::LogIntegrityStatus_SendFail);
        if (!flag)
            info->mSendSucceededFlag = false;
    }
}

void LogIntegrity::SendLogIntegrityInfo() {
    static int32_t lastSendTime = 0, lastEliminateTime = 0, lastSendEliminatedFileTime = 0;

    int32_t curTime = time(NULL);
    if (curTime - lastSendTime > INT32_FLAG(log_integrity_send_interval)) {
        lastSendTime = curTime;
        // lock
        PTScopedLock lock(mLogIntegrityMapLock);

        if (mRegionLogIntegrityInfoMap.empty())
            return;

        std::map<LogDest, LogGroup> logGroupMap;
        // traverse map
        for (RegionLogIntegrityInfoMap::iterator regionIter = mRegionLogIntegrityInfoMap.begin();
             regionIter != mRegionLogIntegrityInfoMap.end();
             ++regionIter) {
            const std::string& region = regionIter->first;
            LogIntegrityInfoMap* logIntegrityInfoMap = regionIter->second;
            if (logIntegrityInfoMap->empty())
                continue;

            // build log groups, one integrity log store one log group
            for (LogIntegrityInfoMap::iterator projectLogStoreFilenameIter = logIntegrityInfoMap->begin();
                 projectLogStoreFilenameIter != logIntegrityInfoMap->end();
                 ++projectLogStoreFilenameIter) {
                LogIntegrityInfo* logIntegrityInfo = projectLogStoreFilenameIter->second;
                // check whether it's valid to send data, if it's not valid, we should not calc integrity info
                LogstoreFeedBackKey feedbackKey = GenerateLogstoreFeedBackKey(logIntegrityInfo->mIntegrityProject,
                                                                              logIntegrityInfo->mIntegrityLogstore);
                if (!Sender::Instance()->GetSenderFeedBackInterface()->IsValidToPush(feedbackKey))
                    continue;

                LogDest dst(logIntegrityInfo->mAliuid,
                            region,
                            logIntegrityInfo->mIntegrityProject,
                            logIntegrityInfo->mIntegrityLogstore);
                std::map<LogDest, LogGroup>::iterator iter = logGroupMap.find(dst);
                if (iter == logGroupMap.end()) {
                    LogGroup logGroup;
                    iter = logGroupMap.insert(std::make_pair(dst, logGroup)).first;
                }
                LogGroup& logGroup = iter->second;
                // calc integrity time and succeeded lines
                time_t integrityTime = -1;
                int32_t lines = 0;
                std::string status;
                logIntegrityInfo->CalcIntegrityInfo(integrityTime, lines, status);

                BuildLogGroup(logGroup,
                              logIntegrityInfo->mProjectName,
                              logIntegrityInfo->mLogstore,
                              logIntegrityInfo->mFilename,
                              integrityTime,
                              lines,
                              status);
                LOG_DEBUG(sLogger,
                          ("build log group, size",
                           logGroup.logs_size())("region", region)("project", logIntegrityInfo->mProjectName)(
                              "logstore", logIntegrityInfo->mLogstore)("filename", logIntegrityInfo->mFilename)(
                              "integrity time", integrityTime)("lines", lines)("status", status));
            }

            // send
            for (std::map<LogDest, LogGroup>::iterator iter = logGroupMap.begin(); iter != logGroupMap.end(); ++iter) {
                const LogDest& dst = iter->first;
                LogGroup& logGroup = iter->second;

                logGroup.set_source(LogFileProfiler::mIpAddr);
                logGroup.set_category(dst.mLogstore);
                logGroup.set_machineuuid(ConfigManager::GetInstance()->GetUUID());

                LogTag* logTagPtr = logGroup.add_logtags();
                logTagPtr->set_key(LOG_RESERVED_KEY_HOSTNAME);
                logTagPtr->set_value(LogFileProfiler::mHostname);

                // send integrity log group
                bool sendSucceeded
                    = mProfileSender.SendInstantly(logGroup, dst.mAliuid, dst.mRegion, dst.mProjectName, dst.mLogstore);
                if (!sendSucceeded) {
                    LogtailAlarm::GetInstance()->SendAlarm(DISCARD_DATA_ALARM,
                                                           "push data integrity data into batch map fail",
                                                           dst.mProjectName,
                                                           dst.mLogstore,
                                                           region);
                    LOG_ERROR(sLogger,
                              ("failed to push data integrity data into batch map, discard logs", logGroup.logs_size())(
                                  "region", region)("project", dst.mProjectName)("logstore", dst.mLogstore));
                }
            }
        }

#ifdef LOGTAIL_DEBUG_FLAG
        PrintLogTimeInfoListStatus();
#endif

        // shrink map if size reaches max size limit, erase elements
        if (GetMapItemCount() >= (size_t)INT32_FLAG(file_map_max_size)) {
            ShrinkLogIntegrityInfoMap(INT32_FLAG(file_map_shrink_size));
        }
    }

    // eliminate out-dated data
    if (curTime - lastEliminateTime > INT32_FLAG(file_eliminate_interval)) {
        lastEliminateTime = curTime;

        // eliminate out-dated data in map
        PTScopedLock lock(mLogIntegrityMapLock);
        EliminateOutDatedData(INT32_FLAG(file_eliminate_interval));
    }

    if (curTime - lastSendEliminatedFileTime > INT32_FLAG(eliminated_file_heart_beat_send_interval)) {
        lastSendEliminatedFileTime = curTime;

        PTScopedLock lock(mLogIntegrityMapLock);
        SendOutDatedFileIntegrityInfo();
    }
}

void LogIntegrity::EraseItemInMap(const std::string& region,
                                  const std::string& projectName,
                                  const std::string& logstore) {
    PTScopedLock lock(mLogIntegrityMapLock);
    if (mRegionLogIntegrityInfoMap.empty())
        return;

    // find region
    RegionLogIntegrityInfoMap::iterator regionIter = mRegionLogIntegrityInfoMap.find(region);
    if (regionIter == mRegionLogIntegrityInfoMap.end())
        return;

    LogIntegrityInfoMap* logIntegrityInfoMap = regionIter->second;
    // find log time info
    std::string keyPrefix = projectName + "_" + logstore + "_";
    size_t keyPrefixSize = keyPrefix.size();
    std::vector<std::string> eraseKeys;
    for (LogIntegrityInfoMap::iterator projectLogStoreFilenameIter = logIntegrityInfoMap->begin();
         projectLogStoreFilenameIter != logIntegrityInfoMap->end();
         ++projectLogStoreFilenameIter) {
        const std::string& key = projectLogStoreFilenameIter->first;
        if (key.size() > keyPrefixSize && memcmp(key.c_str(), keyPrefix.c_str(), keyPrefixSize) == 0)
            eraseKeys.push_back(key);
    }
    for (size_t i = 0; i < eraseKeys.size(); ++i) {
        LogIntegrityInfoMap::iterator projectLogStoreFilenameIter = logIntegrityInfoMap->find(eraseKeys[i]);
        if (projectLogStoreFilenameIter != logIntegrityInfoMap->end()) {
            delete projectLogStoreFilenameIter->second;
            logIntegrityInfoMap->erase(projectLogStoreFilenameIter);
        }
    }
    if (eraseKeys.size() > (size_t)0)
        LOG_INFO(sLogger, ("erase log time map, keys", ToString(eraseKeys)));
    else
        LOG_DEBUG(sLogger, ("erase log time map", "no any item"));
}

void LogIntegrity::DumpIntegrityDataToLocal() {
    {
        PTScopedLock lock(mLogIntegrityMapLock);
        if (mRegionLogIntegrityInfoMap.empty()) {
            LOG_DEBUG(sLogger, ("empty integrity map", "please check"));
            return;
        }
    }

    // to json
    Json::Value root;
    {
        // lock
        PTScopedLock lock(mLogIntegrityMapLock);
        // traverse log time map
        for (RegionLogIntegrityInfoMap::iterator regionIter = mRegionLogIntegrityInfoMap.begin();
             regionIter != mRegionLogIntegrityInfoMap.end();
             ++regionIter) {
            const std::string& region = regionIter->first;
            LogIntegrityInfoMap* logIntegrityInfoMap = regionIter->second;

            Json::Value regionValue;
            for (LogIntegrityInfoMap::iterator projectLogStoreFilenameIter = logIntegrityInfoMap->begin();
                 projectLogStoreFilenameIter != logIntegrityInfoMap->end();
                 ++projectLogStoreFilenameIter) {
                const std::string& projectLogStoreFilename = projectLogStoreFilenameIter->first;
                LogIntegrityInfo* logIntegrityInfo = projectLogStoreFilenameIter->second;

                Json::Value logIntegrityValue;
                logIntegrityValue["region"] = logIntegrityInfo->mRegion;
                logIntegrityValue["project_name"] = logIntegrityInfo->mProjectName;
                logIntegrityValue["logstore"] = logIntegrityInfo->mLogstore;
                logIntegrityValue["filename"] = logIntegrityInfo->mFilename;
                logIntegrityValue["data_integrity_project_name"] = logIntegrityInfo->mIntegrityProject;
                logIntegrityValue["data_integrity_logstore"] = logIntegrityInfo->mIntegrityLogstore;
                logIntegrityValue["last_update_time"] = (Json::Int)logIntegrityInfo->mLastUpdateTime;
                logIntegrityValue["succeeded_flag"] = logIntegrityInfo->mSendSucceededFlag;

                // dump list nodes
                Json::Value listNode;
                LogIntegrity::SerializeListNodes(logIntegrityInfo->mLogTimeInfoList, listNode);
                logIntegrityValue["list"] = listNode;

                regionValue[projectLogStoreFilename.c_str()] = logIntegrityValue;
            }

            root[region.c_str()] = regionValue;
        }
    }

    FILE* file = fopen(mBakIntegrityDumpFileName.c_str(), "w");
    if (file == NULL) {
        LOG_ERROR(sLogger, ("failed to open file", mBakIntegrityDumpFileName)("error", strerror(errno)));
        return;
    }
    std::string dumpContent = root.toStyledString();
    fwrite(dumpContent.c_str(), 1, dumpContent.size(), file);
    fclose(file);
    if (rename(mBakIntegrityDumpFileName.c_str(), mIntegrityDumpFileName.c_str()) == -1)
        LOG_ERROR(sLogger, ("failed to rename integrity file", mIntegrityDumpFileName)("error", strerror(errno)));
}

bool LogIntegrity::ReloadIntegrityDataFromLocalFile() {
    Json::Value root;
    ParseConfResult result = ParseConfig(mIntegrityDumpFileName, root);
    if (result != CONFIG_OK) {
        if (result == CONFIG_NOT_EXIST)
            LOG_INFO(sLogger, ("no integrity file to load", mIntegrityDumpFileName));
        else if (result == CONFIG_INVALID_FORMAT) {
            LOG_ERROR(sLogger, ("load integrity file fail, file content is not valid json", mIntegrityDumpFileName));
            LogtailAlarm::GetInstance()->SendAlarm(CHECKPOINT_ALARM, "content of integrity file is not valid json");
        }
        return false;
    }

    // lock
    PTScopedLock lock(mLogIntegrityMapLock);
    string errorMessage;
    // parse file and reload data
    const Json::Value::Members& regions = root.getMemberNames();
    for (size_t i = 0; i < regions.size(); ++i) {
        const string& region = regions[i];
        LogIntegrityInfoMap* logIntegrityInfoMap = new LogIntegrityInfoMap;

        const Json::Value& regionValue = root[region];
        const Json::Value::Members& projectLogstoreFilenames = regionValue.getMemberNames();
        for (size_t j = 0; j < projectLogstoreFilenames.size(); ++j) {
            const string& projectLogstoreFilename = projectLogstoreFilenames[j];
            LogIntegrityInfo* logIntegrityInfo = new LogIntegrityInfo;

            const Json::Value& projectLogstoreFilenameValue = regionValue[projectLogstoreFilename];
            const Json::Value::Members& logIntegrityInfos = projectLogstoreFilenameValue.getMemberNames();
            for (size_t k = 0; k < logIntegrityInfos.size(); ++k) {
                const string& name = logIntegrityInfos[k];
                try {
                    FillLogTimeInfo(logIntegrityInfo, name, projectLogstoreFilenameValue);
                } catch (const ExceptionBase& e) {
                    LOG_ERROR(sLogger,
                              ("failed to parse log time info, region",
                               region)("project_logstore_filename", projectLogstoreFilename)("Reason",
                                                                                             e.GetExceptionMessage()));
                    errorMessage = "invalid json, reason: " + e.GetExceptionMessage();
                } catch (...) {
                    LOG_ERROR(sLogger,
                              ("failed to parse log time info, region",
                               region)("project_logstore_filename", projectLogstoreFilename)("Reason", "unknown"));
                    errorMessage = "invalid json, reason: unknown";
                }

                if (!errorMessage.empty()) {
                    LogtailAlarm::GetInstance()->SendAlarm(
                        LOGTAIL_CONFIG_ALARM, errorMessage, region, projectLogstoreFilename);
                    errorMessage.clear();
                }
            }

            logIntegrityInfoMap->insert(std::make_pair(projectLogstoreFilename, logIntegrityInfo));
        }

        mRegionLogIntegrityInfoMap.insert(std::make_pair(region, logIntegrityInfoMap));
    }

    // delete file
    if (remove(mIntegrityDumpFileName.c_str()) != 0)
        LOG_INFO(sLogger, ("failed to delete integrity file", mIntegrityDumpFileName));

    return true;
}

// eliminate data under two circumstances
// 1. erase files two days ago, 86400 * 2 = 172800
// 2. map size reaches 10000, then erase map elements to 9000
void LogIntegrity::EliminateOutDatedData(int32_t gapSeconds) {
    // unlocked
    // check empty
    if (mRegionLogIntegrityInfoMap.empty()) {
        LOG_DEBUG(sLogger, ("region map is empty", "please check"));
        return;
    }

    time_t curTime = time(NULL);
    for (RegionLogIntegrityInfoMap::iterator regionIter = mRegionLogIntegrityInfoMap.begin();
         regionIter != mRegionLogIntegrityInfoMap.end();
         ++regionIter) {
        std::vector<std::string> vec;
        LogIntegrityInfoMap* logIntegrityInfoMap = regionIter->second;
        for (LogIntegrityInfoMap::iterator projectLogStoreFilenameIter = logIntegrityInfoMap->begin();
             projectLogStoreFilenameIter != logIntegrityInfoMap->end();
             ++projectLogStoreFilenameIter) {
            LogIntegrityInfo* logIntegrityInfo = projectLogStoreFilenameIter->second;
            // time gap too large, erase element
            if (curTime - logIntegrityInfo->mLastUpdateTime > gapSeconds) {
                vec.push_back(projectLogStoreFilenameIter->first);

                // insert into out-dated map
                InsertItemIntoOutDatedFileMap(logIntegrityInfo);
            }
        }
        for (std::vector<std::string>::iterator iter = vec.begin(); iter != vec.end(); ++iter) {
            LogIntegrityInfoMap::iterator projectLogStoreFilenameIter = logIntegrityInfoMap->find(*iter);
            if (projectLogStoreFilenameIter != logIntegrityInfoMap->end()) {
                delete projectLogStoreFilenameIter->second;
                logIntegrityInfoMap->erase(projectLogStoreFilenameIter);
            }
        }
        if (vec.size() > (size_t)0) {
            if (vec.size() < 20)
                LOG_INFO(sLogger,
                         ("erase element in log integrity map, region", regionIter->first)("items", ToString(vec)));
            else
                LOG_INFO(sLogger,
                         ("erase element in log integrity map, region", regionIter->first)("items", vec.size()));
        } else
            LOG_DEBUG(sLogger, ("erase element in log integrity map, region", regionIter->first)("items", vec.size()));
    }
}

void LogIntegrity::ShrinkLogIntegrityInfoMap(int32_t shrinkSize) {
    // unlocked
    // check empty
    if (mRegionLogIntegrityInfoMap.empty()) {
        LOG_DEBUG(sLogger, ("region map is empty", "please check"));
        return;
    }

    std::vector<std::pair<std::string, LogIntegrityInfo*> > vec;
    for (RegionLogIntegrityInfoMap::iterator regionIter = mRegionLogIntegrityInfoMap.begin();
         regionIter != mRegionLogIntegrityInfoMap.end();
         ++regionIter) {
        LogIntegrityInfoMap* logIntegrityInfoMap = regionIter->second;
        for (LogIntegrityInfoMap::iterator projectLogStoreFilenameIter = logIntegrityInfoMap->begin();
             projectLogStoreFilenameIter != logIntegrityInfoMap->end();
             ++projectLogStoreFilenameIter) {
            LogIntegrityInfo* logIntegrityInfo = projectLogStoreFilenameIter->second;
            vec.push_back(std::make_pair(projectLogStoreFilenameIter->first, logIntegrityInfo));
        }
    }
    std::sort(vec.begin(), vec.end(), SortByLastUpdateTime);

    for (int32_t i = 0; i < shrinkSize; ++i) {
        const std::string& region = vec[i].second->mRegion;
        // find region
        RegionLogIntegrityInfoMap::iterator regionIter = mRegionLogIntegrityInfoMap.find(region);
        LogIntegrityInfoMap* logIntegrityInfoMap = regionIter->second;
        // find item, then erase
        const std::string& key = vec[i].first;
        LogIntegrityInfoMap::iterator projectLogStoreFilenameIter = logIntegrityInfoMap->find(key);
        if (projectLogStoreFilenameIter != logIntegrityInfoMap->end()) {
            LogIntegrityInfo* logIntegrityInfo = projectLogStoreFilenameIter->second;
            // insert into out-dated map
            InsertItemIntoOutDatedFileMap(logIntegrityInfo);

            delete logIntegrityInfo;
            logIntegrityInfoMap->erase(projectLogStoreFilenameIter);
        }
    }

    LOG_INFO(sLogger,
             ("shrink log time map, size before shrinking", vec.size())("size after shrinking", GetMapItemCount()));
}

bool LogIntegrity::FindLogIntegrityInfo(const std::string& region,
                                        const std::string& projectName,
                                        const std::string& logstore,
                                        const std::string& filename,
                                        LogIntegrityInfo*& info) {
    RegionLogIntegrityInfoMap::iterator regionIter = mRegionLogIntegrityInfoMap.find(region);
    if (regionIter == mRegionLogIntegrityInfoMap.end()) {
        LOG_ERROR(sLogger,
                  ("failed to find integrity info, region",
                   region)("project_name", projectName)("logstore", logstore)("filename", filename));
        return false;
    }

    LogIntegrityInfoMap* logIntegrityInfoMap = regionIter->second;
    string key = GetIntegrityInfoKey(projectName, logstore, filename);
    LogIntegrityInfoMap::iterator projectLogStoreFilenameIter = logIntegrityInfoMap->find(key);
    if (projectLogStoreFilenameIter == logIntegrityInfoMap->end()) {
        LOG_ERROR(sLogger,
                  ("failed to find integrity info, region",
                   region)("project_name", projectName)("logstore", logstore)("filename", filename));
        return false;
    }

    info = projectLogStoreFilenameIter->second;
    return true;
}

void LogIntegrity::BuildLogGroup(sls_logs::LogGroup& logGroup,
                                 const std::string& projectName,
                                 const std::string& logstore,
                                 const std::string& filename,
                                 time_t businessTime,
                                 int32_t succeededLines,
                                 const std::string& status) {
    time_t curTime(time(NULL));
    Log* logPtr = logGroup.add_logs();
    logPtr->set_time(curTime);

    Log_Content* contentPtr = logPtr->add_contents();
    contentPtr->set_key("project_name");
    contentPtr->set_value(projectName);

    contentPtr = logPtr->add_contents();
    contentPtr->set_key("logstore");
    contentPtr->set_value(logstore);

    contentPtr = logPtr->add_contents();
    contentPtr->set_key("filename");
    contentPtr->set_value(filename);

    contentPtr = logPtr->add_contents();
    contentPtr->set_key("business_time");
    contentPtr->set_value(ToString(businessTime));

    contentPtr = logPtr->add_contents();
    contentPtr->set_key("heartbeat_time");
    contentPtr->set_value(ToString(curTime));

    contentPtr = logPtr->add_contents();
    contentPtr->set_key("succeeded_lines");
    contentPtr->set_value(ToString(succeededLines));

    contentPtr = logPtr->add_contents();
    contentPtr->set_key("status");
    contentPtr->set_value(status);
}

size_t LogIntegrity::GetMapItemCount() {
    // unlocked
    size_t size = 0;
    for (RegionLogIntegrityInfoMap::iterator regionIter = mRegionLogIntegrityInfoMap.begin();
         regionIter != mRegionLogIntegrityInfoMap.end();
         ++regionIter) {
        LogIntegrityInfoMap* logIntegrityInfoMap = regionIter->second;
        size += logIntegrityInfoMap->size();
    }
    return size;
}

bool LogIntegrity::SortByLastUpdateTime(const std::pair<std::string, LogIntegrityInfo*>& lhs,
                                        const std::pair<std::string, LogIntegrityInfo*>& rhs) {
    return lhs.second->mLastUpdateTime < rhs.second->mLastUpdateTime;
}

void LogIntegrity::SerializeListNodes(const std::list<LogTimeInfo>& logTimeInfoList, Json::Value& value) {
    for (std::list<LogTimeInfo>::const_iterator iter = logTimeInfoList.begin(); iter != logTimeInfoList.end(); ++iter) {
        Json::Value node;
        node["seq_num"] = (Json::Int64)iter->mLogGroupSequenceNum;
        node["log_time"] = (Json::Int)iter->mLogTime;
        node["succeeded_lines"] = (Json::Int)iter->mSucceededLines;
        node["status"] = (Json::Int)iter->mStatus;

        value.append(node);
    }
}

void LogIntegrity::DeSerializeListNodes(const Json::Value& value, std::list<LogTimeInfo>& logTimeInfoList) {
    const Json::Value::Members& names = value.getMemberNames();
    for (Json::ArrayIndex i = 0; i < names.size(); ++i) {
        LogTimeInfo logTimeInfo;
        const std::string& name = names[i];
        if (name.compare("seq_num") == 0)
            logTimeInfo.mLogGroupSequenceNum = GetInt64Value(value, name);
        else if (name.compare("log_time") == 0)
            logTimeInfo.mLogTime = (time_t)GetIntValue(value, name);
        else if (name.compare("succeeded_lines") == 0)
            logTimeInfo.mSucceededLines = GetIntValue(value, name);
        else if (name.compare("status") == 0)
            logTimeInfo.mStatus = GetIntValue(value, name);
        else
            LOG_ERROR(sLogger, ("invalid member in json, name", name));
    }
}

void LogIntegrity::FillLogTimeInfo(LogIntegrityInfo* info, const std::string& name, const Json::Value& value) {
    if (name.compare("data_integrity_logstore") == 0)
        info->mIntegrityLogstore = GetStringValue(value, name);
    else if (name.compare("data_integrity_project_name") == 0)
        info->mIntegrityProject = GetStringValue(value, name);
    else if (name.compare("filename") == 0)
        info->mFilename = GetStringValue(value, name);
    else if (name.compare("last_update_time") == 0)
        info->mLastUpdateTime = (time_t)GetIntValue(value, name);
    else if (name.compare("list") == 0)
        LogIntegrity::DeSerializeListNodes(value, info->mLogTimeInfoList);
    else if (name.compare("logstore") == 0)
        info->mLogstore = GetStringValue(value, name);
    else if (name.compare("project_name") == 0)
        info->mProjectName = GetStringValue(value, name);
    else if (name.compare("region") == 0)
        info->mRegion = GetStringValue(value, name);
    else if (name.compare("succeeded_flag") == 0)
        info->mSendSucceededFlag = GetBoolValue(value, name);
    else
        LOG_ERROR(sLogger, ("invalid field name", name));
}

std::string LogIntegrity::GetLastLogLine(MergeItem* item) {
    const LogGroup& logGroup = item->mLogGroup;
    if (logGroup.logs_size() < 1) {
        LOG_ERROR(sLogger, ("invalid log group, size", logGroup.logs_size()));
        return "";
    }

    const Log& lastLogLine = logGroup.logs(logGroup.logs_size() - 1);
    if (lastLogLine.contents_size() != 1) {
        LOG_ERROR(sLogger, ("invalid log group, content size", lastLogLine.contents_size()));
        return "";
    }

    const Log_Content& logContent = lastLogLine.contents(0);
    return logContent.value();
}

void LogIntegrity::InsertItemIntoOutDatedFileMap(LogIntegrityInfo* info) {
    // find region
    RegionOutDatedFileMap::iterator regionIter = mRegionOutDatedFileMap.find(info->mRegion);
    if (regionIter == mRegionOutDatedFileMap.end()) {
        OutDatedFileMap* outDatedFileMap = new OutDatedFileMap;
        regionIter = mRegionOutDatedFileMap.insert(std::make_pair(info->mRegion, outDatedFileMap)).first;
    }
    OutDatedFileMap* outDatedFileMap = regionIter->second;

    std::string key = GetIntegrityInfoKey(info->mProjectName, info->mLogstore, info->mFilename);
    OutDatedFileMap::iterator fileIter = outDatedFileMap->find(key);
    if (fileIter == outDatedFileMap->end()) {
        OutDatedFile* outDatedFile = new OutDatedFile(info->mAliuid,
                                                      info->mRegion,
                                                      info->mProjectName,
                                                      info->mLogstore,
                                                      info->mFilename,
                                                      info->mIntegrityProject,
                                                      info->mIntegrityLogstore,
                                                      info->mLastUpdateTime);
        outDatedFileMap->insert(std::make_pair(key, outDatedFile));
    }
    LOG_INFO(
        sLogger,
        ("insert item into out-dated file map, region", info->mRegion)("project", info->mProjectName)(
            "log store", info->mLogstore)("filename", info->mFilename)("integrity project", info->mIntegrityProject)(
            "integrity log store", info->mIntegrityLogstore)("last update time", info->mLastUpdateTime));
}

time_t LogIntegrity::EraseItemInOutDatedFileMap(const std::string& region,
                                                const std::string& project,
                                                const std::string& logstore,
                                                const std::string& filename) {
    // unlock
    time_t lastUpdateTime = -1;
    if (mRegionOutDatedFileMap.empty())
        return lastUpdateTime;

    // find region
    RegionOutDatedFileMap::iterator regionIter = mRegionOutDatedFileMap.find(region);
    if (regionIter == mRegionOutDatedFileMap.end()) {
        LOG_DEBUG(sLogger,
                  ("failed to find item in out-dated file map, region",
                   region)("project", project)("log store", logstore)("filename", filename));
        return lastUpdateTime;
    }
    OutDatedFileMap* outDatedFileMap = regionIter->second;

    std::string key = GetIntegrityInfoKey(project, logstore, filename);
    OutDatedFileMap::iterator fileIter = outDatedFileMap->find(key);
    if (fileIter == outDatedFileMap->end()) {
        LOG_DEBUG(sLogger,
                  ("failed to find item in out-dated file map, region",
                   region)("project", project)("log store", logstore)("filename", filename));
        return lastUpdateTime;
    }
    // erase item
    lastUpdateTime = fileIter->second->mLastUpdateTime;
    outDatedFileMap->erase(fileIter);
    if (outDatedFileMap->empty()) {
        mRegionOutDatedFileMap.erase(regionIter);
        LOG_DEBUG(sLogger, ("erase item in out-dated file map, region", region));
    }

    return lastUpdateTime;
}

void LogIntegrity::SendOutDatedFileIntegrityInfo() {
    // unlock
    if (mRegionOutDatedFileMap.empty())
        return;

    std::map<LogDest, LogGroup> logGroupMap;
    for (RegionOutDatedFileMap::iterator regionIter = mRegionOutDatedFileMap.begin();
         regionIter != mRegionOutDatedFileMap.end();
         ++regionIter) {
        const std::string& region = regionIter->first;
        OutDatedFileMap* outDatedFileMap = regionIter->second;

        if (outDatedFileMap->empty())
            continue;

        // build log groups, one integrity log store one log group
        for (OutDatedFileMap::iterator fileIter = outDatedFileMap->begin(); fileIter != outDatedFileMap->end();
             ++fileIter) {
            OutDatedFile* outDatedFile = fileIter->second;
            // check whether it's valid to send data, if it's not valid, we should not calc integrity info
            LogstoreFeedBackKey feedbackKey
                = GenerateLogstoreFeedBackKey(outDatedFile->mIntegrityProject, outDatedFile->mIntegrityLogstore);
            if (!Sender::Instance()->GetSenderFeedBackInterface()->IsValidToPush(feedbackKey))
                continue;

            LogDest dst(
                outDatedFile->mAliuid, region, outDatedFile->mIntegrityProject, outDatedFile->mIntegrityLogstore);
            std::map<LogDest, LogGroup>::iterator iter = logGroupMap.find(dst);
            if (iter == logGroupMap.end()) {
                LogGroup logGroup;
                iter = logGroupMap.insert(std::make_pair(dst, logGroup)).first;
            }
            LogGroup& logGroup = iter->second;

            // out-dated file log group, status is TooOld, integrity time is -1
            BuildLogGroup(logGroup,
                          outDatedFile->mProjectName,
                          outDatedFile->mLogstore,
                          outDatedFile->mFilename,
                          -1,
                          0,
                          "TooOld");
            LOG_DEBUG(sLogger,
                      ("build log group, size", logGroup.logs_size())("region", outDatedFile->mRegion)(
                          "project", outDatedFile->mProjectName)("logstore", outDatedFile->mLogstore)(
                          "filename", outDatedFile->mFilename)("integrity time", -1)("lines", 0)("status", "TooOld"));
        }

        // send
        for (std::map<LogDest, LogGroup>::iterator iter = logGroupMap.begin(); iter != logGroupMap.end(); ++iter) {
            const LogDest& dst = iter->first;
            LogGroup& logGroup = iter->second;

            logGroup.set_source(LogFileProfiler::mIpAddr);
            logGroup.set_category(dst.mLogstore);
            logGroup.set_machineuuid(ConfigManager::GetInstance()->GetUUID());

            LogTag* logTagPtr = logGroup.add_logtags();
            logTagPtr->set_key(LOG_RESERVED_KEY_HOSTNAME);
            logTagPtr->set_value(LogFileProfiler::mHostname);

            // send integrity log group
            bool sendSucceeded
                = mProfileSender.SendInstantly(logGroup, dst.mAliuid, dst.mRegion, dst.mProjectName, dst.mLogstore);
            if (!sendSucceeded) {
                LogtailAlarm::GetInstance()->SendAlarm(DISCARD_DATA_ALARM,
                                                       "push data integrity data into batch map fail",
                                                       dst.mProjectName,
                                                       dst.mLogstore,
                                                       region);
                LOG_ERROR(sLogger,
                          ("failed to push data integrity data into batch map, discard logs", logGroup.logs_size())(
                              "region", region)("project", dst.mProjectName)("logstore", dst.mLogstore));
            }
        }
    }
}

#ifdef LOGTAIL_DEBUG_FLAG
void LogIntegrity::PrintLogTimeInfoListStatus() {
    if (mRegionLogIntegrityInfoMap.empty()) {
        LOG_DEBUG(sLogger, ("region map is empty", "please check"));
        return;
    }

    for (RegionLogIntegrityInfoMap::const_iterator regionIter = mRegionLogIntegrityInfoMap.begin();
         regionIter != mRegionLogIntegrityInfoMap.end();
         ++regionIter) {
        const string& region = regionIter->first;
        const LogIntegrityInfoMap* logIntegrityInfoMap = regionIter->second;
        if (logIntegrityInfoMap->empty())
            continue;

        for (LogIntegrityInfoMap::const_iterator projectLogStoreFilenameIter = logIntegrityInfoMap->begin();
             projectLogStoreFilenameIter != logIntegrityInfoMap->end();
             ++projectLogStoreFilenameIter) {
            const LogIntegrityInfo* logIntegrityInfo = projectLogStoreFilenameIter->second;
            const list<LogTimeInfo>& logTimeInfoList = logIntegrityInfo->mLogTimeInfoList;
            vector<int64_t> SendOKSeqNums, SendFailSeqNums;
            for (list<LogTimeInfo>::const_iterator listIter = logTimeInfoList.begin();
                 listIter != logTimeInfoList.end();
                 ++listIter) {
                if (listIter->mStatus % 2 == LogTimeInfo::LogIntegrityStatus_SendOK)
                    SendOKSeqNums.push_back(listIter->mLogGroupSequenceNum);
                else
                    SendFailSeqNums.push_back(listIter->mLogGroupSequenceNum);
            }

            // dump
            if (SendOKSeqNums.size() > 20)
                LOG_DEBUG(sLogger,
                          ("send OK seq num, region",
                           region)("project", logIntegrityInfo->mProjectName)("log store", logIntegrityInfo->mLogstore)(
                              "file name", logIntegrityInfo->mFilename)("OK size", SendOKSeqNums.size()));
            else {
                string SendOKSeqNumsStr;
                for (size_t i = 0; i < SendOKSeqNums.size(); ++i) {
                    SendOKSeqNumsStr.append(ToString(SendOKSeqNums[i]));
                    if (i != SendOKSeqNums.size() - 1)
                        SendOKSeqNumsStr.append(",");
                }
                LOG_DEBUG(sLogger,
                          ("send OK seq num, region",
                           region)("project", logIntegrityInfo->mProjectName)("log store", logIntegrityInfo->mLogstore)(
                              "file name", logIntegrityInfo->mFilename)("OK seq num", SendOKSeqNumsStr));
            }

            if (SendFailSeqNums.size() > 20)
                LOG_DEBUG(sLogger,
                          ("send fail seq num, region",
                           region)("project", logIntegrityInfo->mProjectName)("log store", logIntegrityInfo->mLogstore)(
                              "file name", logIntegrityInfo->mFilename)("fail size", SendFailSeqNums.size()));
            else {
                string SendFailSeqNumsStr;
                for (size_t i = 0; i < SendFailSeqNums.size(); ++i) {
                    SendFailSeqNumsStr.append(ToString(SendFailSeqNums[i]));
                    if (i != SendFailSeqNums.size() - 1)
                        SendFailSeqNumsStr.append(",");
                }
                LOG_DEBUG(sLogger,
                          ("send fail seq num, region",
                           region)("project", logIntegrityInfo->mProjectName)("log store", logIntegrityInfo->mLogstore)(
                              "file name", logIntegrityInfo->mFilename)("fail seq num", SendFailSeqNumsStr));
            }
        }
    }
}
#endif
} // namespace logtail
