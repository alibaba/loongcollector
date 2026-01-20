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

#include <cstdio>
#include <ctime>

#include <filesystem>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "app_config/AppConfig.h"
#include "application/Application.h"
#include "common/FileSystemUtil.h"
#include "common/Flags.h"
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "common/version.h"
#include "constants/Constants.h"
#include "constants/TagConstants.h"
#include "logger/Logger.h"
#include "monitor/Monitor.h"
#include "monitor/SelfMonitorServer.h"
#include "plugin/flusher/sls/FlusherSLS.h"

DEFINE_FLAG_INT32(logtail_alarm_interval, "the interval of two same type alarm message", 30);
DEFINE_FLAG_INT32(logtail_low_level_alarm_speed, "the speed(count/second) which logtail's low level alarm allow", 100);
DEFINE_FLAG_INT32(logtail_startup_alarm_window_seconds,
                  "the time window in seconds for writing alarms to disk buffer during startup",
                  60);
DEFINE_FLAG_INT32(logtail_startup_alarm_file_max_size,
                  "the maximum size in bytes for the alarm disk buffer file",
                  10 * 1024 * 1024);
DEFINE_FLAG_INT32(logtail_startup_alarm_file_min_level, "the minimum alarm level to write to disk buffer file", 1);

using namespace std;
using namespace logtail;

namespace logtail {

const string ALARM_SLS_LOGSTORE_NAME = "logtail_alarm";

AlarmManager::AlarmManager() {
    mAlarmDiskBufferFilePath = PathJoin(GetAgentDataDir(), "alarm_disk_buffer.json");
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
    mMessageType[HOST_MONITOR_ALARM] = "HOST_MONITOR_ALARM";
}

AlarmManager::~AlarmManager() {
    CloseAlarmDiskBufferFile();
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
        for (auto& [_, messagePtr] : alarmMap) {
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
            logEvent->SetContent("instance_id", Application::GetInstance()->GetInstanceId());
            logEvent->SetContent("hostname", LoongCollectorMonitor::mHostname);
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
        for (uint32_t i = 0; i < ALL_LOGTAIL_ALARM_NUM; ++i) {
            lastUpdateTime[i] = now - rand() % 180;
        }
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

    if (projectName.empty()) {
        string projects = FlusherSLS::GetAllProjects();
        // 如果是进程级别的告警，可能有多个project，在记录的时候需要发送到每个project的region。
        // 这里为了便于实现，不采取一个project字段里写多个project名的方式，避免索引等的调整
        if (!projects.empty()) {
            auto projectsVec = SplitString(projects, " ");
            for (const auto& project : projectsVec) {
                SendAlarm(alarmType, level, message, FlusherSLS::GetProjectRegion(project), project, config, category);
            }
            return;
        }
        // 空的project、region会自动发到default region，这种情况只能用instance_id查询。
    }

    // 如果 AlarmPipeline 已就绪，写 buffer；否则写文件（最多60s）
    if (mAlarmPipelineReady.load()) {
        // 写 buffer
        PTScopedLock lock(mAlarmBufferMutex);
        string levelStr = ToString(level);
        string key = projectName + "_" + category + "_" + config + "_" + levelStr;
        AlarmVector& alarmBufferVec = *MakesureLogtailAlarmMapVecUnlocked(region);
        if (alarmBufferVec[alarmType].find(key) == alarmBufferVec[alarmType].end()) {
            auto* messagePtr
                = new AlarmMessage(mMessageType[alarmType], levelStr, projectName, category, config, message, 1);
            alarmBufferVec[alarmType].emplace(key, messagePtr);
        } else {
            alarmBufferVec[alarmType][key]->IncCount();
        }
    } else {
        // 写文件（如果允许）
        if (!region.empty()) {
            bool stopWriting = mStopWritingFile.load();
            if (ShouldWriteAlarmToFile(mAlarmPipelineReady.load(), stopWriting)) {
                if (stopWriting != mStopWritingFile.load()) {
                    mStopWritingFile.store(stopWriting);
                }
                if (static_cast<int32_t>(level) >= INT32_FLAG(logtail_startup_alarm_file_min_level)) {
                    WriteAlarmToFile(
                        region, mMessageType[alarmType], ToString(level), message, projectName, category, config);
                }
            } else if (stopWriting != mStopWritingFile.load()) {
                mStopWritingFile.store(stopWriting);
            }
        }
        // 如果已停止写文件，alarm 被丢弃（不写 buffer）
    }
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

bool AlarmManager::CheckAndSetAlarmPipelineReady() {
    bool expected = false;
    // 如果之前是 false，设置为 true 并返回 false（第一次就绪，需要处理文件）
    // 如果之前已经是 true，返回 true（已经就绪过，不需要处理文件）
    if (mAlarmPipelineReady.compare_exchange_strong(expected, true)) {
        return false; // 第一次就绪
    }
    return true; // 已经就绪过
}

bool AlarmManager::ReadAlarmsFromFile(std::vector<PipelineEventGroup>& pipelineEventGroupList,
                                      std::map<std::string, std::string>& regionToRawJson) {
    // 读取前先关闭写文件句柄，确保数据已刷新到磁盘
    CloseAlarmDiskBufferFile();

    const std::string& filePath = mAlarmDiskBufferFilePath;
    if (!CheckExistance(filePath)) {
        return false;
    }

    std::vector<std::string> lines;
    std::string errMsg;
    if (GetFileLines(std::filesystem::path(filePath), lines, false, &errMsg) != 0 || lines.empty()) {
        return false;
    }

    // 按 key 分组并累加 count
    std::unordered_map<std::string, int32_t> keyToCount; // key -> count
    std::unordered_map<std::string, rapidjson::Document> keyToDoc; // key -> 第一个有效的 doc（用于提取其他字段）
    std::unordered_map<std::string, std::string> keyToRawJson; // key -> 原始 JSON 行（用于错误日志）

    for (const auto& line : lines) {
        if (line.empty()) {
            continue;
        }
        rapidjson::Document doc;
        if (doc.Parse(line.data(), line.size()).HasParseError()) {
            continue;
        }
        if (!doc.IsObject()) {
            continue;
        }

        const auto& region = doc["region"];
        const auto& alarmType = doc["alarm_type"];
        const auto& level = doc["alarm_level"];
        const auto& ipValue = doc["ip"];
        const auto& osValue = doc["os"];
        const auto& verValue = doc["ver"];
        const auto& instanceIdValue = doc["instance_id"];
        const auto& hostnameValue = doc["hostname"];
        const auto& projectName = doc.FindMember("project_name");
        const auto& category = doc.FindMember("category");
        const auto& config = doc.FindMember("config");

        if (!region.IsString() || !alarmType.IsString() || !level.IsString()) {
            continue;
        }

        std::string regionStr = region.GetString();
        if (regionStr.empty()) {
            continue;
        }

        std::string key = std::string(region.GetString()) + "_" + std::string(alarmType.GetString()) + "_";
        if (projectName != doc.MemberEnd() && projectName->value.IsString()) {
            key += projectName->value.GetString();
        }
        key += "_";
        if (category != doc.MemberEnd() && category->value.IsString()) {
            key += category->value.GetString();
        }
        key += "_";
        if (config != doc.MemberEnd() && config->value.IsString()) {
            key += config->value.GetString();
        }
        key += "_" + std::string(level.GetString());
        key += "_" + std::string(ipValue.GetString());
        key += "_" + std::string(osValue.GetString());
        key += "_" + std::string(verValue.GetString());
        key += "_" + std::string(instanceIdValue.GetString());
        key += "_" + std::string(hostnameValue.GetString());

        // 累加 count
        keyToCount[key]++;

        // 保存第一个有效的 doc 和原始 JSON
        if (keyToDoc.find(key) == keyToDoc.end()) {
            keyToDoc[key] = std::move(doc);
            keyToRawJson[key] = line;
        } else {
            // 累加原始 JSON（用于错误日志）
            keyToRawJson[key] += "\n";
            keyToRawJson[key] += line;
        }
    }

    // 按 region 分组构造 PipelineEventGroup
    std::unordered_map<std::string, size_t> regionToIndex;
    for (const auto& [key, count] : keyToCount) {
        const auto& doc = keyToDoc[key];
        const auto& region = doc["region"];
        std::string regionStr = region.GetString();

        // 保存原始 JSON，按 region 分组
        if (regionToRawJson.find(regionStr) == regionToRawJson.end()) {
            regionToRawJson[regionStr] = keyToRawJson[key];
        } else {
            regionToRawJson[regionStr] += "\n";
            regionToRawJson[regionStr] += keyToRawJson[key];
        }

        size_t groupIdx = 0;
        auto it = regionToIndex.find(regionStr);
        if (it == regionToIndex.end()) {
            PipelineEventGroup group(std::make_shared<SourceBuffer>());
            group.SetTagNoCopy(LOG_RESERVED_KEY_SOURCE, LoongCollectorMonitor::mIpAddr);
            group.SetMetadata(EventGroupMetaKey::INTERNAL_DATA_TARGET_REGION, regionStr);
            group.SetMetadata(EventGroupMetaKey::INTERNAL_DATA_TYPE, SelfMonitorServer::INTERNAL_DATA_TYPE_ALARM);
            pipelineEventGroupList.emplace_back(std::move(group));
            groupIdx = pipelineEventGroupList.size() - 1;
            regionToIndex.emplace(regionStr, groupIdx);
        } else {
            groupIdx = it->second;
        }

        auto* logEvent = pipelineEventGroupList[groupIdx].AddLogEvent();
        const auto& timestamp = doc["timestamp"];
        const auto& message = doc["alarm_message"];
        if (timestamp.IsInt64()) {
            logEvent->SetTimestamp(static_cast<time_t>(timestamp.GetInt64()));
        }
        logEvent->SetContent("alarm_type", std::string(doc["alarm_type"].GetString()));
        logEvent->SetContent("alarm_level", std::string(doc["alarm_level"].GetString()));
        logEvent->SetContent("alarm_message", std::string(message.IsString() ? std::string(message.GetString()) : ""));
        logEvent->SetContent("alarm_count", ToString(count));
        logEvent->SetContent("ip", std::string(doc["ip"].GetString()));
        logEvent->SetContent("os", std::string(doc["os"].GetString()));
        logEvent->SetContent("ver", std::string(doc["ver"].GetString()));
        logEvent->SetContent("instance_id", std::string(doc["instance_id"].GetString()));
        logEvent->SetContent("hostname", std::string(doc["hostname"].GetString()));
        const auto& projectName = doc.FindMember("project_name");
        if (projectName != doc.MemberEnd() && projectName->value.IsString()) {
            logEvent->SetContent("project_name", std::string(projectName->value.GetString()));
        }
        const auto& category = doc.FindMember("category");
        if (category != doc.MemberEnd() && category->value.IsString()) {
            logEvent->SetContent("category", std::string(category->value.GetString()));
        }
        const auto& config = doc.FindMember("config");
        if (config != doc.MemberEnd() && config->value.IsString()) {
            logEvent->SetContent("config", std::string(config->value.GetString()));
        }
    }

    return !pipelineEventGroupList.empty();
}

void AlarmManager::DeleteAlarmFile() {
    if (!mAlarmDiskBufferFilePath.empty()) {
        CloseAlarmDiskBufferFile(); // 确保文件句柄关闭，以便删除文件
        // 删除文件
        std::error_code ec;
        std::filesystem::remove(std::filesystem::path(mAlarmDiskBufferFilePath), ec);
    }
}

bool AlarmManager::ShouldWriteAlarmToFile(bool alarmPipelineReady, bool& stopWritingFile) {
    if (alarmPipelineReady || stopWritingFile) {
        // 如果不再需要写入，关闭文件句柄
        if (mAlarmDiskBufferFileHandle != nullptr) {
            CloseAlarmDiskBufferFile();
        }
        return false;
    }
    const int32_t startTime = Application::GetInstance()->GetStartTime();
    if (startTime <= 0) {
        return false;
    }
    const int32_t elapsed = time(nullptr) - startTime;
    const int32_t windowSeconds = INT32_FLAG(logtail_startup_alarm_window_seconds);
    if (elapsed > windowSeconds) {
        if (!stopWritingFile) {
            stopWritingFile = true;
            LOG_ERROR(sLogger,
                      ("alarm pipeline not ready after",
                       ToString(windowSeconds) + "s, stop writing alarm to file and discard all subsequent alarms")(
                          "start_time", startTime));
        }
        return false;
    }

    // 检查文件大小限制
    if (CheckExistance(mAlarmDiskBufferFilePath)) {
        std::error_code ec;
        const int64_t fileSize = std::filesystem::file_size(std::filesystem::path(mAlarmDiskBufferFilePath), ec);
        if (!ec) {
            const auto maxSizeBytes = static_cast<int64_t>(INT32_FLAG(logtail_startup_alarm_file_max_size));
            if (fileSize >= maxSizeBytes) {
                if (!stopWritingFile) {
                    stopWritingFile = true;
                    LOG_ERROR(sLogger,
                              ("alarm disk buffer file size exceeds limit",
                               ToString(INT32_FLAG(logtail_startup_alarm_file_max_size))
                                   + " bytes, stop writing alarm to file and discard all subsequent alarms")(
                                  "file_path", mAlarmDiskBufferFilePath)("file_size_bytes", ToString(fileSize)));
                }
                return false;
            }
        }
    }

    return true;
}

void AlarmManager::EnsureAlarmDiskBufferFileOpen() {
    PTScopedLock lock(mAlarmDiskBufferFileMutex);
    if (mAlarmDiskBufferFileHandle != nullptr) {
        return;
    }

    mAlarmDiskBufferFileHandle = FileAppendOpen(mAlarmDiskBufferFilePath.c_str(), "a");
    if (mAlarmDiskBufferFileHandle == nullptr) {
        LOG_ERROR(sLogger, ("failed to open alarm disk buffer file", mAlarmDiskBufferFilePath));
    }
}

void AlarmManager::CloseAlarmDiskBufferFile() {
    PTScopedLock lock(mAlarmDiskBufferFileMutex);
    if (mAlarmDiskBufferFileHandle != nullptr) {
        fclose(mAlarmDiskBufferFileHandle);
        mAlarmDiskBufferFileHandle = nullptr;
    }
}

void AlarmManager::WriteAlarmToFile(const std::string& region,
                                    const std::string& alarmType,
                                    const std::string& level,
                                    const std::string& message,
                                    const std::string& projectName,
                                    const std::string& category,
                                    const std::string& config) {
    EnsureAlarmDiskBufferFileOpen();
    if (mAlarmDiskBufferFileHandle == nullptr) {
        return;
    }

    // 写文件时不管count，按原始输入写，count固定为1
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();
    writer.Key("region");
    writer.String(region.c_str());
    writer.Key("alarm_type");
    writer.String(alarmType.c_str());
    writer.Key("alarm_level");
    writer.String(level.c_str());
    writer.Key("alarm_message");
    writer.String(message.c_str());
    writer.Key("alarm_count");
    writer.String("1");
    writer.Key("timestamp");
    writer.Int64(time(nullptr));
    writer.Key("ip");
    writer.String(LoongCollectorMonitor::mIpAddr.c_str());
    writer.Key("os");
    writer.String(OS_NAME.c_str());
    writer.Key("ver");
    writer.String(ILOGTAIL_VERSION);
    writer.Key("instance_id");
    writer.String(Application::GetInstance()->GetInstanceId().c_str());
    writer.Key("hostname");
    writer.String(LoongCollectorMonitor::mHostname.c_str());
    if (!projectName.empty()) {
        writer.Key("project_name");
        writer.String(projectName.c_str());
    }
    if (!category.empty()) {
        writer.Key("category");
        writer.String(category.c_str());
    }
    if (!config.empty()) {
        writer.Key("config");
        writer.String(config.c_str());
    }
    writer.EndObject();

    PTScopedLock lock(mAlarmDiskBufferFileMutex);
    if (mAlarmDiskBufferFileHandle != nullptr) {
        fprintf(mAlarmDiskBufferFileHandle, "%s\n", buffer.GetString());
        fflush(mAlarmDiskBufferFileHandle); // 确保数据及时写入
    }
}


#ifdef APSARA_UNIT_TEST_MAIN
void AlarmManager::ClearTestState() {
    // 清理文件和去重统计 map
    DeleteAlarmFile();

    // 清理状态标志
    mAlarmPipelineReady.store(false);
    mStopWritingFile.store(false);
}
#endif

} // namespace logtail
