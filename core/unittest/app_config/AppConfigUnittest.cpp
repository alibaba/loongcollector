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

#include "app_config/AppConfig.h"
#include "common/FileSystemUtil.h"
#include "common/Flags.h"
#include "common/JsonUtil.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_INT32(checkpoint_find_max_file_count);
DECLARE_FLAG_BOOL(logtail_mode);
DECLARE_FLAG_STRING(host_path_blacklist);
DECLARE_FLAG_DOUBLE(default_machine_cpu_usage_threshold);
DEFINE_FLAG_INT32(test_receive_event_chan_cap, "test receive kernel event queue size", 4096);
DEFINE_FLAG_BOOL(test_admin_config_debug_mode, "test admin config debug mode", false);
DEFINE_FLAG_STRING(test_admin_config_log_level, "test admin config log level", "warn");
DEFINE_FLAG_BOOL(test_admin_config_push_all_span, "if admin config push all span", false);
DEFINE_FLAG_INT32(test_aggregation_config_agg_window_second, "test data aggregation window time", 15);
DEFINE_FLAG_STRING(test_converage_config_strategy, "test converage strategy", "combine");
DEFINE_FLAG_STRING(test_sample_config_strategy, "test sample strategy", "fixedRate");
DEFINE_FLAG_DOUBLE(test_sample_config_config_rate, "test sample rate", 0.01);

namespace logtail {

class AppConfigUnittest : public ::testing::Test {
public:
    void TestRecurseParseJsonToFlags();
    void TestParseEnvToFlags();
    void TestLoadSingleValueEnvConfig();
    void TestLoadStringParameter();

private:
    void writeLogtailConfigJSON(const Json::Value& v) {
        LOG_INFO(sLogger, ("writeLogtailConfigJSON", v.toStyledString()));
        if (BOOL_FLAG(logtail_mode)) {
            OverwriteFile(STRING_FLAG(ilogtail_config), v.toStyledString());
        } else {
            CreateAgentDir();
            std::string conf = GetAgentConfDir() + "/instance_config/local/loongcollector_config.json";
            AppConfig::GetInstance()->LoadAppConfig(conf);
            OverwriteFile(conf, v.toStyledString());
        }
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
};

void AppConfigUnittest::TestRecurseParseJsonToFlags() {
    Json::Value value;
    std::string configStr, errorMsg;
    // test single layer json
    configStr = R"(
        {
            "checkpoint_find_max_file_count": 600
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, value, errorMsg));
    value["logtail_sys_conf_dir"] = GetProcessExecutionDir();
    writeLogtailConfigJSON(value);
    AppConfig* app_config = AppConfig::GetInstance();
    app_config->LoadAppConfig(STRING_FLAG(ilogtail_config));
    APSARA_TEST_EQUAL(INT32_FLAG(checkpoint_find_max_file_count), 600);

    // test multi-layer json, include bool, string, int, double
    configStr = R"(
        {
            "test": {
                "receive_event_chan_cap": 1024,
                "admin_config": {
                    "debug_mode": true,
                    "log_level": "error",
                    "push_all_span": true
                },
                "aggregation_config": {
                    "agg_window_second": 8
                },
                "converage_config": {
                    "strategy": "combine1"
                },
                "sample_config": {
                    "strategy": "fixedRate1",
                    "config": {
                        "rate": 0.001
                    }
                }
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, value, errorMsg));
    value["logtail_sys_conf_dir"] = GetProcessExecutionDir();
    writeLogtailConfigJSON(value);
    app_config->LoadAppConfig(STRING_FLAG(ilogtail_config));
    APSARA_TEST_EQUAL(INT32_FLAG(test_receive_event_chan_cap), 1024);
    APSARA_TEST_EQUAL(BOOL_FLAG(test_admin_config_debug_mode), true);
    APSARA_TEST_EQUAL(STRING_FLAG(test_admin_config_log_level), "error");
    APSARA_TEST_EQUAL(BOOL_FLAG(test_admin_config_push_all_span), true);
    APSARA_TEST_EQUAL(INT32_FLAG(test_aggregation_config_agg_window_second), 8);
    APSARA_TEST_EQUAL(STRING_FLAG(test_converage_config_strategy), "combine1");
    APSARA_TEST_EQUAL(STRING_FLAG(test_sample_config_strategy), "fixedRate1");
    APSARA_TEST_EQUAL(DOUBLE_FLAG(test_sample_config_config_rate), 0.001);

    // test json with array
    configStr = R"(
        {
            "test": {
                "receive_event_chan_cap": [1,2,3],
                "admin_config": {
                    "debug_mode": true,
                    "log_level": "error",
                    "push_all_span": true
                }
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, value, errorMsg));
    value["logtail_sys_conf_dir"] = GetProcessExecutionDir();
    writeLogtailConfigJSON(value);
    app_config->LoadAppConfig(STRING_FLAG(ilogtail_config));
    auto old_test_receive_event_chan_cap = INT32_FLAG(test_receive_event_chan_cap);
    // array is not supported, so the value should not be changed
    APSARA_TEST_EQUAL(INT32_FLAG(test_receive_event_chan_cap), old_test_receive_event_chan_cap);
    // other values should be changed
    APSARA_TEST_EQUAL(BOOL_FLAG(test_admin_config_debug_mode), true);
    APSARA_TEST_EQUAL(STRING_FLAG(test_admin_config_log_level), "error");
    APSARA_TEST_EQUAL(BOOL_FLAG(test_admin_config_push_all_span), true);

    // test null object in json
    configStr = R"(
        {
            "test": {
                "admin_config": {},
                "receive_event_chan_cap": 55
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, value, errorMsg));
    value["logtail_sys_conf_dir"] = GetProcessExecutionDir();
    writeLogtailConfigJSON(value);
    app_config->LoadAppConfig(STRING_FLAG(ilogtail_config));
    // admin_config is null, so the value should not be changed
    auto old_test_admin_config_debug_mode = BOOL_FLAG(test_admin_config_debug_mode);
    APSARA_TEST_EQUAL(BOOL_FLAG(test_admin_config_debug_mode), old_test_admin_config_debug_mode);
    // other values should be changed
    APSARA_TEST_EQUAL(INT32_FLAG(test_receive_event_chan_cap), 55);
}

void AppConfigUnittest::TestParseEnvToFlags() {
    {
        SetEnv("host_path_blacklist", "test1");
        AppConfig::GetInstance()->ParseEnvToFlags();
        APSARA_TEST_EQUAL(STRING_FLAG(host_path_blacklist), "test1");
        UnsetEnv("host_path_blacklist");

        SetEnv("LOONG_host_path_blacklist", "test2");
        AppConfig::GetInstance()->ParseEnvToFlags();
        APSARA_TEST_EQUAL(STRING_FLAG(host_path_blacklist), "test2");
    }
    {
        SetEnv("default_machine_cpu_usage_threshold", "1");
        AppConfig::GetInstance()->ParseEnvToFlags();
        APSARA_TEST_EQUAL(DOUBLE_FLAG(default_machine_cpu_usage_threshold), 1);
        UnsetEnv("default_machine_cpu_usage_threshold");

        SetEnv("LOONG_DEFAULT_MACHINE_CPU_USAGE_THRESHOLD", "2");
        AppConfig::GetInstance()->ParseEnvToFlags();
        APSARA_TEST_EQUAL(DOUBLE_FLAG(default_machine_cpu_usage_threshold), 2);
    }
}

void AppConfigUnittest::TestLoadSingleValueEnvConfig() {
    SetEnv("cpu_usage_limit", "0.5");
    AppConfig::GetInstance()->LoadEnvResourceLimit();
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCpuUsageUpLimit(), 0.5);
    UnsetEnv("cpu_usage_limit");
    SetEnv("LOONG_CPU_USAGE_LIMIT", "0.6");
    AppConfig::GetInstance()->LoadEnvResourceLimit();
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCpuUsageUpLimit(), float(0.6));
    UnsetEnv("LOONG_CPU_USAGE_LIMIT");
}

void AppConfigUnittest::TestLoadStringParameter() {
    Json::Value value;
    std::string res;
    SetEnv("cpu_usage_limit_env", "0.5");
    LoadStringParameter(res, value, "cpu_usage_limit", "cpu_usage_limit_env");
    APSARA_TEST_EQUAL(res, "0.5");

    SetEnv("LOONG_CPU_USAGE_LIMIT", "0.6");
    LoadStringParameter(res, value, "cpu_usage_limit", "cpu_usage_limit_env");
    APSARA_TEST_EQUAL(res, "0.6");

    value["cpu_usage_limit"] = "0.7";
    LoadStringParameter(res, value, "cpu_usage_limit", "cpu_usage_limit_env");
    APSARA_TEST_EQUAL(res, "0.7");
}

UNIT_TEST_CASE(AppConfigUnittest, TestRecurseParseJsonToFlags);
UNIT_TEST_CASE(AppConfigUnittest, TestParseEnvToFlags);
UNIT_TEST_CASE(AppConfigUnittest, TestLoadSingleValueEnvConfig);
UNIT_TEST_CASE(AppConfigUnittest, TestLoadStringParameter);

} // namespace logtail

UNIT_TEST_MAIN
