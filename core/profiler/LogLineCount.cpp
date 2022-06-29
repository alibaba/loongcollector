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

#include <utility>
#include "LogLineCount.h"
#include "LogstoreSenderQueue.h"
#include "LogFileProfiler.h"
#include "common/LogtailCommonFlags.h"
#include "common/StringTools.h"
#include "common/RuntimeUtil.h"
#include "common/JsonUtil.h"
#include "common/ExceptionBase.h"
#include "config_manager/ConfigManager.h"
#include "Sender.h"
#include "LogtailAlarm.h"

using namespace sls_logs;
using namespace std;

DEFINE_FLAG_INT32(line_count_data_send_interval, "interval of sending line count data, seconds", 60);

namespace logtail {

LogLineCount::RegionLineCountMap LogLineCount::mRegionLineCountMap;

LogLineCount::LogLineCount() {
    mLineCountSendInterval = INT32_FLAG(line_count_data_send_interval);
    mLineCountLastSendTime = time(NULL) - (rand() % (mLineCountSendInterval / 10)) * 10;
    mLineCountDumpFileName = GetProcessExecutionDir() + STRING_FLAG(logtail_line_count_snapshot);
    mBakLineCountDumpFileName = GetProcessExecutionDir() + STRING_FLAG(logtail_line_count_snapshot) + "_bak";
}

LogLineCount::~LogLineCount() {
    ClearLineCountData();
}

void LogLineCount::NotifySuccess(LoggroupTimeValue* data) {
    const std::string& region = data->mRegion;
    const std::string& projectName = data->mProjectName;
    const std::string& logStore = data->mLogstore;
    const std::string& filename = data->mFilename;

    // empty filename, filter metric data and data-integrity data
    if (filename.empty()) {
        LOG_DEBUG(sLogger,
                  ("successfully send metric data or integrity data or line count data, region",
                   region)("project", projectName)("logStore", logStore));
        return;
    }

    // update line count map
    PTScopedLock lock(mLineCountMapLock);
    RegionLineCountMap::iterator regionIter = mRegionLineCountMap.find(region);
    if (regionIter == mRegionLineCountMap.end()) {
        LogStoreLineCountMap* logStoreLineCountMap = new LogStoreLineCountMap;
        regionIter = mRegionLineCountMap.insert(std::make_pair(region, logStoreLineCountMap)).first;
    }
    LogStoreLineCountMap* logStoreLineCountMap = regionIter->second;

    const string& key = projectName + "_" + logStore;
    LogStoreLineCountMap::iterator projectLogStoreIter = logStoreLineCountMap->find(key);
    if (projectLogStoreIter == logStoreLineCountMap->end()) {
        LogStoreLineCount* lineCount
            = new LogStoreLineCount(region,
                                    projectName,
                                    logStore,
                                    data->mLogGroupContext.mLineCountConfigPtr->mLineCountProjectName,
                                    data->mLogGroupContext.mLineCountConfigPtr->mLineCountLogstore);
        projectLogStoreIter = logStoreLineCountMap->insert(std::make_pair(key, lineCount)).first;
    }
    LogStoreLineCount* lineCount = projectLogStoreIter->second;

    const int32_t logLines = data->mLogLines;
    const int32_t minuteTime = data->mLogTimeInMinute;
    LogStoreLineCount::LogCountPerMinuteMap::iterator minuteIter = lineCount->mLogCountPerMinuteMap.find(minuteTime);
    if (minuteIter == lineCount->mLogCountPerMinuteMap.end())
        lineCount->mLogCountPerMinuteMap.insert(std::make_pair(minuteTime, logLines));
    else
        minuteIter->second += logLines;

    LOG_DEBUG(sLogger,
              ("update succeeded lines, region",
               region)("project", projectName)("logStore", logStore)("minute", minuteTime)("count", logLines));
}

void LogLineCount::InsertLineCountDataToLogGroup(sls_logs::LogGroup& logGroup,
                                                 const std::string& region,
                                                 const std::string& projectName,
                                                 const std::string& logStore,
                                                 int32_t minuteTime,
                                                 int32_t count) {
    Log* logPtr = logGroup.add_logs();
    logPtr->set_time(time(NULL));

    Log_Content* contentPtr = logPtr->add_contents();
    contentPtr->set_key("region");
    contentPtr->set_value(region);

    contentPtr = logPtr->add_contents();
    contentPtr->set_key("project_name");
    contentPtr->set_value(projectName);

    contentPtr = logPtr->add_contents();
    contentPtr->set_key("logstore");
    contentPtr->set_value(logStore);

    contentPtr = logPtr->add_contents();
    contentPtr->set_key("time");
    contentPtr->set_value(ToString(minuteTime));

    contentPtr = logPtr->add_contents();
    contentPtr->set_key("count");
    contentPtr->set_value(ToString(count));
}

void LogLineCount::SendLineCountData(bool forceSend) {
    int32_t curTime = time(NULL);
    if (!forceSend && (curTime - mLineCountLastSendTime < mLineCountSendInterval))
        return;

    {
        // send data(read map), then clear map, so we need to lock
        PTScopedLock lock(mLineCountMapLock);
        mLineCountLastSendTime = curTime;
        if (mRegionLineCountMap.empty()) {
            return;
        }
        for (RegionLineCountMap::iterator regionIter = mRegionLineCountMap.begin();
             regionIter != mRegionLineCountMap.end();
             ++regionIter) {
            LogStoreLineCountMap* logStoreLineCountMap = regionIter->second;
            const std::string& region = regionIter->first;
            LogGroup logGroup;
            const std::string& lineCountProject = logStoreLineCountMap->begin()->second->mCntProjectName;
            const std::string& lineCountLogStore = logStoreLineCountMap->begin()->second->mCntLogStore;

            // check whether it's valid to send data
            LogstoreFeedBackKey feedbackKey = GenerateLogstoreFeedBackKey(lineCountProject, lineCountLogStore);
            if (!Sender::Instance()->GetSenderFeedBackInterface()->IsValidToPush(feedbackKey))
                continue;

            // NOTICE: line count info in a region is gathered by one line count log store
            for (LogStoreLineCountMap::iterator projectLogStoreIter = logStoreLineCountMap->begin();
                 projectLogStoreIter != logStoreLineCountMap->end();
                 ++projectLogStoreIter) {
                LogStoreLineCount* lineCount = projectLogStoreIter->second;
                // traverse item in lineCount->mLogCountPerMinuteMap
                for (LogStoreLineCount::LogCountPerMinuteMap::iterator minuteTimeIter
                     = lineCount->mLogCountPerMinuteMap.begin();
                     minuteTimeIter != lineCount->mLogCountPerMinuteMap.end();
                     ++minuteTimeIter) {
                    InsertLineCountDataToLogGroup(logGroup,
                                                  region,
                                                  lineCount->mProjectName,
                                                  lineCount->mBizLogStore,
                                                  minuteTimeIter->first,
                                                  minuteTimeIter->second);
                }
            }
            logGroup.set_category(lineCountLogStore);
            logGroup.set_machineuuid(ConfigManager::GetInstance()->GetUUID());

            LogTag* logTagPtr = logGroup.add_logtags();
            logTagPtr->set_key("__hostname__"); // NOTICE: "__hostname__" for kepler
            logTagPtr->set_value(LogFileProfiler::mHostname);

            logTagPtr = logGroup.add_logtags();
            logTagPtr->set_key("ip");
            logTagPtr->set_value(LogFileProfiler::mIpAddr);

            mProfileSender.SendToLineCountProject(region, lineCountProject, logGroup);
            LOG_DEBUG(
                sLogger,
                ("send line count data, region", region)("project", lineCountProject)("log store", lineCountLogStore));
        }
    }
    // clear map after sending log group
    ClearLineCountData();
}

void LogLineCount::DumpLineCountDataToLocal() {
    {
        PTScopedLock lock(mLineCountMapLock);
        if (mRegionLineCountMap.empty()) {
            LOG_DEBUG(sLogger, ("no line count data to dump", ""));
            return;
        }
    }
    // to json
    Json::Value root;
    {
        // lock
        PTScopedLock lock(mLineCountMapLock);
        // traverse line count map
        for (RegionLineCountMap::iterator regionIter = mRegionLineCountMap.begin();
             regionIter != mRegionLineCountMap.end();
             ++regionIter) {
            const std::string& region = regionIter->first;
            LogStoreLineCountMap* lineCountMap = regionIter->second;

            Json::Value regionValue;
            for (LogStoreLineCountMap::iterator projectLogStoreIter = lineCountMap->begin();
                 projectLogStoreIter != lineCountMap->end();
                 ++projectLogStoreIter) {
                const std::string& projectLogStore = projectLogStoreIter->first;
                LogStoreLineCount* lineCount = projectLogStoreIter->second;

                Json::Value projectLogStoreValue;
                Json::Value countValue;
                for (LogStoreLineCount::LogCountPerMinuteMap::iterator minuteCountIter
                     = lineCount->mLogCountPerMinuteMap.begin();
                     minuteCountIter != lineCount->mLogCountPerMinuteMap.end();
                     ++minuteCountIter) {
                    uint32_t minuteTime = minuteCountIter->first;
                    uint32_t count = minuteCountIter->second;

                    Json::Value minuteCountValue;
                    minuteCountValue["minute"] = (Json::Int)minuteTime;
                    minuteCountValue["count"] = (Json::Int)count;
                    countValue.append(minuteCountValue);
                }
                projectLogStoreValue["count"] = countValue;
                projectLogStoreValue["region"] = lineCount->mRegion;
                projectLogStoreValue["biz_project_name"] = lineCount->mProjectName;
                projectLogStoreValue["biz_logstore"] = lineCount->mBizLogStore;
                projectLogStoreValue["cnt_project_name"] = lineCount->mCntProjectName;
                projectLogStoreValue["cnt_logstore"] = lineCount->mCntLogStore;

                regionValue[projectLogStore.c_str()] = projectLogStoreValue;
            }
            root[region.c_str()] = regionValue;
        }
    }

    FILE* file = fopen(mBakLineCountDumpFileName.c_str(), "w");
    if (file == NULL) {
        LOG_ERROR(sLogger, ("failed to open file", mBakLineCountDumpFileName)("error", strerror(errno)));
        return;
    }
    std::string dumpContent = root.toStyledString();
    fwrite(dumpContent.c_str(), 1, dumpContent.size(), file);
    fclose(file);
    if (rename(mBakLineCountDumpFileName.c_str(), mLineCountDumpFileName.c_str()) == -1)
        LOG_ERROR(sLogger, ("failed to rename line count file", mLineCountDumpFileName)("error", strerror(errno)));
}

bool LogLineCount::ReloadLineCountDataFromLocalFile() {
    Json::Value root;
    ParseConfResult result = ParseConfig(mLineCountDumpFileName, root);
    if (result != CONFIG_OK) {
        if (result == CONFIG_NOT_EXIST)
            LOG_INFO(sLogger, ("no line count file to load", mLineCountDumpFileName));
        else if (result == CONFIG_INVALID_FORMAT) {
            LOG_ERROR(sLogger, ("load line count file fail, file content is not valid json", mLineCountDumpFileName));
            LogtailAlarm::GetInstance()->SendAlarm(CHECKPOINT_ALARM, "content of line count file is not valid json");
        }
        return false;
    }

    // parse file and reload data
    std::string errorMessage;
    const Json::Value::Members& regions = root.getMemberNames();
    for (size_t i = 0; i < regions.size(); ++i) {
        const std::string& region = regions[i];
        LogStoreLineCountMap* lineCountMap = new LogStoreLineCountMap;

        const Json::Value& regionValue = root[region];
        const Json::Value::Members& projectLogstores = regionValue.getMemberNames();
        for (size_t j = 0; j < projectLogstores.size(); ++j) {
            const std::string& projectLogstore = projectLogstores[j];
            LogStoreLineCount* lineCount = new LogStoreLineCount;

            const Json::Value& projectLogstoreValue = regionValue[projectLogstore];
            const Json::Value::Members& lineCountMembers = projectLogstoreValue.getMemberNames();
            for (size_t k = 0; k < lineCountMembers.size(); ++k) {
                const std::string& lineCountMember = lineCountMembers[k];
                try {
                    FillLineCountDataFromLocalFile(lineCount, lineCountMember, projectLogstoreValue);
                } catch (const ExceptionBase& e) {
                    LOG_ERROR(sLogger,
                              ("failed to parse log time info, region",
                               region)("project_logstore", projectLogstore)("Reason", e.GetExceptionMessage()));
                    errorMessage = "invalid json, reason: " + e.GetExceptionMessage();
                } catch (...) {
                    LOG_ERROR(sLogger,
                              ("failed to parse log time info, region", region)("project_logstore",
                                                                                projectLogstore)("Reason", "unknown"));
                    errorMessage = "invalid json, reason: unknown";
                }

                if (!errorMessage.empty()) {
                    LogtailAlarm::GetInstance()->SendAlarm(LOGTAIL_CONFIG_ALARM, errorMessage, region, projectLogstore);
                    errorMessage.clear();
                }
            }

            lineCountMap->insert(std::make_pair(projectLogstore, lineCount));
        }

        mRegionLineCountMap.insert(std::make_pair(region, lineCountMap));
    }

    // delete file
    if (remove(mLineCountDumpFileName.c_str()) != 0)
        LOG_INFO(sLogger, ("failed to delete line count file", mLineCountDumpFileName));

    return true;
}

void LogLineCount::ClearLineCountData() {
    PTScopedLock lock(mLineCountMapLock);

    for (RegionLineCountMap::iterator regionIter = mRegionLineCountMap.begin(); regionIter != mRegionLineCountMap.end();
         ++regionIter) {
        LogStoreLineCountMap* logStoreLineCountMap = regionIter->second;
        for (LogStoreLineCountMap::iterator logStoreIter = logStoreLineCountMap->begin();
             logStoreIter != logStoreLineCountMap->end();
             ++logStoreIter) {
            LogStoreLineCount* lineCount = logStoreIter->second;
            delete lineCount;
        }
        logStoreLineCountMap->clear();
        delete logStoreLineCountMap;
    }
    mRegionLineCountMap.clear();
}

void LogLineCount::EraseItemInMap(const std::string& region,
                                  const std::string& projectName,
                                  const std::string& logstore) {
    // lock
    PTScopedLock lock(mLineCountMapLock);

    if (mRegionLineCountMap.empty())
        return;

    // find region
    RegionLineCountMap::iterator regionIter = mRegionLineCountMap.find(region);
    if (regionIter == mRegionLineCountMap.end())
        return;

    LogStoreLineCountMap* logStoreLineCountMap = regionIter->second;
    // find line count
    std::string key = projectName + "_" + logstore;
    LogStoreLineCountMap::iterator projectLogstoreIter = logStoreLineCountMap->find(key);
    if (projectLogstoreIter == logStoreLineCountMap->end()) {
        return;
    } else {
        delete projectLogstoreIter->second;
        logStoreLineCountMap->erase(projectLogstoreIter);
        LOG_INFO(sLogger,
                 ("erase item in line count map, region", region)("project", projectName)("logstore", logstore));
    }

    // if line count map of a region is empty, then delete this map
    if (logStoreLineCountMap->empty()) {
        mRegionLineCountMap.erase(regionIter);
    }
}

void LogLineCount::FillLineCountDataFromLocalFile(LogStoreLineCount* lineCount,
                                                  const std::string& name,
                                                  const Json::Value& value) {
    if (name.compare("region") == 0)
        lineCount->mRegion = GetStringValue(value, name);
    else if (name.compare("biz_project_name") == 0)
        lineCount->mProjectName = GetStringValue(value, name);
    else if (name.compare("biz_logstore") == 0)
        lineCount->mBizLogStore = GetStringValue(value, name);
    else if (name.compare("cnt_project_name") == 0)
        lineCount->mCntProjectName = GetStringValue(value, name);
    else if (name.compare("cnt_logstore") == 0)
        lineCount->mCntLogStore = GetStringValue(value, name);
    else if (name.compare("count") == 0) {
        // check valid array
        CheckNameExist(value, name);
        if (!value[name].isArray()) {
            throw ExceptionBase(string("The key '") + name + "' not valid array value");
        }
        Json::Value countValues = value[name];

        int32_t count = 0, minute = -1;
        for (Json::ArrayIndex i = 0; i < countValues.size(); ++i) {
            const Json::Value& countValue = countValues[i];
            const Json::Value::Members& memberNames = countValue.getMemberNames();
            for (size_t j = 0; j < memberNames.size(); ++j) {
                const std::string& memberName = memberNames[j];
                if (memberName.compare("count") == 0)
                    count = GetIntValue(countValue, memberName);
                else if (memberName.compare("minute") == 0)
                    minute = GetIntValue(countValue, memberName);
                else
                    LOG_ERROR(sLogger, ("invalid field name in count map", memberName));
            }
            lineCount->mLogCountPerMinuteMap.insert(std::make_pair(minute, count));
        }
    } else
        LOG_ERROR(sLogger, ("invalid field name", name));
}

} // namespace logtail
