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
#include "common/CgroupUtil.h"
#include "common/FileSystemUtil.h"
#include "common/Flags.h"
#include "common/JsonUtil.h"
#include "common/LogtailCommonFlags.h"
#include "common/MachineInfoUtil.h"
#include "common/RuntimeUtil.h"
#include "unittest/Unittest.h"

#if defined(__linux__)
#include <filesystem>
#include <fstream>
#endif

DECLARE_FLAG_INT32(checkpoint_find_max_file_count);
DECLARE_FLAG_BOOL(logtail_mode);
DECLARE_FLAG_STRING(host_path_blacklist);
DECLARE_FLAG_DOUBLE(default_machine_cpu_usage_threshold);
DECLARE_FLAG_STRING(ALIYUN_LOG_FILE_TAGS);
DECLARE_FLAG_STRING(LOONG_FILE_TAGS);
DECLARE_FLAG_BOOL(resource_limit_from_cgroup);
DECLARE_FLAG_STRING(cgroup_root_path);
DECLARE_FLAG_DOUBLE(cpu_usage_up_limit);
DECLARE_FLAG_INT64(memory_usage_up_limit);
DECLARE_FLAG_INT32(polling_max_stat_count);
DECLARE_FLAG_INT32(polling_max_stat_count_per_dir);
DECLARE_FLAG_INT32(polling_max_stat_count_per_config);
DECLARE_FLAG_INT32(config_match_max_cache_size);
DECLARE_FLAG_INT32(modify_cache_max);
DECLARE_FLAG_INT32(max_watch_dir_count);
DECLARE_FLAG_INT32(max_open_files_limit);
DECLARE_FLAG_INT32(max_reader_open_files);
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
    void TestGenerateFileTagsDir();
    void TestIgnoredInterfacesConfig();
    void TestIgnoredInterfacesEnv();
    void TestBindInterfaceWorkingInterfacePrecedence();
#if defined(__linux__)
    void TestCgroupResourceLimitFlagOff();
    void TestCgroupResourceLimitV2();
    void TestCgroupResourceLimitPrecedence();
    void TestCgroupResourceLimitReadFailFallback();
    void TestCgroupResourceLimitUnlimited();
    void TestCgroupResourceLimitPerResourceIndependence();
    void TestCgroupResourceLimitV1();
    void TestCgroupResourceLimitV1CpuAcct();
    void TestCgroupResourceLimitV1UnlimitedSentinels();
    void TestCgroupResourceLimitMalformed();
    void TestCgroupResourceLimitTinyValues();
    void TestCgroupResourceLimitAutoScale();
    void TestCgroupResourceLimitEnvOptIn();
    void TestCgroupResourceLimitFeedsAdjustParameters();
#endif

private:
    static Json::Value MakeDefaultIgnoredInterfacesConfig() {
        Json::Value conf;
        conf[kIgnoredInterfacesKey] = "kube-ipvs0,nodelocaldns,docker0";
        return conf;
    }

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

#if defined(__linux__)
namespace {
namespace fs = std::filesystem;

void WriteCgroupFile(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream ofs(path);
    ofs << content;
}

struct CgroupTempRoot {
    fs::path root;
    CgroupTempRoot() : root(fs::path(GetProcessExecutionDir()) / "AppConfigCgroupUnittestDir") {
        fs::remove_all(root);
        fs::create_directories(root);
    }
    ~CgroupTempRoot() { fs::remove_all(root); }
};

// Saves and restores every piece of state the cgroup tests touch, so the never-reset AppConfig
// singleton and global gflags do not leak across cases (even on non-fatal EXPECT failures).
struct ResourceLimitGuard {
    AppConfig* cfg = AppConfig::GetInstance();
    float cpu = cfg->mCpuUsageUpLimit;
    float scaled = cfg->mScaledCpuUsageUpLimit;
    int64_t mem = cfg->mMemUsageUpLimit;
    bool autoScale = cfg->mResourceAutoScale;
    bool fromCgroup = BOOL_FLAG(resource_limit_from_cgroup);
    std::string rootPath = STRING_FLAG(cgroup_root_path);
    double cpuFlag = DOUBLE_FLAG(cpu_usage_up_limit);
    int64_t memFlag = INT64_FLAG(memory_usage_up_limit);
    int32_t statCount = INT32_FLAG(polling_max_stat_count);
    int32_t statPerDir = INT32_FLAG(polling_max_stat_count_per_dir);
    int32_t statPerConfig = INT32_FLAG(polling_max_stat_count_per_config);
    int32_t cacheSize = INT32_FLAG(config_match_max_cache_size);
    int32_t modifyCache = INT32_FLAG(modify_cache_max);
    int32_t watchDir = INT32_FLAG(max_watch_dir_count);
    int32_t openFiles = INT32_FLAG(max_open_files_limit);
    int32_t readerOpenFiles = INT32_FLAG(max_reader_open_files);

    ResourceLimitGuard() {
        UnsetEnv("cpu_usage_limit");
        UnsetEnv("LOONG_CPU_USAGE_LIMIT");
        UnsetEnv("mem_usage_limit");
        UnsetEnv("LOONG_MEM_USAGE_LIMIT");
    }
    ~ResourceLimitGuard() {
        cfg->mCpuUsageUpLimit = cpu;
        cfg->mScaledCpuUsageUpLimit = scaled;
        cfg->mMemUsageUpLimit = mem;
        cfg->mResourceAutoScale = autoScale;
        BOOL_FLAG(resource_limit_from_cgroup) = fromCgroup;
        STRING_FLAG(cgroup_root_path) = rootPath;
        DOUBLE_FLAG(cpu_usage_up_limit) = cpuFlag;
        INT64_FLAG(memory_usage_up_limit) = memFlag;
        INT32_FLAG(polling_max_stat_count) = statCount;
        INT32_FLAG(polling_max_stat_count_per_dir) = statPerDir;
        INT32_FLAG(polling_max_stat_count_per_config) = statPerConfig;
        INT32_FLAG(config_match_max_cache_size) = cacheSize;
        INT32_FLAG(modify_cache_max) = modifyCache;
        INT32_FLAG(max_watch_dir_count) = watchDir;
        INT32_FLAG(max_open_files_limit) = openFiles;
        INT32_FLAG(max_reader_open_files) = readerOpenFiles;
        UnsetEnv("cpu_usage_limit");
        UnsetEnv("LOONG_CPU_USAGE_LIMIT");
        UnsetEnv("mem_usage_limit");
        UnsetEnv("LOONG_MEM_USAGE_LIMIT");
    }
};
} // namespace
#endif

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

void AppConfigUnittest::TestIgnoredInterfacesConfig() {
    AppConfig* cfg = AppConfig::GetInstance();
    cfg->ParseJsonToFlags(MakeDefaultIgnoredInterfacesConfig());
    APSARA_TEST_TRUE(IsIgnoredInterfaceForHostIdentity("kube-ipvs0"));
    APSARA_TEST_TRUE(IsIgnoredInterfaceForHostIdentity("docker0"));

    // JSON array is not applied as a gflag; flag value stays unchanged.
    Json::Value bad;
    bad[kIgnoredInterfacesKey] = Json::arrayValue;
    bad[kIgnoredInterfacesKey].append("x");
    cfg->ParseJsonToFlags(bad);
    APSARA_TEST_TRUE(IsIgnoredInterfaceForHostIdentity("kube-ipvs0"));

    Json::Value good;
    good[kIgnoredInterfacesKey] = "iface-a, iface-b";
    cfg->ParseJsonToFlags(good);
    APSARA_TEST_TRUE(IsIgnoredInterfaceForHostIdentity("iface-a"));
    APSARA_TEST_TRUE(IsIgnoredInterfaceForHostIdentity("iface-b"));
    APSARA_TEST_FALSE(IsIgnoredInterfaceForHostIdentity("kube-ipvs0"));

    Json::Value empty;
    empty[kIgnoredInterfacesKey] = "";
    cfg->ParseJsonToFlags(empty);
    APSARA_TEST_FALSE(IsIgnoredInterfaceForHostIdentity("kube-ipvs0"));

    cfg->ParseJsonToFlags(MakeDefaultIgnoredInterfacesConfig());
}

void AppConfigUnittest::TestIgnoredInterfacesEnv() {
    AppConfig* cfg = AppConfig::GetInstance();
    cfg->ParseJsonToFlags(MakeDefaultIgnoredInterfacesConfig());

    SetEnv(kIgnoredInterfacesKey, "env-a, env-b");
    cfg->ParseEnvToFlags();
    APSARA_TEST_TRUE(IsIgnoredInterfaceForHostIdentity("env-a"));
    APSARA_TEST_TRUE(IsIgnoredInterfaceForHostIdentity("env-b"));
    UnsetEnv(kIgnoredInterfacesKey);

    SetEnv("LOONG_IGNORED_INTERFACES", "p,q");
    cfg->ParseEnvToFlags();
    APSARA_TEST_TRUE(IsIgnoredInterfaceForHostIdentity("p"));
    APSARA_TEST_TRUE(IsIgnoredInterfaceForHostIdentity("q"));
    UnsetEnv("LOONG_IGNORED_INTERFACES");

    cfg->ParseJsonToFlags(MakeDefaultIgnoredInterfacesConfig());
}

void AppConfigUnittest::TestBindInterfaceWorkingInterfacePrecedence() {
    AppConfig* cfg = AppConfig::GetInstance();
    std::string savedWorking = STRING_FLAG(working_interface);
    std::string savedBind = cfg->mBindInterface;

    // working_interface empty: LoadResourceConf keeps the configured bind_interface as-is.
    STRING_FLAG(working_interface) = "";
    {
        Json::Value conf;
        conf["bind_interface"] = "eth0";
        cfg->LoadResourceConf(conf);
        APSARA_TEST_EQUAL(cfg->GetBindInterface(), "eth0");
    }

    // working_interface set: it overrides bind_interface at load time, so GetBindInterface (and thus
    // the curl egress binding) uses the same interface from which GetHostIp resolves the reported IP.
    STRING_FLAG(working_interface) = "eth1";
    {
        Json::Value conf;
        conf["bind_interface"] = "eth0";
        cfg->LoadResourceConf(conf);
        APSARA_TEST_EQUAL(cfg->GetBindInterface(), "eth1");
    }

    // The override applies even when bind_interface is absent from the config.
    cfg->mBindInterface = "eth2";
    {
        Json::Value conf; // no bind_interface key
        cfg->LoadResourceConf(conf);
        APSARA_TEST_EQUAL(cfg->GetBindInterface(), "eth1");
    }

    STRING_FLAG(working_interface) = savedWorking;
    cfg->mBindInterface = savedBind;
}

void AppConfigUnittest::TestGenerateFileTagsDir() {
    {
        STRING_FLAG(ALIYUN_LOG_FILE_TAGS) = "";
        STRING_FLAG(LOONG_FILE_TAGS) = "";

        std::string result = GenerateFileTagsDir();
        APSARA_TEST_EQUAL(result, "");
    }

    {
        STRING_FLAG(ALIYUN_LOG_FILE_TAGS) = "/path/to/aliyun/tags";
        STRING_FLAG(LOONG_FILE_TAGS) = "";
        BOOL_FLAG(logtail_mode) = true;

        std::string result = GenerateFileTagsDir();
        APSARA_TEST_EQUAL(result, "/path/to/aliyun/tags");
    }

    {
        STRING_FLAG(ALIYUN_LOG_FILE_TAGS) = "";
        STRING_FLAG(LOONG_FILE_TAGS) = "/path/to/loong/tags";
        BOOL_FLAG(logtail_mode) = true;

        std::string result = GenerateFileTagsDir();
        APSARA_TEST_EQUAL(result, "/path/to/loong/tags");
    }

    {
        STRING_FLAG(ALIYUN_LOG_FILE_TAGS) = "/path/to/aliyun/tags";
        STRING_FLAG(LOONG_FILE_TAGS) = "/path/to/loong/tags";
        BOOL_FLAG(logtail_mode) = true;

        std::string result = GenerateFileTagsDir();
        APSARA_TEST_EQUAL(result, "/path/to/loong/tags");
    }

    {
        STRING_FLAG(ALIYUN_LOG_FILE_TAGS) = "";
        STRING_FLAG(LOONG_FILE_TAGS) = "relative/path/tags";
        BOOL_FLAG(logtail_mode) = false;

        std::string result = GenerateFileTagsDir();
        APSARA_TEST_EQUAL(result,
                          AbsolutePath("relative/path/tags", AppConfig::GetInstance()->GetLoongcollectorConfDir()));
    }
}

#if defined(__linux__)
void AppConfigUnittest::TestCgroupResourceLimitFlagOff() {
    ResourceLimitGuard guard;
    CgroupTempRoot tmp;
    WriteCgroupFile(tmp.root / "cpu.max", "40000 100000\n");
    WriteCgroupFile(tmp.root / "memory.max", "536870912\n");
    BOOL_FLAG(resource_limit_from_cgroup) = false;
    STRING_FLAG(cgroup_root_path) = tmp.root.string();
    DOUBLE_FLAG(cpu_usage_up_limit) = 2.0;
    INT64_FLAG(memory_usage_up_limit) = 2048;
    Json::Value conf;
    AppConfig::GetInstance()->LoadResourceConf(conf);
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCpuUsageUpLimit(), float(2.0));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMemUsageUpLimit(), int64_t(2048));
}

void AppConfigUnittest::TestCgroupResourceLimitV2() {
    ResourceLimitGuard guard;
    CgroupTempRoot tmp;
    WriteCgroupFile(tmp.root / "cpu.max", "40000 100000\n");
    WriteCgroupFile(tmp.root / "memory.max", "536870912\n");
    BOOL_FLAG(resource_limit_from_cgroup) = true;
    STRING_FLAG(cgroup_root_path) = tmp.root.string();
    AppConfig::GetInstance()->LoadCgroupResourceLimit();
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCpuUsageUpLimit(), float(0.4));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetScaledCpuUsageUpLimit(), float(0.4));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMemUsageUpLimit(), int64_t(512));
}

void AppConfigUnittest::TestCgroupResourceLimitPrecedence() {
    ResourceLimitGuard guard;
    CgroupTempRoot tmp;
    WriteCgroupFile(tmp.root / "cpu.max", "40000 100000\n");
    WriteCgroupFile(tmp.root / "memory.max", "536870912\n");
    BOOL_FLAG(resource_limit_from_cgroup) = true;
    STRING_FLAG(cgroup_root_path) = tmp.root.string();
    SetEnv("cpu_usage_limit", "1.5");
    SetEnv("mem_usage_limit", "1024");
    Json::Value conf;
    conf["cpu_usage_limit"] = 2.5;
    conf["mem_usage_limit"] = 2048;
    AppConfig::GetInstance()->LoadResourceConf(conf);
    // cgroup wins over both env and json when enabled and readable.
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCpuUsageUpLimit(), float(0.4));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMemUsageUpLimit(), int64_t(512));
}

void AppConfigUnittest::TestCgroupResourceLimitReadFailFallback() {
    ResourceLimitGuard guard;
    CgroupTempRoot tmp; // empty root: no limit files
    BOOL_FLAG(resource_limit_from_cgroup) = true;
    STRING_FLAG(cgroup_root_path) = tmp.root.string();
    // (a) env baseline kept
    SetEnv("cpu_usage_limit", "0.8");
    SetEnv("mem_usage_limit", "1024");
    Json::Value conf;
    AppConfig::GetInstance()->LoadResourceConf(conf);
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCpuUsageUpLimit(), float(0.8));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMemUsageUpLimit(), int64_t(1024));
    UnsetEnv("cpu_usage_limit");
    UnsetEnv("mem_usage_limit");
    // (b) json baseline kept
    conf["cpu_usage_limit"] = 1.2;
    conf["mem_usage_limit"] = 768;
    AppConfig::GetInstance()->LoadResourceConf(conf);
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCpuUsageUpLimit(), float(1.2));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMemUsageUpLimit(), int64_t(768));
    // (c) flag default kept
    DOUBLE_FLAG(cpu_usage_up_limit) = 2.0;
    INT64_FLAG(memory_usage_up_limit) = 2048;
    Json::Value empty;
    AppConfig::GetInstance()->LoadResourceConf(empty);
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCpuUsageUpLimit(), float(2.0));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMemUsageUpLimit(), int64_t(2048));
}

void AppConfigUnittest::TestCgroupResourceLimitUnlimited() {
    ResourceLimitGuard guard;
    CgroupTempRoot tmp;
    WriteCgroupFile(tmp.root / "cpu.max", "max 100000\n");
    WriteCgroupFile(tmp.root / "memory.max", "max\n");
    BOOL_FLAG(resource_limit_from_cgroup) = true;
    STRING_FLAG(cgroup_root_path) = tmp.root.string();
    AppConfig::GetInstance()->mCpuUsageUpLimit = 1.0F;
    AppConfig::GetInstance()->mMemUsageUpLimit = 1024;
    AppConfig::GetInstance()->LoadCgroupResourceLimit();
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCpuUsageUpLimit(), float(1.0));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMemUsageUpLimit(), int64_t(1024));
}

void AppConfigUnittest::TestCgroupResourceLimitPerResourceIndependence() {
    ResourceLimitGuard guard;
    CgroupTempRoot tmp;
    BOOL_FLAG(resource_limit_from_cgroup) = true;
    STRING_FLAG(cgroup_root_path) = tmp.root.string();
    // cpu finite, mem unlimited -> cpu applied, mem kept
    WriteCgroupFile(tmp.root / "cpu.max", "40000 100000\n");
    WriteCgroupFile(tmp.root / "memory.max", "max\n");
    AppConfig::GetInstance()->mCpuUsageUpLimit = 1.0F;
    AppConfig::GetInstance()->mMemUsageUpLimit = 1024;
    AppConfig::GetInstance()->LoadCgroupResourceLimit();
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCpuUsageUpLimit(), float(0.4));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMemUsageUpLimit(), int64_t(1024));
    // mirror: cpu unlimited, mem finite -> cpu kept, mem applied
    WriteCgroupFile(tmp.root / "cpu.max", "max 100000\n");
    WriteCgroupFile(tmp.root / "memory.max", "536870912\n");
    AppConfig::GetInstance()->mCpuUsageUpLimit = 1.0F;
    AppConfig::GetInstance()->mMemUsageUpLimit = 1024;
    AppConfig::GetInstance()->LoadCgroupResourceLimit();
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCpuUsageUpLimit(), float(1.0));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMemUsageUpLimit(), int64_t(512));
}

void AppConfigUnittest::TestCgroupResourceLimitV1() {
    ResourceLimitGuard guard;
    CgroupTempRoot tmp;
    WriteCgroupFile(tmp.root / "cpu" / "cpu.cfs_quota_us", "40000\n");
    WriteCgroupFile(tmp.root / "cpu" / "cpu.cfs_period_us", "100000\n");
    WriteCgroupFile(tmp.root / "memory" / "memory.limit_in_bytes", "536870912\n");
    BOOL_FLAG(resource_limit_from_cgroup) = true;
    STRING_FLAG(cgroup_root_path) = tmp.root.string();
    AppConfig::GetInstance()->LoadCgroupResourceLimit();
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCpuUsageUpLimit(), float(0.4));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMemUsageUpLimit(), int64_t(512));
}

void AppConfigUnittest::TestCgroupResourceLimitV1CpuAcct() {
    ResourceLimitGuard guard;
    CgroupTempRoot tmp;
    WriteCgroupFile(tmp.root / "cpu,cpuacct" / "cpu.cfs_quota_us", "40000\n");
    WriteCgroupFile(tmp.root / "cpu,cpuacct" / "cpu.cfs_period_us", "100000\n");
    WriteCgroupFile(tmp.root / "memory" / "memory.limit_in_bytes", "536870912\n");
    BOOL_FLAG(resource_limit_from_cgroup) = true;
    STRING_FLAG(cgroup_root_path) = tmp.root.string();
    AppConfig::GetInstance()->LoadCgroupResourceLimit();
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCpuUsageUpLimit(), float(0.4));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMemUsageUpLimit(), int64_t(512));
}

void AppConfigUnittest::TestCgroupResourceLimitV1UnlimitedSentinels() {
    ResourceLimitGuard guard;
    CgroupTempRoot tmp;
    BOOL_FLAG(resource_limit_from_cgroup) = true;
    STRING_FLAG(cgroup_root_path) = tmp.root.string();
    WriteCgroupFile(tmp.root / "cpu" / "cpu.cfs_quota_us", "-1\n");
    WriteCgroupFile(tmp.root / "cpu" / "cpu.cfs_period_us", "100000\n");
    WriteCgroupFile(tmp.root / "memory" / "memory.limit_in_bytes", "9223372036854771712\n");
    AppConfig::GetInstance()->mCpuUsageUpLimit = 1.0F;
    AppConfig::GetInstance()->mMemUsageUpLimit = 1024;
    AppConfig::GetInstance()->LoadCgroupResourceLimit();
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCpuUsageUpLimit(), float(1.0));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMemUsageUpLimit(), int64_t(1024));
    // old-kernel LONG_MAX sentinel
    WriteCgroupFile(tmp.root / "memory" / "memory.limit_in_bytes", "9223372036854775807\n");
    AppConfig::GetInstance()->LoadCgroupResourceLimit();
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMemUsageUpLimit(), int64_t(1024));
}

void AppConfigUnittest::TestCgroupResourceLimitMalformed() {
    ResourceLimitGuard guard;
    CgroupTempRoot tmp;
    BOOL_FLAG(resource_limit_from_cgroup) = true;
    STRING_FLAG(cgroup_root_path) = tmp.root.string();
    const std::vector<std::string> badCpu = {"abc 100000", "40000", "40000 0", "0 100000", "", "inf 100000"};
    for (const auto& content : badCpu) {
        WriteCgroupFile(tmp.root / "cpu.max", content);
        AppConfig::GetInstance()->mCpuUsageUpLimit = 1.0F;
        AppConfig::GetInstance()->LoadCgroupResourceLimit();
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCpuUsageUpLimit(), float(1.0));
    }
    // double space is valid
    WriteCgroupFile(tmp.root / "cpu.max", "40000  100000");
    AppConfig::GetInstance()->LoadCgroupResourceLimit();
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCpuUsageUpLimit(), float(0.4));
    // malformed memory
    const std::vector<std::string> badMem = {"abc", "", "inf", "-5"};
    for (const auto& content : badMem) {
        WriteCgroupFile(tmp.root / "memory.max", content);
        AppConfig::GetInstance()->mMemUsageUpLimit = 1024;
        AppConfig::GetInstance()->LoadCgroupResourceLimit();
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMemUsageUpLimit(), int64_t(1024));
    }
}

void AppConfigUnittest::TestCgroupResourceLimitTinyValues() {
    ResourceLimitGuard guard;
    CgroupTempRoot tmp;
    BOOL_FLAG(resource_limit_from_cgroup) = true;
    STRING_FLAG(cgroup_root_path) = tmp.root.string();
    WriteCgroupFile(tmp.root / "cpu.max", "1000 100000\n"); // 0.01 core, below 0.4 floor but accepted
    WriteCgroupFile(tmp.root / "memory.max", "524288\n"); // 0.5 MiB -> 0 MiB, rejected
    AppConfig::GetInstance()->mMemUsageUpLimit = 1024;
    AppConfig::GetInstance()->LoadCgroupResourceLimit();
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCpuUsageUpLimit(), float(0.01));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMemUsageUpLimit(), int64_t(1024));
    // 128 MiB accepted below the 384 floor
    WriteCgroupFile(tmp.root / "memory.max", "134217728\n");
    AppConfig::GetInstance()->LoadCgroupResourceLimit();
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMemUsageUpLimit(), int64_t(128));
}

void AppConfigUnittest::TestCgroupResourceLimitAutoScale() {
    ResourceLimitGuard guard;
    CgroupTempRoot tmp;
    WriteCgroupFile(tmp.root / "cpu.max", "40000 100000\n");
    WriteCgroupFile(tmp.root / "memory.max", "536870912\n");
    BOOL_FLAG(resource_limit_from_cgroup) = true;
    STRING_FLAG(cgroup_root_path) = tmp.root.string();
    Json::Value conf;
    conf["resource_auto_scale"] = true;
    AppConfig::GetInstance()->LoadResourceConf(conf);
    APSARA_TEST_TRUE(AppConfig::GetInstance()->IsResourceAutoScale());
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCpuUsageUpLimit(), float(0.4));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetScaledCpuUsageUpLimit(), float(0.4));
}

void AppConfigUnittest::TestCgroupResourceLimitEnvOptIn() {
    ResourceLimitGuard guard;
    // Lowercase bare env name, same style as cpu_usage_limit / mem_usage_limit.
    BOOL_FLAG(resource_limit_from_cgroup) = false;
    SetEnv("resource_limit_from_cgroup", "true");
    AppConfig::GetInstance()->ParseEnvToFlags();
    APSARA_TEST_TRUE(BOOL_FLAG(resource_limit_from_cgroup));
    UnsetEnv("resource_limit_from_cgroup");
    // Lowercase cgroup_root_path.
    SetEnv("cgroup_root_path", "/tmp/some-cgroup-root");
    AppConfig::GetInstance()->ParseEnvToFlags();
    APSARA_TEST_EQUAL(STRING_FLAG(cgroup_root_path), std::string("/tmp/some-cgroup-root"));
    UnsetEnv("cgroup_root_path");
    // LOONG_ uppercase variant is equivalent.
    BOOL_FLAG(resource_limit_from_cgroup) = false;
    SetEnv("LOONG_RESOURCE_LIMIT_FROM_CGROUP", "true");
    AppConfig::GetInstance()->ParseEnvToFlags();
    APSARA_TEST_TRUE(BOOL_FLAG(resource_limit_from_cgroup));
    UnsetEnv("LOONG_RESOURCE_LIMIT_FROM_CGROUP");
}

void AppConfigUnittest::TestCgroupResourceLimitFeedsAdjustParameters() {
    ResourceLimitGuard guard;
    CgroupTempRoot tmp;
    WriteCgroupFile(tmp.root / "cpu.max", "40000 100000\n");
    WriteCgroupFile(tmp.root / "memory.max", "536870912\n"); // 512 MiB
    BOOL_FLAG(resource_limit_from_cgroup) = true;
    STRING_FLAG(cgroup_root_path) = tmp.root.string();
    INT32_FLAG(polling_max_stat_count) = 200000;
    Json::Value conf;
    AppConfig::GetInstance()->LoadResourceConf(conf);
    // scaleRatio = 512 / 100 = 5; nowMaxStatCount = 200000 / 20 = 10000; 10000 * 5 = 50000 (< 200000 cap)
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMemUsageUpLimit(), int64_t(512));
    APSARA_TEST_EQUAL(INT32_FLAG(polling_max_stat_count), 50000);
}
#endif

UNIT_TEST_CASE(AppConfigUnittest, TestRecurseParseJsonToFlags);
UNIT_TEST_CASE(AppConfigUnittest, TestParseEnvToFlags);
UNIT_TEST_CASE(AppConfigUnittest, TestLoadSingleValueEnvConfig);
UNIT_TEST_CASE(AppConfigUnittest, TestLoadStringParameter);
UNIT_TEST_CASE(AppConfigUnittest, TestIgnoredInterfacesConfig);
UNIT_TEST_CASE(AppConfigUnittest, TestIgnoredInterfacesEnv);
UNIT_TEST_CASE(AppConfigUnittest, TestBindInterfaceWorkingInterfacePrecedence);
UNIT_TEST_CASE(AppConfigUnittest, TestGenerateFileTagsDir);
#if defined(__linux__)
UNIT_TEST_CASE(AppConfigUnittest, TestCgroupResourceLimitFlagOff);
UNIT_TEST_CASE(AppConfigUnittest, TestCgroupResourceLimitV2);
UNIT_TEST_CASE(AppConfigUnittest, TestCgroupResourceLimitPrecedence);
UNIT_TEST_CASE(AppConfigUnittest, TestCgroupResourceLimitReadFailFallback);
UNIT_TEST_CASE(AppConfigUnittest, TestCgroupResourceLimitUnlimited);
UNIT_TEST_CASE(AppConfigUnittest, TestCgroupResourceLimitPerResourceIndependence);
UNIT_TEST_CASE(AppConfigUnittest, TestCgroupResourceLimitV1);
UNIT_TEST_CASE(AppConfigUnittest, TestCgroupResourceLimitV1CpuAcct);
UNIT_TEST_CASE(AppConfigUnittest, TestCgroupResourceLimitV1UnlimitedSentinels);
UNIT_TEST_CASE(AppConfigUnittest, TestCgroupResourceLimitMalformed);
UNIT_TEST_CASE(AppConfigUnittest, TestCgroupResourceLimitTinyValues);
UNIT_TEST_CASE(AppConfigUnittest, TestCgroupResourceLimitAutoScale);
UNIT_TEST_CASE(AppConfigUnittest, TestCgroupResourceLimitEnvOptIn);
UNIT_TEST_CASE(AppConfigUnittest, TestCgroupResourceLimitFeedsAdjustParameters);
#endif

} // namespace logtail

UNIT_TEST_MAIN
