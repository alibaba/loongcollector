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

#include "common/Flags.h"
#include "common/JsonUtil.h"
#include "unittest/Unittest.h"
#include "common/FileSystemUtil.h"
#include "app_config/AppConfig.h"

DECLARE_FLAG_INT32(checkpoint_find_max_file_count);
DECLARE_FLAG_INT32(ebpf_receive_event_chan_cap);
DECLARE_FLAG_BOOL(ebpf_admin_config_debug_mode);
DECLARE_FLAG_STRING(ebpf_admin_config_log_level);
DECLARE_FLAG_BOOL(ebpf_admin_config_push_all_span);
DECLARE_FLAG_INT32(ebpf_aggregation_config_agg_window_second);
DECLARE_FLAG_STRING(ebpf_converage_config_strategy);
DECLARE_FLAG_STRING(ebpf_sample_config_strategy);
DECLARE_FLAG_DOUBLE(ebpf_sample_config_config_rate);
DECLARE_FLAG_BOOL(logtail_mode);

namespace logtail {

class AppConfigUnittest : public ::testing::Test {
public:
    void TestRecurseParseJsonToFlags();

private:
    void writeLogtailConfigJSON(const Json::Value& v) {
        LOG_INFO(sLogger, ("writeLogtailConfigJSON", v.toStyledString()));
        if (BOOL_FLAG(logtail_mode)) {
            OverwriteFile(STRING_FLAG(ilogtail_config), v.toStyledString());
        } else {
            CreateAgentDir();
            std::string conf  = GetAgentConfDir() + "/instance_config/local/loongcollector_config.json";
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
            "ebpf": {
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
    APSARA_TEST_EQUAL(INT32_FLAG(ebpf_receive_event_chan_cap), 1024);
    APSARA_TEST_EQUAL(BOOL_FLAG(ebpf_admin_config_debug_mode), true);
    APSARA_TEST_EQUAL(STRING_FLAG(ebpf_admin_config_log_level), "error");
    APSARA_TEST_EQUAL(BOOL_FLAG(ebpf_admin_config_push_all_span), true);
    APSARA_TEST_EQUAL(INT32_FLAG(ebpf_aggregation_config_agg_window_second), 8);
    APSARA_TEST_EQUAL(STRING_FLAG(ebpf_converage_config_strategy), "combine1");
    APSARA_TEST_EQUAL(STRING_FLAG(ebpf_sample_config_strategy), "fixedRate1");
    APSARA_TEST_EQUAL(DOUBLE_FLAG(ebpf_sample_config_config_rate), 0.001);

    // test json with array
    configStr = R"(
        {
            "ebpf": {
                "receive_event_chan_cap": [1,2,3],
                "admin_config": {
                    "debug_mode": true,
                    "log_level": "error",
                    "push_all_span": true
                },
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, value, errorMsg));
    value["logtail_sys_conf_dir"] = GetProcessExecutionDir();
    writeLogtailConfigJSON(value);
    app_config->LoadAppConfig(STRING_FLAG(ilogtail_config));
    auto old_ebpf_receive_event_chan_cap = INT32_FLAG(ebpf_receive_event_chan_cap);
    // array is not supported, so the value should not be changed
    APSARA_TEST_EQUAL(INT32_FLAG(ebpf_receive_event_chan_cap), old_ebpf_receive_event_chan_cap);
    // other values should be changed
    APSARA_TEST_EQUAL(BOOL_FLAG(ebpf_admin_config_debug_mode), true);
    APSARA_TEST_EQUAL(STRING_FLAG(ebpf_admin_config_log_level), "error");
    APSARA_TEST_EQUAL(BOOL_FLAG(ebpf_admin_config_push_all_span), true);

    // test null object in json
    configStr = R"(
        {
            "ebpf": {
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
    auto old_ebpf_admin_config_debug_mode = BOOL_FLAG(ebpf_admin_config_debug_mode);
    APSARA_TEST_EQUAL(BOOL_FLAG(ebpf_admin_config_debug_mode), old_ebpf_admin_config_debug_mode);
    // other values should be changed
    APSARA_TEST_EQUAL(INT32_FLAG(ebpf_receive_event_chan_cap), 55);
}

UNIT_TEST_CASE(AppConfigUnittest, TestRecurseParseJsonToFlags);

} // namespace logtail

UNIT_TEST_MAIN
