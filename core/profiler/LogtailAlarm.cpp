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

#include "LogtailAlarm.h"
#include "common/Constants.h"
#include "common/StringTools.h"
#include "common/Thread.h"
#include "common/LogtailCommonFlags.h"
#include "common/version.h"
#include "common/TimeUtil.h"
#include "log_pb/sls_logs.pb.h"
#include "sender/Sender.h"
#include "config_manager/ConfigManager.h"
#include "app_config/AppConfig.h"
#include "LogFileProfiler.h"

DEFINE_FLAG_INT32(logtail_alarm_interval, "the interval of two same type alarm message", 30);
DEFINE_FLAG_INT32(logtail_low_level_alarm_speed, "the speed(count/second) which logtail's low level alarm allow", 100);

using namespace std;
using namespace logtail;
using namespace sls_logs;

namespace logtail {

LogtailAlarm::LogtailAlarm() {
    mMessageType.resize(ALL_LOGTAIL_ALARM_NUM);
    mMessageType[USER_CONFIG_ALARM] = "USER_CONFIG_ALARM";
    mMessageType[GLOBAL_CONFIG_ALARM] = "GLOBAL_CONFIG_ALARM";
    mMessageType[DOMAIN_SOCKET_BIND_ALARM] = "DOMAIN_SOCKET_BIND_ALARM";
    mMessageType[SECONDARY_READ_WRITE_ALARM] = "SECONDARY_READ_WRITE_ALARM";
    mMessageType[LOGFILE_PERMINSSION_ALARM] = "LOGFILE_PERMINSSION_ALARM";
    mMessageType[SEND_QUOTA_EXCEED_ALARM] = "SEND_QUOTA_EXCEED_ALARM";
    mMessageType[LOGTAIL_CRASH_ALARM] = "LOGTAIL_CRASH_ALARM";
    mMessageType[INOTIFY_DIR_NUM_LIMIT_ALARM] = "INOTIFY_DIR_NUM_LIMIT_ALARM";
    mMessageType[EPOLL_ERROR_ALARM] = "EPOLL_ERROR_ALARM";
    mMessageType[DISCARD_DATA_ALARM] = "DISCARD_DATA_ALARM";
    mMessageType[MULTI_CONFIG_MATCH_ALARM] = "MULTI_CONFIG_MATCH_ALARM";
    mMessageType[READ_LOG_DELAY_ALARM] = "READ_LOG_DELAY_ALARM";
    mMessageType[REGISTER_INOTIFY_FAIL_ALARM] = "REGISTER_INOTIFY_FAIL_ALARM";
    mMessageType[LOGTAIL_CONFIG_ALARM] = "LOGTAIL_CONFIG_ALARM";
    mMessageType[ENCRYPT_DECRYPT_FAIL_ALARM] = "ENCRYPT_DECRYPT_FAIL_ALARM";
    mMessageType[LOG_GROUP_PARSE_FAIL_ALARM] = "LOG_GROUP_PARSE_FAIL_ALARM";
    mMessageType[METRIC_GROUP_PARSE_FAIL_ALARM] = "METRIC_GROUP_PARSE_FAIL_ALARM";
    mMessageType[LOGDIR_PERMINSSION_ALARM] = "LOGDIR_PERMINSSION_ALARM";
    mMessageType[REGEX_MATCH_ALARM] = "REGEX_MATCH_ALARM";
    mMessageType[DISCARD_SECONDARY_ALARM] = "DISCARD_SECONDARY_ALARM";
    mMessageType[BINARY_UPDATE_ALARM] = "BINARY_UPDATE_ALARM";
    mMessageType[CONFIG_UPDATE_ALARM] = "CONFIG_UPDATE_ALARM";
    mMessageType[CHECKPOINT_ALARM] = "CHECKPOINT_ALARM";
    mMessageType[CATEGORY_CONFIG_ALARM] = "CATEGORY_CONFIG_ALARM";
    mMessageType[INOTIFY_EVENT_OVERFLOW_ALARM] = "INOTIFY_EVENT_OVERFLOW_ALARM";
    mMessageType[INVALID_MEMORY_ACCESS_ALARM] = "INVALID_MEMORY_ACCESS_ALARM";
    mMessageType[ENCODING_CONVERT_ALARM] = "ENCODING_CONVERT_ALARM";
    mMessageType[SPLIT_LOG_FAIL_ALARM] = "SPLIT_LOG_FAIL_ALARM";
    mMessageType[OPEN_LOGFILE_FAIL_ALARM] = "OPEN_LOGFILE_FAIL_ALARM";
    mMessageType[SEND_DATA_FAIL_ALARM] = "SEND_DATA_FAIL_ALARM";
    mMessageType[PARSE_TIME_FAIL_ALARM] = "PARSE_TIME_FAIL_ALARM";
    mMessageType[OUTDATED_LOG_ALARM] = "OUTDATED_LOG_ALARM";
    mMessageType[STREAMLOG_TCP_SOCKET_BIND_ALARM] = "STREAMLOG_TCP_SOCKET_BIND_ALARM";
    mMessageType[SKIP_READ_LOG_ALARM] = "SKIP_READ_LOG_ALARM";
    mMessageType[LZ4_COMPRESS_FAIL_ALARM] = "LZ4_COMPRESS_FAIL_ALARM";
    mMessageType[PARSE_LOG_FAIL_ALARM] = "PARSE_LOG_FAIL_ALARM";
    mMessageType[LOG_TRUNCATE_ALARM] = "LOG_TRUNCATE_ALARM";
    mMessageType[DIR_EXCEED_LIMIT_ALARM] = "DIR_EXCEED_LIMIT_ALARM";
    mMessageType[STAT_LIMIT_ALARM] = "STAT_LIMIT_ALARM";
    mMessageType[FILE_READER_EXCEED_ALARM] = "FILE_READER_EXCEED_ALARM";
    mMessageType[LOGTAIL_CRASH_STACK_ALARM] = "LOGTAIL_CRASH_STACK_ALARM";
    mMessageType[MODIFY_FILE_EXCEED_ALARM] = "MODIFY_FILE_EXCEED_ALARM";
    mMessageType[TOO_MANY_CONFIG_ALARM] = "TOO_MANY_CONFIG_ALARM";
    mMessageType[OPEN_FILE_LIMIT_ALARM] = "OPEN_FILE_LIMIT_ALARM";
    mMessageType[SAME_CONFIG_ALARM] = "SAME_CONFIG_ALARM";
    mMessageType[PROCESS_QUEUE_BUSY_ALARM] = "PROCESS_QUEUE_BUSY_ALARM";
    mMessageType[DROP_LOG_ALARM] = "DROP_LOG_ALARM";
    mMessageType[CAST_SENSITIVE_WORD_ALARM] = "CAST_SENSITIVE_WORD_ALARM";
    mMessageType[PROCESS_TOO_SLOW_ALARM] = "PROCESS_TOO_SLOW_ALARM";
    mMessageType[LOAD_LOCAL_EVENT_ALARM] = "LOAD_LOCAL_EVENT_ALARM";
    mMessageType[WINDOWS_WORKER_START_HINTS_ALARM] = "WINDOWS_WORKER_START_HINTS_ALARM";
    mMessageType[HOLD_ON_TOO_SLOW_ALARM] = "HOLD_ON_TOO_SLOW_ALARM";
    mMessageType[INNER_PROFILE_ALARM] = "INNER_PROFILE_ALARM";
    mMessageType[FUSE_FILE_TRUNCATE_ALARM] = "FUSE_FILE_TRUNCATE_ALARM";
    mMessageType[SENDING_COSTS_TOO_MUCH_TIME_ALARM] = "SENDING_COSTS_TOO_MUCH_TIME_ALARM";
    mMessageType[UNEXPECTED_FILE_TYPE_MODE_ALARM] = "UNEXPECTED_FILE_TYPE_MODE_ALARM";
    mMessageType[LOG_GROUP_WAIT_TOO_LONG_ALARM] = "LOG_GROUP_WAIT_TOO_LONG_ALARM";
    mMessageType[CHECKPOINT_V2_ALARM] = "CHECKPOINT_V2_ALARM";
    mMessageType[EXACTLY_ONCE_ALARM] = "EXACTLY_ONCE_ALARM";

    new Thread([this]() { SendAlarmLoop(); });
}

LogtailAlarm::~LogtailAlarm() {
}

bool LogtailAlarm::SendAlarmLoop() {
    LogtailAlarmMessage* messagePtr = NULL;
    while (true) {
        int32_t currentTime = time(NULL);

        size_t sendRegionIndex = 0;
        size_t sendAlarmTypeIndex = 0;
        do {
            LogGroup logGroup;
            string region;
            {
                PTScopedLock lock(mAlarmBufferMutex);
                if (mAllAlarmMap.size() <= sendRegionIndex) {
                    break;
                }
                std::map<std::string, std::pair<LogtailAlarmVector*, std::vector<int32_t> > >::iterator allAlarmIter
                    = mAllAlarmMap.begin();
                size_t iterIndex = 0;
                while (iterIndex != sendRegionIndex) {
                    ++iterIndex;
                    ++allAlarmIter;
                }
                region = allAlarmIter->first;
                //LOG_DEBUG(sLogger, ("1Send Alarm", region)("region", sendRegionIndex));
                LogtailAlarmVector& alarmBufferVec = *(allAlarmIter->second.first);
                std::vector<int32_t>& lastUpdateTimeVec = allAlarmIter->second.second;
                // check this region end
                if (sendAlarmTypeIndex >= alarmBufferVec.size()) {
                    // jump this region
                    ++sendRegionIndex;
                    sendAlarmTypeIndex = 0;
                    continue;
                }
                //LOG_DEBUG(sLogger, ("2Send Alarm", region)("region", sendRegionIndex)("alarm index", mMessageType[sendAlarmTypeIndex]));
                // check valid
                if (alarmBufferVec.size() != (size_t)ALL_LOGTAIL_ALARM_NUM
                    || lastUpdateTimeVec.size() != (size_t)ALL_LOGTAIL_ALARM_NUM) {
                    LOG_ERROR(sLogger,
                              ("invalid alarm item",
                               region)("alarm vec", alarmBufferVec.size())("update vec", lastUpdateTimeVec.size()));
                    // jump this region
                    ++sendRegionIndex;
                    sendAlarmTypeIndex = 0;
                    continue;
                }

                //LOG_DEBUG(sLogger, ("3Send Alarm", region)("region", sendRegionIndex)("alarm index", mMessageType[sendAlarmTypeIndex]));
                map<string, LogtailAlarmMessage*>& alarmMap = alarmBufferVec[sendAlarmTypeIndex];
                if (alarmMap.size() == 0
                    || currentTime - lastUpdateTimeVec[sendAlarmTypeIndex] < INT32_FLAG(logtail_alarm_interval)) {
                    // go next alarm type
                    ++sendAlarmTypeIndex;
                    continue;
                }
                // check sender queue status, if invalid jump this region
                LogstoreFeedBackKey alarmPrjLogstoreKey = GenerateLogstoreFeedBackKey(
                    ConfigManager::GetInstance()->GetProfileProjectName(region), string("logtail_alarm"));
                if (!Sender::Instance()->GetSenderFeedBackInterface()->IsValidToPush(alarmPrjLogstoreKey)) {
                    // jump this region
                    ++sendRegionIndex;
                    sendAlarmTypeIndex = 0;
                    continue;
                }

                //LOG_DEBUG(sLogger, ("4Send Alarm", region)("region", sendRegionIndex)("alarm index", mMessageType[sendAlarmTypeIndex]));
                logGroup.set_source(LogFileProfiler::mIpAddr);
                logGroup.set_category("logtail_alarm");
                for (map<string, LogtailAlarmMessage*>::iterator mapIter = alarmMap.begin(); mapIter != alarmMap.end();
                     ++mapIter) {
                    messagePtr = mapIter->second;

                    // LOG_DEBUG(sLogger, ("5Send Alarm", region)("region", sendRegionIndex)("alarm index", sendAlarmTypeIndex)("msg", messagePtr->mMessage));

                    Log* logPtr = logGroup.add_logs();
                    logPtr->set_time(AppConfig::GetInstance()->EnableLogTimeAutoAdjust() ? time(NULL) + GetTimeDelta()
                                                                                         : time(NULL));
                    Log_Content* contentPtr = logPtr->add_contents();
                    contentPtr->set_key("alarm_type");
                    contentPtr->set_value(messagePtr->mMessageType);

                    contentPtr = logPtr->add_contents();
                    contentPtr->set_key("alarm_message");
                    contentPtr->set_value(messagePtr->mMessage);

                    contentPtr = logPtr->add_contents();
                    contentPtr->set_key("alarm_count");
                    contentPtr->set_value(ToString(messagePtr->mCount));

                    contentPtr = logPtr->add_contents();
                    contentPtr->set_key("ip");
                    contentPtr->set_value(LogFileProfiler::mIpAddr);

                    contentPtr = logPtr->add_contents();
                    contentPtr->set_key("os");
                    contentPtr->set_value(OS_NAME);

                    contentPtr = logPtr->add_contents();
                    contentPtr->set_key("ver");
                    contentPtr->set_value(ILOGTAIL_VERSION);

                    if (!messagePtr->mProjectName.empty()) {
                        contentPtr = logPtr->add_contents();
                        contentPtr->set_key("project_name");
                        contentPtr->set_value(messagePtr->mProjectName);
                    }

                    if (!messagePtr->mCategory.empty()) {
                        contentPtr = logPtr->add_contents();
                        contentPtr->set_key("category");
                        contentPtr->set_value(messagePtr->mCategory);
                    }
                    delete messagePtr;
                }
                lastUpdateTimeVec[sendAlarmTypeIndex] = currentTime;
                alarmMap.clear();
                ++sendAlarmTypeIndex;
            }
            if (logGroup.logs_size() <= 0) {
                continue;
            }
            // this is an anonymous send and non lock send
            mProfileSender.SendToProfileProject(region, logGroup);
        } while (true);

        sleep(3);
    }
    return true;
}

LogtailAlarm::LogtailAlarmVector* LogtailAlarm::MakesureLogtailAlarmMapVecUnlocked(const string& region) {
    // @todo
    //string region;
    std::map<std::string, std::pair<LogtailAlarmVector*, std::vector<int32_t> > >::iterator iter
        = mAllAlarmMap.find(region);
    if (iter == mAllAlarmMap.end()) {
        LogtailAlarmVector* pMapVec = new LogtailAlarmVector;
        // need resize to init this obj
        pMapVec->resize(ALL_LOGTAIL_ALARM_NUM);

        int32_t now = time(NULL);
        std::vector<int32_t> lastUpdateTime;
        lastUpdateTime.resize(ALL_LOGTAIL_ALARM_NUM);
        for (uint32_t i = 0; i < ALL_LOGTAIL_ALARM_NUM; ++i)
            lastUpdateTime[i] = now - rand() % 180;
        mAllAlarmMap[region] = std::make_pair(pMapVec, lastUpdateTime);
        return pMapVec;
    }
    return iter->second.first;
}

void LogtailAlarm::SendAlarm(const LogtailAlarmType alarmType,
                             const std::string& message,
                             const std::string& projectName,
                             const std::string& category,
                             const std::string& region) {
    if (alarmType < 0 || alarmType >= ALL_LOGTAIL_ALARM_NUM) {
        return;
    }

    // ignore logtail self alarm
    string profileProject = ConfigManager::GetInstance()->GetProfileProjectName(region);
    if (!profileProject.empty() && profileProject == projectName) {
        return;
    }
    //LOG_DEBUG(sLogger, ("Add Alarm", region)("projectName", projectName)("alarm index", mMessageType[alarmType])("msg", message));
    std::lock_guard<std::mutex> lock(mAlarmBufferMutex);
    string key = projectName + "_" + category;
    LogtailAlarmVector& alarmBufferVec = *MakesureLogtailAlarmMapVecUnlocked(region);
    if (alarmBufferVec[alarmType].find(key) == alarmBufferVec[alarmType].end()) {
        LogtailAlarmMessage* messagePtr
            = new LogtailAlarmMessage(mMessageType[alarmType], projectName, category, message, 1);
        alarmBufferVec[alarmType].insert(pair<string, LogtailAlarmMessage*>(key, messagePtr));
    } else
        alarmBufferVec[alarmType][key]->IncCount();
}

void LogtailAlarm::ForceToSend() {
    INT32_FLAG(logtail_alarm_interval) = 0;
}

bool LogtailAlarm::IsLowLevelAlarmValid() {
    int32_t curTime = time(NULL);
    if (curTime == mLastLowLevelTime) {
        if (++mLastLowLevelCount > INT32_FLAG(logtail_low_level_alarm_speed)) {
            return false;
        }
    } else {
        mLastLowLevelTime = curTime;
        mLastLowLevelCount = 1;
    }
    return true;
}

} // namespace logtail
