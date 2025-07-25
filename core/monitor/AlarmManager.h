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

#include <cstdint>
#include <stdio.h>

#include <atomic>
#include <condition_variable>
#include <future>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "common/Lock.h"
#include "models/PipelineEventGroup.h"

namespace logtail {

enum AlarmType {
    USER_CONFIG_ALARM = 0,
    GLOBAL_CONFIG_ALARM = 1,
    DOMAIN_SOCKET_BIND_ALARM = 2,
    SECONDARY_READ_WRITE_ALARM = 3,
    LOGFILE_PERMINSSION_ALARM = 4,
    SEND_QUOTA_EXCEED_ALARM = 5,
    LOGTAIL_CRASH_ALARM = 6,
    INOTIFY_DIR_NUM_LIMIT_ALARM = 7,
    EPOLL_ERROR_ALARM = 8,
    DISCARD_DATA_ALARM = 9,
    READ_LOG_DELAY_ALARM = 10,
    MULTI_CONFIG_MATCH_ALARM = 11,
    REGISTER_INOTIFY_FAIL_ALARM = 12,
    LOGTAIL_CONFIG_ALARM = 13,
    ENCRYPT_DECRYPT_FAIL_ALARM = 14,
    LOG_GROUP_PARSE_FAIL_ALARM = 15,
    METRIC_GROUP_PARSE_FAIL_ALARM = 16,
    LOGDIR_PERMISSION_ALARM = 17,
    REGEX_MATCH_ALARM = 18,
    DISCARD_SECONDARY_ALARM = 19,
    BINARY_UPDATE_ALARM = 20,
    CONFIG_UPDATE_ALARM = 21,
    CHECKPOINT_ALARM = 22,
    CATEGORY_CONFIG_ALARM = 23,
    INOTIFY_EVENT_OVERFLOW_ALARM = 24,
    INVALID_MEMORY_ACCESS_ALARM = 25,
    ENCODING_CONVERT_ALARM = 26,
    SPLIT_LOG_FAIL_ALARM = 27,
    OPEN_LOGFILE_FAIL_ALARM = 28,
    SEND_DATA_FAIL_ALARM = 29,
    PARSE_TIME_FAIL_ALARM = 30,
    OUTDATED_LOG_ALARM = 31,
    STREAMLOG_TCP_SOCKET_BIND_ALARM = 32,
    SKIP_READ_LOG_ALARM = 33,
    SEND_COMPRESS_FAIL_ALARM = 34,
    PARSE_LOG_FAIL_ALARM = 35,
    LOG_TRUNCATE_ALARM = 36,
    DIR_EXCEED_LIMIT_ALARM = 37,
    STAT_LIMIT_ALARM = 38,
    FILE_READER_EXCEED_ALARM = 39,
    LOGTAIL_CRASH_STACK_ALARM = 40,
    MODIFY_FILE_EXCEED_ALARM = 41,
    OPEN_FILE_LIMIT_ALARM = 42,
    TOO_MANY_CONFIG_ALARM = 43,
    SAME_CONFIG_ALARM = 44,
    PROCESS_QUEUE_BUSY_ALARM = 45,
    DROP_LOG_ALARM = 46,
    CAST_SENSITIVE_WORD_ALARM = 47,
    PROCESS_TOO_SLOW_ALARM = 48,
    LOAD_LOCAL_EVENT_ALARM = 49,
    WINDOWS_WORKER_START_HINTS_ALARM = 50,
    HOLD_ON_TOO_SLOW_ALARM = 51,
    INNER_PROFILE_ALARM = 52,
    FUSE_FILE_TRUNCATE_ALARM = 53,
    SENDING_COSTS_TOO_MUCH_TIME_ALARM = 54,
    UNEXPECTED_FILE_TYPE_MODE_ALARM = 55,
    LOG_GROUP_WAIT_TOO_LONG_ALARM = 56,
    CHECKPOINT_V2_ALARM = 57,
    EXACTLY_ONCE_ALARM = 58,
    READ_STOPPED_CONTAINER_ALARM = 59,
    INVALID_CONTAINER_PATH_ALARM = 64,
    COMPRESS_FAIL_ALARM = 65,
    SERIALIZE_FAIL_ALARM = 66,
    RELABEL_METRIC_FAIL_ALARM = 67,
    REGISTER_HANDLERS_TOO_SLOW_ALARM = 68,
    ALL_LOGTAIL_ALARM_NUM = 69
};

enum AlarmLevel {
    ALARM_LEVEL_WARNING = 1, // 单点报错，不影响整体流程
    ALARM_LEVEL_ERROR = 2, // 对主要流程有影响，如果不优化处理可能导致风险
    ALARM_LEVEL_CRITICAL = 3, // 采集配置/重要模块不可用;对Agent稳定性造成影响;导致资损（数据丢失等）
};

struct AlarmMessage {
    std::string mMessageType;
    std::string mLevel;
    std::string mProjectName;
    std::string mCategory;
    std::string mConfig;
    std::string mMessage;
    int32_t mCount;

    AlarmMessage(const std::string& type,
                 const std::string& level,
                 const std::string& projectName,
                 const std::string& category,
                 const std::string& config,
                 const std::string& message,
                 const int32_t count)
        : mMessageType(type),
          mLevel(level),
          mProjectName(projectName),
          mCategory(category),
          mConfig(config),
          mMessage(message),
          mCount(count) {}
    void IncCount(int32_t inc = 1) { mCount += inc; }
};

class AlarmManager {
public:
    static AlarmManager* GetInstance() {
        static AlarmManager instance;
        return &instance;
    }

    void SendAlarmWarning(const AlarmType& alarmType,
                          const std::string& message,
                          const std::string& region = "",
                          const std::string& projectName = "",
                          const std::string& config = "",
                          const std::string& category = "") {
        SendAlarm(alarmType, ALARM_LEVEL_WARNING, message, region, projectName, config, category);
    }
    void SendAlarmError(const AlarmType& alarmType,
                        const std::string& message,
                        const std::string& region = "",
                        const std::string& projectName = "",
                        const std::string& config = "",
                        const std::string& category = "") {
        SendAlarm(alarmType, ALARM_LEVEL_ERROR, message, region, projectName, config, category);
    }
    void SendAlarmCritical(const AlarmType& alarmType,
                           const std::string& message,
                           const std::string& region = "",
                           const std::string& projectName = "",
                           const std::string& config = "",
                           const std::string& category = "") {
        SendAlarm(alarmType, ALARM_LEVEL_CRITICAL, message, region, projectName, config, category);
    }
    // only be called when prepare to exit
    void ForceToSend();
    bool IsLowLevelAlarmValid();

    void FlushAllRegionAlarm(std::vector<PipelineEventGroup>& pipelineEventGroupList);

private:
    using AlarmVector = std::vector<std::map<std::string, std::unique_ptr<AlarmMessage>>>;

    AlarmManager();
    ~AlarmManager() = default;

    void SendAlarm(const AlarmType& alarmType,
                   const AlarmLevel& level,
                   const std::string& message,
                   const std::string& region,
                   const std::string& projectName,
                   const std::string& config,
                   const std::string& category);

    // without lock
    AlarmVector* MakesureLogtailAlarmMapVecUnlocked(const std::string& region);

    std::vector<std::string> mMessageType;
    std::map<std::string, std::pair<std::shared_ptr<AlarmVector>, std::vector<int32_t>>> mAllAlarmMap;
    PTMutex mAlarmBufferMutex;

    std::atomic_int mLastLowLevelTime{0};
    std::atomic_int mLastLowLevelCount{0};

#ifdef APSARA_UNIT_TEST_MAIN
    friend class AlarmManagerUnittest;
#endif
};

} // namespace logtail
