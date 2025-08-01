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

#include "monitor/AlarmManager.h"

#include "app_config/AppConfig.h"
#include "collection_pipeline/queue/QueueKeyManager.h"
#include "collection_pipeline/queue/SenderQueueManager.h"
#include "common/LogtailCommonFlags.h"
#include "common/StringTools.h"
#include "common/Thread.h"
#include "common/TimeUtil.h"
#include "common/version.h"
#include "constants/Constants.h"
#include "monitor/SelfMonitorServer.h"
#include "protobuf/sls/sls_logs.pb.h"
#include "provider/Provider.h"

DEFINE_FLAG_INT32(logtail_alarm_interval, "the interval of two same type alarm message", 30);
DEFINE_FLAG_INT32(logtail_low_level_alarm_speed, "the speed(count/second) which logtail's low level alarm allow", 100);

using namespace std;
using namespace logtail;
using namespace sls_logs;

namespace logtail {

const string ALARM_SLS_LOGSTORE_NAME = "logtail_alarm";

AlarmManager::AlarmManager() {
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
    mMessageType[LOGDIR_PERMISSION_ALARM] = "LOGDIR_PERMISSION_ALARM";
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
    mMessageType[SEND_COMPRESS_FAIL_ALARM] = "SEND_COMPRESS_FAIL_ALARM";
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
    mMessageType[READ_STOPPED_CONTAINER_ALARM] = "READ_STOPPED_CONTAINER_ALARM";
    mMessageType[INVALID_CONTAINER_PATH_ALARM] = "INVALID_CONTAINER_PATH_ALARM";
    mMessageType[COMPRESS_FAIL_ALARM] = "COMPRESS_FAIL_ALARM";
    mMessageType[SERIALIZE_FAIL_ALARM] = "SERIALIZE_FAIL_ALARM";
    mMessageType[RELABEL_METRIC_FAIL_ALARM] = "RELABEL_METRIC_FAIL_ALARM";
    mMessageType[REGISTER_HANDLERS_TOO_SLOW_ALARM] = "REGISTER_HANDLERS_TOO_SLOW_ALARM";
}

void AlarmManager::FlushAllRegionAlarm(vector<PipelineEventGroup>& pipelineEventGroupList) {
    int32_t currentTime = time(nullptr);
    size_t sendRegionIndex = 0;
    size_t sendAlarmTypeIndex = 0;
    do {
        PTScopedLock lock(mAlarmBufferMutex);
        if (mAllAlarmMap.size() <= sendRegionIndex) {
            break;
        }
        auto allAlarmIter = mAllAlarmMap.begin();
        size_t iterIndex = 0;
        while (iterIndex != sendRegionIndex) {
            ++iterIndex;
            ++allAlarmIter;
        }
        string region = allAlarmIter->first;

        AlarmVector& alarmBufferVec = *(allAlarmIter->second.first);
        std::vector<int32_t>& lastUpdateTimeVec = allAlarmIter->second.second;
        // check this region end
        if (sendAlarmTypeIndex >= alarmBufferVec.size()) {
            // jump this region
            ++sendRegionIndex;
            sendAlarmTypeIndex = 0;
            continue;
        }

        //  check valid
        if (alarmBufferVec.size() != (size_t)ALL_LOGTAIL_ALARM_NUM
            || lastUpdateTimeVec.size() != (size_t)ALL_LOGTAIL_ALARM_NUM) {
            LOG_ERROR(sLogger,
                      ("invalid alarm item", region)("alarm vec", alarmBufferVec.size())("update vec",
                                                                                         lastUpdateTimeVec.size()));
            // jump this region
            ++sendRegionIndex;
            sendAlarmTypeIndex = 0;
            continue;
        }

        map<string, unique_ptr<AlarmMessage>>& alarmMap = alarmBufferVec[sendAlarmTypeIndex];
        if (alarmMap.size() == 0
            || currentTime - lastUpdateTimeVec[sendAlarmTypeIndex] < INT32_FLAG(logtail_alarm_interval)) {
            // go next alarm type
            ++sendAlarmTypeIndex;
            continue;
        }

        PipelineEventGroup pipelineEventGroup(std::make_shared<SourceBuffer>());
        pipelineEventGroup.SetTagNoCopy(LOG_RESERVED_KEY_SOURCE, LoongCollectorMonitor::mIpAddr);
        pipelineEventGroup.SetMetadata(EventGroupMetaKey::INTERNAL_DATA_TARGET_REGION, region);
        pipelineEventGroup.SetMetadata(EventGroupMetaKey::INTERNAL_DATA_TYPE,
                                       SelfMonitorServer::INTERNAL_DATA_TYPE_ALARM);
        auto now = GetCurrentLogtailTime();
        for (map<string, unique_ptr<AlarmMessage>>::iterator mapIter = alarmMap.begin(); mapIter != alarmMap.end();
             ++mapIter) {
            auto& messagePtr = mapIter->second;

            LogEvent* logEvent = pipelineEventGroup.AddLogEvent();
            logEvent->SetTimestamp(AppConfig::GetInstance()->EnableLogTimeAutoAdjust() ? now.tv_sec + GetTimeDelta()
                                                                                       : now.tv_sec);
            logEvent->SetContent("alarm_type", messagePtr->mMessageType);
            logEvent->SetContent("alarm_level", messagePtr->mLevel);
            logEvent->SetContent("alarm_message", messagePtr->mMessage);
            logEvent->SetContent("alarm_count", ToString(messagePtr->mCount));
            logEvent->SetContent("ip", LoongCollectorMonitor::mIpAddr);
            logEvent->SetContent("os", OS_NAME);
            logEvent->SetContent("ver", string(ILOGTAIL_VERSION));
            if (!messagePtr->mProjectName.empty()) {
                logEvent->SetContent("project_name", messagePtr->mProjectName);
            }
            if (!messagePtr->mCategory.empty()) {
                logEvent->SetContent("category", messagePtr->mCategory);
            }
            if (!messagePtr->mConfig.empty()) {
                logEvent->SetContent("config", messagePtr->mConfig);
            }
        }
        lastUpdateTimeVec[sendAlarmTypeIndex] = currentTime;
        alarmMap.clear();
        ++sendAlarmTypeIndex;

        if (pipelineEventGroup.GetEvents().size() <= 0) {
            continue;
        }
        // this is an anonymous send and non lock send
        pipelineEventGroupList.emplace_back(std::move(pipelineEventGroup));
    } while (true);
}

AlarmManager::AlarmVector* AlarmManager::MakesureLogtailAlarmMapVecUnlocked(const string& region) {
    // @todo
    // string region;
    auto iter = mAllAlarmMap.find(region);
    if (iter == mAllAlarmMap.end()) {
        // in windows, AlarmVector item move constructor is not noexcept, can't call resize.
        auto pMapVec = std::make_shared<AlarmVector>(ALL_LOGTAIL_ALARM_NUM);

        int32_t now = time(NULL);
        std::vector<int32_t> lastUpdateTime;
        lastUpdateTime.resize(ALL_LOGTAIL_ALARM_NUM);
        for (uint32_t i = 0; i < ALL_LOGTAIL_ALARM_NUM; ++i)
            lastUpdateTime[i] = now - rand() % 180;
        mAllAlarmMap[region] = std::make_pair(pMapVec, lastUpdateTime);
        return pMapVec.get();
    }
    return iter->second.first.get();
}

void AlarmManager::SendAlarm(const AlarmType& alarmType,
                             const AlarmLevel& level,
                             const std::string& message,
                             const std::string& region,
                             const std::string& projectName,
                             const std::string& config,
                             const std::string& category) {
    if (alarmType < 0 || alarmType >= ALL_LOGTAIL_ALARM_NUM) {
        return;
    }

    // ignore alarm for profile data
    if (GetProfileSender()->IsProfileData(region, projectName, category)) {
        return;
    }
    // LOG_DEBUG(sLogger, ("Add Alarm", region)("projectName", projectName)("alarm index",
    // mMessageType[alarmType])("msg", message));
    std::lock_guard<std::mutex> lock(mAlarmBufferMutex);
    string levelStr = ToString(level);
    string key = projectName + "_" + category + "_" + config + "_" + levelStr;
    AlarmVector& alarmBufferVec = *MakesureLogtailAlarmMapVecUnlocked(region);
    if (alarmBufferVec[alarmType].find(key) == alarmBufferVec[alarmType].end()) {
        auto* messagePtr
            = new AlarmMessage(mMessageType[alarmType], levelStr, projectName, category, config, message, 1);
        alarmBufferVec[alarmType].emplace(key, messagePtr);
    } else
        alarmBufferVec[alarmType][key]->IncCount();
}

void AlarmManager::ForceToSend() {
    INT32_FLAG(logtail_alarm_interval) = 0;
}

bool AlarmManager::IsLowLevelAlarmValid() {
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
