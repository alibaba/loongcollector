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

#include <cstdlib>
#include "unittest/Unittest.h"
#include "common/StringTools.h"
#include "common/FileSystemUtil.h"
#include "common/RuntimeUtil.h"
#include "app_config/AppConfig.h"
#include "reader/LogFileReader.h"

DECLARE_FLAG_STRING(ilogtail_config);
DECLARE_FLAG_INT32(logreader_filedeleted_remove_interval);
DECLARE_FLAG_INT32(logreader_max_rotate_queue_size);
DECLARE_FLAG_INT32(check_handler_timeout_interval);
DECLARE_FLAG_INT32(reader_close_unused_file_time);
DECLARE_FLAG_INT32(max_docker_config_update_times);
DECLARE_FLAG_INT32(docker_config_update_interval);
DECLARE_FLAG_INT32(truncate_pos_skip_bytes);
DECLARE_FLAG_INT32(default_tail_limit_kb);
DECLARE_FLAG_INT32(check_point_dump_interval);
DECLARE_FLAG_INT32(default_oas_connect_timeout);
DECLARE_FLAG_INT32(default_oas_request_timeout);
DECLARE_FLAG_INT32(search_checkpoint_default_dir_depth);
DECLARE_FLAG_INT32(checkpoint_find_max_file_count);
DECLARE_FLAG_BOOL(enable_polling_discovery);
DECLARE_FLAG_INT32(dirfile_check_interval_ms);
DECLARE_FLAG_INT32(polling_dir_first_watch_timeout);
DECLARE_FLAG_INT32(polling_file_first_watch_timeout);
DECLARE_FLAG_INT32(modify_check_interval);
DECLARE_FLAG_INT32(ignore_file_modify_timeout);

namespace logtail {

class AppConfigUnittest : public ::testing::Test {
public:
    void TestLoadEnvParameters();

    void TestLoadFileParameters();

    void TestLoadJsonAndEnvParameters();

private:
    void writeLogtailConfigJSON(const Json::Value& v) {
        LOG_INFO(sLogger, ("writeLogtailConfigJSON", v.toStyledString()));
        OverwriteFile(STRING_FLAG(ilogtail_config), v.toStyledString());
    }

    template <typename T>
    void setEnv(const std::string& key, const T& value) {
        SetEnv(key.c_str(), ToString(value).c_str());
        mEnvKeys.push_back(key);
    }

    void unsetEnvKeys() {
        for (size_t idx = 0; idx < mEnvKeys.size(); ++idx) {
            UnsetEnv(mEnvKeys[idx].c_str());
        }
        mEnvKeys.clear();
    }

    std::vector<std::string> mEnvKeys;

    template <typename T>
    void setJSON(Json::Value& v, const std::string& key, const T& value) {
        v[key] = value;
    }

    void testParameters(const std::string& sysConfDir);
};

APSARA_UNIT_TEST_CASE(AppConfigUnittest, TestLoadEnvParameters, 0);
APSARA_UNIT_TEST_CASE(AppConfigUnittest, TestLoadFileParameters, 0);
APSARA_UNIT_TEST_CASE(AppConfigUnittest, TestLoadJsonAndEnvParameters, 0);

const bool kAccessMultiConfig = true;
const int32_t kMaxMultiConfig = 11;
const int32_t kBatchSendInterval = 5;
const int32_t kBatchSendMetricSize = 1000032;
const int32_t kRotateQueueSize = 11;
const int32_t kRemoveDeletedFileInterval = 35;
const int32_t kCheckTimeoutInterval = 49;
const int32_t kCloseUnusedFileTime = 12;
const std::string kCptPath = "check_point_path";
const int32_t kTruncatePosSkipBytes = 1023;
const std::string kUserConfigPath = "user_cfg_path";
const std::string kUserLocalPath = "user_local_path";
const bool kDiscardOldData = false;
const std::string kWorkingIP = "127.0.0.3";
const std::string kWorkingHostname = "workingHost";
const std::string kDockerFileCachePath = "docker_file_cache";
const int32_t kMaxDockerCfgUpdateTimes = 9;
const int32_t kDockerCfgUpdateInterval = 15;
const int32_t kDataServerPort = 400;
const int32_t kMaxReadBufferSize = 555 * 1024;
const int32_t kDefaultTailLimit = 2048;
const int32_t kCptDumpInterval = 19;
const int32_t kOasConnectTimeout = 19;
const int32_t kOasRequestTimeout = 100;
const int32_t kForceQuitTimeout = 3600 * 96;
const bool kEnableCptSyncWrite = true;
const bool kEnableHostIPReplace = false;
const bool kEnableResponseVerification = false;
const int32_t kSearchCheckpointDefaultDirDepth = 1;
const int32_t kCheckpointFindMaxFileCount = 2000;
const bool kEnablePollingDiscovery = false;
const int32_t kPollingDirfileCheckIntervalMs = 10000;
const int32_t kPollingDirFirstWatchTimeout = 100;
const int32_t kPollingFileFirstWatchTimeout = 100;
const int32_t kPollingModifyCheckInterval = 10000;
const int32_t kPollingIgnoreFileModifyTimeout = 100;

void AppConfigUnittest::testParameters(const std::string& sysConfDir) {
    AppConfig* appConfig = AppConfig::GetInstance();
    appConfig->LoadAppConfig(STRING_FLAG(ilogtail_config));

    APSARA_TEST_EQUAL(appConfig->GetLogtailSysConfDir(), sysConfDir);
    APSARA_TEST_EQUAL(appConfig->IsAcceptMultiConfig(), kAccessMultiConfig);
    APSARA_TEST_EQUAL(appConfig->GetMaxMultiConfigSize(), kMaxMultiConfig);
    APSARA_TEST_EQUAL(INT32_FLAG(batch_send_interval), kBatchSendInterval);
    APSARA_TEST_EQUAL(INT32_FLAG(batch_send_metric_size), kBatchSendMetricSize);
    APSARA_TEST_EQUAL(INT32_FLAG(logreader_max_rotate_queue_size), kRotateQueueSize);
    APSARA_TEST_EQUAL(INT32_FLAG(logreader_filedeleted_remove_interval), kRemoveDeletedFileInterval);
    APSARA_TEST_EQUAL(INT32_FLAG(check_handler_timeout_interval), kCheckTimeoutInterval);
    APSARA_TEST_EQUAL(INT32_FLAG(reader_close_unused_file_time), kCloseUnusedFileTime);
    APSARA_TEST_EQUAL(appConfig->GetCheckPointFilePath(), kCptPath);
    APSARA_TEST_EQUAL(INT32_FLAG(truncate_pos_skip_bytes), kTruncatePosSkipBytes);
    APSARA_TEST_EQUAL(appConfig->GetUserConfigPath(), kUserConfigPath);
    APSARA_TEST_EQUAL(appConfig->GetLocalUserConfigPath(), kUserLocalPath);
    APSARA_TEST_EQUAL(BOOL_FLAG(ilogtail_discard_old_data), kDiscardOldData);
    APSARA_TEST_EQUAL(appConfig->GetConfigIP(), kWorkingIP);
    APSARA_TEST_EQUAL(appConfig->GetConfigHostName(), kWorkingHostname);
    APSARA_TEST_EQUAL(appConfig->GetDockerFilePathConfig(), kDockerFileCachePath);
    APSARA_TEST_EQUAL(INT32_FLAG(max_docker_config_update_times), kMaxDockerCfgUpdateTimes);
    APSARA_TEST_EQUAL(INT32_FLAG(docker_config_update_interval), kDockerCfgUpdateInterval);
    APSARA_TEST_EQUAL(appConfig->GetDataServerPort(), kDataServerPort);
    APSARA_TEST_EQUAL(LogFileReader::BUFFER_SIZE, kMaxReadBufferSize);
    APSARA_TEST_EQUAL(INT32_FLAG(default_tail_limit_kb), kDefaultTailLimit);
    APSARA_TEST_EQUAL(INT32_FLAG(default_oas_connect_timeout), kOasConnectTimeout);
    APSARA_TEST_EQUAL(INT32_FLAG(default_oas_request_timeout), kOasRequestTimeout);
    APSARA_TEST_EQUAL(appConfig->GetForceQuitReadTimeout(), kForceQuitTimeout);
    APSARA_TEST_EQUAL(appConfig->EnableCheckpointSyncWrite(), kEnableCptSyncWrite);
    APSARA_TEST_EQUAL(appConfig->IsHostIPReplacePolicyEnabled(), kEnableHostIPReplace);
    APSARA_TEST_EQUAL(appConfig->IsResponseVerificationEnabled(), kEnableResponseVerification);
    APSARA_TEST_EQUAL(INT32_FLAG(search_checkpoint_default_dir_depth), kSearchCheckpointDefaultDirDepth);
    APSARA_TEST_EQUAL(INT32_FLAG(checkpoint_find_max_file_count), kCheckpointFindMaxFileCount);
    APSARA_TEST_EQUAL(BOOL_FLAG(enable_polling_discovery), kEnablePollingDiscovery);
    APSARA_TEST_EQUAL(INT32_FLAG(dirfile_check_interval_ms), kPollingDirfileCheckIntervalMs);
    APSARA_TEST_EQUAL(INT32_FLAG(polling_dir_first_watch_timeout), kPollingDirFirstWatchTimeout);
    APSARA_TEST_EQUAL(INT32_FLAG(polling_file_first_watch_timeout), kPollingFileFirstWatchTimeout);
    APSARA_TEST_EQUAL(INT32_FLAG(modify_check_interval), kPollingModifyCheckInterval);
    APSARA_TEST_EQUAL(INT32_FLAG(ignore_file_modify_timeout), kPollingIgnoreFileModifyTimeout);
}

// env > gflag
void AppConfigUnittest::TestLoadEnvParameters() {
    const std::string kLogtailSysConfDir = GetProcessExecutionDir();

    setEnv("logtail_sys_conf_dir", kLogtailSysConfDir);
    setEnv("accept_multi_config", kAccessMultiConfig);
    setEnv("max_multi_config", kMaxMultiConfig);
    setEnv("batch_send_interval", kBatchSendInterval);
    setEnv("batch_send_metric_size", kBatchSendMetricSize);
    setEnv("logreader_max_rotate_queue_size", kRotateQueueSize);
    setEnv("logreader_filedeleted_remove_interval", kRemoveDeletedFileInterval);
    setEnv("check_handler_timeout_interval", kCheckTimeoutInterval);
    setEnv("reader_close_unused_file_time", kCloseUnusedFileTime);
    setEnv("ALIYUN_LOGTAIL_CHECK_POINT_PATH", kCptPath);
    setEnv("truncate_pos_skip_bytes", kTruncatePosSkipBytes);
    setEnv("ALIYUN_LOGTAIL_USER_CONIFG_PATH", kUserConfigPath);
    setEnv("user_local_config_filename", kUserLocalPath);
    setEnv("discard_old_data", kDiscardOldData);
    setEnv("ALIYUN_LOGTAIL_WORKING_IP", kWorkingIP);
    setEnv("ALIYUN_LOGTAIL_WORKING_HOSTNAME", kWorkingHostname);
    setEnv("docker_file_cache_path", kDockerFileCachePath);
    setEnv("ALIYUN_LOGTAIL_MAX_DOCKER_CONFIG_UPDATE_TIMES", kMaxDockerCfgUpdateTimes);
    setEnv("ALIYUN_LOGTAIL_DOCKER_CONFIG_UPDATE_INTERVAL", kDockerCfgUpdateInterval);
    setEnv("data_server_port", kDataServerPort);
    setEnv("max_read_buffer_size", kMaxReadBufferSize);
    setEnv("default_tail_limit_kb", kDefaultTailLimit);
    setEnv("check_point_dump_interval", kCptDumpInterval);
    setEnv("oas_connect_timeout", kOasConnectTimeout);
    setEnv("oas_request_timeout", kOasRequestTimeout);
    setEnv("force_quit_read_timeout", kForceQuitTimeout);
    setEnv("enable_checkpoint_sync_write", kEnableCptSyncWrite);
    setEnv("ALIYUN_LOGTAIL_ENABLE_HOST_IP_REPLACE", kEnableHostIPReplace);
    setEnv("ALIYUN_LOGTAIL_ENABLE_RESPONSE_VERIFICATION", kEnableResponseVerification);
    setEnv("search_checkpoint_default_dir_depth", kSearchCheckpointDefaultDirDepth);
    setEnv("checkpoint_find_max_file_count", kCheckpointFindMaxFileCount);
    setEnv("enable_polling_discovery", kEnablePollingDiscovery);
    setEnv("polling_dirfile_check_interval_ms", kPollingDirfileCheckIntervalMs);
    setEnv("polling_dir_first_watch_timeout", kPollingDirFirstWatchTimeout);
    setEnv("polling_file_first_watch_timeout", kPollingFileFirstWatchTimeout);
    setEnv("polling_modify_check_interval", kPollingModifyCheckInterval);
    setEnv("polling_ignore_file_modify_timeout", kPollingIgnoreFileModifyTimeout);

    writeLogtailConfigJSON(Json::Value(Json::objectValue));
    testParameters(kLogtailSysConfDir);

    unsetEnvKeys();
}

// ilogtail_config.json > gflag
void AppConfigUnittest::TestLoadFileParameters() {
    const std::string kLogtailSysConfDir = GetProcessExecutionDir();

    Json::Value value;
    setJSON(value, "logtail_sys_conf_dir", kLogtailSysConfDir);
    setJSON(value, "accept_multi_config", kAccessMultiConfig);
    setJSON(value, "max_multi_config", kMaxMultiConfig);
    setJSON(value, "batch_send_interval", kBatchSendInterval);
    setJSON(value, "batch_send_metric_size", kBatchSendMetricSize);
    setJSON(value, "logreader_max_rotate_queue_size", kRotateQueueSize);
    setJSON(value, "logreader_filedeleted_remove_interval", kRemoveDeletedFileInterval);
    setJSON(value, "check_handler_timeout_interval", kCheckTimeoutInterval);
    setJSON(value, "reader_close_unused_file_time", kCloseUnusedFileTime);
    setJSON(value, "check_point_file_path", kCptPath);
    setJSON(value, "truncate_pos_skip_bytes", kTruncatePosSkipBytes);
    setJSON(value, "user_config_file_path", kUserConfigPath);
    setJSON(value, "user_local_config_filename", kUserLocalPath);
    setJSON(value, "discard_old_data", kDiscardOldData);
    setJSON(value, "working_ip", kWorkingIP);
    setJSON(value, "working_hostname", kWorkingHostname);
    setJSON(value, "docker_file_cache_path", kDockerFileCachePath);
    setJSON(value, "max_docker_config_update_times", kMaxDockerCfgUpdateTimes);
    setJSON(value, "docker_config_update_interval", kDockerCfgUpdateInterval);
    setJSON(value, "data_server_port", kDataServerPort);
    setJSON(value, "max_read_buffer_size", kMaxReadBufferSize);
    setJSON(value, "default_tail_limit_kb", kDefaultTailLimit);
    setJSON(value, "check_point_dump_interval", kCptDumpInterval);
    setJSON(value, "oas_connect_timeout", kOasConnectTimeout);
    setJSON(value, "oas_request_timeout", kOasRequestTimeout);
    setJSON(value, "force_quit_read_timeout", kForceQuitTimeout);
    setJSON(value, "enable_checkpoint_sync_write", kEnableCptSyncWrite);
    setJSON(value, "enable_host_ip_replace", kEnableHostIPReplace);
    setJSON(value, "enable_response_verification", kEnableResponseVerification);
    setJSON(value, "search_checkpoint_default_dir_depth", kSearchCheckpointDefaultDirDepth);
    setJSON(value, "checkpoint_find_max_file_count", kCheckpointFindMaxFileCount);
    setJSON(value, "enable_polling_discovery", kEnablePollingDiscovery);
    setJSON(value, "polling_dirfile_check_interval_ms", kPollingDirfileCheckIntervalMs);
    setJSON(value, "polling_dir_first_watch_timeout", kPollingDirFirstWatchTimeout);
    setJSON(value, "polling_file_first_watch_timeout", kPollingFileFirstWatchTimeout);
    setJSON(value, "polling_modify_check_interval", kPollingModifyCheckInterval);
    setJSON(value, "polling_ignore_file_modify_timeout", kPollingIgnoreFileModifyTimeout);

    writeLogtailConfigJSON(value);
    testParameters(kLogtailSysConfDir);
}

DEFINE_FLAG_BOOL(test_param_bool, "test_param_bool", true);
DEFINE_FLAG_INT32(test_param_int32, "test_param_int32", 32);
DEFINE_FLAG_INT64(test_param_int64, "test_param_int64", 64);
DEFINE_FLAG_DOUBLE(test_param_double, "test_param_double", 1.1);
DEFINE_FLAG_STRING(test_param_string, "test_param_string", "str");
DEFINE_FLAG_STRING(test_param_string_env_empty, "test_param_string_env_empty", "empty");

DEFINE_FLAG_BOOL(test_param_bool_error, "test_param_bool", true);
DEFINE_FLAG_INT32(test_param_int32_error, "test_param_int32", 32);
DEFINE_FLAG_INT64(test_param_int64_error, "test_param_int64", 64);
DEFINE_FLAG_DOUBLE(test_param_double_error, "test_param_double", 1.1);
DEFINE_FLAG_STRING(test_param_string_error, "test_param_string", "str");

// ilogtail_config.json > gflag
void AppConfigUnittest::TestLoadJsonAndEnvParameters() {
    Json::Value jsonconfig;
    setJSON(jsonconfig, "test_param_bool", false);
    setJSON(jsonconfig, "test_param_int32", 320);
    setJSON(jsonconfig, "test_param_int64", 640);
    setJSON(jsonconfig, "test_param_double", 2.2);
    setJSON(jsonconfig, "test_param_string", "jsonstr");

    setJSON(jsonconfig, "test_param_bool_error", "bol");
    setJSON(jsonconfig, "test_param_int32_error", "error_int32");
    setJSON(jsonconfig, "test_param_int64_error", "error_int64");
    setJSON(jsonconfig, "test_param_double_error", "error_double");
    AppConfig::GetInstance()->ParseJsonToFlags(jsonconfig);

    APSARA_TEST_EQUAL(BOOL_FLAG(test_param_bool), false);
    APSARA_TEST_EQUAL(INT32_FLAG(test_param_int32), 320);
    APSARA_TEST_EQUAL(INT64_FLAG(test_param_int64), 640);
    APSARA_TEST_EQUAL(DOUBLE_FLAG(test_param_double), 2.2);
    APSARA_TEST_EQUAL(STRING_FLAG(test_param_string), "jsonstr");

    APSARA_TEST_EQUAL(BOOL_FLAG(test_param_bool_error), true);
    APSARA_TEST_EQUAL(INT32_FLAG(test_param_int32_error), 32);
    APSARA_TEST_EQUAL(INT64_FLAG(test_param_int64_error), 64);
    APSARA_TEST_EQUAL(DOUBLE_FLAG(test_param_double_error), 1.1);
    APSARA_TEST_EQUAL(STRING_FLAG(test_param_string_error), "str");


    setEnv("test_param_bool", true);
    setEnv("test_param_int32", 3200);
    setEnv("test_param_int64", 6400);
    setEnv("test_param_double", 3.3);
    setEnv("test_param_string", "envstr");
    setEnv("test_param_string_env_empty", "");

    setEnv("test_param_bool_error", "error_bool_env");
    setEnv("test_param_int32_error", "error_int32_env");
    setEnv("test_param_int64_error", "error_int64_env");
    setEnv("test_param_double_error", "error_double_env");

    AppConfig::GetInstance()->ParseEnvToFlags();

    APSARA_TEST_EQUAL(BOOL_FLAG(test_param_bool), true);
    APSARA_TEST_EQUAL(INT32_FLAG(test_param_int32), 3200);
    APSARA_TEST_EQUAL(INT64_FLAG(test_param_int64), 6400);
    APSARA_TEST_EQUAL(DOUBLE_FLAG(test_param_double), 3.3);
    APSARA_TEST_EQUAL(STRING_FLAG(test_param_string), "envstr");
    APSARA_TEST_EQUAL(STRING_FLAG(test_param_string_env_empty), "");

    APSARA_TEST_EQUAL(BOOL_FLAG(test_param_bool_error), true);
    APSARA_TEST_EQUAL(INT32_FLAG(test_param_int32_error), 32);
    APSARA_TEST_EQUAL(INT64_FLAG(test_param_int64_error), 64);
    APSARA_TEST_EQUAL(DOUBLE_FLAG(test_param_double_error), 1.1);
    APSARA_TEST_EQUAL(STRING_FLAG(test_param_string_error), "str");
}
} // namespace logtail

UNIT_TEST_MAIN
