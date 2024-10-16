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

#include "AppConfig.h"

#include <algorithm>
#include <boost/filesystem.hpp>
#include <filesystem>
#include <iostream>
#include <utility>

#include "RuntimeUtil.h"
#include "common/EnvUtil.h"
#include "common/FileSystemUtil.h"
#include "common/JsonUtil.h"
#include "common/LogtailCommonFlags.h"
#include "config/watcher/InstanceConfigWatcher.h"
#include "file_server/ConfigManager.h"
#include "file_server/reader/LogFileReader.h"
#include "json/value.h"
#include "logger/Logger.h"
#include "monitor/LogFileProfiler.h"
#include "monitor/LogtailAlarm.h"
#include "monitor/Monitor.h"
#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#endif

using namespace std;

DEFINE_FLAG_BOOL(logtail_mode, "logtail mode", false);
DEFINE_FLAG_INT32(max_buffer_num, "max size", 40);
DEFINE_FLAG_INT32(pub_max_buffer_num, "max size", 8);
DEFINE_FLAG_INT32(pub_max_send_byte_per_sec, "the max send speed per sec, realtime thread", 20 * 1024 * 1024);
DEFINE_FLAG_INT32(default_send_byte_per_sec, "the max send speed per sec, replay buffer thread", 2 * 1024 * 1024);
DEFINE_FLAG_INT32(pub_send_byte_per_sec, "the max send speed per sec, replay buffer thread", 1 * 1024 * 1024);
DEFINE_FLAG_INT32(default_buffer_file_num, "how many buffer files in default", 50);
DEFINE_FLAG_INT32(pub_buffer_file_num, "how many buffer files in default", 25);
DEFINE_FLAG_INT32(default_local_file_size, "default size of one buffer file", 20 * 1024 * 1024);
DEFINE_FLAG_INT32(pub_local_file_size, "default size of one buffer file", 20 * 1024 * 1024);
DEFINE_FLAG_INT32(process_thread_count, "", 1);
DEFINE_FLAG_INT32(send_request_concurrency, "max count keep in mem when async send", 10);
DEFINE_FLAG_STRING(default_buffer_file_path, "set current execution dir in default", "");
DEFINE_FLAG_STRING(buffer_file_path, "set buffer dir", "");
// DEFINE_FLAG_STRING(default_mapping_config_path, "", "mapping_config.json");
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
// const char* DEFAULT_ILOGTAIL_LOCAL_CONFIG_FLAG_VALUE = "user_local_config.json";
// DEFINE_FLAG_STRING(ilogtail_local_config, "local ilogtail config file", DEFAULT_ILOGTAIL_LOCAL_CONFIG_FLAG_VALUE);
// const char* DEFAULT_ILOGTAIL_LOCAL_CONFIG_DIR_FLAG_VALUE = "user_config.d";
// DEFINE_FLAG_STRING(ilogtail_local_config_dir,
//                    "local ilogtail config file dir",
//                    DEFAULT_ILOGTAIL_LOCAL_CONFIG_DIR_FLAG_VALUE);
// const char* DEFAULT_ILOGTAIL_LOCAL_YAML_CONFIG_DIR_FLAG_VALUE = "user_yaml_config.d";
// DEFINE_FLAG_STRING(ilogtail_local_yaml_config_dir,
//                    "local ilogtail yaml config file dir",
//                    DEFAULT_ILOGTAIL_LOCAL_YAML_CONFIG_DIR_FLAG_VALUE);
// const char* DEFAULT_ILOGTAIL_REMOTE_YAML_CONFIG_DIR_FLAG_VALUE = "remote_yaml_config.d";
// DEFINE_FLAG_STRING(ilogtail_remote_yaml_config_dir,
//                    "remote ilogtail yaml config file dir",
//                    DEFAULT_ILOGTAIL_REMOTE_YAML_CONFIG_DIR_FLAG_VALUE);

// DEFINE_FLAG_BOOL(default_global_fuse_mode, "default global fuse mode", false);
// DEFINE_FLAG_BOOL(default_global_mark_offset_flag, "default global mark offset flag", false);

// DEFINE_FLAG_STRING(default_container_mount_path, "", "container_mount.json");
DEFINE_FLAG_STRING(default_include_config_path, "", "config.d");

DEFINE_FLAG_INT32(default_oas_connect_timeout, "default (minimum) connect timeout for OSARequest", 5);
DEFINE_FLAG_INT32(default_oas_request_timeout, "default (minimum) request timeout for OSARequest", 10);
// DEFINE_FLAG_BOOL(rapid_retry_update_config, "", false);
DEFINE_FLAG_BOOL(check_profile_region, "", false);
// DEFINE_FLAG_BOOL(enable_collection_mark,
//                  "enable collection mark function to override check_ulogfs_env in user config",
//                  false);
// DEFINE_FLAG_BOOL(enable_env_ref_in_config, "enable environment variable reference replacement in configuration",
// false);
DEFINE_FLAG_INT32(data_server_port, "", 80);

// DEFINE_FLAG_STRING(alipay_app_zone, "", "ALIPAY_APP_ZONE");
// DEFINE_FLAG_STRING(alipay_zone, "", "ALIPAY_ZONE");
// DEFINE_FLAG_STRING(alipay_zone_env_name, "", "");

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

DECLARE_FLAG_INT32(batch_send_interval);
DECLARE_FLAG_INT32(batch_send_metric_size);

DECLARE_FLAG_BOOL(send_prefer_real_ip);
DECLARE_FLAG_INT32(send_switch_real_ip_interval);
DECLARE_FLAG_INT32(truncate_pos_skip_bytes);
DECLARE_FLAG_INT32(default_tail_limit_kb);

DEFINE_FLAG_STRING(ilogtail_docker_file_path_config, "ilogtail docker path config file", "docker_path_config.json");

DECLARE_FLAG_INT32(max_docker_config_update_times);
DECLARE_FLAG_INT32(docker_config_update_interval);

DECLARE_FLAG_INT32(read_local_event_interval);

DECLARE_FLAG_INT32(check_point_dump_interval);
#ifdef __ENTERPRISE__
DECLARE_FLAG_STRING(ilogtail_user_defined_id_env_name);
#endif

DECLARE_FLAG_INT32(logreader_max_rotate_queue_size);
DECLARE_FLAG_INT32(search_checkpoint_default_dir_depth);
DECLARE_FLAG_INT32(checkpoint_find_max_file_count);
DECLARE_FLAG_BOOL(enable_polling_discovery);
DECLARE_FLAG_INT32(dirfile_check_interval_ms);
DECLARE_FLAG_INT32(polling_dir_first_watch_timeout);
DECLARE_FLAG_INT32(polling_file_first_watch_timeout);
DECLARE_FLAG_INT32(modify_check_interval);
DECLARE_FLAG_INT32(ignore_file_modify_timeout);
DEFINE_FLAG_STRING(host_path_blacklist, "host path matches substring in blacklist will be ignored", "");
DEFINE_FLAG_STRING(ALIYUN_LOG_FILE_TAGS, "default env file key to load tags", "");
DEFINE_FLAG_INT32(max_holded_data_size,
                  "for every id and metric name, the max data size can be holded in memory (default 512KB)",
                  512 * 1024);
DEFINE_FLAG_INT32(pub_max_holded_data_size,
                  "for every id and metric name, the max data size can be holded in memory (default 512KB)",
                  512 * 1024);
DEFINE_FLAG_STRING(metrics_report_method,
                   "method to report metrics (default none, means logtail will not report metrics)",
                   "sls");

DEFINE_FLAG_STRING(loong_collector_operator_service, "loong collector operator service", "");
DEFINE_FLAG_INT32(loong_collector_operator_service_port, "loong collector operator service port", 8888);
DEFINE_FLAG_STRING(_pod_name_, "agent pod name", "");

DEFINE_FLAG_STRING(app_info_file, "", "app_info.json");
DEFINE_FLAG_STRING(crash_stack_file_name, "crash stack back trace file name", "backtrace.dat");
DEFINE_FLAG_STRING(local_event_data_file_name, "local event data file name", "local_event.json");
DEFINE_FLAG_STRING(inotify_watcher_dirs_dump_filename, "", "inotify_watcher_dirs");
DEFINE_FLAG_STRING(logtail_snapshot_dir, "snapshot dir on local disk", "snapshot");
DEFINE_FLAG_STRING(logtail_profile_snapshot, "reader profile on local disk", "logtail_profile_snapshot");
DEFINE_FLAG_STRING(ilogtail_config_env_name, "config file path", "ALIYUN_LOGTAIL_CONFIG");

#if defined(__linux__)
DEFINE_FLAG_STRING(adhoc_check_point_file_dir, "", "/tmp/logtail_adhoc_checkpoint");
#elif defined(_MSC_VER)
DEFINE_FLAG_STRING(adhoc_check_point_file_dir, "", "C:\\LogtailData\\logtail_adhoc_checkpoint");
#endif

#if defined(__linux__)
DEFINE_FLAG_STRING(check_point_filename, "", "/tmp/logtail_check_point");
#elif defined(_MSC_VER)
DEFINE_FLAG_STRING(check_point_filename, "", "C:\\LogtailData\\logtail_check_point");
#endif

DEFINE_FLAG_STRING(sls_observer_ebpf_host_path,
                   "the backup real host path for store libebpf.so",
                   "/etc/ilogtail/ebpf/");

namespace logtail {
constexpr int32_t kDefaultMaxSendBytePerSec = 25 * 1024 * 1024; // the max send speed per sec, realtime thread

std::string AppConfig::sLocalConfigDir = "local";
void CreateAgentDir() {
    try {
        const char* value = getenv("logtail_mode");
        if (value != NULL) {
            STRING_FLAG(logtail_mode) = StringTo<bool>(value);
        }
    } catch (const exception& e) {
        std::cout << "load config from env error, env_name:logtail_mode, error:" << e.what() << std::endl;
    }
    if (BOOL_FLAG(logtail_mode)) {
        return;
    }
    std::string processExecutionDir = GetProcessExecutionDir();
    Json::Value emptyJson;
#define PROCESSDIRFLAG(flag_name) \
    try { \
        const char* value = getenv(#flag_name); \
        if (value != NULL) { \
            STRING_FLAG(flag_name) = StringTo<string>(value); \
        } \
    } catch (const exception& e) { \
        std::cout << "load config from env error, env_name:" << #flag_name << "\terror:" << e.what() << std::endl; \
    } \
    if (STRING_FLAG(flag_name).empty()) { \
        STRING_FLAG(flag_name) = processExecutionDir + PATH_SEPARATOR; \
    } else { \
        STRING_FLAG(flag_name) = AbsolutePath(STRING_FLAG(flag_name), processExecutionDir); \
    } \
    if (!CheckExistance(STRING_FLAG(flag_name))) { \
        if (Mkdirs(STRING_FLAG(flag_name))) { \
            std::cout << STRING_FLAG(flag_name) + " dir is not existing, create done" << std::endl; \
        } else { \
            std::cout << STRING_FLAG(flag_name) + " dir is not existing, create failed" << std::endl; \
            exit(0); \
        } \
    }

    PROCESSDIRFLAG(loongcollector_conf_dir);
    PROCESSDIRFLAG(loongcollector_log_dir);
    PROCESSDIRFLAG(loongcollector_data_dir);
    PROCESSDIRFLAG(loongcollector_run_dir);
    PROCESSDIRFLAG(loongcollector_third_party_dir);
}

std::string GetAgentThirdPartyDir() {
    static std::string dir;
    if (!dir.empty()) {
        return dir;
    }
    if (BOOL_FLAG(logtail_mode)) {
        dir = AppConfig::GetInstance()->GetLoongcollectorConfDir();
    } else {
        dir = STRING_FLAG(loongcollector_third_party_dir) + PATH_SEPARATOR;
    }
    return dir;
}

std::string GetAgentLogDir() {
    static std::string dir;
    if (!dir.empty()) {
        return dir;
    }
#if defined(APSARA_UNIT_TEST_MAIN)
    dir = GetProcessExecutionDir();
#else
    if (BOOL_FLAG(logtail_mode)) {
        dir = GetProcessExecutionDir();
    } else {
        dir = STRING_FLAG(loongcollector_log_dir) + PATH_SEPARATOR;
    }
#endif
    return dir;
}

std::string GetAgentDataDir() {
    static std::string dir;
    if (!dir.empty()) {
        return dir;
    }
#if defined(APSARA_UNIT_TEST_MAIN)
    dir = GetProcessExecutionDir();
#else
    if (BOOL_FLAG(logtail_mode)) {
        dir = GetProcessExecutionDir();
    } else {
        dir = STRING_FLAG(loongcollector_data_dir) + PATH_SEPARATOR;
    }
#endif
    return dir;
}

std::string GetAgentConfDir() {
    static std::string dir;
    if (!dir.empty()) {
        return dir;
    }
#if defined(APSARA_UNIT_TEST_MAIN)
    dir = GetProcessExecutionDir();
#else
    if (BOOL_FLAG(logtail_mode)) {
        dir = GetProcessExecutionDir();
    } else {
        dir = STRING_FLAG(loongcollector_conf_dir) + PATH_SEPARATOR;
    }
#endif
    return dir;
}

std::string GetAgentRunDir() {
    static std::string dir;
    if (!dir.empty()) {
        return dir;
    }
#if defined(APSARA_UNIT_TEST_MAIN)
    dir = GetProcessExecutionDir();
#else
    if (BOOL_FLAG(logtail_mode)) {
        dir = GetProcessExecutionDir();
    } else {
        dir = STRING_FLAG(loongcollector_run_dir) + PATH_SEPARATOR;
    }
#endif
    return dir;
}

std::string GetAgentDockerPathConfig() {
    static std::string file_path;
    if (!file_path.empty()) {
        return file_path;
    }
    if (BOOL_FLAG(logtail_mode)) {
        file_path = GetAgentDataDir() + STRING_FLAG(ilogtail_docker_file_path_config);
    } else {
        file_path = GetAgentDataDir() + "docker_path_config.json";
    }
    return file_path;
}

std::string GetAgentConfDir(const ParseConfResult& res, const Json::Value& confJson) {
    std::string newConfDir;
    if (BOOL_FLAG(logtail_mode)) {
        if (res == CONFIG_OK) {
            // Should be loaded here because other parameters depend on it.
            LoadStringParameter(newConfDir, confJson, "logtail_sys_conf_dir", "ALIYUN_LOGTAIL_SYS_CONF_DIR");
        }
        if (newConfDir.empty()) {
            newConfDir = STRING_FLAG(logtail_sys_conf_dir);
        }
    } else {
        newConfDir = GetAgentConfDir();
    }
    return newConfDir;
}

std::string GetAgentConfigFile() {
    if (BOOL_FLAG(logtail_mode)) {
        // load ilogtail_config.json
        char* configEnv = getenv(STRING_FLAG(ilogtail_config_env_name).c_str());
        if (configEnv == NULL || strlen(configEnv) == 0) {
            return STRING_FLAG(ilogtail_config);
        } else {
            return configEnv;
        }
    } else {
        return LOONGCOLLECTOR_CONFIG;
    }
}

std::string GetAgentAppInfoFile() {
    static std::string file;
    if (!file.empty()) {
        return file;
    }
    if (BOOL_FLAG(logtail_mode)) {
        file = GetAgentRunDir() + STRING_FLAG(app_info_file);
    } else {
        file = GetAgentRunDir() + "app_info.json";
    }
    return file;
}

string GetAdhocCheckpointDirPath() {
    if (BOOL_FLAG(logtail_mode)) {
        return STRING_FLAG(adhoc_check_point_file_dir);
    } else {
        return GetAgentDataDir() + "adhoc_checkpoint";
    }
}

string GetCheckPointFileName() {
    if (BOOL_FLAG(logtail_mode)) {
        return STRING_FLAG(check_point_filename);
    } else {
        return GetAgentDataDir() + "file_check_point";
    }
}

string GetCrashStackFileName() {
    if (BOOL_FLAG(logtail_mode)) {
        return GetProcessExecutionDir() + STRING_FLAG(crash_stack_file_name);
    } else {
        return GetAgentDataDir() + "backtrace.dat";
    }
}

string GetLocalEventDataFileName() {
    if (BOOL_FLAG(logtail_mode)) {
        return STRING_FLAG(local_event_data_file_name);
    } else {
        return AppConfig::GetInstance()->GetLoongcollectorConfDir() + "local_event.json";
    }
}

string GetInotifyWatcherDirsDumpFileName() {
    if (BOOL_FLAG(logtail_mode)) {
        return GetProcessExecutionDir() + STRING_FLAG(inotify_watcher_dirs_dump_filename);
    } else {
        return GetAgentRunDir() + "inotify_watcher_dirs";
    }
}

string GetAgentLoggersPrefix() {
    if (BOOL_FLAG(logtail_mode)) {
        return "/apsara/sls/ilogtail";
    } else {
        return "/apsara/loongcollector";
    }
}

string GetAgentLogName() {
    if (BOOL_FLAG(logtail_mode)) {
        return "ilogtail.LOG";
    } else {
        return "loongcollector.LOG";
    }
}

string GetAgentSnapshotDir() {
    if (BOOL_FLAG(logtail_mode)) {
        return GetProcessExecutionDir() + STRING_FLAG(logtail_snapshot_dir);
    } else {
        return GetAgentLogDir() + "snapshot";
    }
}

string GetAgentProfileLogName() {
    if (BOOL_FLAG(logtail_mode)) {
        return "ilogtail_profile.LOG";
    } else {
        return "loongcollector_profile.LOG";
    }
}

string GetAgentStatusLogName() {
    if (BOOL_FLAG(logtail_mode)) {
        return "ilogtail_status.LOG";
    } else {
        return "loongcollector_status.LOG";
    }
}

string GetProfileSnapshotDumpFileName() {
    if (BOOL_FLAG(logtail_mode)) {
        return GetProcessExecutionDir() + STRING_FLAG(logtail_profile_snapshot);
    } else {
        return GetAgentLogDir() + "loongcollector_profile_snapshot";
    }
}


string GetObserverEbpfHostPath() {
    if (BOOL_FLAG(logtail_mode)) {
        return STRING_FLAG(sls_observer_ebpf_host_path);
    } else {
        return GetAgentDataDir();
    }
}

string GetSendBufferFileNamePrefix() {
    if (BOOL_FLAG(logtail_mode)) {
        return "logtail_buffer_file_";
    } else {
        return "send_buffer_file_";
    }
}

string GetLegacyUserLocalConfigFilePath() {
    if (BOOL_FLAG(logtail_mode)) {
        return AppConfig::GetInstance()->GetProcessExecutionDir();
    } else {
        return AppConfig::GetInstance()->GetLoongcollectorConfDir();
    }
}

string GetExactlyOnceCheckpoint() {
    if (BOOL_FLAG(logtail_mode)) {
        auto fp = boost::filesystem::path(AppConfig::GetInstance()->GetLoongcollectorConfDir());
        return (fp / "checkpoint_v2").string();
    } else {
        auto fp = boost::filesystem::path(GetAgentDataDir());
        return (fp / "exactly_once_checkpoint").string();
    }
}

string GetFileTagsDir() {
    if (BOOL_FLAG(logtail_mode)) {
        return STRING_FLAG(ALIYUN_LOG_FILE_TAGS);
    } else {
        return AbsolutePath(STRING_FLAG(ALIYUN_LOG_FILE_TAGS), AppConfig::GetInstance()->GetLoongcollectorConfDir());
    }
}

AppConfig::AppConfig() {
    LOG_INFO(sLogger, ("AppConfig AppConfig", "success"));
    SetIlogtailConfigJson("");
    // mStreamLogAddress = "0.0.0.0";
    // mIsOldPubRegion = false;
    // mOpenStreamLog = false;
    mSendRequestConcurrency = INT32_FLAG(send_request_concurrency);
    mProcessThreadCount = INT32_FLAG(process_thread_count);
    // mMappingConfigPath = STRING_FLAG(default_mapping_config_path);
    mMachineCpuUsageThreshold = DOUBLE_FLAG(default_machine_cpu_usage_threshold);
    mCpuUsageUpLimit = DOUBLE_FLAG(cpu_usage_up_limit);
    mScaledCpuUsageUpLimit = DOUBLE_FLAG(cpu_usage_up_limit);
    mMemUsageUpLimit = INT64_FLAG(memory_usage_up_limit);
    mResourceAutoScale = BOOL_FLAG(default_resource_auto_scale);
    mInputFlowControl = BOOL_FLAG(default_input_flow_control);
    // mDefaultRegion = STRING_FLAG(default_region_name);
    mAcceptMultiConfigFlag = BOOL_FLAG(default_accept_multi_config);
    mMaxMultiConfigSize = INT32_FLAG(max_multi_config_size);
    // mUserConfigPath = STRING_FLAG(user_log_config);
    mIgnoreDirInodeChanged = false;
    mLogParseAlarmFlag = true;
    mNoInotify = false;
    mSendDataPort = 80;
    mShennongSocket = true;
    // mInotifyBlackList.insert("/tmp");

    mPurageContainerMode = false;
    mForceQuitReadTimeout = 7200;
    LoadEnvTags();
    CheckPurageContainerMode();
}

void AppConfig::MergeJson(Json::Value& mainConfJson, const Json::Value& subConfJson) {
    for (auto subkey : subConfJson.getMemberNames()) {
        mainConfJson[subkey] = subConfJson[subkey];
    }
}

// 只有 logtail 模式才使用
void AppConfig::LoadIncludeConfig(Json::Value& confJson) {
    // New default value of the flag is renamed from /etc/ilogtail/config.d/
    // to config.d, be compatible with old default value.
    string dirPath = STRING_FLAG(default_include_config_path);
    if (!dirPath.empty() && dirPath[0] != '/') {
        dirPath = mLoongcollectorConfDir + dirPath + PATH_SEPARATOR;
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
    while ((entry = dir.ReadNext(false))) {
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

void AppConfig::LoadLocalInstanceConfig() {
    filesystem::path localConfigPath
        = filesystem::path(AppConfig::GetInstance()->GetLoongcollectorConfDir()) / "instance_config" / "local";
    error_code ec;
    filesystem::create_directories(localConfigPath, ec);
    if (ec) {
        LOG_WARNING(sLogger,
                    ("failed to create dir for local instanceconfig",
                     "manual creation may be required")("error code", ec.value())("error msg", ec.message()));
    }
    InstanceConfigWatcher::GetInstance()->AddSource(localConfigPath.string());
    InstanceConfigDiff instanceConfigDiff = InstanceConfigWatcher::GetInstance()->CheckConfigDiff();
    if (!instanceConfigDiff.IsEmpty()) {
        InstanceConfigManager::GetInstance()->UpdateInstanceConfigs(instanceConfigDiff);
    }
}

void AppConfig::LoadAppConfig(const std::string& ilogtailConfigFile) {
    mDockerFilePathConfig = GetAgentDockerPathConfig();
    if (BOOL_FLAG(logtail_mode)) {
        loadAppConfigLogtailMode(ilogtailConfigFile);
    } else {
        std::string confDir  = GetAgentConfDir();
        SetLoongcollectorConfDir(AbsolutePath(confDir, mProcessExecutionDir));
    }
    // 加载本地instanceconfig
    LoadLocalInstanceConfig();

    ParseJsonToFlags(mLocalInstanceConfig);
    ParseEnvToFlags();

    LoadResourceConf(mLocalInstanceConfig);
    // load addr will init sender, sender param depend on LoadResourceConf
    // LoadAddrConfig(mLocalInstanceConfig);
    LoadOtherConf(mLocalInstanceConfig);

    CheckAndResetProxyEnv();
}

void AppConfig::loadAppConfigLogtailMode(const std::string& ilogtailConfigFile) {

    Json::Value confJson(Json::objectValue);
    std::string newConfDir;

    ParseConfResult res = CONFIG_NOT_EXIST;
    if (!ilogtailConfigFile.empty()) {
        res = ParseConfig(ilogtailConfigFile, confJson);

#ifdef __ENTERPRISE__
        if (res == CONFIG_NOT_EXIST) {
            LOG_INFO(sLogger, ("config file not exist, try generate config by path", ilogtailConfigFile));
            if (EnterpriseConfigProvider::GetInstance()->GenerateAPPConfigByConfigPath(ilogtailConfigFile, confJson)) {
                res = CONFIG_OK;
                LOG_INFO(sLogger, ("generate config success", ilogtailConfigFile));
            } else {
                LOG_WARNING(sLogger, ("generate config failed", ilogtailConfigFile));
            }
        }
#endif

        if (res == CONFIG_OK) {
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

    newConfDir = GetAgentConfDir(res, confJson);
    SetLoongcollectorConfDir(AbsolutePath(newConfDir, mProcessExecutionDir));

    LoadIncludeConfig(confJson);
    string configJsonString = confJson.toStyledString();
    SetIlogtailConfigJson(configJsonString);
    LOG_INFO(sLogger, ("load logtail config file, path", ilogtailConfigFile));
    LOG_INFO(sLogger, ("load logtail config file, detail", configJsonString));

    mLocalInstanceConfig = confJson;
}

/**
 * @brief 从环境变量加载Tag
 *
 * 该函数从环境变量中加载预定义的Tag。
 * Tag键从环境变量中获取，对应的值也从环境变量中读取。
 */
void AppConfig::LoadEnvTags() {
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

/**
 * @brief 从环境变量加载单个配置值
 *
 * @tparam T 配置值的类型
 * @param envKey 环境变量的键
 * @param configValue 配置值的引用，如果环境变量存在且有效，将被更新
 * @param minValue 配置值的最小允许值
 * @return 如果配置值被更新，返回true；否则返回false
 */
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

/**
 * @brief 从环境变量加载配置值（如果存在）
 *
 * @tparam T 配置值的类型
 * @param envKey 环境变量的键
 * @param cfgValue 配置值的引用，如果环境变量存在，将被更新
 */
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

void AppConfig::LoadEnvResourceLimit() {
    LoadSingleValueEnvConfig("cpu_usage_limit", mCpuUsageUpLimit, (float)0.4);
    LoadSingleValueEnvConfig("mem_usage_limit", mMemUsageUpLimit, (int64_t)384);
    LoadSingleValueEnvConfig("max_bytes_per_sec", mMaxBytePerSec, (int32_t)(1024 * 1024));
    LoadSingleValueEnvConfig("process_thread_count", mProcessThreadCount, (int32_t)1);
    LoadSingleValueEnvConfig("send_request_concurrency", mSendRequestConcurrency, (int32_t)2);
}

/**
 * @brief 检查是否处于纯容器模式
 *
 * 该函数检查系统是否运行在纯容器模式下。
 *
 * 主要步骤：
 * 1. 在企业版中，检查是否设置了用户定义的ID环境变量
 * 2. 检查默认容器主机路径是否存在
 * 3. 根据检查结果设置mPurageContainerMode标志
 *
 * @note 该函数会更新mPurageContainerMode成员变量
 */
void AppConfig::CheckPurageContainerMode() {
#ifdef __ENTERPRISE__
    if (getenv(STRING_FLAG(ilogtail_user_defined_id_env_name).c_str()) == NULL) {
        LOG_INFO(sLogger, ("purage container mode", false));
        return;
    }
#endif
    fsutil::PathStat buf;
    if (fsutil::PathStat::stat(STRING_FLAG(default_container_host_path).c_str(), buf)) {
        mPurageContainerMode = true;
    }
    LOG_INFO(sLogger, ("purage container mode", mPurageContainerMode));
}

void AppConfig::LoadResourceConf(const Json::Value& confJson) {
    LoadInt32Parameter(
        INT32_FLAG(batch_send_interval), confJson, "batch_send_interval", "ALIYUN_LOGTAIL_BATCH_SEND_INTERVAL");

    LoadInt32Parameter(INT32_FLAG(batch_send_metric_size),
                       confJson,
                       "batch_send_metric_size",
                       "ALIYUN_LOGTAIL_BATCH_SEND_METRIC_SIZE");

    if (confJson.isMember("max_bytes_per_sec") && confJson["max_bytes_per_sec"].isInt())
        mMaxBytePerSec = confJson["max_bytes_per_sec"].asInt();
#ifdef __ENTERPRISE__
    else if (EnterpriseConfigProvider::GetInstance()->IsOldPubRegion())
        mMaxBytePerSec = INT32_FLAG(pub_max_send_byte_per_sec);
#endif
    else
        mMaxBytePerSec = kDefaultMaxSendBytePerSec;

    if (confJson.isMember("bytes_per_sec") && confJson["bytes_per_sec"].isInt())
        mBytePerSec = confJson["bytes_per_sec"].asInt();
#ifdef __ENTERPRISE__
    else if (EnterpriseConfigProvider::GetInstance()->IsOldPubRegion())
        mBytePerSec = INT32_FLAG(pub_send_byte_per_sec);
#endif
    else
        mBytePerSec = INT32_FLAG(default_send_byte_per_sec);

    if (confJson.isMember("buffer_file_num") && confJson["buffer_file_num"].isInt())
        mNumOfBufferFile = confJson["buffer_file_num"].asInt();
#ifdef __ENTERPRISE__
    else if (EnterpriseConfigProvider::GetInstance()->IsOldPubRegion())
        mNumOfBufferFile = INT32_FLAG(pub_buffer_file_num);
#endif
    else
        mNumOfBufferFile = INT32_FLAG(default_buffer_file_num);

    if (confJson.isMember("buffer_file_size") && confJson["buffer_file_size"].isInt())
        mLocalFileSize = confJson["buffer_file_size"].asInt();
#ifdef __ENTERPRISE__
    else if (EnterpriseConfigProvider::GetInstance()->IsOldPubRegion())
        mLocalFileSize = INT32_FLAG(pub_local_file_size);
#endif
    else
        mLocalFileSize = INT32_FLAG(default_local_file_size);

    if (confJson.isMember("buffer_map_size") && confJson["buffer_map_size"].isInt())
        mMaxHoldedDataSize = confJson["buffer_map_size"].asInt();
#ifdef __ENTERPRISE__
    else if (EnterpriseConfigProvider::GetInstance()->IsOldPubRegion())
        mMaxHoldedDataSize = INT32_FLAG(pub_max_holded_data_size);
#endif
    else
        mMaxHoldedDataSize = INT32_FLAG(max_holded_data_size);

    if (confJson.isMember("buffer_map_num") && confJson["buffer_map_num"].isInt())
        mMaxBufferNum = confJson["buffer_map_num"].asInt();
#ifdef __ENTERPRISE__
    else if (EnterpriseConfigProvider::GetInstance()->IsOldPubRegion())
        mMaxBufferNum = INT32_FLAG(pub_max_buffer_num);
#endif
    else
        mMaxBufferNum = INT32_FLAG(max_buffer_num);

    if (confJson.isMember("send_request_concurrency") && confJson["send_request_concurrency"].isInt())
        mSendRequestConcurrency = confJson["send_request_concurrency"].asInt();
    else
        mSendRequestConcurrency = INT32_FLAG(send_request_concurrency);
    LogtailMonitor::GetInstance()->UpdateConstMetric("send_request_concurrency", mSendRequestConcurrency);

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

    if (confJson.isMember("log_profile_save_interval") && confJson["log_profile_save_interval"].isInt())
        LogFileProfiler::GetInstance()->SetProfileInterval(confJson["log_profile_save_interval"].asInt());

    LOG_DEBUG(sLogger,
              ("logreader delete interval", INT32_FLAG(logreader_filedeleted_remove_interval))(
                  "check handler interval", INT32_FLAG(check_handler_timeout_interval))(
                  "reader close interval", INT32_FLAG(reader_close_unused_file_time))(
                  "profile interval", LogFileProfiler::GetInstance()->GetProfileInterval()));


    if (confJson.isMember("cpu_usage_limit")) {
        if (confJson["cpu_usage_limit"].isDouble())
            mCpuUsageUpLimit = confJson["cpu_usage_limit"].asDouble();
        else if (confJson["cpu_usage_limit"].isIntegral())
            mCpuUsageUpLimit = double(confJson["cpu_usage_limit"].asInt64());
#ifdef __ENTERPRISE__
        else if (EnterpriseConfigProvider::GetInstance()->IsOldPubRegion())
            mCpuUsageUpLimit = DOUBLE_FLAG(pub_cpu_usage_up_limit);
#endif
        else
            mCpuUsageUpLimit = DOUBLE_FLAG(cpu_usage_up_limit);
#ifdef __ENTERPRISE__
    } else if (EnterpriseConfigProvider::GetInstance()->IsOldPubRegion()) {
        mCpuUsageUpLimit = DOUBLE_FLAG(pub_cpu_usage_up_limit);
#endif
    } else
        mCpuUsageUpLimit = DOUBLE_FLAG(cpu_usage_up_limit);

    if (confJson.isMember("mem_usage_limit") && confJson["mem_usage_limit"].isIntegral())
        mMemUsageUpLimit = confJson["mem_usage_limit"].asInt64();
#ifdef __ENTERPRISE__
    else if (EnterpriseConfigProvider::GetInstance()->IsOldPubRegion())
        mMemUsageUpLimit = INT64_FLAG(pub_memory_usage_up_limit);
#endif
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

    // first set buffer_file_path, if buffer_file_path is null then set default_buffer_file_path
    if (confJson.isMember("buffer_file_path") && confJson["buffer_file_path"].isString())
        mBufferFilePath = confJson["buffer_file_path"].asString();
    else if (STRING_FLAG(buffer_file_path) != "")
        mBufferFilePath = STRING_FLAG(buffer_file_path);
    else
        mBufferFilePath = STRING_FLAG(default_buffer_file_path);

    if (confJson.isMember("check_point_filename") && confJson["check_point_filename"].isString())
        mCheckPointFilePath = confJson["check_point_filename"].asString();
    else if (confJson.isMember("check_point_file_path") && confJson["check_point_file_path"].isString())
        mCheckPointFilePath = confJson["check_point_file_path"].asString();
    else
        mCheckPointFilePath = GetCheckPointFileName();
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

    // LoadStringParameter(mUserConfigPath, confJson, "user_config_file_path", "ALIYUN_LOGTAIL_USER_CONIFG_PATH");

    // LoadStringParameter(
    //     mUserLocalConfigPath, confJson, "user_local_config_filename", "ALIYUN_LOGTAIL_USER_LOCAL_CONFIG_FILENAME");

    LoadBooleanParameter(
        BOOL_FLAG(ilogtail_discard_old_data), confJson, "discard_old_data", "ALIYUN_LOGTAIL_DISCARD_OLD_DATA");

    // if (confJson.isMember("container_mount_path") && confJson["container_mount_path"].isString()) {
    //     mContainerMountConfigPath = confJson["container_mount_path"].asString();
    // } else {
    //     mContainerMountConfigPath = GetProcessExecutionDir() + STRING_FLAG(default_container_mount_path);
    // }

    LoadStringParameter(mConfigIP, confJson, "working_ip", "ALIYUN_LOGTAIL_WORKING_IP");

    // LoadStringParameter(mCustomizedConfigIP, confJson, "customized_config_ip",
    // "ALIYUN_LOGTAIL_CUSTOMIZED_CONFIG_IP");

    LoadStringParameter(mConfigHostName, confJson, "working_hostname", "ALIYUN_LOGTAIL_WORKING_HOSTNAME");

    // // try to get zone env name from conf json
    // if (confJson.isMember("alipay_zone_env_name") && confJson["alipay_zone_env_name"].isString()) {
    //     STRING_FLAG(alipay_zone_env_name) = confJson["alipay_zone_env_name"].asString();
    // }
    // // get zone info from env, for ant
    // do {
    //     if (!STRING_FLAG(alipay_zone_env_name).empty()) {
    //         const char* alipayZone = getenv(STRING_FLAG(alipay_zone_env_name).c_str());
    //         if (alipayZone != NULL) {
    //             mAlipayZone = alipayZone;
    //             break;
    //         }
    //     }
    //     const char* alipayZone = getenv(STRING_FLAG(alipay_app_zone).c_str());
    //     if (alipayZone != NULL) {
    //         mAlipayZone = alipayZone;
    //         break;
    //     }
    //     alipayZone = getenv(STRING_FLAG(alipay_zone).c_str());
    //     if (alipayZone != NULL) {
    //         mAlipayZone = alipayZone;
    //         break;
    //     }
    // } while (false);

    // if (confJson.isMember("alipay_zone") && confJson["alipay_zone"].isString()) {
    //     mAlipayZone = confJson["alipay_zone"].asString();
    // }

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

    if (!STRING_FLAG(host_path_blacklist).empty()) {
#ifdef _MSC_VER
        static const std::string delim = ";";
#else
        static const std::string delim = ":";
#endif
        auto blacklist = SplitString(TrimString(STRING_FLAG(host_path_blacklist)), delim);
        for (const auto& s : blacklist) {
            auto s1 = TrimString(s);
            if (!s1.empty()) {
                mHostPathBlacklist.emplace_back(std::move(s1));
            }
        }
    }

    if (!LoadInt32Parameter(mSendDataPort, confJson, "data_server_port", "ALIYUN_LOGTAIL_DATA_SERVER_PORT")) {
        mSendDataPort = INT32_FLAG(data_server_port);
    }

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

    // if (confJson.isMember("rapid_retry_update_config") && confJson["rapid_retry_update_config"].isBool()) {
    //     BOOL_FLAG(rapid_retry_update_config) = confJson["rapid_retry_update_config"].asBool();
    //     LOG_INFO(sLogger, ("set rapid_retry_update_config", BOOL_FLAG(rapid_retry_update_config)));
    // }

    if (confJson.isMember("check_profile_region") && confJson["check_profile_region"].isBool()) {
        BOOL_FLAG(check_profile_region) = confJson["check_profile_region"].asBool();
        LOG_INFO(sLogger, ("set check_profile_region", BOOL_FLAG(check_profile_region)));
    }

    // if (confJson.isMember("enable_collection_mark") && confJson["enable_collection_mark"].isBool()) {
    //     BOOL_FLAG(enable_collection_mark) = confJson["enable_collection_mark"].asBool();
    //     LOG_INFO(sLogger, ("set enable_collection_mark", BOOL_FLAG(enable_collection_mark)));
    // }

    // LoadBooleanParameter(BOOL_FLAG(enable_collection_mark),
    //                      confJson,
    //                      "enable_env_ref_in_config",
    //                      "ALIYUN_LOGTAIL_ENABLE_ENV_REF_IN_CONFIG");

    LoadBooleanParameter(
        mEnableHostIPReplace, confJson, "enable_host_ip_replace", "ALIYUN_LOGTAIL_ENABLE_HOST_IP_REPLACE");

    LoadBooleanParameter(mEnableResponseVerification,
                         confJson,
                         "enable_response_verification",
                         "ALIYUN_LOGTAIL_ENABLE_RESPONSE_VERIFICATION");

    LoadEnvResourceLimit();
    CheckAndAdjustParameters();

    if (confJson.isMember("bind_interface") && confJson["bind_interface"].isString()) {
        mBindInterface = TrimString(confJson["bind_interface"].asString());
        if (ToLowerCaseString(mBindInterface) == "default")
            mBindInterface.clear();
        LOG_INFO(sLogger, ("bind_interface", mBindInterface));
    }
}

bool AppConfig::CheckAndResetProxyEnv() {
    // envs capitalized prioritize those in lower case
    string httpProxy = ToString(getenv("HTTP_PROXY"));
    if (httpProxy.empty()) {
        httpProxy = ToString(getenv("http_proxy"));
        if (!CheckAndResetProxyAddress("http_proxy", httpProxy)) {
            UnsetEnv("http_proxy");
            LOG_WARNING(sLogger,
                        ("proxy mode", "off")("reason", "http proxy env value not valid")("http proxy", httpProxy));
            return false;
        }
    } else {
        if (!CheckAndResetProxyAddress("HTTP_PROXY", httpProxy)) {
            UnsetEnv("http_proxy");
            UnsetEnv("HTTP_PROXY");
            LOG_WARNING(sLogger,
                        ("proxy mode", "off")("reason", "http proxy env value not valid")("http proxy", httpProxy));
            return false;
        }
        // libcurl do not recognize env HTTP_PROXY, thus env http_proxy need to be copied to env HTTP_PROXY if present
        SetEnv("http_proxy", httpProxy.c_str());
    }

    string httpsProxyKey = "HTTPS_PROXY";
    string httpsProxyValue = ToString(getenv(httpsProxyKey.c_str()));
    if (httpsProxyValue.empty()) {
        httpsProxyKey = "https_proxy";
        httpsProxyValue = ToString(getenv(httpsProxyKey.c_str()));
    } else {
        UnsetEnv("https_proxy");
    }
    if (!CheckAndResetProxyAddress(httpsProxyKey.c_str(), httpsProxyValue)) {
        UnsetEnv("https_proxy");
        UnsetEnv("HTTPS_PROXY");
        LOG_WARNING(sLogger,
                    ("proxy mode", "off")("reason", "https proxy env value not valid")("https proxy", httpsProxyValue));
        return false;
    }

    string allProxyKey = "ALL_PROXY";
    string allProxy = ToString(getenv(allProxyKey.c_str()));
    if (allProxy.empty()) {
        allProxyKey = "all_proxy";
        allProxy = ToString(getenv(allProxyKey.c_str()));
    } else {
        UnsetEnv("all_proxy");
    }
    if (!CheckAndResetProxyAddress(allProxyKey.c_str(), allProxy)) {
        UnsetEnv("all_proxy");
        UnsetEnv("ALL_PROXY");
        LOG_WARNING(sLogger, ("proxy mode", "off")("reason", "all proxy env value not valid")("all proxy", allProxy));
        return false;
    }

    string noProxy = ToString(getenv("NO_PROXY"));
    if (noProxy.empty()) {
        noProxy = ToString(getenv("no_proxy"));
    } else {
        UnsetEnv("no_proxy");
    }

    if (!httpProxy.empty() || !httpsProxyValue.empty() || !allProxy.empty()) {
        mEnableHostIPReplace = false;
        LOG_INFO(sLogger,
                 ("proxy mode", "on")("http proxy", httpProxy)("https proxy", httpsProxyValue)("all proxy", allProxy)(
                     "no proxy", noProxy));
    }
    return true;
}

// valid proxy address format: [scheme://[user:pwd\@]]address[:port], 'http' and '80' assumed if no scheme or port
// provided
bool AppConfig::CheckAndResetProxyAddress(const char* envKey, string& address) {
    if (address.empty()) {
        return true;
    }

    size_t pos = 0, tmp = 0;
    if ((tmp = address.find("://")) != string::npos) {
        string scheme = address.substr(0, tmp);
        if (scheme != "http" && scheme != "https" && scheme != "socks5") {
            return false;
        }
        pos = tmp + 3;
    }
    if ((tmp = address.find("@", pos)) != string::npos) {
        pos = tmp + 1;
    }
    if (address.find(":", pos) == string::npos) {
        address += ":80";
        SetEnv(envKey, address.c_str());
    }
    return true;
}

void AppConfig::LoadOtherConf(const Json::Value& confJson) {
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

    // dynamic plugins
    if (confJson.isMember("dynamic_plugins")) {
        auto& dynamic_plugins = confJson["dynamic_plugins"];
        if (dynamic_plugins.isArray()) {
            for (Json::Value::ArrayIndex i = 0; i < dynamic_plugins.size(); ++i) {
                mDynamicPlugins.insert(TrimString(dynamic_plugins[i].asString()));
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

    LoadBooleanParameter(mAcceptMultiConfigFlag, confJson, "accept_multi_config", "ALIYUN_LOGTAIL_ACCEPT_MULTI_CONFIG");

    LoadInt32Parameter(mMaxMultiConfigSize, confJson, "max_multi_config", "ALIYUN_LOGTAIL_MAX_MULTI_CONFIG");

    if (confJson.isMember("log_parse_error") && confJson["log_parse_error"].isBool()) {
        mLogParseAlarmFlag = confJson["log_parse_error"].asBool();
    }
}

void AppConfig::InitEnvMapping(const std::string& envStr, std::map<std::string, std::string>& envMapping) {
    int pos = envStr.find('=');
    if (pos > 0 && size_t(pos) < envStr.size()) {
        const std::string& key = envStr.substr(0, pos);
        const std::string& value = envStr.substr(pos + 1);
        envMapping[key] = value;
    } else {
        APSARA_LOG_WARNING(sLogger, ("error to find ", "")("pos", pos)("env string", envStr));
    }
}
void AppConfig::SetConfigFlag(const std::string& flagName, const std::string& value) {
    static set<string> sIgnoreFlagSet
        = {"loongcollector_conf_dir", "loongcollector_log_dir", "loongcollector_data_dir", "loongcollector_run_dir", "logtail_mode"};
    if (sIgnoreFlagSet.find(flagName) != sIgnoreFlagSet.end()) {
        return;
    }
    GFLAGS_NAMESPACE::CommandLineFlagInfo info;
    bool rst = GetCommandLineFlagInfo(flagName.c_str(), &info);
    if (rst) {
        string beforeValue = info.current_value;
        string setrst = GFLAGS_NAMESPACE::SetCommandLineOption(flagName.c_str(), value.c_str());
        GetCommandLineFlagInfo(flagName.c_str(), &info);
        APSARA_LOG_INFO(sLogger,
                        ("Set config flag", flagName)("before value", beforeValue)("after value", info.current_value)(
                            "result", setrst.size() == 0 ? ("error with value " + value) : setrst));
    } else {
        APSARA_LOG_DEBUG(sLogger, ("Flag not define", flagName));
    }
}

#if defined(_MSC_VER)
#include "processenv.h"
#endif

void AppConfig::ParseEnvToFlags() {
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
    for (const auto & iter : envMapping) {
        const std::string& key = iter.first;
        const std::string& value = iter.second;
        SetConfigFlag(key, value);
        // 尝试解析为 double
        char* end;
        double doubleValue = strtod(value.c_str(), &end);
        mEnvConfigKeyToConfigName[key] = "env";
        if (*end == '\0') {
            mEnvConfig[key] = doubleValue;
            continue;
        }

        // 尝试解析为 int64_t
        int64_t int64Value = strtoll(value.c_str(), &end, 10);
        if (*end == '\0' && errno != ERANGE) {
            mEnvConfig[key] = Json::Int64(int64Value);
            continue;
        }

        // 尝试解析为 int32_t
        auto int32Value = static_cast<int32_t>(strtol(value.c_str(), &end, 10));
        if (*end == '\0' && errno != ERANGE && static_cast<int64_t>(int32Value) == int64Value) {
            mEnvConfig[key] = int32Value;
            continue;
        }

        // 检查是否为 bool
        if (value == "true") {
            mEnvConfig[key] = true;
            continue;
        }
        if (value == "false") {
            mEnvConfig[key] = false;
            continue;
        }
        mEnvConfig[key] = value;
    }
}

void AppConfig::DumpAllFlagsToMap(std::unordered_map<std::string, std::string>& flagMap) {
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

void AppConfig::ReadFlagsFromMap(const std::unordered_map<std::string, std::string>& flagMap) {
    for (auto iter : flagMap) {
        SetConfigFlag(iter.first, iter.second);
    }
    LOG_DEBUG(sLogger, ("ReadFlagsFromMap", flagMap.size()));
}

/**
 * @brief 递归解析JSON配置到gflag
 *
 * 该函数递归地遍历JSON对象，将其键值对转换为gflag
 *
 * @param confJson 要解析的JSON对象
 * @param prefix 当前键的前缀，用于构建完整的标志名
 *
 * 主要逻辑:
 * 1. 遍历JSON对象的所有成员
 * 2. 对于嵌套的对象，递归调用自身
 * 3. 对于非对象值:
 *    - 忽略特定的键（如data_server_list）
 *    - 将可转换为字符串的值设置为标志
 *    - 对特定键（如config_server_address_list）强制转换为字符串
 *    - 记录无法转换的值
 */
void AppConfig::RecurseParseJsonToFlags(const Json::Value& confJson, std::string prefix) {
    const static unordered_set<string> sIgnoreKeySet = {"data_server_list"};
    const static unordered_set<string> sForceKeySet = {"config_server_address_list"};
    for (auto name : confJson.getMemberNames()) {
        auto jsonvalue = confJson[name];
        string fullName;
        if (prefix.empty()) {
            fullName = name;
        } else {
            fullName = prefix + "_" + name;
        }
        if (jsonvalue.isObject()) {
            RecurseParseJsonToFlags(jsonvalue, fullName);
        } else {
            if (sIgnoreKeySet.find(fullName) != sIgnoreKeySet.end()) {
                continue;
            }
            if (jsonvalue.isConvertibleTo(Json::stringValue)) {
                SetConfigFlag(fullName, jsonvalue.asString());
            } else if (sForceKeySet.find(fullName) != sForceKeySet.end()) {
                SetConfigFlag(fullName, jsonvalue.toStyledString());
            } else {
                APSARA_LOG_INFO(sLogger,
                                ("Set config flag failed", "can not convert json value to flag")("flag name", fullName)(
                                    "jsonvalue", jsonvalue.toStyledString()));
            }
        }
    }
}

void AppConfig::ParseJsonToFlags(const Json::Value& confJson) {
    RecurseParseJsonToFlags(confJson, "");
}

void AppConfig::CheckAndAdjustParameters() {
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


    int64_t baseMemSize = 100; // MB
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
             ("batch send interval", INT32_FLAG(batch_send_interval))("batch send size",
                                                                      INT32_FLAG(batch_send_metric_size)));
}

bool AppConfig::IsInInotifyBlackList(const std::string& path) const {
    bool rst = mInotifyBlackList.find(path) != mInotifyBlackList.end();
    if (rst) {
        LOG_INFO(sLogger, ("this path is in inotify black list, skip inoitfy add watch", path));
    }
    return rst;
}

// TODO: Use Boost instead.
// boost::filesystem::directory_iterator end;
// try { boost::filesystem::directory_iterator(path); } catch (...) { // failed } // OK
void AppConfig::SetLoongcollectorConfDir(const std::string& dirPath) {
    mLoongcollectorConfDir = dirPath;
    if (dirPath.back() != '/' || dirPath.back() != '\\') {
        mLoongcollectorConfDir += PATH_SEPARATOR;
    }

    if (!CheckExistance(mLoongcollectorConfDir)) {
        if (Mkdir(mLoongcollectorConfDir)) {
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
            mLoongcollectorConfDir = GetAgentConfDir();
        }
    } else {
        closedir(dir);
    }
#elif defined(_MSC_VER)
    DWORD ret = GetFileAttributes(mLoongcollectorConfDir.c_str());
    if (INVALID_FILE_ATTRIBUTES == ret) {
        mLoongcollectorConfDir = GetAgentConfDir();
    }
#endif

    // Update related configurations (local user config).
    // if (STRING_FLAG(ilogtail_local_config).empty()) {
    //     LOG_WARNING(sLogger, ("flag error", "ilogtail_local_config must be non-empty"));
    //     STRING_FLAG(ilogtail_local_config) = DEFAULT_ILOGTAIL_LOCAL_CONFIG_FLAG_VALUE;
    // }
    // if (STRING_FLAG(ilogtail_local_config_dir).empty()) {
    //     LOG_WARNING(sLogger, ("flag error", "ilogtail_local_config_dir must be non-empty"));
    //     STRING_FLAG(ilogtail_local_config_dir) = DEFAULT_ILOGTAIL_LOCAL_CONFIG_DIR_FLAG_VALUE;
    // }
    // mUserLocalConfigPath = AbsolutePath(STRING_FLAG(ilogtail_local_config), mLogtailSysConfDir);
    // mUserLocalConfigDirPath = AbsolutePath(STRING_FLAG(ilogtail_local_config_dir), mLogtailSysConfDir) +
    // PATH_SEPARATOR; mUserLocalYamlConfigDirPath
    //     = AbsolutePath(STRING_FLAG(ilogtail_local_yaml_config_dir), mLogtailSysConfDir) + PATH_SEPARATOR;
    // mUserRemoteYamlConfigDirPath
    //     = AbsolutePath(STRING_FLAG(ilogtail_remote_yaml_config_dir), mLogtailSysConfDir) + PATH_SEPARATOR;
    LOG_INFO(sLogger, ("set loongcollector conf dir", mLoongcollectorConfDir));
}

bool AppConfig::IsHostPathMatchBlacklist(const string& dirPath) const {
    for (auto& dp : mHostPathBlacklist) {
        if (dirPath.find(dp) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void AppConfig::UpdateFileTags() {
    if (STRING_FLAG(ALIYUN_LOG_FILE_TAGS).empty()) {
        return;
    }
    // read local config
    Json::Value localFileTagsJson;
    string file_tags_dir = GetFileTagsDir();
    ParseConfResult userLogRes = ParseConfig(file_tags_dir, localFileTagsJson);
    if (userLogRes != CONFIG_OK) {
        if (userLogRes == CONFIG_NOT_EXIST)
            LOG_ERROR(sLogger, ("load file tags fail, file not exist", file_tags_dir));
        else if (userLogRes == CONFIG_INVALID_FORMAT) {
            LOG_ERROR(sLogger, ("load file tags fail, file content is not valid json", file_tags_dir));
        }
    } else {
        if (localFileTagsJson != mFileTagsJson) {
            int32_t i = 0;
            vector<sls_logs::LogTag>& sFileTags = mFileTags.getWriteBuffer();
            sFileTags.clear();
            sFileTags.resize(localFileTagsJson.size());
            for (auto it = localFileTagsJson.begin(); it != localFileTagsJson.end(); ++it) {
                if (it->isString()) {
                    sFileTags[i].set_key(it.key().asString());
                    sFileTags[i].set_value(it->asString());
                    ++i;
                }
            }
            mFileTags.swap();
            LOG_INFO(sLogger, ("local file tags update, old config", mFileTagsJson.toStyledString()));
            mFileTagsJson = localFileTagsJson;
            LOG_INFO(sLogger, ("local file tags update, new config", mFileTagsJson.toStyledString()));
        }
    }
    return;
}

void AppConfig::MergeJson(Json::Value& mainConfJson, const Json::Value& subConfJson, std::unordered_map<std::string, std::string>& keyToConfigName, const std::string& configName) {
    for (const auto& subkey : subConfJson.getMemberNames()) {
        mainConfJson[subkey] = subConfJson[subkey];
        keyToConfigName[subkey] = configName;
    }
}

void AppConfig::LoadInstanceConfig(const std::map<std::string, std::shared_ptr<InstanceConfig>>& instanceConfig) {
    Json::Value remoteInstanceConfig;
    Json::Value localInstanceConfig;
    mLocalInstanceConfigKeyToConfigName.clear();
    mRemoteInstanceConfigKeyToConfigName.clear();
    for (const auto& config : instanceConfig) {
        if (EndWith(config.second->mDirName, AppConfig::sLocalConfigDir)) {
            MergeJson(localInstanceConfig, config.second->GetConfig(), mLocalInstanceConfigKeyToConfigName, config.second->mDirName+"/"+config.second->mConfigName);
        } else {
            MergeJson(remoteInstanceConfig, config.second->GetConfig(), mRemoteInstanceConfigKeyToConfigName, config.second->mDirName+"/"+config.second->mConfigName);
        }
    }
    if (localInstanceConfig != mLocalInstanceConfig || mRemoteInstanceConfig != remoteInstanceConfig) {
        LOG_INFO(sLogger,
                 ("load all local instanceConfig", localInstanceConfig.toStyledString())(
                     "load all remote instanceConfig", remoteInstanceConfig.toStyledString()));
        std::set<std::function<bool()>*> callbackCall;
        for (const auto& callback : mCallbacks) {
            const std::string& key = callback.first;
            bool configChanged = false;
            // 检查本地配置是否发生变化
            if (localInstanceConfig.isMember(key) != mLocalInstanceConfig.isMember(key)
                || (localInstanceConfig.isMember(key) && localInstanceConfig[key] != mLocalInstanceConfig[key])) {
                configChanged = true;
            }
            // 检查远程配置是否发生变化
            if (!configChanged
                && (remoteInstanceConfig.isMember(key) != mRemoteInstanceConfig.isMember(key)
                    || (remoteInstanceConfig.isMember(key)
                        && remoteInstanceConfig[key] != mRemoteInstanceConfig[key]))) {
                configChanged = true;
            }
            if (configChanged) {
                callbackCall.insert(callback.second);
            }
        }
        mLocalInstanceConfig = std::move(localInstanceConfig);
        mRemoteInstanceConfig = std::move(remoteInstanceConfig);
        for (const auto& callback : callbackCall) {
            (*callback)();
        }
    }
}

void AppConfig::RegisterCallback(const std::string& key, std::function<bool()>* callback) {
    mCallbacks[key] = callback;
}

template<typename T>
T AppConfig::MergeConfig(const T& defaultValue,
                         const T& currentValue,
                         const std::string& name,
                         const std::function<bool(const std::string&, const T&)>& validateFn) {
    const auto& localInstanceConfig = AppConfig::GetInstance()->GetLocalInstanceConfig();
    const auto& envConfig = AppConfig::GetInstance()->GetEnvConfig();
    const auto& remoteInstanceConfig = AppConfig::GetInstance()->GetRemoteInstanceConfig();

    T res = defaultValue;
    std::string configName = "default";

    auto tryMerge = [&](const Json::Value& config, std::unordered_map<std::string, std::string>& keyToConfigName) {
        if (config.isMember(name)) {
            if constexpr (std::is_same_v<T, int32_t>) {
                if (config[name].isInt() && validateFn(name, config[name].asInt())) {
                    res = config[name].asInt();
                    configName = keyToConfigName[name];
                }
            } else if constexpr (std::is_same_v<T, int64_t>) {
                if (config[name].isInt64() && validateFn(name, config[name].asInt64())) {
                    res = config[name].asInt64();
                    configName = keyToConfigName[name];
                }
            } else if constexpr (std::is_same_v<T, bool>) {
                if (config[name].isBool() && validateFn(name, config[name].asBool())) {
                    res = config[name].asBool();
                    configName = keyToConfigName[name];
                }
            } else if constexpr (std::is_same_v<T, std::string>) {
                if (config[name].isString() && validateFn(name, config[name].asString())) {
                    res = config[name].asString();
                    configName = keyToConfigName[name];
                }
            } else if constexpr (std::is_same_v<T, double>) {
                if (config[name].isDouble() && validateFn(name, config[name].asDouble())) {
                    res = config[name].asDouble();
                    configName = keyToConfigName[name];
                }
            }
        }
    };

    tryMerge(localInstanceConfig, mLocalInstanceConfigKeyToConfigName);
    tryMerge(envConfig, mEnvConfigKeyToConfigName);
    tryMerge(remoteInstanceConfig, mRemoteInstanceConfigKeyToConfigName);
    LOG_INFO(
        sLogger,
        ("merge instance config", name)("key", name)("newValue", res)("lastValue", currentValue)("from", configName));
    return res;
}

int32_t AppConfig::MergeInt32(int32_t defaultValue,
                              int32_t currentValue,
                              const std::string& name,
                              const std::function<bool(const std::string&, const int32_t)>& validateFn) {
    return MergeConfig<int32_t>(defaultValue, currentValue, name, validateFn);
}

int64_t AppConfig::MergeInt64(int64_t defaultValue,
                              int64_t currentValue,
                              const std::string& name,
                              const std::function<bool(const std::string&, const int64_t)>& validateFn) {
    return MergeConfig<int64_t>(defaultValue, currentValue, name, validateFn);
}

bool AppConfig::MergeBool(bool defaultValue,
                          bool currentValue,
                          const std::string& name,
                          const std::function<bool(const std::string&, const bool)>& validateFn) {
    return MergeConfig<bool>(defaultValue, currentValue, name, validateFn);
}

std::string AppConfig::MergeString(const std::string& defaultValue,
                                   const std::string& currentValue,
                                   const std::string& name,
                                   const std::function<bool(const std::string&, const std::string&)>& validateFn) {
    return MergeConfig<std::string>(defaultValue, currentValue, name, validateFn);
}

double AppConfig::MergeDouble(double defaultValue,
                              double currentValue,
                              const std::string& name,
                              const std::function<bool(const std::string&, const double)>& validateFn) {
    return MergeConfig<double>(defaultValue, currentValue, name, validateFn);
}

} // namespace logtail
