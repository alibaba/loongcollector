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
#include <stdio.h>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include "common/Lock.h"
#include "profile_sender/ProfileSender.h"

namespace logtail {

enum LogtailAlarmType {
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
    LOGDIR_PERMINSSION_ALARM = 17,
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
    LZ4_COMPRESS_FAIL_ALARM = 34,
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
    ALL_LOGTAIL_ALARM_NUM = 60
};

struct LogtailAlarmMessage {
    std::string mMessageType;
    std::string mProjectName;
    std::string mCategory;
    std::string mMessage;
    int32_t mCount;

    LogtailAlarmMessage(const std::string& type,
                        const std::string& projectName,
                        const std::string& category,
                        const std::string& message,
                        const int32_t count)
        : mMessageType(type), mProjectName(projectName), mCategory(category), mMessage(message), mCount(count) {}
    void IncCount(int32_t inc = 1) { mCount += inc; }
};

class LogtailAlarm {
private:
    std::vector<std::string> mMessageType;
    typedef std::vector<std::map<std::string, LogtailAlarmMessage*> > LogtailAlarmVector;
    std::map<std::string, std::pair<LogtailAlarmVector*, std::vector<int32_t> > > mAllAlarmMap;
    PTMutex mAlarmBufferMutex;

    LogtailAlarm();
    ~LogtailAlarm();
    bool SendAlarmLoop();
    // without lock
    LogtailAlarmVector* MakesureLogtailAlarmMapVecUnlocked(const std::string& region);

    std::atomic_int mLastLowLevelTime{0};
    std::atomic_int mLastLowLevelCount{0};
    ProfileSender mProfileSender;

public:
    void SendAlarm(const LogtailAlarmType alarmType,
                   const std::string& message,
                   const std::string& projectName = "",
                   const std::string& category = "",
                   const std::string& region = "");
    // only be called when prepare to exit
    void ForceToSend();
    bool IsLowLevelAlarmValid();
    static LogtailAlarm* GetInstance() {
        static LogtailAlarm* ptr = new LogtailAlarm();
        return ptr;
    }
};

} // namespace logtail
