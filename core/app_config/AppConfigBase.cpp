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

#include "AppConfigBase.h"
#include <algorithm>
#if defined(__linux__)
#include "streamlog/StreamLogFormat.h"
#endif
#include "shennong/MetricSender.h"
#include "sender/Sender.h"
#include "profiler/LogFileProfiler.h"
#include "profiler/LogtailAlarm.h"
#include "monitor/Monitor.h"
#include "common/util.h"
#include "common/LogtailCommonFlags.h"
#include "common/RuntimeUtil.h"
#include "common/FileSystemUtil.h"
#include "common/JsonUtil.h"
#include "config_manager/ConfigManager.h"
#include "logger/Logger.h"
#include "reader/LogFileReader.h"
using namespace std;

DEFINE_FLAG_INT32(max_buffer_num, "max size", 40);
DEFINE_FLAG_INT32(pub_max_buffer_num, "max size", 8);
DEFINE_FLAG_INT32(default_max_send_byte_per_sec, "the max send speed per sec, realtime thread", 25 * 1024 * 1024);
DEFINE_FLAG_INT32(pub_max_send_byte_per_sec, "the max send speed per sec, realtime thread", 20 * 1024 * 1024);
DEFINE_FLAG_INT32(default_send_byte_per_sec, "the max send speed per sec, replay buffer thread", 2 * 1024 * 1024);
DEFINE_FLAG_INT32(pub_send_byte_per_sec, "the max send speed per sec, replay buffer thread", 1 * 1024 * 1024);
DEFINE_FLAG_INT32(default_buffer_file_num, "how many buffer files in default", 50);
DEFINE_FLAG_INT32(pub_buffer_file_num, "how many buffer files in default", 25);
DEFINE_FLAG_INT32(default_local_file_size, "default size of one buffer file", 20 * 1024 * 1024);
DEFINE_FLAG_INT32(pub_local_file_size, "default size of one buffer file", 20 * 1024 * 1024);
DEFINE_FLAG_INT32(process_thread_count, "", 1);
DEFINE_FLAG_INT32(send_request_concurrency, "max count keep in mem when async send", 10);
DECLARE_FLAG_INT32(default_StreamLog_tcp_port);
DECLARE_FLAG_INT32(default_StreamLog_poll_size_in_mb);
DECLARE_FLAG_INT32(default_StreamLog_recv_size_each_call);
DECLARE_FLAG_BOOL(ilogtail_discard_old_data);
DEFINE_FLAG_BOOL(enable_send_tps_smoothing, "avoid web server load burst", true);
DEFINE_FLAG_BOOL(enable_flow_control, "if enable flow control", true);
DEFINE_FLAG_STRING(default_buffer_file_path, "set current execution dir in default", "");
DEFINE_FLAG_STRING(default_mapping_config_path, "", "mapping_config.json");
DEFINE_FLAG_DOUBLE(default_machine_cpu_usage_threshold, "machine level", 0.4);
DEFINE_FLAG_BOOL(default_resource_auto_scale, "", false);
DEFINE_FLAG_BOOL(default_input_flow_control, "", false);
DEFINE_FLAG_INT32(max_open_files_limit, "max open files ulimit -n", 131072);
DEFINE_FLAG_INT32(max_multi_config_size, "max multi config size", 20);
DEFINE_FLAG_BOOL(default_accept_multi_config, "", false);
DEFINE_FLAG_STRING(default_env_tag_keys, "default env key to load tags", "ALIYUN_LOG_ENV_TAGS");
#if defined(__linux__) || defined(__APPLE__)
DEFINE_FLAG_STRING(logtail_sys_conf_dir, "store machine-unique-id, user-defined-id, aliuid", "/etc/ilogtail/");
#elif defined(_MSC_VER)
DEFINE_FLAG_STRING(logtail_sys_conf_dir, "store machine-unique-id, user-defined-id, aliuid", "C:\\LogtailData\\");
#endif
const char* DEFAULT_ILOGTAIL_LOCAL_CONFIG_FLAG_VALUE = "user_local_config.json";
DEFINE_FLAG_STRING(ilogtail_local_config, "local ilogtail config file", DEFAULT_ILOGTAIL_LOCAL_CONFIG_FLAG_VALUE);
const char* DEFAULT_ILOGTAIL_LOCAL_CONFIG_DIR_FLAG_VALUE = "user_config.d";
DEFINE_FLAG_STRING(ilogtail_local_config_dir,
                   "local ilogtail config file dir",
                   DEFAULT_ILOGTAIL_LOCAL_CONFIG_DIR_FLAG_VALUE);
const char* DEFAULT_ILOGTAIL_LOCAL_YAML_CONFIG_DIR_FLAG_VALUE = "user_yaml_config.d";
DEFINE_FLAG_STRING(ilogtail_local_yaml_config_dir,
                   "local ilogtail yaml config file dir",
                   DEFAULT_ILOGTAIL_LOCAL_YAML_CONFIG_DIR_FLAG_VALUE);

DEFINE_FLAG_BOOL(default_global_fuse_mode, "default global fuse mode", false);
DEFINE_FLAG_BOOL(default_global_mark_offset_flag, "default global mark offset flag", false);

DEFINE_FLAG_STRING(default_container_mount_path, "", "container_mount.json");
DEFINE_FLAG_STRING(default_include_config_path, "", "config.d");
#if defined(_MSC_VER)
DEFINE_FLAG_STRING(default_container_host_path, "", "C:\\logtail_host");
#else
DEFINE_FLAG_STRING(default_container_host_path, "", "/logtail_host");
#endif

DEFINE_FLAG_INT32(default_oas_connect_timeout, "default (minimum) connect timeout for OSARequest", 5);
DEFINE_FLAG_INT32(default_oas_request_timeout, "default (minimum) request timeout for OSARequest", 10);
DEFINE_FLAG_BOOL(rapid_retry_update_config, "", false);
DEFINE_FLAG_BOOL(check_profile_region, "", false);
DEFINE_FLAG_BOOL(enable_collection_mark,
                 "enable collection mark function to override check_ulogfs_env in user config",
                 false);
DEFINE_FLAG_BOOL(enable_env_ref_in_config, "enable environment variable reference replacement in configuration", false);

DEFINE_FLAG_STRING(alipay_app_zone, "", "ALIPAY_APP_ZONE");
DEFINE_FLAG_STRING(alipay_zone, "", "ALIPAY_ZONE");
DEFINE_FLAG_STRING(alipay_zone_env_name, "", "");

DECLARE_FLAG_STRING(check_point_filename);

DECLARE_FLAG_INT32(polling_max_stat_count);
DECLARE_FLAG_INT32(polling_max_stat_count_per_dir);
DECLARE_FLAG_INT32(polling_max_stat_count_per_config);
DECLARE_FLAG_INT32(config_match_max_cache_size);
DECLARE_FLAG_INT32(modify_cache_max);
DECLARE_FLAG_INT32(max_watch_dir_count);
DECLARE_FLAG_INT32(max_reader_open_files);

DECLARE_FLAG_INT32(logreader_filedeleted_remove_interval);
DECLARE_FLAG_INT32(check_handler_timeout_interval);
DECLARE_FLAG_INT32(reader_close_unused_file_time);
DECLARE_FLAG_INT32(main_loop_check_interval);

DECLARE_FLAG_INT32(batch_send_interval);
DECLARE_FLAG_INT32(batch_send_metric_size);

DECLARE_FLAG_BOOL(send_prefer_real_ip);
DECLARE_FLAG_INT32(send_switch_real_ip_interval);
DECLARE_FLAG_INT32(truncate_pos_skip_bytes);
DECLARE_FLAG_INT32(default_tail_limit_kb);

DECLARE_FLAG_STRING(user_log_config);
DECLARE_FLAG_STRING(ilogtail_docker_file_path_config);

DECLARE_FLAG_INT32(max_docker_config_update_times);
DECLARE_FLAG_INT32(docker_config_update_interval);

DECLARE_FLAG_INT32(read_local_event_interval);

DECLARE_FLAG_INT32(check_point_dump_interval);
DECLARE_FLAG_STRING(ilogtail_user_defined_id_env_name);

DECLARE_FLAG_INT32(logreader_max_rotate_queue_size);
DECLARE_FLAG_INT32(search_checkpoint_default_dir_depth);
DECLARE_FLAG_INT32(checkpoint_find_max_file_count);
DECLARE_FLAG_BOOL(enable_polling_discovery);
DECLARE_FLAG_INT32(dirfile_check_interval_ms);
DECLARE_FLAG_INT32(polling_dir_first_watch_timeout);
DECLARE_FLAG_INT32(polling_file_first_watch_timeout);
DECLARE_FLAG_INT32(modify_check_interval);
DECLARE_FLAG_INT32(ignore_file_modify_timeout);


namespace logtail {
AppConfigBase::AppConfigBase() {
    LOG_INFO(sLogger, ("AppConfigBase AppConfigBase", "success"));
    mSendRandomSleep = BOOL_FLAG(enable_send_tps_smoothing);
    mSendFlowControl = BOOL_FLAG(enable_flow_control);
    SetIlogtailConfigJson("");
    mStreamLogAddress = "0.0.0.0";
    mIsOldPubRegion = false;
    mOpenStreamLog = false;
    mSendRequestConcurrency = INT32_FLAG(send_request_concurrency);
    mProcessThreadCount = INT32_FLAG(process_thread_count);
    mBufferFilePath = STRING_FLAG(default_buffer_file_path);
    mMappingConfigPath = STRING_FLAG(default_mapping_config_path);
    mMachineCpuUsageThreshold = DOUBLE_FLAG(default_machine_cpu_usage_threshold);
    mCpuUsageUpLimit = DOUBLE_FLAG(cpu_usage_up_limit);
    mScaledCpuUsageUpLimit = DOUBLE_FLAG(cpu_usage_up_limit);
    mMemUsageUpLimit = INT64_FLAG(memory_usage_up_limit);
    mResourceAutoScale = BOOL_FLAG(default_resource_auto_scale);
    mInputFlowControl = BOOL_FLAG(default_input_flow_control);
    mDefaultRegion = STRING_FLAG(default_region_name);
    mAcceptMultiConfigFlag = BOOL_FLAG(default_accept_multi_config);
    mMaxMultiConfigSize = INT32_FLAG(max_multi_config_size);
    mUserConfigPath = STRING_FLAG(user_log_config);
    mIgnoreDirInodeChanged = false;
    mLogParseAlarmFlag = true;
    mContainerMode = false;
    std::string processExecutionDir = GetProcessExecutionDir();
    mDockerFilePathConfig = processExecutionDir + STRING_FLAG(ilogtail_docker_file_path_config);
    mNoInotify = false;
    mSendDataPort = 80;
    mShennongSocket = true;
    //mInotifyBlackList.insert("/tmp");

    mPurageContainerMode = false;
    mForceQuitReadTimeout = 7200;
    LoadEnvTags();
    CheckPurageContainerMode();
}

void AppConfigBase::MergeJson(Json::Value& mainConfJson, const Json::Value& subConfJson) {
    for (auto subkey : subConfJson.getMemberNames()) {
        mainConfJson[subkey] = subConfJson[subkey];
    }
}

void AppConfigBase::LoadIncludeConfig(Json::Value& confJson) {
    // New default value of the flag is renamed from /etc/ilogtail/config.d/
    // to config.d, be compatible with old default value.
    string dirPath = STRING_FLAG(default_include_config_path);
    if (!dirPath.empty() && dirPath[0] != '/') {
        dirPath = mLogtailSysConfDir + dirPath + PATH_SEPARATOR;
    }
    if (confJson.isMember("include_config_path") && confJson["include_config_path"].isString()) {
        dirPath = confJson["include_config_path"].asString();
    }
    fsutil::Dir dir(dirPath);
    if (!dir.Open()) {
        auto e = GetErrno();
        if (fsutil::Dir::IsENOENT(e)) {
            LOG_DEBUG(sLogger, ("opendir error, ENOENT, dir", dirPath));
        } else {
            LOG_ERROR(sLogger, ("opendir error: ", e)("dir", dirPath));
        }
        return;
    }

    std::vector<std::string> v;
    fsutil::Entry entry;
    while (entry = dir.ReadNext(false)) {
        if (!entry.IsRegFile()) {
            continue;
        }

        string flName = entry.Name();
        if (flName.size() < 5 || flName.substr(flName.size() - 5) != ".json") {
            continue;
        }
        string fullPath = dirPath + PATH_SEPARATOR + flName;
        v.push_back(fullPath);
    }

    std::sort(v.begin(), v.end());
    for (size_t i = 0; i < v.size(); i++) {
        Json::Value subConfJson;
        ParseConfResult res = ParseConfig(v[i], subConfJson);
        if (res != CONFIG_OK) {
            continue;
        }

        MergeJson(confJson, subConfJson);
        LOG_INFO(sLogger, ("merge sub config, file", v[i])("config", subConfJson.toStyledString()));
    }
}

void AppConfigBase::LoadAppConfig(const std::string& ilogtailConfigFile) {
    Json::Value confJson(Json::objectValue);
    std::string newSysConfDir;

    if (!ilogtailConfigFile.empty()) {
        ParseConfResult res = ParseConfig(ilogtailConfigFile, confJson);

        if (res == CONFIG_NOT_EXIST) {
            LOG_INFO(sLogger, ("config file not exist, try generate config by path", ilogtailConfigFile));
            if (GenerateAPPConfigByConfigPath(ilogtailConfigFile, confJson)) {
                res = CONFIG_OK;
                LOG_INFO(sLogger, ("generate config success", ilogtailConfigFile));
            } else {
                LOG_WARNING(sLogger, ("generate config failed", ilogtailConfigFile));
            }
        }

        if (res == CONFIG_OK) {
            // Should be loaded here because other parameters depend on it.
            LoadStringParameter(newSysConfDir, confJson, "logtail_sys_conf_dir", "ALIYUN_LOGTAIL_SYS_CONF_DIR");
        } else {
            confJson.clear();
            if (res == CONFIG_NOT_EXIST) {
                LOG_ERROR(sLogger, ("can not find start config", ilogtailConfigFile));
                LogtailAlarm::GetInstance()->SendAlarm(LOGTAIL_CONFIG_ALARM, "can not find start config");
            } else if (res == CONFIG_INVALID_FORMAT) {
                LOG_ERROR(sLogger, ("start config is not valid json", ilogtailConfigFile));
                LogtailAlarm::GetInstance()->SendAlarm(LOGTAIL_CONFIG_ALARM, "start config is not valid json");
            }
        }
    }

    if (newSysConfDir.empty()) {
        newSysConfDir = STRING_FLAG(logtail_sys_conf_dir);
    }
    SetLogtailSysConfDir(AbsolutePath(newSysConfDir, mProcessExecutionDir));

    LoadIncludeConfig(confJson);
    string configJsonString = confJson.toStyledString();
    SetIlogtailConfigJson(configJsonString);
    LOG_INFO(sLogger, ("load logtail config file, path", ilogtailConfigFile));
    LOG_INFO(sLogger, ("load logtail config file, detail", configJsonString));

    ParseJsonToFlags(confJson);
    ParseEnvToFlags();

    LoadSyslogConf(confJson);
    LoadResourceConf(confJson);
    // load addr will init sender, sender param depend on LoadResourceConf
    LoadAddrConfig(confJson);
    LoadOtherConf(confJson);

    LoadGlobalFuseConf(confJson);
}

std::string AppConfigBase::GetDefaultRegion() const {
    ScopedSpinLock lock(mAppConfigLock);
    return mDefaultRegion;
}

void AppConfigBase::SetDefaultRegion(const string& region) {
    LOG_DEBUG(sLogger, ("SetDefaultRegion before", mDefaultRegion)("after", region));
    ScopedSpinLock lock(mAppConfigLock);
    mDefaultRegion = region;
}

void AppConfigBase::LoadEnvTags() {
    char* envTagKeys = getenv(STRING_FLAG(default_env_tag_keys).c_str());
    if (envTagKeys == NULL) {
        return;
    }
    LOG_INFO(sLogger, ("load env tags from env key", envTagKeys));
    vector<string> envKeys = SplitString(string(envTagKeys), "|");
    mEnvTags.resize(envKeys.size());
    for (size_t i = 0; i < envKeys.size(); ++i) {
        mEnvTags[i].set_key(envKeys[i]);
        char* envValue = getenv(envKeys[i].c_str());
        if (envValue != NULL) {
            mEnvTags[i].set_value(string(envValue));
            LOG_INFO(sLogger, ("load env, key", envKeys[i])("value", string(envValue)));
        } else {
            mEnvTags[i].set_value(string(""));
            LOG_INFO(sLogger, ("load env, key", envKeys[i])("value", ""));
        }
    }
}

// @return true if input configValue has been updated.
template <typename T>
bool LoadSingleValueEnvConfig(const char* envKey, T& configValue, const T minValue) {
    try {
        char* value = NULL;
        value = getenv(envKey);
        if (value != NULL) {
            T val = StringTo<T>(value);
            if (val >= minValue) {
                configValue = val;
                LOG_INFO(sLogger, (string("set ") + envKey + " from env, value", value));
                return true;
            }
        }
    } catch (const exception& e) {
        LOG_WARNING(sLogger, (string("set ") + envKey + " from env failed, exception", e.what()));
    }
    return false;
}

template <typename T>
void LoadEnvValueIfExisting(const char* envKey, T& cfgValue) {
    try {
        const char* value = getenv(envKey);
        if (value != NULL) {
            T val = StringTo<T>(value);
            cfgValue = val;
            LOG_INFO(sLogger, ("load config from env", envKey)("value", val));
        }
    } catch (const std::exception& e) {
        LOG_WARNING(sLogger, ("load config from env error", envKey)("error", e.what()));
    }
}

void AppConfigBase::LoadEnvResourceLimit() {
    LoadSingleValueEnvConfig("cpu_usage_limit", mCpuUsageUpLimit, (float)0.4);
    LoadSingleValueEnvConfig("mem_usage_limit", mMemUsageUpLimit, (int64_t)384);
    LoadSingleValueEnvConfig("max_bytes_per_sec", mMaxBytePerSec, (int32_t)(1024 * 1024));
    LoadSingleValueEnvConfig("process_thread_count", mProcessThreadCount, (int32_t)1);
    LoadSingleValueEnvConfig("send_request_concurrency", mSendRequestConcurrency, (int32_t)2);
}

void AppConfigBase::CheckPurageContainerMode() {
    if (getenv(STRING_FLAG(ilogtail_user_defined_id_env_name).c_str()) != NULL) {
        fsutil::PathStat buf;
        if (fsutil::PathStat::stat(STRING_FLAG(default_container_host_path).c_str(), buf)) {
            mPurageContainerMode = true;
        }
    }
    LOG_INFO(sLogger, ("purage container mode", mPurageContainerMode));
}

void AppConfigBase::LoadSyslogConf(const Json::Value& confJson) {
    if (confJson.isMember("streamlog_tcp_addr") && confJson["streamlog_tcp_addr"].isString())
        mStreamLogAddress = confJson["streamlog_tcp_addr"].asString();
    if (mStreamLogAddress.empty()) {
        mStreamLogAddress = "0.0.0.0";
    }
    if (confJson.isMember("streamlog_tcp_port") && confJson["streamlog_tcp_port"].isInt())
        mStreamLogTcpPort = confJson["streamlog_tcp_port"].asInt();
    else
        mStreamLogTcpPort = INT32_FLAG(default_StreamLog_tcp_port);

    if (confJson.isMember("streamlog_pool_size_in_mb") && confJson["streamlog_pool_size_in_mb"].isInt())
        mStreamLogPoolSizeInMb = confJson["streamlog_pool_size_in_mb"].asInt();
    else
        mStreamLogPoolSizeInMb = INT32_FLAG(default_StreamLog_poll_size_in_mb);

    if (confJson.isMember("streamlog_open") && confJson["streamlog_open"].isBool())
        mOpenStreamLog = confJson["streamlog_open"].asBool();
    else
        mOpenStreamLog = false;

    LoadBooleanParameter(mAcceptMultiConfigFlag, confJson, "accept_multi_config", "ALIYUN_LOGTAIL_ACCEPT_MULTI_CONFIG");

    LoadInt32Parameter(mMaxMultiConfigSize, confJson, "max_multi_config", "ALIYUN_LOGTAIL_MAX_MULTI_CONFIG");

    if (confJson.isMember("streamlog_rcv_size_each_call") && confJson["streamlog_rcv_size_each_call"].isInt())
        mStreamLogRcvLenPerCall = confJson["streamlog_rcv_size_each_call"].asInt();
    else
        mStreamLogRcvLenPerCall = INT32_FLAG(default_StreamLog_recv_size_each_call);

#if defined(__linux__)
    StreamLogLine::ClearFormats();
    StreamLogLine::AddDefaultFormats();
    if (confJson.isMember("streamlog_formats") && confJson["streamlog_formats"].isArray()) {
        StreamLogLine::InitFormats(confJson["streamlog_formats"]);
    } else if (confJson.isMember("streamlog_formats") && !confJson["streamlog_formats"].isArray()) {
        LOG_ERROR(sLogger,
                  ("message", "StreamLog_format must be of json array format")(
                      "json", confJson["streamlog_formats"].toStyledString()));
    }
#endif

    if (confJson.isMember("log_parse_error") && confJson["log_parse_error"].isBool()) {
        mLogParseAlarmFlag = confJson["log_parse_error"].asBool();
    }
}

void AppConfigBase::LoadResourceConf(const Json::Value& confJson) {
    LoadInt32Parameter(
        INT32_FLAG(batch_send_interval), confJson, "batch_send_interval", "ALIYUN_LOGTAIL_BATCH_SEND_INTERVAL");

    LoadInt32Parameter(INT32_FLAG(batch_send_metric_size),
                       confJson,
                       "batch_send_metric_size",
                       "ALIYUN_LOGTAIL_BATCH_SEND_METRIC_SIZE");

    if (confJson.isMember("max_bytes_per_sec") && confJson["max_bytes_per_sec"].isInt())
        mMaxBytePerSec = confJson["max_bytes_per_sec"].asInt();
    else if (mIsOldPubRegion)
        mMaxBytePerSec = INT32_FLAG(pub_max_send_byte_per_sec);
    else
        mMaxBytePerSec = INT32_FLAG(default_max_send_byte_per_sec);

    if (confJson.isMember("bytes_per_sec") && confJson["bytes_per_sec"].isInt())
        mBytePerSec = confJson["bytes_per_sec"].asInt();
    else if (mIsOldPubRegion)
        mBytePerSec = INT32_FLAG(pub_send_byte_per_sec);
    else
        mBytePerSec = INT32_FLAG(default_send_byte_per_sec);

    if (confJson.isMember("buffer_file_num") && confJson["buffer_file_num"].isInt())
        mNumOfBufferFile = confJson["buffer_file_num"].asInt();
    else if (mIsOldPubRegion)
        mNumOfBufferFile = INT32_FLAG(pub_buffer_file_num);
    else
        mNumOfBufferFile = INT32_FLAG(default_buffer_file_num);

    if (confJson.isMember("buffer_file_size") && confJson["buffer_file_size"].isInt())
        mLocalFileSize = confJson["buffer_file_size"].asInt();
    else if (mIsOldPubRegion)
        mLocalFileSize = INT32_FLAG(pub_local_file_size);
    else
        mLocalFileSize = INT32_FLAG(default_local_file_size);

    if (confJson.isMember("buffer_map_size") && confJson["buffer_map_size"].isInt())
        mMaxHoldedDataSize = confJson["buffer_map_size"].asInt();
    else if (mIsOldPubRegion)
        mMaxHoldedDataSize = INT32_FLAG(pub_max_holded_data_size);
    else
        mMaxHoldedDataSize = INT32_FLAG(max_holded_data_size);

    if (confJson.isMember("buffer_map_num") && confJson["buffer_map_num"].isInt())
        mMaxBufferNum = confJson["buffer_map_num"].asInt();
    else if (mIsOldPubRegion)
        mMaxBufferNum = INT32_FLAG(pub_max_buffer_num);
    else
        mMaxBufferNum = INT32_FLAG(max_buffer_num);

    if (confJson.isMember("send_request_concurrency") && confJson["send_request_concurrency"].isInt())
        mSendRequestConcurrency = confJson["send_request_concurrency"].asInt();
    else
        mSendRequestConcurrency = INT32_FLAG(send_request_concurrency);
    LogtailMonitor::Instance()->UpdateConstMetric("send_request_concurrency", mSendRequestConcurrency);

    if (confJson.isMember("process_thread_count") && confJson["process_thread_count"].isInt())
        mProcessThreadCount = confJson["process_thread_count"].asInt();
    else
        mProcessThreadCount = INT32_FLAG(process_thread_count);

    LoadInt32Parameter(INT32_FLAG(logreader_max_rotate_queue_size),
                       confJson,
                       "logreader_max_rotate_queue_size",
                       "ALIYUN_LOGTAIL_LOGREADER_MAX_ROTATE_QUEUE_SIZE");

    LoadInt32Parameter(INT32_FLAG(logreader_filedeleted_remove_interval),
                       confJson,
                       "logreader_filedeleted_remove_interval",
                       "ALIYUN_LOGTAIL_LOGREADER_FILEDELETED_REMOVE_INTERVAL");

    LoadInt32Parameter(INT32_FLAG(check_handler_timeout_interval),
                       confJson,
                       "check_handler_timeout_interval",
                       "ALIYUN_LOGTAIL_CHECK_HANDLER_TIMEOUT_INTERVAL");

    LoadInt32Parameter(INT32_FLAG(reader_close_unused_file_time),
                       confJson,
                       "reader_close_unused_file_time",
                       "ALIYUN_LOGTAIL_READER_CLOSE_UNUSED_FILE_TIME");

    if (confJson.isMember("main_loop_check_interval") && confJson["main_loop_check_interval"].isInt())
        INT32_FLAG(main_loop_check_interval) = confJson["main_loop_check_interval"].asInt();


    if (confJson.isMember("log_profile_save_interval") && confJson["log_profile_save_interval"].isInt())
        LogFileProfiler::GetInstance()->SetProfileInterval(confJson["log_profile_save_interval"].asInt());

    LOG_DEBUG(sLogger,
              ("logreader delete interval", INT32_FLAG(logreader_filedeleted_remove_interval))(
                  "check handler interval", INT32_FLAG(check_handler_timeout_interval))(
                  "reader close interval", INT32_FLAG(reader_close_unused_file_time))(
                  "main loop interval", INT32_FLAG(main_loop_check_interval))(
                  "profile interval", LogFileProfiler::GetInstance()->GetProfileInterval()));


    if (confJson.isMember("cpu_usage_limit")) {
        if (confJson["cpu_usage_limit"].isDouble())
            mCpuUsageUpLimit = confJson["cpu_usage_limit"].asDouble();
        else if (confJson["cpu_usage_limit"].isIntegral())
            mCpuUsageUpLimit = double(confJson["cpu_usage_limit"].asInt64());
        else if (mIsOldPubRegion)
            mCpuUsageUpLimit = DOUBLE_FLAG(pub_cpu_usage_up_limit);
        else
            mCpuUsageUpLimit = DOUBLE_FLAG(cpu_usage_up_limit);
    } else if (mIsOldPubRegion)
        mCpuUsageUpLimit = DOUBLE_FLAG(pub_cpu_usage_up_limit);
    else
        mCpuUsageUpLimit = DOUBLE_FLAG(cpu_usage_up_limit);

    if (confJson.isMember("mem_usage_limit") && confJson["mem_usage_limit"].isIntegral())
        mMemUsageUpLimit = confJson["mem_usage_limit"].asInt64();
    else if (mIsOldPubRegion)
        mMemUsageUpLimit = INT64_FLAG(pub_memory_usage_up_limit);
    else
        mMemUsageUpLimit = INT64_FLAG(memory_usage_up_limit);

    if ((mMemUsageUpLimit == 80 || mMemUsageUpLimit == 100)
        && mMemUsageUpLimit < INT64_FLAG(pub_memory_usage_up_limit)) {
        mMemUsageUpLimit = INT64_FLAG(pub_memory_usage_up_limit);
    }

    if (confJson.isMember("resource_auto_scale") && confJson["resource_auto_scale"].isBool())
        mResourceAutoScale = confJson["resource_auto_scale"].asBool();
    else
        mResourceAutoScale = BOOL_FLAG(default_resource_auto_scale);

    if (confJson.isMember("input_flow_control") && confJson["input_flow_control"].isBool())
        mInputFlowControl = confJson["input_flow_control"].asBool();
    else
        mInputFlowControl = BOOL_FLAG(default_input_flow_control);

    if (confJson.isMember("machine_cpu_usage_threshold") && confJson["machine_cpu_usage_threshold"].isDouble())
        mMachineCpuUsageThreshold = confJson["machine_cpu_usage_threshold"].asDouble();
    else
        mMachineCpuUsageThreshold = DOUBLE_FLAG(default_machine_cpu_usage_threshold);

    if (confJson.isMember("scaled_cpu_usage_limit")) {
        if (confJson["scaled_cpu_usage_limit"].isDouble())
            mScaledCpuUsageUpLimit = confJson["scaled_cpu_usage_limit"].asDouble();
        else if (confJson["scaled_cpu_usage_limit"].isIntegral())
            mScaledCpuUsageUpLimit = double(confJson["scaled_cpu_usage_limit"].asInt64());
        else
            mScaledCpuUsageUpLimit = DOUBLE_FLAG(cpu_usage_up_limit);
    }

    if (confJson.isMember("buffer_file_path") && confJson["buffer_file_path"].isString())
        mBufferFilePath = confJson["buffer_file_path"].asString();
    else
        mBufferFilePath = STRING_FLAG(default_buffer_file_path);

    if (confJson.isMember("check_point_filename") && confJson["check_point_filename"].isString())
        mCheckPointFilePath = confJson["check_point_filename"].asString();
    else if (confJson.isMember("check_point_file_path") && confJson["check_point_file_path"].isString())
        mCheckPointFilePath = confJson["check_point_file_path"].asString();
    else
        mCheckPointFilePath = STRING_FLAG(check_point_filename);
    LoadStringParameter(mCheckPointFilePath,
                        confJson,
                        NULL, // Only load from env.
                        "ALIYUN_LOGTAIL_CHECK_POINT_PATH");
    mCheckPointFilePath = AbsolutePath(mCheckPointFilePath, mProcessExecutionDir);
    LOG_INFO(sLogger, ("logtail checkpoint path", mCheckPointFilePath));

    if (confJson.isMember("send_prefer_real_ip") && confJson["send_prefer_real_ip"].isBool()) {
        BOOL_FLAG(send_prefer_real_ip) = confJson["send_prefer_real_ip"].asBool();
    }

    if (confJson.isMember("send_switch_real_ip_interval") && confJson["send_switch_real_ip_interval"].isInt()) {
        INT32_FLAG(send_switch_real_ip_interval) = confJson["send_switch_real_ip_interval"].asInt();
    }

    LoadInt32Parameter(INT32_FLAG(truncate_pos_skip_bytes),
                       confJson,
                       "truncate_pos_skip_bytes",
                       "ALIYUN_LOGTAIL_TRUNCATE_POS_SKIP_BYTES");

    if (BOOL_FLAG(send_prefer_real_ip)) {
        LOG_INFO(sLogger,
                 ("change send policy, prefer use real ip, switch interval seconds",
                  INT32_FLAG(send_switch_real_ip_interval))("truncate skip read offset",
                                                            INT32_FLAG(truncate_pos_skip_bytes)));
    }

    if (confJson.isMember("ignore_dir_inode_changed") && confJson["ignore_dir_inode_changed"].isBool()) {
        mIgnoreDirInodeChanged = confJson["ignore_dir_inode_changed"].asBool();
    }

    LoadStringParameter(mUserConfigPath, confJson, "user_config_file_path", "ALIYUN_LOGTAIL_USER_CONIFG_PATH");

    LoadStringParameter(
        mUserLocalConfigPath, confJson, "user_local_config_filename", "ALIYUN_LOGTAIL_USER_LOCAL_CONFIG_FILENAME");

    LoadBooleanParameter(
        BOOL_FLAG(ilogtail_discard_old_data), confJson, "discard_old_data", "ALIYUN_LOGTAIL_DISCARD_OLD_DATA");

    if (confJson.isMember("container_mode") && confJson["container_mode"].isBool()) {
        mContainerMode = confJson["container_mode"].asBool();
    }

    if (confJson.isMember("container_mount_path") && confJson["container_mount_path"].isString()) {
        mContainerMountConfigPath = confJson["container_mount_path"].asString();
    } else {
        mContainerMountConfigPath = GetProcessExecutionDir() + STRING_FLAG(default_container_mount_path);
    }

    LoadStringParameter(mConfigIP, confJson, "working_ip", "ALIYUN_LOGTAIL_WORKING_IP");

    LoadStringParameter(mCustomizedConfigIP, confJson, "customized_config_ip", "ALIYUN_LOGTAIL_CUSTOMIZED_CONFIG_IP");

    LoadStringParameter(mConfigHostName, confJson, "working_hostname", "ALIYUN_LOGTAIL_WORKING_HOSTNAME");

    // try to get zone env name from conf json
    if (confJson.isMember("alipay_zone_env_name") && confJson["alipay_zone_env_name"].isString()) {
        STRING_FLAG(alipay_zone_env_name) = confJson["alipay_zone_env_name"].asString();
    }
    // get zone info from env, for ant
    do {
        if (!STRING_FLAG(alipay_zone_env_name).empty()) {
            const char* alipayZone = getenv(STRING_FLAG(alipay_zone_env_name).c_str());
            if (alipayZone != NULL) {
                mAlipayZone = alipayZone;
                break;
            }
        }
        const char* alipayZone = getenv(STRING_FLAG(alipay_app_zone).c_str());
        if (alipayZone != NULL) {
            mAlipayZone = alipayZone;
            break;
        }
        alipayZone = getenv(STRING_FLAG(alipay_zone).c_str());
        if (alipayZone != NULL) {
            mAlipayZone = alipayZone;
            break;
        }
    } while (false);

    if (confJson.isMember("alipay_zone") && confJson["alipay_zone"].isString()) {
        mAlipayZone = confJson["alipay_zone"].asString();
    }

    if (confJson.isMember("system_boot_time") && confJson["system_boot_time"].isInt()) {
        mSystemBootTime = confJson["system_boot_time"].asInt();
    }

    LoadStringParameter(
        mDockerFilePathConfig, confJson, "docker_file_cache_path", "ALIYUN_LOGTAIL_DOCKER_FILE_CACHE_PATH");

    LoadInt32Parameter(INT32_FLAG(max_docker_config_update_times),
                       confJson,
                       "max_docker_config_update_times",
                       "ALIYUN_LOGTAIL_MAX_DOCKER_CONFIG_UPDATE_TIMES");

    LoadInt32Parameter(INT32_FLAG(docker_config_update_interval),
                       confJson,
                       "docker_config_update_interval",
                       "ALIYUN_LOGTAIL_DOCKER_CONFIG_UPDATE_INTERVAL");

    if (mContainerMode) {
        LogtailMonitor::Instance()->UpdateConstMetric("container_mode", true);
        LogtailMonitor::Instance()->UpdateConstMetric("container_mount_config", mContainerMountConfigPath);
        LOG_INFO(sLogger,
                 ("logtail now working on container mode, container mount path",
                  mContainerMountConfigPath)("working ip", mConfigIP)("working hostname", mConfigHostName));
        // LoadMountPaths should been called before LoadConfig
        ConfigManager::GetInstance()->LoadMountPaths();
    }

    if (confJson.isMember("no_inotify") && confJson["no_inotify"].isBool()) {
        mNoInotify = confJson["no_inotify"].asBool();
        LOG_INFO(sLogger, ("set no_inotify flag", mNoInotify));
    }

    if (confJson.isMember("inotify_black_list") && confJson["inotify_black_list"].isArray()) {
        const Json::Value& blackList = confJson["inotify_black_list"];
        for (Json::ArrayIndex i = 0; i < blackList.size(); ++i) {
            const Json::Value& item = blackList[i];
            if (item.isString()) {
                mInotifyBlackList.insert(item.asString());
                LOG_INFO(sLogger, ("add inoitfy black list", item.asString()));
            }
        }
    }

    LoadInt32Parameter(mSendDataPort, confJson, "data_server_port", "ALIYUN_LOGTAIL_DATA_SERVER_PORT");

    if (confJson.isMember("shennong_unix_socket") && confJson["shennong_unix_socket"].isBool()) {
        mShennongSocket = confJson["shennong_unix_socket"].asBool();
    }

    {
        int32_t maxReadBufferSize = 0;
        if (LoadInt32Parameter(
                maxReadBufferSize, confJson, "max_read_buffer_size", "ALIYUN_LOGTAIL_MAX_READ_BUFFER_SIZE")) {
            LogFileReader::SetReadBufferSize(maxReadBufferSize);
        }
    }

    LoadInt32Parameter(
        INT32_FLAG(default_tail_limit_kb), confJson, "default_tail_limit_kb", "ALIYUN_LOGTAIL_DEFAULT_TAIL_LIMIT_KB");

    if (confJson.isMember("read_local_event_interval") && confJson["read_local_event_interval"].isInt()) {
        INT32_FLAG(read_local_event_interval) = confJson["read_local_event_interval"].asInt();
        LOG_INFO(sLogger, ("set read_local_event_interval", INT32_FLAG(read_local_event_interval)));
    }

    LoadInt32Parameter(INT32_FLAG(check_point_dump_interval),
                       confJson,
                       "check_point_dump_interval",
                       "ALIYUN_LOGTAIL_CHECKPOINT_DUMP_INTERVAL");

    if (confJson.isMember("rapid_retry_update_config") && confJson["rapid_retry_update_config"].isBool()) {
        BOOL_FLAG(rapid_retry_update_config) = confJson["rapid_retry_update_config"].asBool();
        LOG_INFO(sLogger, ("set rapid_retry_update_config", BOOL_FLAG(rapid_retry_update_config)));
    }

    if (confJson.isMember("check_profile_region") && confJson["check_profile_region"].isBool()) {
        BOOL_FLAG(check_profile_region) = confJson["check_profile_region"].asBool();
        LOG_INFO(sLogger, ("set check_profile_region", BOOL_FLAG(check_profile_region)));
    }

    if (confJson.isMember("enable_collection_mark") && confJson["enable_collection_mark"].isBool()) {
        BOOL_FLAG(enable_collection_mark) = confJson["enable_collection_mark"].asBool();
        LOG_INFO(sLogger, ("set enable_collection_mark", BOOL_FLAG(enable_collection_mark)));
    }

    LoadBooleanParameter(BOOL_FLAG(enable_collection_mark),
                         confJson,
                         "enable_env_ref_in_config",
                         "ALIYUN_LOGTAIL_ENABLE_ENV_REF_IN_CONFIG");

    LoadBooleanParameter(
        mEnableHostIPReplace, confJson, "enable_host_ip_replace", "ALIYUN_LOGTAIL_ENABLE_HOST_IP_REPLACE");

    LoadBooleanParameter(mEnableResponseVerification,
                         confJson,
                         "enable_response_verification",
                         "ALIYUN_LOGTAIL_ENABLE_RESPONSE_VERIFICATION");

    LoadEnvResourceLimit();
    CheckAndAdjustParameters();
}

void AppConfigBase::LoadOtherConf(const Json::Value& confJson) {
    if (confJson.isMember("mapping_conf_path") && confJson["mapping_conf_path"].isString())
        mMappingConfigPath = confJson["mapping_conf_path"].asString();
    else
        mMappingConfigPath = STRING_FLAG(default_mapping_config_path);

    {
        int32_t oasConnectTimeout = 0;
        if (LoadInt32Parameter(
                oasConnectTimeout, confJson, "oas_connect_timeout", "ALIYUN_LOGTAIL_OAS_CONNECT_TIMEOUT")) {
            INT32_FLAG(default_oas_connect_timeout)
                = std::max(INT32_FLAG(default_oas_connect_timeout), oasConnectTimeout);
        }
    }

    {
        int32_t oasRequestTimeout = 0;
        if (LoadInt32Parameter(
                oasRequestTimeout, confJson, "oas_request_timeout", "ALIYUN_LOGTAIL_OAS_REQUEST_TIMEOUT")) {
            INT32_FLAG(default_oas_request_timeout)
                = std::max(INT32_FLAG(default_oas_request_timeout), oasRequestTimeout);
        }
    }

    {
        int32_t timeout = 0;
        if (LoadInt32Parameter(
                timeout, confJson, "force_quit_read_timeout", "ALIYUN_LOGTAIL_FORCE_QUIT_READ_TIMEOUT")) {
            if (timeout <= 600) {
                LOG_WARNING(sLogger, ("invalid force_quit_read_timeout value, at least 600", timeout));
            } else {
                mForceQuitReadTimeout = timeout;
            }
        }
    }

    LoadBooleanParameter(mEnableCheckpointSyncWrite,
                         confJson,
                         "enable_checkpoint_sync_write",
                         "ALIYUN_LOGTAIL_ENABLE_CHECKPOINT_SYNC_WRITE");

    LoadBooleanParameter(mEnableLogTimeAutoAdjust,
                         confJson,
                         "enable_log_time_auto_adjust",
                         "ALIYUN_LOGTAIL_ENABLE_LOG_TIME_AUTO_ADJUST");

    LoadInt32Parameter(INT32_FLAG(search_checkpoint_default_dir_depth),
                       confJson,
                       "search_checkpoint_default_dir_depth",
                       "ALIYUN_LOGTAIL_SEARCH_CHECKPOINT_DEFAULT_DIR_DEPTH");

    LoadInt32Parameter(INT32_FLAG(checkpoint_find_max_file_count),
                       confJson,
                       "checkpoint_find_max_file_count",
                       "ALIYUN_LOGTAIL_CHECKPOINT_FIND_MAX_FILE_COUNT");

    LoadBooleanParameter(BOOL_FLAG(enable_polling_discovery),
                         confJson,
                         "enable_polling_discovery",
                         "ALIYUN_LOGTAIL_ENABLE_POLLING_DISCOVERY");
    LoadInt32Parameter(INT32_FLAG(dirfile_check_interval_ms),
                       confJson,
                       "polling_dirfile_check_interval_ms",
                       "ALIYUN_LOGTAIL_POLLING_DIRFILE_CHECK_INTERVAL_MS");
    LoadInt32Parameter(INT32_FLAG(polling_dir_first_watch_timeout),
                       confJson,
                       "polling_dir_first_watch_timeout",
                       "ALIYUN_LOGTAIL_POLLING_DIR_FIRST_WATCH_TIMEOUT");
    LoadInt32Parameter(INT32_FLAG(polling_file_first_watch_timeout),
                       confJson,
                       "polling_file_first_watch_timeout",
                       "ALIYUN_LOGTAIL_POLLING_FILE_FIRST_WATCH_TIMEOUT");
    LoadInt32Parameter(INT32_FLAG(modify_check_interval),
                       confJson,
                       "polling_modify_check_interval",
                       "ALIYUN_LOGTAIL_POLLING_MODIFY_CHECK_INTERVAL");
    LoadInt32Parameter(INT32_FLAG(ignore_file_modify_timeout),
                       confJson,
                       "polling_ignore_file_modify_timeout",
                       "ALIYUN_LOGTAIL_POLLING_IGNORE_FILE_MODIFY_TIMEOUT");
}

void AppConfigBase::InitEnvMapping(const std::string& envStr, std::map<std::string, std::string>& envMapping) {
    int pos = envStr.find('=');
    if (pos > 0 && size_t(pos) < envStr.size()) {
        const std::string& key = envStr.substr(0, pos);
        const std::string& value = envStr.substr(pos + 1);
        envMapping[key] = value;
    } else {
        APSARA_LOG_WARNING(sLogger, ("error to find ", "")("pos", pos)("env string", envStr));
    }
}
void AppConfigBase::SetConfigFlag(const std::string& flagName, const std::string& value) {
    GFLAGS_NAMESPACE::CommandLineFlagInfo info;
    bool rst = GetCommandLineFlagInfo(flagName.c_str(), &info);
    if (rst) {
        string beforeValue = info.current_value;
        string setrst = GFLAGS_NAMESPACE::SetCommandLineOption(flagName.c_str(), value.c_str());
        GetCommandLineFlagInfo(flagName.c_str(), &info);
        APSARA_LOG_DEBUG(sLogger,
                         ("Set config flag", flagName)("before value", beforeValue)("after value", info.current_value)(
                             "result", setrst.size() == 0 ? ("error with value " + value) : setrst));
    } else {
        APSARA_LOG_DEBUG(sLogger, ("Flag not define", flagName));
    }
}

#if defined(_MSC_VER)
#include "processenv.h"
#endif

void AppConfigBase::ParseEnvToFlags() {
    std::map<std::string, std::string> envMapping;

#if defined(__linux__) || defined(__APPLE__)
    if (environ != NULL) {
        for (size_t i = 0; environ[i] != NULL; i++) {
            std::string envStr = environ[i];
            InitEnvMapping(envStr, envMapping);
        }
    }
#elif defined(_MSC_VER)
    LPTSTR lpszVariable;
    LPTCH lpvEnv;
    lpvEnv = GetEnvironmentStrings();
    if (lpvEnv == NULL) {
        APSARA_LOG_WARNING(sLogger, ("GetEnvironmentStrings failed ", GetLastError()));
    } else {
        lpszVariable = (LPTSTR)lpvEnv;
        while (*lpszVariable) {
            std::string envStr = (std::string)lpszVariable;
            InitEnvMapping(envStr, envMapping);
            lpszVariable += lstrlen(lpszVariable) + 1;
        }
    }
#endif
    for (auto iter = envMapping.begin(); iter != envMapping.end(); ++iter) {
        const std::string& key = iter->first;
        const std::string& value = iter->second;
        SetConfigFlag(key, value);
    }
}

void AppConfigBase::DumpAllFlagsToMap(std::unordered_map<std::string, std::string>& flagMap) {
    flagMap.clear();
    std::vector<GFLAGS_NAMESPACE::CommandLineFlagInfo> OUTPUT;
    vector<string> detail;
    GFLAGS_NAMESPACE::GetAllFlags(&OUTPUT);
    for (auto flagInfo : OUTPUT) {
        flagMap[flagInfo.name] = flagInfo.current_value;
        detail.push_back(flagInfo.name + ":" + flagInfo.current_value + "\n");
    }
    LOG_DEBUG(sLogger, ("DumpAllFlagsToMap", flagMap.size())("Detail", ToString(detail)));
}

void AppConfigBase::ReadFlagsFromMap(const std::unordered_map<std::string, std::string>& flagMap) {
    for (auto iter : flagMap) {
        SetConfigFlag(iter.first, iter.second);
    }
    LOG_DEBUG(sLogger, ("ReadFlagsFromMap", flagMap.size()));
}

void AppConfigBase::ParseJsonToFlags(const Json::Value& confJson) {
    const static unordered_set<string> sForceKeySet = {"config_server_address_list"};
    const static unordered_set<string> sIgnoreKeySet = {"data_server_list"};
    for (auto name : confJson.getMemberNames()) {
        if (sIgnoreKeySet.find(name) != sIgnoreKeySet.end()) {
            continue;
        }
        auto jsonvalue = confJson[name];
        if (jsonvalue.isConvertibleTo(Json::stringValue)) {
            SetConfigFlag(name, jsonvalue.asString());
        } else if (sForceKeySet.find(name) != sForceKeySet.end()) {
            SetConfigFlag(name, jsonvalue.toStyledString());
        } else {
            APSARA_LOG_INFO(sLogger,
                            ("Set config flag failed", "can not convert json value to flag")("flag name", name)(
                                "jsonvalue", jsonvalue.toStyledString()));
        }
    }
}

void AppConfigBase::LoadGlobalFuseConf(const Json::Value& confJson) {
    if (confJson.isMember("global_fuse_mode") && confJson["global_fuse_mode"].isBool()) {
        BOOL_FLAG(default_global_fuse_mode) = confJson["global_fuse_mode"].asBool();
        LOG_INFO(sLogger, ("set global_fuse_mode", BOOL_FLAG(default_global_fuse_mode)));
    }
}

void AppConfigBase::CheckAndAdjustParameters() {
    // the max buffer size is 4GB
    // if "fileNum * fileSize" from config more than 4GB, logtail will do restrictions
    int32_t fileNum = GetNumOfBufferFile();
    int32_t fileSize = GetLocalFileSize();
    if (fileNum > 50)
        fileNum = 50;
    int64_t max_buffer_size = ((int64_t)4) * 1024 * 1024 * 1024;
    if (((int64_t)GetNumOfBufferFile() * GetLocalFileSize()) > max_buffer_size)
        fileSize = max_buffer_size / fileNum;
    mNumOfBufferFile = fileNum;
    mLocalFileSize = fileSize;

    int32_t defaultMaxStatCount = INT32_FLAG(polling_max_stat_count);
    int32_t defaultMaxStatPerDirCount = INT32_FLAG(polling_max_stat_count_per_dir);
    int32_t defaultMaxStatPerConfigCount = INT32_FLAG(polling_max_stat_count_per_config);
    int32_t defaultMaxCacheSize = INT32_FLAG(config_match_max_cache_size);
    int32_t defaultMaxModifyCache = INT32_FLAG(modify_cache_max);
    int32_t defaultMaxWatchDirCount = INT32_FLAG(max_watch_dir_count);
    int32_t defaultMaxOpenFiles = INT32_FLAG(max_open_files_limit);
    int32_t defaultMaxReaderOpenFiles = INT32_FLAG(max_reader_open_files);


    int64_t baseMemSize = 100; //MB
    int32_t nowMaxStatCount = defaultMaxStatCount / 20;
    int32_t nowMaxStatPerDirCount = defaultMaxStatPerDirCount / 20;
    int32_t nowMaxCacheSize = defaultMaxCacheSize / 20;
    int32_t nowMaxModifyCache = defaultMaxModifyCache / 20;
    int32_t nowMaxWatchDirCount = defaultMaxWatchDirCount / 20;
    int32_t nowMaxOpenFiles = defaultMaxOpenFiles / 20;
    int32_t nowMaxReaderOpenFiles = defaultMaxReaderOpenFiles / 20;

    double scaleRatio = mMemUsageUpLimit / baseMemSize;
    if (scaleRatio > 1000.) {
        scaleRatio = 1000.;
    }
    if (scaleRatio < 0.1) {
        scaleRatio = 0.1;
    }
    INT32_FLAG(polling_max_stat_count) = (int32_t)(nowMaxStatCount * scaleRatio);
    INT32_FLAG(polling_max_stat_count_per_dir) = (int32_t)(nowMaxStatPerDirCount * scaleRatio);
    INT32_FLAG(polling_max_stat_count_per_config) = (int32_t)(nowMaxStatPerDirCount * scaleRatio);
    INT32_FLAG(config_match_max_cache_size) = (int32_t)(nowMaxCacheSize * scaleRatio);
    INT32_FLAG(modify_cache_max) = (int32_t)(nowMaxModifyCache * scaleRatio);
    INT32_FLAG(max_watch_dir_count) = (int32_t)(nowMaxWatchDirCount * scaleRatio);
    INT32_FLAG(max_open_files_limit) = (int32_t)(nowMaxOpenFiles * scaleRatio);
    INT32_FLAG(max_reader_open_files) = (int32_t)(nowMaxReaderOpenFiles * scaleRatio);

    if (INT32_FLAG(polling_max_stat_count) > defaultMaxStatCount) {
        INT32_FLAG(polling_max_stat_count) = defaultMaxStatCount;
    }
    if (INT32_FLAG(polling_max_stat_count_per_dir) > defaultMaxStatPerDirCount) {
        INT32_FLAG(polling_max_stat_count_per_dir) = defaultMaxStatPerDirCount;
    }
    if (INT32_FLAG(polling_max_stat_count_per_config) > defaultMaxStatPerConfigCount) {
        INT32_FLAG(polling_max_stat_count_per_config) = defaultMaxStatPerConfigCount;
    }
    if (INT32_FLAG(config_match_max_cache_size) > defaultMaxCacheSize) {
        INT32_FLAG(config_match_max_cache_size) = defaultMaxCacheSize;
    }
    if (INT32_FLAG(modify_cache_max) > defaultMaxModifyCache) {
        INT32_FLAG(modify_cache_max) = defaultMaxModifyCache;
    }
    if (INT32_FLAG(max_watch_dir_count) > defaultMaxWatchDirCount) {
        INT32_FLAG(max_watch_dir_count) = defaultMaxWatchDirCount;
    }
    if (INT32_FLAG(max_open_files_limit) > defaultMaxOpenFiles) {
        INT32_FLAG(max_open_files_limit) = defaultMaxOpenFiles;
    }
    if (INT32_FLAG(max_reader_open_files) > defaultMaxReaderOpenFiles) {
        INT32_FLAG(max_reader_open_files) = defaultMaxReaderOpenFiles;
    }
    LOG_INFO(sLogger,
             ("default cache size, polling total max stat",
              INT32_FLAG(polling_max_stat_count))("per dir max stat", INT32_FLAG(polling_max_stat_count_per_dir))(
                 "per config max stat", INT32_FLAG(polling_max_stat_count_per_config))(
                 "cache config max size", INT32_FLAG(config_match_max_cache_size))(
                 "modify max", INT32_FLAG(modify_cache_max))("watch dir count max", INT32_FLAG(max_watch_dir_count))(
                 "max open files limit", INT32_FLAG(max_open_files_limit))("max reader open files limit",
                                                                           INT32_FLAG(max_reader_open_files)));

    LOG_INFO(sLogger,
             ("send byte per second limit", mMaxBytePerSec)("batch send interval", INT32_FLAG(batch_send_interval))(
                 "batch send size", INT32_FLAG(batch_send_metric_size)));
    // when inflow exceed 30MB/s, FlowControl lose precision
    if (mMaxBytePerSec >= 30 * 1024 * 1024) {
        if (mSendFlowControl)
            mSendFlowControl = false;
        if (mSendRandomSleep)
            mSendRandomSleep = false;
        LOG_INFO(sLogger, ("send flow control", "disable")("send random sleep", "disable"));
    }
}

bool AppConfigBase::IsInInotifyBlackList(const std::string& path) const {
    bool rst = mInotifyBlackList.find(path) != mInotifyBlackList.end();
    if (rst) {
        LOG_INFO(sLogger, ("this path is in inotify black list, skip inoitfy add watch", path));
    }
    return rst;
}

// TODO: Use Boost instead.
// boost::filesystem::directory_iterator end;
// try { boost::filesystem::directory_iterator(path); } catch (...) { // failed } // OK
void AppConfigBase::SetLogtailSysConfDir(const std::string& dirPath) {
    mLogtailSysConfDir = dirPath;
    if (dirPath.back() != '/' || dirPath.back() != '\\') {
        mLogtailSysConfDir += PATH_SEPARATOR;
    }

    if (!CheckExistance(mLogtailSysConfDir)) {
        if (Mkdir(mLogtailSysConfDir)) {
            LOG_INFO(sLogger, ("sys conf dir is not existing, create", "done"));
        } else {
            LOG_WARNING(sLogger, ("sys conf dir is not existing, create", "failed"));
        }
    }

#if defined(__linux__) || defined(__APPLE__)
    DIR* dir = opendir(dirPath.c_str());
    if (NULL == dir) {
        int savedErrno = errno;
        LOG_WARNING(sLogger, ("open sys conf dir error", dirPath)("error", strerror(errno)));
        if (savedErrno == EACCES || savedErrno == ENOTDIR || savedErrno == ENOENT) {
            mLogtailSysConfDir = GetProcessExecutionDir();
        }
    } else {
        closedir(dir);
    }
#elif defined(_MSC_VER)
    DWORD ret = GetFileAttributes(mLogtailSysConfDir.c_str());
    if (INVALID_FILE_ATTRIBUTES == ret) {
        mLogtailSysConfDir = GetProcessExecutionDir();
    }
#endif

    // Update related configurations (local user config).
    if (STRING_FLAG(ilogtail_local_config).empty()) {
        LOG_WARNING(sLogger, ("flag error", "ilogtail_local_config must be non-empty"));
        STRING_FLAG(ilogtail_local_config) = DEFAULT_ILOGTAIL_LOCAL_CONFIG_FLAG_VALUE;
    }
    if (STRING_FLAG(ilogtail_local_config_dir).empty()) {
        LOG_WARNING(sLogger, ("flag error", "ilogtail_local_config_dir must be non-empty"));
        STRING_FLAG(ilogtail_local_config_dir) = DEFAULT_ILOGTAIL_LOCAL_CONFIG_DIR_FLAG_VALUE;
    }
    mUserLocalConfigPath = AbsolutePath(STRING_FLAG(ilogtail_local_config), mLogtailSysConfDir);
    mUserLocalConfigDirPath = AbsolutePath(STRING_FLAG(ilogtail_local_config_dir), mLogtailSysConfDir) + PATH_SEPARATOR;
    mUserLocalYamlConfigDirPath
        = AbsolutePath(STRING_FLAG(ilogtail_local_yaml_config_dir), mLogtailSysConfDir) + PATH_SEPARATOR;
    LOG_INFO(sLogger,
             ("set logtail sys conf dir", mLogtailSysConfDir)("user local config path", mUserLocalConfigPath)(
                 "user local config dir path", mUserLocalConfigDirPath)("user local yaml config dir path",
                                                                        mUserLocalYamlConfigDirPath));
}

} // namespace logtail
