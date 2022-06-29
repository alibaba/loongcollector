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

#include "unittest/Unittest.h"
#if defined(__linux__)
#include <unistd.h>
#endif
#include <fcntl.h>
#include <cstdlib>
#include <string.h>
#include <thread>
#include <memory>
#include <fstream>
#include <json/json.h>
#include "event_handler/EventHandler.h"
#include "config_manager/ConfigManager.h"
#include "reader/LogFileReader.h"
#include "AppConfig.h"
#include "Monitor.h"
#include "EventDispatcher.h"
#include "util.h"
#include "CheckPointManager.h"
#include "LogInput.h"
#include "Sender.h"
#include "sls_logs.pb.h"
#include "LogtailAlarm.h"
#include "common/Flags.h"
#include "common/Lock.h"
#include "common/Constants.h"
#include "common/FileSystemUtil.h"
#include "logger/Logger.h"
#include "sdk/Common.h"
#include "processor/LogFilter.h"

using namespace std;
using namespace logtail::sdk;
using namespace sls_logs;

DECLARE_FLAG_INT32(max_holded_data_size);
DECLARE_FLAG_INT32(max_buffer_num);
DECLARE_FLAG_BOOL(enable_mock_send);
DECLARE_FLAG_STRING(check_point_filename);
DECLARE_FLAG_STRING(user_log_config);
DECLARE_FLAG_STRING(ilogtail_config);
DECLARE_FLAG_INT32(mem_check_point_time_out);
DECLARE_FLAG_INT32(file_check_point_time_out);
DECLARE_FLAG_INT32(check_point_check_interval);
DECLARE_FLAG_STRING(default_global_topic);
DECLARE_FLAG_INT32(default_max_send_byte_per_sec);
DECLARE_FLAG_INT32(default_send_byte_per_sec);
DECLARE_FLAG_INT32(default_buffer_file_num);
DECLARE_FLAG_INT32(default_local_file_size);
DECLARE_FLAG_STRING(default_buffer_file_path);
DECLARE_FLAG_INT32(default_max_inotify_watch_num);
DECLARE_FLAG_STRING(profile_project_name);
DECLARE_FLAG_INT32(check_base_dir_interval);
DECLARE_FLAG_INT32(batch_send_interval);
DECLARE_FLAG_STRING(local_machine_uuid);
DECLARE_FLAG_INT32(check_point_version);
DECLARE_FLAG_INT32(check_point_dump_interval);
DECLARE_FLAG_INT32(global_pub_config_retry_interval);
DECLARE_FLAG_INT32(dirfile_check_interval_ms);
DECLARE_FLAG_INT32(polling_modify_repush_interval);
DECLARE_FLAG_STRING(fuse_customized_config_name);
DECLARE_FLAG_BOOL(rapid_retry_update_config);
DECLARE_FLAG_BOOL(default_global_fuse_mode);

namespace logtail {

string gPath[] = {"apsara_log", "comm"};
const int WRITE_LOG_SLEEP_INTERVAL = 10 * 1000;
const int32_t WAIT_CONFIG_UPDATE_INTERVAL = 5;
const string LOCAL_UUID = "97cfc7d3-bc21-dc4f-a190-43a83e552156";
SpinLock mResponseLock;
std::unique_ptr<std::thread> gDispatchThreadId;

void RunningDispatcher() {
    LOG_INFO(sLogger, ("RunningDispatcher", "Begin"));
    ConfigManager::GetInstance()->RegisterHandlers();
    EventDispatcher::GetInstance()->Dispatch();
    LOG_INFO(sLogger, ("RunningDispatcher", "End"));
}

class ConfigUpdatorUnittest : public ::testing::Test {
private:
    // Shorten typing of PATH_SEPARATOR.
    const std::string& PS = PATH_SEPARATOR;

    static std::unordered_map<std::string, std::string> flagMap;
    static string mRootDir;
    static string sResponse;
    static unordered_map<string, int> sProjectNameCountMap;
    static unordered_map<string, int> sProjectCategoryTopicCountMap;
    static unordered_map<string, int> sTopicCountMap;
    static string sDaemonDir;
    static string sSecurityResponse;
    static string sAccessKeyResponse;
    static bool sReplaceKey;
    static unordered_map<string, string> sRegionResponseMap;
    static unordered_set<string> sConfigRejectSet;

    void CaseSetup(bool replaceConfigAllowed = true);
    void CaseCleanup();
    void RemoveConfigFile();
    static void GetLogContent(LogType logType, char* buffer);
    static void DumpLog(int logNum, string path, enum LogType logType, string fileName = string("job.log"));
    void DumpInitConfigToLocal(string localPath = string(""));
    void SetupGlobalFuseMode(bool globalFuseMode);
    void DumpCustomizedConfigToLocal(bool check_ulogfs_env, bool fuse_mode);
    static bool MockGetLogtailConfig(const std::string& targetURL,
                                     const std::string& intf,
                                     const std::string& ip,
                                     const std::string& config,
                                     std::string& response,
                                     std::string& errorMessage,
                                     bool isCompress);
    static bool MockGetLogtailSecurity(const std::string& targetURL,
                                       const std::string& intf,
                                       bool httpsVerifyPeer,
                                       const std::string& request,
                                       std::string& response,
                                       std::string& errorMessage,
                                       const std::string& caCert);
    static bool MockGetAccessKey(const std::string& targetURL,
                                 const std::string& intf,
                                 bool httpsVerifyPeer,
                                 const std::string& request,
                                 std::string& response,
                                 std::string& errorMessage,
                                 const std::string& caCert);
    static void MockAsyncSend(const std::string& projectName,
                              const std::string& logstore,
                              const std::string& logData,
                              SEND_DATA_TYPE dataType,
                              int32_t rawSize,
                              SendClosure* sendClosure);
    static string GetConfigResponse();
    static void SetConfigResponse(const string& response);

    static void SetConfigReject(const string& configUrl) {
        ScopedSpinLock lock(mResponseLock);
        sConfigRejectSet.insert(configUrl);
        if (0 == configUrl.find("http://")) {
            sConfigRejectSet.insert("https://" + configUrl.substr(std::string("http://").length()));
        }
    }

    static bool IsConfigRejected(const string& configUrl) {
        ScopedSpinLock lock(mResponseLock);
        if (sConfigRejectSet.find(configUrl) != sConfigRejectSet.end()) {
            return true;
        }
        return false;
    }

    static void SetRegionConfigResponse(const string& region, const string& response) {
        LOG_INFO(sLogger, ("SetRegionConfigResponse", region)("response", response));
        ScopedSpinLock lock(mResponseLock);
        sRegionResponseMap[region] = response;
        if (0 == region.find("http://")) {
            sRegionResponseMap["https://" + region.substr(std::string("http://").length())] = response;
        }
    }

    static string GetRegionConfigResponse(const string& region) {
        ScopedSpinLock lock(mResponseLock);
        return sRegionResponseMap[region];
    }

    static void ClearConfigResponse() {
        ScopedSpinLock lock(mResponseLock);
        sRegionResponseMap.clear();
        sResponse.clear();
        sConfigRejectSet.clear();
    }

    // SetConfigFilePattern sets the file_pattern field for @configJson.
    // On Windows, bracket pattern is not supported.
    static void SetConfigFilePattern(Json::Value& configJson) {
#if defined(__linux__)
        configJson["file_pattern"] = Json::Value("*.[Ll][Oo][Gg]");
#elif defined(_MSC_VER)
        configJson["file_pattern"] = Json::Value("*.log");
#endif
    }

    // Wait several seconds to make sure test log files have been read.
    static void WaitForFileBeenRead() {
#if defined(_MSC_VER)
        // Because of lackness of event based discovery, on Windows, we can only use
        // polling, so wait for polling interval plus several process time (2s here).
        logtail::sleep(INT32_FLAG(dirfile_check_interval_ms) / 1000 + 2);
#endif
    }

public:
    static void SetUpTestCase();

    void SetUp() {
        AppConfig::GetInstance()->DumpAllFlagsToMap(flagMap);
        bfs::create_directories(mRootDir);
    }
    void TearDown() {
        AppConfig::GetInstance()->ReadFlagsFromMap(flagMap);
        bfs::remove_all(mRootDir);
        bfs::remove(mRootDir + PS + "ilogtail_config.json");
        bfs::remove(STRING_FLAG(check_point_filename));
    }

    void SetupContainerModeConfig();
    void ReplaceWithContainerModeConfig();

    void TestCheckPointManager();
    void TestConfigUpdate();
    void TestLocalConfigUpdate();
    void TestUpdatePath();
    void TestUpdateGlobalConfig();
    void TestUpdateProfileProject();
    void TestValidPath();
    void TestBlackDirList();
    void TestDirCheckPoint();
    void TestTimeoutCheckPoint();
    void TestLoadIlogtailConfig();
    void TestUpdateGroupTopic();
    void TestCheckPointSaveInterval();
    void TestCheckPointUserDefinedFilePath();
    void TestCheckPointLoadDefaultFile();
    void TestValidWildcardPath();
    void TestValidWildcardPath2();
    void TestWithinMaxDepth();
    void TestParseWildcardPath();
    void TestIsWildcardPathMatch();
    void TestLogRotateWhenUpdate();
    // test for container mode
    void TestContainerModeNormal();
    void TestContainerModeWildcardConfig();
    void TestContainerModeConfigUpdate();
    void TestLoadGlobalFuseConf();
    void TestCheckUlogfsEnv();
};

APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestLogRotateWhenUpdate, -1);
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestCheckPointManager, 0);
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestConfigUpdate, 1);
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestLocalConfigUpdate, 2);
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestUpdatePath, 3);
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestUpdateGlobalConfig, 4);
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestUpdateProfileProject, 5);
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestValidPath, 6);
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestBlackDirList, 7);
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestDirCheckPoint, 8);
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestTimeoutCheckPoint, 9);
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestLoadIlogtailConfig, 10);
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestUpdateGroupTopic, 11);
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestValidWildcardPath, 14);
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestWithinMaxDepth, 15);
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestParseWildcardPath, 16);
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestIsWildcardPathMatch, 17);
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestCheckPointSaveInterval, 19);
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestCheckPointUserDefinedFilePath, 20);
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestCheckPointLoadDefaultFile, 21);
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestValidWildcardPath2, 25);
#if defined(__linux__)
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestContainerModeNormal, 26);
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestContainerModeWildcardConfig, 27);
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestContainerModeConfigUpdate, 28);
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestLoadGlobalFuseConf, 29);
APSARA_UNIT_TEST_CASE(ConfigUpdatorUnittest, TestCheckUlogfsEnv, 30);
#endif

std::string ConfigUpdatorUnittest::mRootDir;
std::unordered_map<std::string, std::string> ConfigUpdatorUnittest::flagMap;
string ConfigUpdatorUnittest::sResponse;
string ConfigUpdatorUnittest::sSecurityResponse;
string ConfigUpdatorUnittest::sAccessKeyResponse;
bool ConfigUpdatorUnittest::sReplaceKey = false;
unordered_map<string, int> ConfigUpdatorUnittest::sProjectNameCountMap;
unordered_map<string, int> ConfigUpdatorUnittest::sProjectCategoryTopicCountMap;
unordered_map<string, int> ConfigUpdatorUnittest::sTopicCountMap;

unordered_map<string, string> ConfigUpdatorUnittest::sRegionResponseMap;
unordered_set<string> ConfigUpdatorUnittest::sConfigRejectSet;
string ConfigUpdatorUnittest::sDaemonDir = "";

static int sRejectCount = 0;

void ConfigUpdatorUnittest::SetUpTestCase() {
    sLogger->set_level(spdlog::level::trace);

    BOOL_FLAG(enable_mock_send) = true;
    INT32_FLAG(check_base_dir_interval) = 10;
    INT32_FLAG(batch_send_interval) = 2;
#if defined(_MSC_VER)
    // Shorten polling interval to reduce UT execution time.
    INT32_FLAG(dirfile_check_interval_ms) = 1000;
#endif
    Sender::Instance()->MockAsyncSend = MockAsyncSend;
    bfs::remove("ilogtail_config.json");
    mRootDir = GetProcessExecutionDir();
    if (PATH_SEPARATOR[0] == mRootDir.at(mRootDir.size() - 1))
        mRootDir.resize(mRootDir.size() - 1);

    STRING_FLAG(check_point_filename) = mRootDir + PATH_SEPARATOR + "logtail_checkpoint_config_udpator_ut";
    AppConfig::GetInstance()->mCheckPointFilePath = STRING_FLAG(check_point_filename);
    mRootDir += PATH_SEPARATOR + "ConfigUpdatorUnittest";
    bfs::remove_all(mRootDir);
    bfs::create_directories(mRootDir);
}

void ConfigUpdatorUnittest::SetupContainerModeConfig() {
    Json::Value logtailConfig;
    logtailConfig["container_mode"] = Json::Value(true);
    logtailConfig["working_ip"] = Json::Value("1.2.3.4");
    logtailConfig["working_hostname"] = Json::Value("sls-zc-test");
    logtailConfig["container_mount_path"] = Json::Value("./container_mount_test.json");

    ofstream fout(STRING_FLAG(ilogtail_config).c_str());
    fout << logtailConfig.toStyledString() << endl;
    fout.close();

    Json::Value mountConfig;
    mountConfig["version"] = Json::Value("0.1.0");
    mountConfig["container_name"] = Json::Value("logtail-docker");
    mountConfig["container_id"] = Json::Value("abcdef1234567890");
    mountConfig["host_path"] = Json::Value(mRootDir);

    Json::Value mount1;
    mount1["destination"] = "/";
    mount1["source"] = "/mount";
    Json::Value mount2;
    mount2["destination"] = "/home/admin/logs";
    mount2["source"] = "/home/admin/t4/docker/logs";
    Json::Value mount3;
    mount3["destination"] = "/app_2";
    mount3["source"] = "/yyyy";
    Json::Value mount4;
    mount4["destination"] = "/app_2/xxx";
    mount4["source"] = "/xxx";

    Json::Value mountArray;
    mountArray.append(mount1);
    mountArray.append(mount2);
    mountArray.append(mount3);
    mountArray.append(mount4);
    mountConfig["container_mount"] = mountArray;

    ofstream foutMount("./container_mount_test.json");
    foutMount << mountConfig.toStyledString() << endl;
    foutMount.close();
}

void ConfigUpdatorUnittest::ReplaceWithContainerModeConfig() {
    Json::Value logtailConfig;
    logtailConfig["container_mode"] = Json::Value(true);
    logtailConfig["working_ip"] = Json::Value("1.2.3.4");
    logtailConfig["working_hostname"] = Json::Value("sls-zc-test");
    logtailConfig["container_mount_path"] = Json::Value("./container_mount_test.json");

    ofstream fout(STRING_FLAG(ilogtail_config).c_str());
    fout << logtailConfig.toStyledString() << endl;
    fout.close();

    Json::Value mountConfig;
    mountConfig["version"] = Json::Value("0.1.0");
    mountConfig["container_name"] = Json::Value("logtail-docker");
    mountConfig["container_id"] = Json::Value("abcdef1234567890");
    mountConfig["host_path"] = Json::Value("/");

    Json::Value mount1;
    mount1["destination"] = "/";
    mount1["source"] = "";
    Json::Value mount2;
    mount2["destination"] = "/home/admin/logs";
    mount2["source"] = "/home/admin/t4/docker/logs";
    Json::Value mount3;
    mount3["destination"] = "/app_2";
    mount3["source"] = "/yyyy";
    Json::Value mount4;
    mount4["destination"] = "/app_2/xxx";
    mount4["source"] = "/xxx";

    Json::Value mountArray;
    mountArray.append(mount1);
    mountArray.append(mount2);
    mountArray.append(mount3);
    mountArray.append(mount4);
    mountConfig["container_mount"] = mountArray;

    ofstream foutMount("./container_mount_test.json");
    foutMount << mountConfig.toStyledString() << endl;
    foutMount.close();
}

void ConfigUpdatorUnittest::TestDirCheckPoint() {
    LOG_INFO(sLogger, ("TestDirCheckPoint() begin", time(NULL)));

    // Depend on inotify, test on Linux only.
#if defined(__linux__)
    bfs::remove_all(STRING_FLAG(check_point_filename));
    string dirs[] = {PS + "dir" + PS + "dir1",
                     PS + "dir" + PS + "dir1" + PS + "dir2_1",
                     PS + "dir" + PS + "dir1" + PS + "dir2_2",
                     PS + "dir" + PS + "dir1" + PS + "dir2_1" + PS + "dir_3_1",
                     PS + "dir" + PS + "dir1" + PS + "dir2_2" + PS + "dir_3_2"};

    bool isInotify[] = {true, false, true, false, true};
    bool isTimeout[] = {false, false, true, false, true};

    bfs::create_directories(bfs::path(mRootDir) / "dir");

    //dirs[0] -- inotify:yes, timeout:no;
    //dirs[1] -- inotify:no
    for (int i = 0; i < 2; ++i) {
        bfs::create_directories(bfs::path(mRootDir) / dirs[i]);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }

    // config --- log_path: /dir,  preserve: false, depth: 1
    Json::Value apsara_log;
    apsara_log["project_name"] = Json::Value("9000000_proj");
    apsara_log["category"] = Json::Value("9000000_category");
    apsara_log["log_type"] = Json::Value("apsara_log");
    apsara_log["log_begin_reg"] = Json::Value("\\[\\d+-\\d+-\\d+ \\d+:\\d+:\\d+.\\d+\\].*");
    apsara_log["log_path"] = Json::Value(mRootDir + PS + "dir");
    SetConfigFilePattern(apsara_log);
    apsara_log["enable"] = Json::Value(true);
    apsara_log["preserve"] = Json::Value(false);
    apsara_log["preserve_depth"] = Json::Value(1);
    apsara_log["topic_format"] = Json::Value("default");

    Json::Value rootJson, metrics;
    rootJson["apsara_log"] = apsara_log;
    metrics["metrics"] = rootJson;
    ofstream fout(STRING_FLAG(user_log_config).c_str());
    fout << metrics << endl;
    fout.close();

    CaseSetup();

    for (int i = 2; i < 5; ++i) {
        bfs::create_directories(bfs::path(mRootDir) / dirs[i]);
        sleep(1);
        //usleep(WRITE_LOG_SLEEP_INTERVAL);
    }

    auto eventDispatcher = EventDispatcher::GetInstance();
    // before update config, check the inotify and timeout?
    for (int i = 0; i < 5; i++) {
        unordered_map<std::string, int>::iterator pwItr = (eventDispatcher->mPathWdMap).find(mRootDir + dirs[i]);
        APSARA_TEST_TRUE_DESC((pwItr != eventDispatcher->mPathWdMap.end()) == isInotify[i],
                              "Inotify error:" + mRootDir + dirs[i]);
        if (pwItr != eventDispatcher->mPathWdMap.end()) {
            unordered_map<int, time_t>::iterator wuItr = eventDispatcher->mWdUpdateTimeMap.find(pwItr->second);
            APSARA_TEST_TRUE_DESC((wuItr != eventDispatcher->mWdUpdateTimeMap.end()) == isTimeout[i],
                                  "watchout error:" + mRootDir + dirs[i]);
        } else {
            LOG_INFO(sLogger, ("path not add into watch", mRootDir + dirs[i]));
        }
    }

    // then update config
    apsara_log["version"] = Json::Value(1);
    rootJson["apsara_log"] = apsara_log;
    metrics["metrics"] = rootJson;
    SetConfigResponse(metrics.toStyledString());

    // need a little time here to let config update
    sleep(WAIT_CONFIG_UPDATE_INTERVAL);

    //waiting for check root dir
    sleep(2 * INT32_FLAG(check_base_dir_interval) + 1);

    // after update config, check the inotify and timeout again
    for (int i = 0; i < 5; i++) {
        unordered_map<std::string, int>::iterator pwItr = (eventDispatcher->mPathWdMap).find(mRootDir + dirs[i]);
        APSARA_TEST_TRUE_DESC((pwItr != eventDispatcher->mPathWdMap.end()) == isInotify[i],
                              "Inotify error:" + mRootDir + dirs[i]);
        if (pwItr != eventDispatcher->mPathWdMap.end()) {
            unordered_map<int, time_t>::iterator wuItr = eventDispatcher->mWdUpdateTimeMap.find(pwItr->second);
            APSARA_TEST_TRUE_DESC((wuItr != eventDispatcher->mWdUpdateTimeMap.end()) == isTimeout[i],
                                  "watchout error:" + mRootDir + dirs[i]);
        } else {
            LOG_INFO(sLogger, ("path not add into watch", mRootDir + dirs[i]));
        }
    }

    CaseCleanup();
    RemoveConfigFile();
#endif

    LOG_INFO(sLogger, ("TestDirCheckPonit() end", time(NULL)));
}

void ConfigUpdatorUnittest::TestTimeoutCheckPoint() {
    bfs::remove_all(STRING_FLAG(check_point_filename));
    LOG_INFO(sLogger, ("TestTimeoutCheckPoint() begin", time(NULL)));
    INT32_FLAG(check_point_check_interval) = 30;

    string dirs[] = {PS + "dir/dir1",
                     PS + "dir/dir1/dir2_1",
                     PS + "dir/dir1/dir2_2",
                     PS + "dir/dir1/dir2_1/dir_3_1",
                     PS + "dir/dir1/dir2_2/dir_3_2"};


    bfs::create_directories(bfs::path(mRootDir) / "dir");
    for (int i = 0; i < 5; ++i) {
        bfs::create_directories(bfs::path(mRootDir) / dirs[i]);
    }

    Json::Value rootJson;

    // config --- log_path: /dir,  preserve: false, depth: 1
    Json::Value apsara_log;
    apsara_log["project_name"] = Json::Value("9000000_proj");
    apsara_log["category"] = Json::Value("9000000_category");
    apsara_log["log_type"] = Json::Value("apsara_log");
    apsara_log["log_begin_reg"] = Json::Value("\\[\\d+-\\d+-\\d+ \\d+:\\d+:\\d+.\\d+\\].*");
    apsara_log["log_path"] = Json::Value(mRootDir + PS + "dir");
    apsara_log["file_pattern"] = Json::Value("*.[Ll][Oo][Gg]");
    apsara_log["enable"] = Json::Value(true);
    apsara_log["preserve"] = Json::Value(true);
    apsara_log["topic_format"] = Json::Value("default");
    rootJson["apsara_log"] = apsara_log;

    Json::Value metrics;
    metrics["metrics"] = rootJson;
    ofstream fout(STRING_FLAG(user_log_config).c_str());
    fout << metrics << endl;
    fout.close();

    CaseSetup();

    for (int i = 0; i < 5; ++i) {
        DumpLog(10, mRootDir + dirs[i], APSARA_LOG);
    }

    sleep(2 * INT32_FLAG(batch_send_interval) + 2);

    // then update config
    apsara_log["version"] = Json::Value(1);
    rootJson["apsara_log"] = apsara_log;
    metrics["metrics"] = rootJson;
    SetConfigResponse(metrics.toStyledString());

    // need a little time here to let config update
    sleep(WAIT_CONFIG_UPDATE_INTERVAL);

    // logtail will readlog after add dir watch, then load checkpoint and clean it
    APSARA_TEST_EQUAL(CheckPointManager::Instance()->mDevInodeCheckPointPtrMap.size(), 0);

    CaseCleanup();
    RemoveConfigFile();
    LOG_INFO(sLogger, ("TestTimeoutCheckPoint() end", time(NULL)));
}

void ConfigUpdatorUnittest::TestCheckPointManager() {
    bfs::remove_all(STRING_FLAG(check_point_filename));
    LOG_INFO(sLogger, ("TestCheckPointManager() begin", time(NULL)));

    string filenames[] = {PS + "tmp" + PS + "apsara" + PS + "log.log.1",
                          "tmp" + PS + "apsara.log.2",
                          PS + "apsara" + PS + "tmp" + PS + "log.3",
                          PS + "apsara" + PS + "tmp" + PS + "log.3"};
    int64_t offsets[] = {100, 10000, 200000, 200000};
    uint64_t devs[] = {34, 56, 98, 98};
    uint64_t inodes[] = {100, 2006, 3000008, 3000008};
    string configs[] = {"config1", "config_2006", "3000008config", "4000008config"};
    string sigs[] = {"jroeijgperieorwpqijpgegu",
                     "jrpehuwklxcnvujoakckghrueioxckllxjgeuin \n\t",
                     "jfuir\tjgpdg\ndfjkoepj\njfpsdfgdpgqe\tfdjksi",
                     "jfuir\tjgpdg\ndfjkoepj\njfpsdfgdpgqe\tfdjksi"};
    uint64_t sigHashs[] = {0, 0, 0, 0};
    uint32_t sigSizes[] = {0, 0, 0, 0};

    for (size_t i = 0; i < sizeof(sigs) / sizeof(string); ++i) {
        SignatureToHash(sigs[i], sigHashs[i], sigSizes[i]);
    }
    string dirs[] = {PS + "tmp" + PS + "1" + PS + "2" + PS + "3" + PS + "4" + PS + "5",
                     PS + "tmp" + PS + "1" + PS + "2" + PS + "3" + PS + "5",
                     PS + "tmp" + PS + "1" + PS + "2" + PS + "3" + PS + "4" + PS,
                     PS + "tmp" + PS + "1" + PS + "3"};
    string pdirs[] = {PS + "tmp" + PS + "1" + PS + "2" + PS + "3" + PS + "4",
                      PS + "tmp" + PS + "1" + PS + "2" + PS + "3",
                      PS + "tmp" + PS + "1"};
    auto checkpointManager = CheckPointManager::Instance();
    for (int i = 0; i < 4; ++i) {
        checkpointManager->AddDirCheckPoint(dirs[i]);
    }
    //timeout dircheckpoint
    (checkpointManager->mDirNameMap[pdirs[0]])->mUpdateTime -= INT32_FLAG(file_check_point_time_out) + 10;
    (checkpointManager->mDirNameMap[pdirs[2]])->mUpdateTime -= INT32_FLAG(file_check_point_time_out) + 5;

    checkpointManager->DumpCheckPointToLocal();


    // case 0 : delete dircheckpoint
    for (int i = 0; i < 3; ++i) {
        checkpointManager->DeleteDirCheckPoint(pdirs[i]);
    }
    for (int i = 0; i < 3; ++i) {
        DirCheckPointPtr ptr;
        APSARA_TEST_EQUAL(checkpointManager->GetDirCheckPoint(pdirs[i], ptr), false);
    }

    // case 1 : load dircheckpoint
    LOG_INFO(sLogger, ("check point file", STRING_FLAG(check_point_filename)));
    checkpointManager->LoadCheckPoint();
    for (int i = 0; i < 3; ++i) {
        DirCheckPointPtr ptr;
        if (i != 1) {
            APSARA_TEST_EQUAL(checkpointManager->GetDirCheckPoint(pdirs[0], ptr), false);
        } else {
            APSARA_TEST_EQUAL_FATAL(checkpointManager->GetDirCheckPoint(pdirs[i], ptr), true);
            APSARA_TEST_EQUAL(ptr.get()->mSubDir.size(), 2);
        }
    }

    // case 2 : add filecheckpoint
    for (int i = 0; i < 4; ++i) {
        CheckPoint* checkPointPtr = new CheckPoint(
            filenames[i], offsets[i], sigSizes[i], sigHashs[i], DevInode(devs[i], inodes[i]), configs[i]);
        checkpointManager->AddCheckPoint(checkPointPtr);
    }
    // timeout checkpoint
    (checkpointManager
         ->mDevInodeCheckPointPtrMap[CheckPointManager::CheckPointKey(DevInode(devs[0], inodes[0]), configs[0])])
        ->mLastUpdateTime
        -= INT32_FLAG(mem_check_point_time_out) + 10;

    for (int i = 3; i >= 0; --i) {
        CheckPointPtr checkPointSharePtr;
        bool result = checkpointManager->GetCheckPoint(DevInode(devs[i], inodes[i]), configs[i], checkPointSharePtr);
        APSARA_TEST_EQUAL(result, true);
        CheckPoint* checkPointPtr = checkPointSharePtr.get();
        APSARA_TEST_EQUAL(checkPointPtr->mFileName, filenames[i]);
        APSARA_TEST_EQUAL(checkPointPtr->mSignatureHash, sigHashs[i]);
        APSARA_TEST_EQUAL(checkPointPtr->mSignatureSize, sigSizes[i]);
        APSARA_TEST_EQUAL(checkPointPtr->mOffset, offsets[i]);
        APSARA_TEST_EQUAL(checkPointPtr->mDevInode.inode, inodes[i]);
        APSARA_TEST_EQUAL(checkPointPtr->mDevInode.dev, devs[i]);
        APSARA_TEST_EQUAL(checkPointPtr->mConfigName, configs[i]);
    }
    checkpointManager->DumpCheckPointToLocal();

    // case 3 : delete filecheckpoint
    for (int i = 0; i < 4; ++i) {
        CheckPointPtr checkPointSharePtr;
        checkpointManager->DeleteCheckPoint(DevInode(devs[i], inodes[i]), configs[i]);
        bool result = checkpointManager->GetCheckPoint(DevInode(devs[i], inodes[i]), configs[i], checkPointSharePtr);
        APSARA_TEST_EQUAL(result, false);
    }

    // case 4 : load filecheckpoint
    checkpointManager->LoadCheckPoint();
    for (int i = 3; i >= 0; --i) {
        CheckPointPtr checkPointSharePtr;
        bool result = checkpointManager->GetCheckPoint(DevInode(devs[i], inodes[i]), configs[i], checkPointSharePtr);
        APSARA_TEST_EQUAL(result, true);
        if (result == false)
            continue;
        CheckPoint* checkPointPtr = checkPointSharePtr.get();
        APSARA_TEST_EQUAL(checkPointPtr->mFileName, filenames[i]);
        APSARA_TEST_EQUAL(checkPointPtr->mSignatureHash, sigHashs[i]);
        APSARA_TEST_EQUAL(checkPointPtr->mSignatureSize, sigSizes[i]);
        APSARA_TEST_EQUAL(checkPointPtr->mOffset, offsets[i]);
        APSARA_TEST_EQUAL(checkPointPtr->mDevInode.inode, inodes[i]);
        APSARA_TEST_EQUAL(checkPointPtr->mDevInode.dev, devs[i]);
        APSARA_TEST_EQUAL(checkPointPtr->mConfigName, configs[i]);
        checkpointManager->DeleteCheckPoint(DevInode(devs[i], inodes[i]), configs[i]);
        result = checkpointManager->GetCheckPoint(DevInode(devs[i], inodes[i]), configs[i], checkPointSharePtr);
        APSARA_TEST_EQUAL(result, false);
    }

    LOG_INFO(sLogger, ("TestCheckPointManager() end", time(NULL)));
}

void ConfigUpdatorUnittest::DumpInitConfigToLocal(string localPath) {
    Json::Value rootJson;
    Json::Value commonreg_com;
    commonreg_com["project_name"] = Json::Value("1000000_proj");
    commonreg_com["category"] = Json::Value("1000000_cateogry");
    commonreg_com["log_type"] = Json::Value("common_reg_log");
    commonreg_com["log_path"] = Json::Value(mRootDir + PATH_SEPARATOR + "comm");
    SetConfigFilePattern(commonreg_com);
    commonreg_com["enable"] = Json::Value(true);
    commonreg_com["timeformat"] = Json::Value("%d/%b/%Y:%H:%M:%S");
    commonreg_com["topic_format"] = Json::Value("none");

    Json::Value customizedFieldsValue, dataIntegrityValue, lineCountValue;
    dataIntegrityValue["switch"] = Json::Value(false);
    dataIntegrityValue["project_name"] = Json::Value("data_integrity");
    dataIntegrityValue["logstore"] = Json::Value("data_integrity");
    dataIntegrityValue["log_time_reg"] = Json::Value("([0-9]{4})-(0[0-9]{1}|1[0-2])-(0[0-9]{1}|[12][0-9]{1}|3[01]) "
                                                     "(0[0-9]{1}|1[0-9]{1}|2[0-3]):[0-5][0-9]{1}:([0-5][0-9]{1})");
    dataIntegrityValue["time_pos"] = Json::Value(0);
    customizedFieldsValue["data_integrity"] = dataIntegrityValue;

    lineCountValue["switch"] = Json::Value(false);
    lineCountValue["project_name"] = Json::Value("line_count");
    lineCountValue["logstore"] = Json::Value("line_count");
    customizedFieldsValue["line_count"] = lineCountValue;

    //customizedFieldsValue["fuse_mode"] = Json::Value(true);

    commonreg_com["customized_fields"] = customizedFieldsValue;

    Json::Value regs;
    regs.append(Json::Value("([\\d\\.]+) \\S+ \\S+ \\[(\\S+) \\S+\\] \\\"(\\w+) ([^\"]*)\\\" (\\d+) (\\d+|-)"));
    regs.append(Json::Value("([\\d\\.]+) \\S+ \\S+ \\[(\\S+) \\S+\\] \\\"(\\w+) ([^\"]*)\\\" (\\d+) (\\d+|-) "
                            "\\\"([^\"]*)\\\" \\\"([^\"]*)\\\""));
    Json::Value keys;
    keys.append(Json::Value("ip,time,method,url,status,length"));
    keys.append(Json::Value("ip,time,method,url,status,length,ref_url,browser"));
    commonreg_com["regex"] = regs;
    commonreg_com["keys"] = keys;
    commonreg_com["preserve"] = Json::Value(true);
    rootJson["commonreg.com"] = commonreg_com;

    Json::Value apsara_log;
    apsara_log["project_name"] = Json::Value("8000000_proj");
    apsara_log["category"] = Json::Value("8000000_category");
    apsara_log["log_type"] = Json::Value("apsara_log");
    apsara_log["log_begin_reg"] = Json::Value("\\[\\d+-\\d+-\\d+ \\d+:\\d+:\\d+.\\d+\\].*");
    apsara_log["log_path"] = Json::Value(mRootDir + PATH_SEPARATOR + "apsara_log");
    SetConfigFilePattern(apsara_log);
    apsara_log["enable"] = Json::Value(true);
    apsara_log["preserve"] = Json::Value(true);
    apsara_log["topic_format"] = Json::Value("none");
    rootJson["apsara_log"] = apsara_log;

    Json::Value metrics;
    metrics["metrics"] = rootJson;
    if (localPath.empty()) {
        ofstream fout(STRING_FLAG(user_log_config).c_str());
        fout << metrics << endl;
        fout.close();
    } else {
        ofstream fout(localPath.c_str());
        fout << metrics << endl;
        fout.close();
    }
}

void ConfigUpdatorUnittest::SetupGlobalFuseMode(bool globalFuseMode) {
    Json::Value logtailConfig;
    logtailConfig["global_fuse_mode"] = Json::Value(globalFuseMode);

    ofstream fout(STRING_FLAG(ilogtail_config).c_str());
    fout << logtailConfig.toStyledString() << endl;
    fout.close();
}

void ConfigUpdatorUnittest::DumpCustomizedConfigToLocal(bool check_ulogfs_env, bool fuse_mode) {
    Json::Value rootJson;
    Json::Value commonreg_com;
    commonreg_com["project_name"] = Json::Value("1000000_proj");
    commonreg_com["category"] = Json::Value("1000000_cateogry");
    commonreg_com["log_type"] = Json::Value("common_reg_log");
    commonreg_com["log_path"] = Json::Value(mRootDir + "/comm");
    commonreg_com["file_pattern"] = Json::Value("*.[Ll][Oo][Gg]");
    commonreg_com["enable"] = Json::Value(true);
    commonreg_com["timeformat"] = Json::Value("%d/%b/%Y:%H:%M:%S");
    commonreg_com["topic_format"] = Json::Value("none");

    Json::Value customizedFieldsValue;
    customizedFieldsValue["check_ulogfs_env"] = Json::Value(check_ulogfs_env);
    customizedFieldsValue["fuse_mode"] = Json::Value(fuse_mode);
    commonreg_com["customized_fields"] = customizedFieldsValue;

    Json::Value regs;
    regs.append(Json::Value("([\\d\\.]+) \\S+ \\S+ \\[(\\S+) \\S+\\] \\\"(\\w+) ([^\"]*)\\\" (\\d+) (\\d+|-)"));
    regs.append(Json::Value("([\\d\\.]+) \\S+ \\S+ \\[(\\S+) \\S+\\] \\\"(\\w+) ([^\"]*)\\\" (\\d+) (\\d+|-) "
                            "\\\"([^\"]*)\\\" \\\"([^\"]*)\\\""));
    Json::Value keys;
    keys.append(Json::Value("ip,time,method,url,status,length"));
    keys.append(Json::Value("ip,time,method,url,status,length,ref_url,browser"));
    commonreg_com["regex"] = regs;
    commonreg_com["keys"] = keys;
    commonreg_com["preserve"] = Json::Value(true);
    rootJson["commonreg.com"] = commonreg_com;

    Json::Value metrics;
    metrics["metrics"] = rootJson;

    ofstream fout(STRING_FLAG(user_log_config).c_str());
    fout << metrics << endl;
    fout.close();
}

void ConfigUpdatorUnittest::CaseSetup(bool replaceConfigAllowed) {
    ClearConfigResponse();
    // set up container mode test
    bool container_mode = CheckExistance("LogtailContainerModeTest");
    if (container_mode && replaceConfigAllowed) {
        cout << "replace with container config" << endl;
        ReplaceWithContainerModeConfig();
    }
    AppConfig::GetInstance()->LoadAppConfig(STRING_FLAG(ilogtail_config));

    bool ret = ConfigManager::GetInstance()->LoadConfig(STRING_FLAG(user_log_config));
    ASSERT_TRUE(ret);

    ret = LogFilter::Instance()->InitFilter(STRING_FLAG(user_log_config));
    ASSERT_TRUE(ret);
    ConfigManager::GetInstance()->mRegionType = REGION_CORP;

    Sender::Instance()->InitSender();
    Sender::Instance()->MockAsyncSend = MockAsyncSend;
    vector<string> filesToSend;
    Sender::Instance()->LoadFileToSend(time(NULL), filesToSend);
    for (size_t i = 0; i < filesToSend.size(); ++i)
        remove((Sender::Instance()->mBufferFilePath + filesToSend[i]).c_str());

    ConfigManager::GetInstance()->mThreadIsRunning = true;
    ConfigManager::GetInstance()->InitUpdateConfig(true);
    STRING_FLAG(local_machine_uuid) = LOCAL_UUID;
    SetConfigResponse("{}");
    sProjectNameCountMap.clear();
    sProjectCategoryTopicCountMap.clear();
    sTopicCountMap.clear();
    sRegionResponseMap.clear();
    sConfigRejectSet.clear();
    sRejectCount = 0;

    gDispatchThreadId.reset(new std::thread(RunningDispatcher));
    sleep(1);
}

void ConfigUpdatorUnittest::CaseCleanup() {
    LogInput::GetInstance()->CleanEnviroments();
    ConfigManager::GetInstance()->mThreadIsRunning = false;
    EventDispatcher::GetInstance()->CleanEnviroments();
    ConfigManager::GetInstance()->CleanEnviroments();
    Sender::Instance()->RemoveSender();
    bfs::remove_all(mRootDir);
    bfs::remove("ilogtail_config.json");

    gDispatchThreadId->join();
    gDispatchThreadId = nullptr;
}

void ConfigUpdatorUnittest::RemoveConfigFile() {
    string userLogConfig = STRING_FLAG(user_log_config);
    bfs::remove_all(userLogConfig);
    bfs::remove_all(AppConfig::GetInstance()->GetLocalUserConfigPath());
    bfs::remove_all(AppConfig::GetInstance()->GetLocalUserConfigDirPath());
}

void ConfigUpdatorUnittest::TestLogRotateWhenUpdate() {
    LOG_INFO(sLogger, ("TestLogRotateWhenUpdate() begin", time(NULL)));
    bfs::remove_all(mRootDir);
    bfs::create_directories(bfs::path(mRootDir) / "apsara_log");
    bfs::create_directories(bfs::path(mRootDir) / "comm");
    DumpInitConfigToLocal();
    CaseSetup();
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 2; ++j)
            DumpLog(10, mRootDir + PATH_SEPARATOR + gPath[j], (LogType)j);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    WaitForFileBeenRead();
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);

    // check config update
    Json::Value rootJson;
    Json::Value commonreg_com;
    commonreg_com["project_name"] = Json::Value("2000000_proj");
    commonreg_com["category"] = Json::Value("2000000_category");
    commonreg_com["log_type"] = Json::Value("common_reg_log");
    commonreg_com["log_path"] = Json::Value(mRootDir + PATH_SEPARATOR + "comm");
    SetConfigFilePattern(commonreg_com);
    commonreg_com["enable"] = Json::Value(true);
    commonreg_com["timeformat"] = Json::Value("%d/%b/%Y:%H:%M:%S");
    Json::Value regs;
    regs.append(Json::Value("([\\d\\.]+) \\S+ \\S+ \\[(\\S+) \\S+\\] \\\"(\\w+) ([^\"]*)\\\" (\\d+) (\\d+|-)"));
    regs.append(Json::Value("([\\d\\.]+) \\S+ \\S+ \\[(\\S+) \\S+\\] \\\"(\\w+) ([^\"]*)\\\" (\\d+) (\\d+|-) "
                            "\\\"([^\"]*)\\\" \\\"([^\"]*)\\\""));
    Json::Value keys;
    keys.append(Json::Value("ip,time,method,url,status,length"));
    keys.append(Json::Value("ip,time,method,url,status,length,ref_url,browser"));
    commonreg_com["regex"] = regs;
    commonreg_com["keys"] = keys;
    commonreg_com["preserve"] = Json::Value(true);
    commonreg_com["version"] = Json::Value(1);
    rootJson["commonreg.com"] = commonreg_com;

    Json::Value apsara_log;
    apsara_log["project_name"] = Json::Value("8000000_proj");
    apsara_log["category"] = Json::Value("8000000_category");
    apsara_log["log_type"] = Json::Value("apsara_log");
    apsara_log["log_begin_reg"] = Json::Value("\\[\\d+-\\d+-\\d+ \\d+:\\d+:\\d+.\\d+\\].*");
    apsara_log["log_path"] = Json::Value(mRootDir + PATH_SEPARATOR + "apsara_log");
    SetConfigFilePattern(apsara_log);
    apsara_log["enable"] = Json::Value(true);
    apsara_log["preserve"] = Json::Value(false);
    apsara_log["preserve_depth"] = Json::Value(2);
    apsara_log["version"] = Json::Value(1);
    rootJson["apsara_log"] = apsara_log;

    Json::Value metrics;
    metrics["metrics"] = rootJson;

    APSARA_TEST_EQUAL(sProjectNameCountMap["1000000_proj"], 100);
    APSARA_TEST_EQUAL(sProjectNameCountMap["8000000_proj"], 100);

    // Move files to parent directory and append more data into them, then move them back (with
    // suffix .1).
    // When logtail updates configuration, it will hold on and resume LogInput, and when LogInput
    // is resuming, it will try to load checkpoints, at this time, it will find that the old file
    // has been renamed to file with suffix (according to their same devInode and signature).
    // Therefore, after resuming, it will skip the first 100 lines in log file.
    for (int j = 0; j < 2; ++j) {
        bfs::rename(bfs::path(mRootDir) / gPath[j] / "job.log",
                    bfs::path(mRootDir) / (std::string("job.log.1") + std::to_string(j)));
    }
    // Wait until Logtail finds that files are removed.
    sleep(1);
#if defined(_MSC_VER)
    sleep(30); // Windows polling needs more time.
#endif

    // mock process stop and log write -> rotate by rotate -> write
    for (int i = 0; i < 20; ++i) {
        for (int j = 0; j < 2; ++j)
            DumpLog(10, mRootDir, (LogType)j, string("job.log.1") + ToString(j));
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    for (int j = 0; j < 2; ++j) {
        bfs::rename(bfs::path(mRootDir) / (std::string("job.log.1") + std::to_string(j)),
                    bfs::path(mRootDir) / gPath[j] / "job.log.1");
    }
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);
    APSARA_TEST_EQUAL(sProjectNameCountMap["1000000_proj"], 100);
    APSARA_TEST_EQUAL(sProjectNameCountMap["8000000_proj"], 100);
    // Update config to trigger hold on and resume in LogInput.
    SetConfigResponse(metrics.toStyledString());
    sleep(WAIT_CONFIG_UPDATE_INTERVAL);
    WaitForFileBeenRead();
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);

    unordered_map<string, Config*>& configMap = ConfigManager::GetInstance()->mNameConfigMap;
    unordered_map<string, Config*>::iterator it = configMap.find("commonreg.com");
    APSARA_TEST_TRUE(it != configMap.end());
    Config* config = it->second;
    APSARA_TEST_EQUAL(config->mProjectName, "2000000_proj");
    APSARA_TEST_EQUAL(config->mBasePath, mRootDir + PATH_SEPARATOR + "comm");
    APSARA_TEST_EQUAL(config->mVersion, 1);
    APSARA_TEST_EQUAL(config->mLogType, REGEX_LOG);

    it = configMap.find("apsara_log");
    APSARA_TEST_TRUE(it != configMap.end());
    config = it->second;
    APSARA_TEST_EQUAL(config->mProjectName, "8000000_proj");
    APSARA_TEST_EQUAL(config->mBasePath, mRootDir + PATH_SEPARATOR + "apsara_log");
    APSARA_TEST_EQUAL(config->mVersion, 1);
    APSARA_TEST_EQUAL(config->mLogType, APSARA_LOG);
    APSARA_TEST_EQUAL(config->mIsPreserve, false);

    APSARA_TEST_EQUAL(configMap.size(), 2);
    APSARA_TEST_EQUAL(sProjectNameCountMap["2000000_proj"] + sProjectNameCountMap["1000000_proj"], 300);
    APSARA_TEST_EQUAL(sProjectNameCountMap["8000000_proj"], 300);

    //test add config
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->IsRegistered((mRootDir + PATH_SEPARATOR + "comm").c_str()));
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->IsRegistered((mRootDir + PATH_SEPARATOR + "apsara_log").c_str()));
    APSARA_TEST_TRUE(
        !EventDispatcher::GetInstance()->IsRegistered((mRootDir + PATH_SEPARATOR + "apsara_log1").c_str()));

    CaseCleanup();
    RemoveConfigFile();

    LOG_INFO(sLogger, ("TestLogRotateWhenUpdate() end", time(NULL)));
}

void ConfigUpdatorUnittest::TestConfigUpdate() {
    LOG_INFO(sLogger, ("TestConfigUpdate() begin", time(NULL)));

    bfs::remove_all(mRootDir);
    bfs::create_directories(bfs::path(mRootDir) / "apsara_log");
    bfs::create_directories(bfs::path(mRootDir) / "comm");
    DumpInitConfigToLocal();
    CaseSetup();
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 2; ++j)
            DumpLog(10, mRootDir + PATH_SEPARATOR + gPath[j], (LogType)j);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    WaitForFileBeenRead();
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);

    // check config update
    Json::Value rootJson;
    Json::Value commonreg_com;
    commonreg_com["project_name"] = Json::Value("2000000_proj");
    commonreg_com["category"] = Json::Value("2000000_category");
    commonreg_com["log_type"] = Json::Value("common_reg_log");
    commonreg_com["log_path"] = Json::Value(mRootDir + PATH_SEPARATOR + "comm");
    SetConfigFilePattern(commonreg_com);
    commonreg_com["enable"] = Json::Value(true);
    commonreg_com["timeformat"] = Json::Value("%d/%b/%Y:%H:%M:%S");
    Json::Value regs;
    regs.append(Json::Value("([\\d\\.]+) \\S+ \\S+ \\[(\\S+) \\S+\\] \\\"(\\w+) ([^\"]*)\\\" (\\d+) (\\d+|-)"));
    regs.append(Json::Value("([\\d\\.]+) \\S+ \\S+ \\[(\\S+) \\S+\\] \\\"(\\w+) ([^\"]*)\\\" (\\d+) (\\d+|-) "
                            "\\\"([^\"]*)\\\" \\\"([^\"]*)\\\""));
    Json::Value keys;
    keys.append(Json::Value("ip,time,method,url,status,length"));
    keys.append(Json::Value("ip,time,method,url,status,length,ref_url,browser"));
    commonreg_com["regex"] = regs;
    commonreg_com["keys"] = keys;
    commonreg_com["max_depth"] = Json::Value(0);
    commonreg_com["preserve"] = Json::Value(true);
    commonreg_com["version"] = Json::Value(1);
    rootJson["commonreg.com"] = commonreg_com;

    Json::Value apsara_log;
    apsara_log["project_name"] = Json::Value("8000000_proj");
    apsara_log["category"] = Json::Value("8000000_category");
    apsara_log["log_type"] = Json::Value("apsara_log");
    apsara_log["log_begin_reg"] = Json::Value("\\[\\d+-\\d+-\\d+ \\d+:\\d+:\\d+.\\d+\\].*");
    apsara_log["log_path"] = Json::Value(mRootDir + PATH_SEPARATOR + "apsara_log");
    SetConfigFilePattern(apsara_log);
    apsara_log["enable"] = Json::Value(true);
    apsara_log["preserve"] = Json::Value(false);
    apsara_log["preserve_depth"] = Json::Value(2);
    apsara_log["version"] = Json::Value(1);
    rootJson["apsara_log"] = apsara_log;

    Json::Value metrics;
    metrics["metrics"] = rootJson;

    SetConfigResponse(metrics.toStyledString());
    sleep(WAIT_CONFIG_UPDATE_INTERVAL);
    for (int i = 0; i < 20; ++i) {
        for (int j = 0; j < 2; ++j)
            DumpLog(10, mRootDir + PATH_SEPARATOR + gPath[j], (LogType)j);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    WaitForFileBeenRead();
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);

    unordered_map<string, Config*>& configMap = ConfigManager::GetInstance()->mNameConfigMap;
    unordered_map<string, Config*>::iterator it = configMap.find("commonreg.com");
    APSARA_TEST_TRUE(it != configMap.end());
    Config* config = it->second;
    APSARA_TEST_EQUAL(config->mProjectName, "2000000_proj");
    APSARA_TEST_EQUAL(config->mBasePath, mRootDir + PATH_SEPARATOR + "comm");
    APSARA_TEST_EQUAL(config->mVersion, 1);
    APSARA_TEST_EQUAL(config->mLogType, REGEX_LOG);

    it = configMap.find("apsara_log");
    APSARA_TEST_TRUE(it != configMap.end());
    config = it->second;
    APSARA_TEST_EQUAL(config->mProjectName, "8000000_proj");
    APSARA_TEST_EQUAL(config->mBasePath, mRootDir + PATH_SEPARATOR + "apsara_log");
    APSARA_TEST_EQUAL(config->mVersion, 1);
    APSARA_TEST_EQUAL(config->mLogType, APSARA_LOG);
    APSARA_TEST_EQUAL(config->mIsPreserve, false);

    APSARA_TEST_EQUAL(configMap.size(), 2);
    APSARA_TEST_EQUAL(sProjectNameCountMap["2000000_proj"] + sProjectNameCountMap["1000000_proj"], 300);
    APSARA_TEST_EQUAL(sProjectNameCountMap["8000000_proj"], 300);

    //test add config

    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->IsRegistered((mRootDir + PATH_SEPARATOR + "comm").c_str()));
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->IsRegistered((mRootDir + PATH_SEPARATOR + "apsara_log").c_str()));
    APSARA_TEST_TRUE(
        !EventDispatcher::GetInstance()->IsRegistered((mRootDir + PATH_SEPARATOR + "apsara_log1").c_str()));


    SetConfigResponse("{}");
    apsara_log["project_name"] = Json::Value("3000000_proj");
    apsara_log["category"] = Json::Value("3000000_category");
    apsara_log["log_type"] = Json::Value("apsara_log");
    apsara_log["log_begin_reg"] = Json::Value("\\[\\d+-\\d+-\\d+ \\d+:\\d+:\\d+.\\d+\\].*");
    apsara_log["log_path"] = Json::Value(mRootDir + PATH_SEPARATOR + "apsara_log1");
    SetConfigFilePattern(apsara_log);
    apsara_log["enable"] = Json::Value(true);
    apsara_log["preserve"] = Json::Value(true);
    commonreg_com["version"] = Json::Value(1);
    Json::Value rootJson1;
    rootJson1["apsara_log1"] = apsara_log;

    Json::Value metrics1;
    metrics1["metrics"] = rootJson1;

    bfs::create_directories(bfs::path(mRootDir) / "apsara_log1");
    SetConfigResponse(metrics1.toStyledString());
    sleep(WAIT_CONFIG_UPDATE_INTERVAL);
    for (int i = 0; i < 20; ++i) {
        DumpLog(10, mRootDir + PATH_SEPARATOR + "apsara_log1", APSARA_LOG);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    WaitForFileBeenRead();
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);
    it = configMap.find("apsara_log1");
    APSARA_TEST_EQUAL(it != configMap.end(), true);
    if (it == configMap.end())
        return;
    config = it->second;
    APSARA_TEST_EQUAL(config->mProjectName, "3000000_proj");
    APSARA_TEST_EQUAL(config->mLogType, APSARA_LOG);
    APSARA_TEST_EQUAL(config->mBasePath, mRootDir + PATH_SEPARATOR + "apsara_log1");
    APSARA_TEST_EQUAL(config->mVersion, 1);

    APSARA_TEST_EQUAL(sProjectNameCountMap["3000000_proj"], 200);


    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->IsRegistered((mRootDir + PATH_SEPARATOR + "comm").c_str()));
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->IsRegistered((mRootDir + PATH_SEPARATOR + "apsara_log").c_str()));
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->IsRegistered((mRootDir + PATH_SEPARATOR + "apsara_log1").c_str()));


    DirCheckPointPtr dirCheckPointPtr;
    CheckPointPtr checkPointPtr;
    CheckPointManager* pCheckPointManager = CheckPointManager::Instance();

    //APSARA_TEST_TRUE(pCheckPointManager->GetCheckPoint(mRootDir + "/comm/job.log", checkPointPtr));
    //APSARA_TEST_TRUE(pCheckPointManager->GetCheckPoint(mRootDir + "/apsara_log/job.log", checkPointPtr));

    APSARA_TEST_TRUE(pCheckPointManager->GetDirCheckPoint(mRootDir, dirCheckPointPtr));
    if (dirCheckPointPtr.get() != NULL) {
        APSARA_TEST_TRUE(dirCheckPointPtr->mSubDir.find(mRootDir + PATH_SEPARATOR + "comm")
                         != dirCheckPointPtr->mSubDir.end());
        APSARA_TEST_TRUE(dirCheckPointPtr->mSubDir.find(mRootDir + PATH_SEPARATOR + "apsara_log")
                         != dirCheckPointPtr->mSubDir.end());
    }

    //pCheckPointManager->PrintStatus();

    //test delete config
    SetConfigResponse("{}");
    apsara_log = Json::Value();
    apsara_log["project_name"] = Json::Value("8000000_proj");
    apsara_log["enable"] = Json::Value(true);
    apsara_log["version"] = Json::Value(-1);
    Json::Value rootJson2;
    rootJson2["apsara_log"] = apsara_log;

    Json::Value metrics2;
    metrics2["metrics"] = rootJson2;

    SetConfigResponse(metrics2.toStyledString());
    sProjectNameCountMap["8000000_proj"] = 0;
    sleep(WAIT_CONFIG_UPDATE_INTERVAL);
    for (int i = 0; i < 20; ++i) {
        DumpLog(10, mRootDir + PATH_SEPARATOR + "apsara_log", APSARA_LOG);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    WaitForFileBeenRead();
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);

    APSARA_TEST_EQUAL(sProjectNameCountMap["8000000_proj"], 0);
    APSARA_TEST_TRUE(configMap.find("apsara_log") == configMap.end());


    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->IsRegistered((mRootDir + PATH_SEPARATOR + "comm").c_str()));
    APSARA_TEST_TRUE(!EventDispatcher::GetInstance()->IsRegistered((mRootDir + PATH_SEPARATOR + "apsara_log").c_str()));
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->IsRegistered((mRootDir + PATH_SEPARATOR + "apsara_log1").c_str()));


    //APSARA_TEST_TRUE(pCheckPointManager->GetCheckPoint(mRootDir + "/comm/job.log", checkPointPtr));
    //APSARA_TEST_TRUE(!pCheckPointManager->GetCheckPoint(mRootDir + "/apsara_log/job.log", checkPointPtr));
    //APSARA_TEST_TRUE(pCheckPointManager->GetCheckPoint(mRootDir + "/apsara_log1/job.log", checkPointPtr));

    APSARA_TEST_TRUE(pCheckPointManager->GetDirCheckPoint(mRootDir, dirCheckPointPtr));
    if (dirCheckPointPtr.get() != NULL) {
        APSARA_TEST_TRUE(dirCheckPointPtr->mSubDir.find(mRootDir + PATH_SEPARATOR + "comm")
                         != dirCheckPointPtr->mSubDir.end());
        APSARA_TEST_TRUE(dirCheckPointPtr->mSubDir.find(mRootDir + PATH_SEPARATOR + "apsara_log")
                         != dirCheckPointPtr->mSubDir.end());
        APSARA_TEST_TRUE(dirCheckPointPtr->mSubDir.find(mRootDir + PATH_SEPARATOR + "apsara_log1")
                         != dirCheckPointPtr->mSubDir.end());
    }


    pCheckPointManager->RemoveAllCheckPoint();
    pCheckPointManager->LoadCheckPoint();

    DevInode devInode1 = GetFileDevInode(mRootDir + PATH_SEPARATOR + "comm" + PATH_SEPARATOR + "job.log");
    DevInode devInode2 = GetFileDevInode(mRootDir + PATH_SEPARATOR + "apsara_log" + PATH_SEPARATOR + "job.log");
    DevInode devInode3 = GetFileDevInode(mRootDir + PATH_SEPARATOR + "apsara_log1" + PATH_SEPARATOR + "job.log");
    APSARA_TEST_TRUE(pCheckPointManager->GetCheckPoint(devInode1, "commonreg.com", checkPointPtr));
    APSARA_TEST_TRUE(!pCheckPointManager->GetCheckPoint(devInode2, "apsara_log", checkPointPtr));
    APSARA_TEST_TRUE(pCheckPointManager->GetCheckPoint(devInode3, "apsara_log1", checkPointPtr));

    APSARA_TEST_TRUE(pCheckPointManager->GetDirCheckPoint(mRootDir, dirCheckPointPtr));
    if (dirCheckPointPtr.get() != NULL) {
        APSARA_TEST_TRUE(dirCheckPointPtr->mSubDir.find(mRootDir + PATH_SEPARATOR + "comm")
                         != dirCheckPointPtr->mSubDir.end());
        APSARA_TEST_TRUE(dirCheckPointPtr->mSubDir.find(mRootDir + PATH_SEPARATOR + "apsara_log")
                         != dirCheckPointPtr->mSubDir.end());
        APSARA_TEST_TRUE(dirCheckPointPtr->mSubDir.find(mRootDir + PATH_SEPARATOR + "apsara_log1")
                         != dirCheckPointPtr->mSubDir.end());
    }

    CaseCleanup();
    RemoveConfigFile();
    LOG_INFO(sLogger, ("TestConfigUpdate() end", time(NULL)));
}

void ConfigUpdatorUnittest::TestLocalConfigUpdate() {
    LOG_INFO(sLogger, ("TestLocalConfigUpdate() begin", time(NULL)));
    bfs::remove_all(mRootDir);
    bfs::create_directories(bfs::path(mRootDir) / "apsara_log");
    bfs::create_directories(bfs::path(mRootDir) / "comm");
    DumpInitConfigToLocal(AppConfig::GetInstance()->GetLocalUserConfigPath());
    DumpInitConfigToLocal();
    CaseSetup();

    LOG_INFO(sLogger, ("Write data with initialized config", ""));
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 2; ++j)
            DumpLog(10, mRootDir + PATH_SEPARATOR + gPath[j], (LogType)j);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    LOG_INFO(sLogger, ("Write data with initialized config", "done"));
    WaitForFileBeenRead();
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);

    // check config update
    Json::Value rootJson;
    Json::Value commonreg_com;
    commonreg_com["project_name"] = Json::Value("2000000_proj");
    commonreg_com["category"] = Json::Value("2000000_category");
    commonreg_com["log_type"] = Json::Value("common_reg_log");
    commonreg_com["log_path"] = Json::Value(mRootDir + PATH_SEPARATOR + "comm");
    SetConfigFilePattern(commonreg_com);
    commonreg_com["enable"] = Json::Value(true);
    commonreg_com["timeformat"] = Json::Value("%d/%b/%Y:%H:%M:%S");
    Json::Value regs;
    regs.append(Json::Value("([\\d\\.]+) \\S+ \\S+ \\[(\\S+) \\S+\\] \\\"(\\w+) ([^\"]*)\\\" (\\d+) (\\d+|-)"));
    regs.append(Json::Value("([\\d\\.]+) \\S+ \\S+ \\[(\\S+) \\S+\\] \\\"(\\w+) ([^\"]*)\\\" (\\d+) (\\d+|-) "
                            "\\\"([^\"]*)\\\" \\\"([^\"]*)\\\""));
    Json::Value keys;
    keys.append(Json::Value("ip,time,method,url,status,length"));
    keys.append(Json::Value("ip,time,method,url,status,length,ref_url,browser"));
    commonreg_com["regex"] = regs;
    commonreg_com["keys"] = keys;
    commonreg_com["max_depth"] = Json::Value(0);
    commonreg_com["preserve"] = Json::Value(true);
    commonreg_com["version"] = Json::Value(1);
    rootJson["commonreg.com"] = commonreg_com;

    Json::Value apsara_log;
    apsara_log["project_name"] = Json::Value("8000000_proj");
    apsara_log["category"] = Json::Value("8000000_category");
    apsara_log["log_type"] = Json::Value("apsara_log");
    apsara_log["log_begin_reg"] = Json::Value("\\[\\d+-\\d+-\\d+ \\d+:\\d+:\\d+.\\d+\\].*");
    apsara_log["log_path"] = Json::Value((bfs::path(mRootDir) / "apsara_log").string());
    SetConfigFilePattern(apsara_log);
    apsara_log["enable"] = Json::Value(true);
    apsara_log["preserve"] = Json::Value(false);
    apsara_log["preserve_depth"] = Json::Value(2);
    apsara_log["version"] = Json::Value(1);
    rootJson["apsara_log"] = apsara_log;

    Json::Value metrics;
    metrics["metrics"] = rootJson;

    LOG_INFO(sLogger, ("Add local user config", "local overwrite remote"));
    ofstream fout(AppConfig::GetInstance()->GetLocalUserConfigPath().c_str());
    fout << metrics.toStyledString() << endl;
    fout.close();
    LOG_INFO(sLogger, ("Add local user config", "done"));
    sleep(WAIT_CONFIG_UPDATE_INTERVAL);
    LOG_INFO(sLogger, ("Wait for local user config to be updated", "done"));

    LOG_INFO(sLogger, ("Write new data", ""));
    for (int i = 0; i < 20; ++i) {
        for (int j = 0; j < 2; ++j)
            DumpLog(10, mRootDir + PATH_SEPARATOR + gPath[j], (LogType)j);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    LOG_INFO(sLogger, ("Write new data", "done"));
    WaitForFileBeenRead();
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);

    LOG_INFO(sLogger, ("Test config update status", ""));
    unordered_map<string, Config*>& configMap = ConfigManager::GetInstance()->mNameConfigMap;
    unordered_map<string, Config*>::iterator it = configMap.find("commonreg.com");
    APSARA_TEST_TRUE(it != configMap.end());
    Config* config = it->second;
    APSARA_TEST_EQUAL(config->mProjectName, "2000000_proj");
    APSARA_TEST_EQUAL(config->mBasePath, (bfs::path(mRootDir) / "comm").string());
    APSARA_TEST_EQUAL(config->mVersion, 1);
    APSARA_TEST_EQUAL(config->mLogType, REGEX_LOG);

    it = configMap.find("apsara_log");
    APSARA_TEST_TRUE(it != configMap.end());
    config = it->second;
    APSARA_TEST_EQUAL(config->mProjectName, "8000000_proj");
    APSARA_TEST_EQUAL(config->mBasePath, (bfs::path(mRootDir) / "apsara_log").string());
    APSARA_TEST_EQUAL(config->mVersion, 1);
    APSARA_TEST_EQUAL(config->mLogType, APSARA_LOG);
    APSARA_TEST_EQUAL(config->mIsPreserve, false);

    LOG_INFO(sLogger, ("Print config map", ""));
    for (auto iter : configMap) {
        LOG_INFO(
            sLogger,
            (iter.first, iter.second->GetProjectName())("count", sProjectNameCountMap[iter.second->GetProjectName()]));
    }
    LOG_INFO(sLogger, ("print config map", "done"));
    APSARA_TEST_EQUAL(configMap.size(), 2);
    APSARA_TEST_EQUAL(sProjectNameCountMap["2000000_proj"] + sProjectNameCountMap["1000000_proj"], 300);
    APSARA_TEST_EQUAL(sProjectNameCountMap["8000000_proj"], 300);

    //test add config

    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->IsRegistered((mRootDir + PATH_SEPARATOR + "comm").c_str()));
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->IsRegistered((mRootDir + PATH_SEPARATOR + "apsara_log").c_str()));
    APSARA_TEST_TRUE(
        !EventDispatcher::GetInstance()->IsRegistered((mRootDir + PATH_SEPARATOR + "apsara_log1").c_str()));

    apsara_log["project_name"] = Json::Value("3000000_proj");
    apsara_log["category"] = Json::Value("3000000_category");
    apsara_log["log_type"] = Json::Value("apsara_log");
    apsara_log["log_begin_reg"] = Json::Value("\\[\\d+-\\d+-\\d+ \\d+:\\d+:\\d+.\\d+\\].*");
    apsara_log["log_path"] = Json::Value(mRootDir + PATH_SEPARATOR + "apsara_log1");
    SetConfigFilePattern(apsara_log);
    apsara_log["enable"] = Json::Value(true);
    apsara_log["preserve"] = Json::Value(true);
    commonreg_com["version"] = Json::Value(1);
    Json::Value rootJson1;
    rootJson1["apsara_log1"] = apsara_log;

    Json::Value metrics1;
    metrics1["metrics"] = rootJson1;

    ofstream fout1((AppConfig::GetInstance()->GetLocalUserConfigDirPath() + "config_3000000.json").c_str());
    fout1 << metrics1.toStyledString() << endl;
    fout1.close();

    bfs::create_directories(bfs::path(mRootDir) / "apsara_log1");
    sleep(WAIT_CONFIG_UPDATE_INTERVAL);
    for (int i = 0; i < 20; ++i) {
        DumpLog(10, mRootDir + PATH_SEPARATOR + "apsara_log1", APSARA_LOG);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    WaitForFileBeenRead();
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);
    it = configMap.find("apsara_log1");
    APSARA_TEST_EQUAL(it != configMap.end(), true);
    if (it == configMap.end())
        return;
    config = it->second;
    APSARA_TEST_EQUAL(config->mProjectName, "3000000_proj");
    APSARA_TEST_EQUAL(config->mLogType, APSARA_LOG);
    APSARA_TEST_EQUAL(config->mBasePath, mRootDir + PATH_SEPARATOR + "apsara_log1");
    APSARA_TEST_EQUAL(config->mVersion, 1);

    APSARA_TEST_EQUAL(sProjectNameCountMap["3000000_proj"], 200);


    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->IsRegistered((mRootDir + PATH_SEPARATOR + "comm").c_str()));
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->IsRegistered((mRootDir + PATH_SEPARATOR + "apsara_log").c_str()));
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->IsRegistered((mRootDir + PATH_SEPARATOR + "apsara_log1").c_str()));


    DirCheckPointPtr dirCheckPointPtr;
    CheckPointPtr checkPointPtr;
    CheckPointManager* pCheckPointManager = CheckPointManager::Instance();

    //APSARA_TEST_TRUE(pCheckPointManager->GetCheckPoint(mRootDir + "/comm/job.log", checkPointPtr));
    //APSARA_TEST_TRUE(pCheckPointManager->GetCheckPoint(mRootDir + "/apsara_log/job.log", checkPointPtr));

    APSARA_TEST_TRUE(pCheckPointManager->GetDirCheckPoint(mRootDir, dirCheckPointPtr));
    if (dirCheckPointPtr.get() != NULL) {
        APSARA_TEST_TRUE(dirCheckPointPtr->mSubDir.find(mRootDir + PATH_SEPARATOR + "comm")
                         != dirCheckPointPtr->mSubDir.end());
        APSARA_TEST_TRUE(dirCheckPointPtr->mSubDir.find(mRootDir + PATH_SEPARATOR + "apsara_log")
                         != dirCheckPointPtr->mSubDir.end());
    }

    //pCheckPointManager->PrintStatus();

    //test delete config
    bfs::remove_all(AppConfig::GetInstance()->GetLocalUserConfigDirPath() + "config_3000000.json");

    sProjectNameCountMap["3000000_proj"] = 0;
    sleep(WAIT_CONFIG_UPDATE_INTERVAL);
    for (int i = 0; i < 20; ++i) {
        DumpLog(10, mRootDir + PATH_SEPARATOR + "apsara_log1", APSARA_LOG);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    WaitForFileBeenRead();
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);

    APSARA_TEST_EQUAL(sProjectNameCountMap["3000000_proj"], 0);
    APSARA_TEST_TRUE(configMap.find("apsara_log1") == configMap.end());


    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->IsRegistered((mRootDir + PATH_SEPARATOR + "comm").c_str()));
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->IsRegistered((mRootDir + PATH_SEPARATOR + "apsara_log").c_str()));
    APSARA_TEST_TRUE(
        !EventDispatcher::GetInstance()->IsRegistered((mRootDir + PATH_SEPARATOR + "apsara_log1").c_str()));


    //APSARA_TEST_TRUE(pCheckPointManager->GetCheckPoint(mRootDir + "/comm/job.log", checkPointPtr));
    //APSARA_TEST_TRUE(!pCheckPointManager->GetCheckPoint(mRootDir + "/apsara_log/job.log", checkPointPtr));
    //APSARA_TEST_TRUE(pCheckPointManager->GetCheckPoint(mRootDir + "/apsara_log1/job.log", checkPointPtr));

    APSARA_TEST_TRUE(pCheckPointManager->GetDirCheckPoint(mRootDir, dirCheckPointPtr));
    if (dirCheckPointPtr.get() != NULL) {
        APSARA_TEST_TRUE(dirCheckPointPtr->mSubDir.find(mRootDir + PATH_SEPARATOR + "comm")
                         != dirCheckPointPtr->mSubDir.end());
        APSARA_TEST_TRUE(dirCheckPointPtr->mSubDir.find(mRootDir + PATH_SEPARATOR + "apsara_log")
                         != dirCheckPointPtr->mSubDir.end());
        APSARA_TEST_TRUE(dirCheckPointPtr->mSubDir.find(mRootDir + PATH_SEPARATOR + "apsara_log1")
                         == dirCheckPointPtr->mSubDir.end());
    }


    pCheckPointManager->RemoveAllCheckPoint();
    pCheckPointManager->LoadCheckPoint();

    auto& PS = PATH_SEPARATOR;
    DevInode devInode1 = GetFileDevInode(mRootDir + PS + "comm" + PS + "job.log");
    DevInode devInode2 = GetFileDevInode(mRootDir + PS + "apsara_log" + PS + "job.log");
    DevInode devInode3 = GetFileDevInode(mRootDir + PS + "apsara_log1" + PS + "job.log");
    APSARA_TEST_TRUE(pCheckPointManager->GetCheckPoint(devInode1, "commonreg.com", checkPointPtr));
    APSARA_TEST_TRUE(pCheckPointManager->GetCheckPoint(devInode2, "apsara_log", checkPointPtr));
    APSARA_TEST_TRUE(!pCheckPointManager->GetCheckPoint(devInode3, "apsara_log1", checkPointPtr));

    APSARA_TEST_TRUE(pCheckPointManager->GetDirCheckPoint(mRootDir, dirCheckPointPtr));
    if (dirCheckPointPtr.get() != NULL) {
        APSARA_TEST_TRUE(dirCheckPointPtr->mSubDir.find(mRootDir + PS + "comm") != dirCheckPointPtr->mSubDir.end());
        APSARA_TEST_TRUE(dirCheckPointPtr->mSubDir.find(mRootDir + PS + "apsara_log")
                         != dirCheckPointPtr->mSubDir.end());
        APSARA_TEST_TRUE(dirCheckPointPtr->mSubDir.find(mRootDir + PS + "apsara_log1")
                         == dirCheckPointPtr->mSubDir.end());
    }

    CaseCleanup();
    RemoveConfigFile();
    LOG_INFO(sLogger, ("TestLocalConfigUpdate() end", time(NULL)));
}

void ConfigUpdatorUnittest::TestUpdatePath() {
    LOG_INFO(sLogger, ("TestUpdatePath() begin", time(NULL)));

    string dirs[] = {PS + "dir" + PS + "dir1",
                     PS + "dir" + PS + "dir1" + PS + "dir2_1",
                     PS + "dir" + PS + "dir1" + PS + "dir2_2",
                     PS + "dir" + PS + "dir1" + PS + "dir2_1" + PS + "dir_3_1",
                     PS + "dir" + PS + "dir1" + PS + "dir2_2" + PS + "dir_3_2"};
    bfs::create_directories(bfs::path(mRootDir) / "dir");
    for (int i = 0; i < 5; ++i) {
        bfs::create_directories(bfs::path(mRootDir) / dirs[i]);
    }
    Json::Value rootJson;

    // at first, only watch depth=2 dirs
    Json::Value apsara_log;
    apsara_log["project_name"] = Json::Value("9000000_proj");
    apsara_log["category"] = Json::Value("9000000_category");
    apsara_log["log_type"] = Json::Value("apsara_log");
    apsara_log["log_begin_reg"] = Json::Value("\\[\\d+-\\d+-\\d+ \\d+:\\d+:\\d+.\\d+\\].*");
    apsara_log["log_path"] = Json::Value(mRootDir + PS + "dir");
    SetConfigFilePattern(apsara_log);
    apsara_log["enable"] = Json::Value(true);
    apsara_log["preserve"] = Json::Value(false);
    apsara_log["preserve_depth"] = Json::Value(2);
    apsara_log["topic_format"] = Json::Value("default");
    rootJson["apsara_log"] = apsara_log;

    Json::Value metrics;
    metrics["metrics"] = rootJson;
    ofstream fout(STRING_FLAG(user_log_config).c_str());
    fout << metrics << endl;
    fout.close();
    CaseSetup();

    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 5; ++j)
            DumpLog(10, mRootDir + dirs[j], APSARA_LOG);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    WaitForFileBeenRead();
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);
    APSARA_TEST_EQUAL_DESC(sTopicCountMap[mRootDir + dirs[0] + PS + "job"], 100, mRootDir + dirs[0] + PS + "job");
    APSARA_TEST_EQUAL_DESC(sTopicCountMap[mRootDir + dirs[1] + PS + "job"], 100, mRootDir + dirs[1] + PS + "job");
    APSARA_TEST_EQUAL_DESC(sTopicCountMap[mRootDir + dirs[2] + PS + "job"], 100, mRootDir + dirs[2] + PS + "job");
    APSARA_TEST_EQUAL_DESC(sTopicCountMap[mRootDir + dirs[3] + PS + "job"], 0, mRootDir + dirs[3] + PS + "job");
    APSARA_TEST_EQUAL_DESC(sTopicCountMap[mRootDir + dirs[4] + PS + "job"], 0, mRootDir + dirs[4] + PS + "job");

    // then update config to all depth
    apsara_log["preserve"] = Json::Value(true);
    apsara_log.removeMember("preserve_depth");
    apsara_log.removeMember("dir_pattern_black_list");
    apsara_log["version"] = Json::Value(1);
    rootJson["apsara_log"] = apsara_log;
    metrics["metrics"] = rootJson;
    SetConfigResponse(metrics.toStyledString());
    // need a little time here to let config update
    sleep(WAIT_CONFIG_UPDATE_INTERVAL);
    for (int i = 0; i < 20; ++i) {
        for (int j = 0; j < 5; ++j)
            DumpLog(10, mRootDir + dirs[j], APSARA_LOG);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    WaitForFileBeenRead();
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);

    APSARA_TEST_EQUAL_DESC(sTopicCountMap[mRootDir + dirs[0] + PS + "job"], 300, mRootDir + dirs[0] + PS + "job");
    APSARA_TEST_EQUAL_DESC(sTopicCountMap[mRootDir + dirs[1] + PS + "job"], 300, mRootDir + dirs[1] + PS + "job");
    APSARA_TEST_EQUAL_DESC(sTopicCountMap[mRootDir + dirs[2] + PS + "job"], 300, mRootDir + dirs[2] + PS + "job");
    APSARA_TEST_EQUAL_DESC(sTopicCountMap[mRootDir + dirs[3] + PS + "job"], 300, mRootDir + dirs[3] + PS + "job");
    APSARA_TEST_EQUAL_DESC(sTopicCountMap[mRootDir + dirs[4] + PS + "job"], 300, mRootDir + dirs[4] + PS + "job");

    // when "preserve" = true, "preserve_depth" is useless
    sTopicCountMap.clear();
    apsara_log["preserve"] = Json::Value(true);
    apsara_log["preserve_depth"] = Json::Value(2);
    apsara_log["version"] = Json::Value(2);
    rootJson["apsara_log"] = apsara_log;
    metrics["metrics"] = rootJson;
    SetConfigResponse(metrics.toStyledString());
    sleep(WAIT_CONFIG_UPDATE_INTERVAL);
    for (int i = 0; i < 20; ++i) {
        for (int j = 0; j < 5; ++j)
            DumpLog(10, mRootDir + dirs[j], APSARA_LOG);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    WaitForFileBeenRead();
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);
    APSARA_TEST_EQUAL_DESC(sTopicCountMap[mRootDir + dirs[0] + PS + "job"], 200, mRootDir + dirs[0] + PS + "job");
    APSARA_TEST_EQUAL_DESC(sTopicCountMap[mRootDir + dirs[1] + PS + "job"], 200, mRootDir + dirs[1] + PS + "job");
    APSARA_TEST_EQUAL_DESC(sTopicCountMap[mRootDir + dirs[2] + PS + "job"], 200, mRootDir + dirs[2] + PS + "job");
    APSARA_TEST_EQUAL_DESC(sTopicCountMap[mRootDir + dirs[3] + PS + "job"], 200, mRootDir + dirs[3] + PS + "job");
    APSARA_TEST_EQUAL_DESC(sTopicCountMap[mRootDir + dirs[4] + PS + "job"], 200, mRootDir + dirs[4] + PS + "job");

    CaseCleanup();
    RemoveConfigFile();
    LOG_INFO(sLogger, ("TestUpdatePath() end", time(NULL)));
}

void ConfigUpdatorUnittest::TestBlackDirList() {
    LOG_INFO(sLogger, ("TestBlackDirList() begin", time(NULL)));

    std::string dirs[]
        = {PS + "dir" + PS + "dir_1_1" + PS + "dir_2_1",
           PS + "dir" + PS + "dir_1_1" + PS + "dir_2_1" + PS + "dir_3_1",
           PS + "dir" + PS + "dir_1_1" + PS + "dir_2_1" + PS + "dir_3_2",
           PS + "dir" + PS + "dir_1_1" + PS + "dir_2_2" + PS + "dir_3_1" + PS + "dir_4_1",
           PS + "dir" + PS + "dir_1_1" + PS + "dir_2_2" + PS + "dir_3_2" + PS + "dir_4_2",
           PS + "dir" + PS + "dir_1_2" + PS + "dir_2_1" + PS + "dir_3_1" + PS + "dir_4_1",
           PS + "dir" + PS + "dir_1_2" + PS + "dir_2_1" + PS + "dir_3_1" + PS + "dir_4_2" + PS + "dir_5_1"};
    const int DIR_COUNT = sizeof(dirs) / sizeof(std::string);
    for (int i = 0; i < DIR_COUNT; ++i) {
        bfs::create_directories(bfs::path(mRootDir) / dirs[i]);
    }

    Json::Value rootJson;
    // apsara_log
    Json::Value apsara_log;
    apsara_log["project_name"] = Json::Value("9000000_proj");
    apsara_log["category"] = Json::Value("9000000_category");
    apsara_log["log_type"] = Json::Value("apsara_log");
    apsara_log["log_begin_reg"] = Json::Value("\\[\\d+-\\d+-\\d+ \\d+:\\d+:\\d+.\\d+\\].*");
    apsara_log["log_path"] = Json::Value(mRootDir + PS + "dir" + PS + "dir_1_1");
    SetConfigFilePattern(apsara_log);
    apsara_log["enable"] = Json::Value(true);
    apsara_log["preserve"] = Json::Value(true);
    apsara_log["topic_format"] = Json::Value("default");
    Json::Value blackDirList_1;
#if defined(__linux__)
    blackDirList_1.append(Json::Value("*" + PS + "dir" + PS + "dir_1_1" + PS + "dir_2_[12]" + PS + "dir_3_2"));
#elif defined(_MSC_VER)
    blackDirList_1.append(Json::Value("*" + PS + "dir" + PS + "dir_1_1" + PS + "dir_2_1" + PS + "dir_3_2"));
    blackDirList_1.append(Json::Value("*" + PS + "dir" + PS + "dir_1_1" + PS + "dir_2_2" + PS + "dir_3_2"));
#endif
    apsara_log["dir_pattern_black_list"] = blackDirList_1;
    rootJson["apsara_log"] = apsara_log;

    // apsara_log_2
    Json::Value apsara_log_2;
    apsara_log_2["project_name"] = Json::Value("9000000_proj");
    apsara_log_2["category"] = Json::Value("9000000_category");
    apsara_log_2["log_type"] = Json::Value("apsara_log");
    apsara_log_2["log_begin_reg"] = Json::Value("\\[\\d+-\\d+-\\d+ \\d+:\\d+:\\d+.\\d+\\].*");
    apsara_log_2["log_path"] = Json::Value(mRootDir + PS + "dir" + PS + "dir_1_2");
    SetConfigFilePattern(apsara_log_2);
    apsara_log_2["enable"] = Json::Value(true);
    apsara_log_2["preserve"] = Json::Value(true);
    apsara_log_2["topic_format"] = Json::Value("default");
    Json::Value blackDirList_2;
    blackDirList_2.append(
        Json::Value("*" + PS + "dir" + PS + "dir_1_2" + PS + "dir_2_1" + PS + "dir_3_1" + PS + "dir_4_2*"));
    apsara_log_2["dir_pattern_black_list"] = blackDirList_2;
    rootJson["apsara_log_2"] = apsara_log_2;

    Json::Value metrics;
    metrics["metrics"] = rootJson;
    ofstream fout(STRING_FLAG(user_log_config).c_str());
    fout << metrics << endl;
    fout.close();
    CaseSetup();

    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < DIR_COUNT; ++j)
            DumpLog(10, mRootDir + dirs[j], APSARA_LOG);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    WaitForFileBeenRead();
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);

    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[0] + PS + "job"], 100);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[1] + PS + "job"], 100);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[2] + PS + "job"], 0);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[3] + PS + "job"], 100);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[4] + PS + "job"], 0);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[5] + PS + "job"], 100);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[6] + PS + "job"], 0);

    CaseCleanup();
    RemoveConfigFile();
    LOG_INFO(sLogger, ("TestBlackDirList() end", time(NULL)));
}

void ConfigUpdatorUnittest::TestValidPath() {
    LOG_INFO(sLogger, ("TestValidPath() begin", time(NULL)));

    string dirs[] = {PS + "modify" + PS + "aa",
                     PS + "modify" + PS + "aa" + PS + "bb",
                     PS + "modify" + PS + "aa" + PS + ".cc",
                     PS + "modify" + PS + "aa" + PS + "cc" + PS + ".ee"};
    for (int i = 0; i < 3; ++i)
        bfs::create_directories(bfs::path(mRootDir) / dirs[i]);

    Json::Value rootJson;
    Json::Value apsara_log;
    apsara_log["project_name"] = Json::Value("9000000_proj");
    apsara_log["category"] = Json::Value("9000000_category");
    apsara_log["log_type"] = Json::Value("apsara_log");
    apsara_log["log_begin_reg"] = Json::Value("\\[\\d+-\\d+-\\d+ \\d+:\\d+:\\d+.\\d+\\].*");
    apsara_log["log_path"] = Json::Value(mRootDir + PS + "modify");
    SetConfigFilePattern(apsara_log);
    apsara_log["enable"] = Json::Value(true);
    apsara_log["preserve"] = Json::Value(false);
    apsara_log["preserve_depth"] = Json::Value(2);
    apsara_log["topic_format"] = Json::Value("default");
    rootJson["apsara_log"] = apsara_log;

    Json::Value metrics;
    metrics["metrics"] = rootJson;
    ofstream fout(STRING_FLAG(user_log_config).c_str());
    fout << metrics << endl;
    fout.close();
    CaseSetup();
    LOG_INFO(sLogger, ("DumpLog", "Begin"));
    for (int i = 0; i < 20; ++i) {
        for (int j = 0; j < 4; ++j)
            DumpLog(10, mRootDir + dirs[j], APSARA_LOG);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    WaitForFileBeenRead();
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[0] + PS + "job"], 200);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[1] + PS + "job"], 200);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[2] + PS + "job"], 0);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[3] + PS + "job"], 0);

    CaseCleanup();
    RemoveConfigFile();
    LOG_INFO(sLogger, ("TestValidPath() end", time(NULL)));
}

void ConfigUpdatorUnittest::TestUpdateGlobalConfig() {
    LOG_INFO(sLogger, ("TestUpdateGlobalConfig() begin", time(NULL)));
    DumpInitConfigToLocal();
    CaseSetup();

    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetBytePerSec(), INT32_FLAG(default_send_byte_per_sec));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMaxBytePerSec(), INT32_FLAG(default_max_send_byte_per_sec));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetNumOfBufferFile(), INT32_FLAG(default_buffer_file_num));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetLocalFileSize(), INT32_FLAG(default_local_file_size));
    APSARA_TEST_EQUAL(Sender::Instance()->GetBufferFilePath(), GetProcessExecutionDir());
    APSARA_TEST_EQUAL(LogtailGlobalPara::Instance()->GetTopic(), STRING_FLAG(default_global_topic));

    Json::Value firstJson;
    firstJson["max_flow_byte_per_sec"] = Json::Value("100000");
    firstJson["max_net_flow_byte_per_sec"] = Json::Value("200000");
    firstJson["max_num_of_file"] = Json::Value("10");
    firstJson["local_file_size"] = Json::Value("1024");
    firstJson["buffer_file_path"] = Json::Value(PS + "tmp/1");
    firstJson["global_topic"] = Json::Value("AT-100");
    Json::Value firstRoot;
    firstRoot["config"] = firstJson;
    SetConfigResponse(firstRoot.toStyledString());
    sleep(WAIT_CONFIG_UPDATE_INTERVAL);
    // APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetBytePerSec(), 100000);
    // APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMaxBytePerSec(), 200000);
    // APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetNumOfBufferFile(), 10);
    // APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetLocalFileSize(), 1024);
    // APSARA_TEST_EQUAL(Sender::Instance()->GetBufferFilePath(), "/tmp/1/");
    APSARA_TEST_EQUAL(LogtailGlobalPara::Instance()->GetTopic(), "AT-100");

    Json::Value secondJson;
    secondJson["max_flow_byte_per_sec"] = Json::Value("300000");
    secondJson["max_net_flow_byte_per_sec"] = Json::Value("600000");
    secondJson["max_num_of_file"] = Json::Value("30");
    secondJson["local_file_size"] = Json::Value("512");
    secondJson["buffer_file_path"] = Json::Value(PS + "tmp/2");
    secondJson["logtail_access_id"] = Json::Value("3000");
    secondJson["logtail_project_name"] = Json::Value("3000_proj");
    secondJson["global_topic"] = Json::Value("AT-300");
    Json::Value thirdRoot;
    thirdRoot["config"] = secondJson;
    SetConfigResponse(thirdRoot.toStyledString());
    sleep(WAIT_CONFIG_UPDATE_INTERVAL);
    // APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetBytePerSec(), 300000);
    // APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMaxBytePerSec(), 600000);
    // APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetNumOfBufferFile(), 30);
    // APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetLocalFileSize(), 512);
    // APSARA_TEST_EQUAL(Sender::Instance()->GetBufferFilePath(), "/tmp/2/");
    APSARA_TEST_EQUAL(LogtailGlobalPara::Instance()->GetTopic(), "AT-300");

    CaseCleanup();
    RemoveConfigFile();
    LOG_INFO(sLogger, ("TestUpdateGlobalConfig() end", time(NULL)));
}

void ConfigUpdatorUnittest::TestUpdateProfileProject() {
    LOG_INFO(sLogger, ("TestUpdateProfileProject() begin", time(NULL)));
    DumpInitConfigToLocal();
    CaseSetup();

    APSARA_TEST_EQUAL(ConfigManager::GetInstance()->GetDefaultProfileProjectName(), STRING_FLAG(profile_project_name));

    Json::Value firstRoot;
    firstRoot["profile_project"] = Json::Value("17_1");
    firstRoot["profile_project_region"] = Json::Value("normal");
    SetConfigResponse(firstRoot.toStyledString());
    sleep(WAIT_CONFIG_UPDATE_INTERVAL);
    APSARA_TEST_EQUAL(ConfigManager::GetInstance()->GetDefaultProfileProjectName(), "17_1");
    APSARA_TEST_EQUAL(ConfigManager::GetInstance()->GetDefaultProfileRegion(), "normal");

    Json::Value secondRoot;
    secondRoot["profile_project"] = Json::Value("17_2");
    secondRoot["profile_project_region"] = Json::Value("abnormal");
    SetConfigResponse(secondRoot.toStyledString());
    sleep(WAIT_CONFIG_UPDATE_INTERVAL);
    APSARA_TEST_EQUAL(ConfigManager::GetInstance()->GetDefaultProfileProjectName(), "17_2");
    APSARA_TEST_EQUAL(ConfigManager::GetInstance()->GetDefaultProfileRegion(), "abnormal");

    CaseCleanup();
    RemoveConfigFile();
    LOG_INFO(sLogger, ("TestUpdateProfileProject() end", time(NULL)));
}

void ConfigUpdatorUnittest::GetLogContent(LogType logType, char* buffer) {
    char timeBuffer[50];
    struct tm timeInfo;
    time_t cur;
    cur = time(NULL);
#if defined(__linux__)
    localtime_r(&cur, &timeInfo);
#elif defined(_MSC_VER)
    localtime_s(&timeInfo, &cur);
#endif
    char buffer2[] = "\"GET /icons/text.gif HTTP/1.1\" 200 229";
    char buffer3[] = " \"http://10.230.201.117/AT-INT-A01/\" \"Mozilla/5.0 (Windows NT 6.1; rv:10.0.2) Gecko/20100101 "
                     "Firefox/10.0.2\"";

    if (logType == REGEX_LOG) {
        strftime(timeBuffer, sizeof(timeBuffer), " - - [%d/%b/%Y:%R:%S +0800] ", &timeInfo);
        sprintf(buffer, "%s%s%s", LogFileProfiler::GetInstance()->mIpAddr.c_str(), timeBuffer, buffer2);
        if ((rand()) % 2 == 0)
            strcat(buffer, buffer3);
        strcat(buffer, "\n");
    } else if (logType == APSARA_LOG) {
        static int seq = 0;
        strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &timeInfo);
        sprintf(buffer,
                "[%s.523094]\t[INFO]\t[29817]\t[build/debug64/sqlonline/OpenTableService/OpenTableServer/worker/"
                "ots_worker.cpp:274]\tAccessId:1vjejd8xmpocf63g1cc8h2xv\tOpenId:CapTest\tOperationType:"
                "PutData\tTargetEntity:ots_ft_test_contact_test12\tSourceIP:10.230.201.25\tAPIVersion:1\tTime:"
                "1332913419517022\tLatency:6070\tInFlowDataSize:793\tOutFlowDataSize:198\tHttpResponseStatus:"
                "200\tOTSStatus:0\tSQLStatus:0\tExtraMessage:\tseq:%d\n",
                timeBuffer,
                seq++);
    }
}

void ConfigUpdatorUnittest::DumpLog(int logNum, string path, enum LogType logType, string logName) {
    // Use binary mode to write to avoid \n being converted to \r\n.
    std::ofstream out(path + logtail::PATH_SEPARATOR + logName, std::ios_base::app | std::ios_base::binary);
    if (out) {
        char buffer[10240];
        for (int i = 0; i < logNum; ++i) {
            GetLogContent(logType, buffer);
            out.write(buffer, strlen(buffer));
        }
    }
}

bool ConfigUpdatorUnittest::MockGetLogtailConfig(const string& targetURL,
                                                 const string& intf,
                                                 const string& ip,
                                                 const string& config,
                                                 string& response,
                                                 string& errorMessage,
                                                 bool isCompress) {
    LOG_INFO(sLogger, ("reject set size", sConfigRejectSet.size()));
    LOG_INFO(sLogger, ("target url", targetURL));
    if (IsConfigRejected(targetURL)) {
        ++sRejectCount;
        LOG_INFO(sLogger, ("target url reject", targetURL)("Reject count", sRejectCount));
        return false;
    }
    Json::Reader parser;
    Json::Value input;
    Json::Value output;
    auto configRes = GetConfigResponse();
    if (!parser.parse(configRes, output)) {
        LOG_WARNING(sLogger, ("Parse response fail", configRes));
        return true;
    }
    string regionResp = GetRegionConfigResponse(targetURL);
    if (!regionResp.empty()) {
        LOG_INFO(sLogger, ("target url use region resp", regionResp));
        output.clear();
        if (!parser.parse(regionResp, output)) {
            LOG_WARNING(sLogger, ("Parse response fail", regionResp));
            return true;
        }
    }

    if (!parser.parse(config, input)) {
        LOG_WARNING(sLogger, ("Parse response fail", config));
        return true;
    }
    if (!output.isMember("metrics") && !output.isMember("config") && !output.isMember("profile_project"))
        return true;
    if (input.isMember("metrics") == false)
        return true;
    Json::Value::Members logNames = input["metrics"].getMemberNames();
    for (size_t i = 0; i < logNames.size(); ++i) {
        string logname = logNames[i];
        if (output["metrics"].isMember(logname) == false)
            continue;
        int version_out = output["metrics"][logname]["version"].asInt();
        int version_in = input["metrics"][logname]["version"].asInt();
        bool inHasGroupTopic = input["metrics"][logname]["topic_format"] == "group_topic";
        bool outHasGroupTopic = input["metrics"][logname]["topic_format"] == "group_topic";
        if (version_in >= version_out && version_out >= 0) {
            if (inHasGroupTopic && outHasGroupTopic
                && input["metrics"][logname]["group_topic"] != output["metrics"][logname]["group_topic"])
                continue;
            output["metrics"].removeMember(logname);
        }
    }
    logNames = output["metrics"].getMemberNames();
    for (size_t i = 0; i < logNames.size(); ++i) {
        string logname = logNames[i];
        if (input["metrics"].isMember(logname) == false) {
            int version_out = output["metrics"][logname]["version"].asInt();
            if (version_out == -1) {
                output["metrics"].removeMember(logname);
            }
        }
    }
    if (output.isMember("region_list")) {
        input["region_list"] = output["region_list"];
    }
    Json::Value outputRoot;
    outputRoot["/GetLogtailConfig"] = output;
    response = outputRoot.toStyledString();
    LOG_DEBUG(sLogger, ("config request", config)("get config update response ", response));
    return true;
}

void ConfigUpdatorUnittest::TestCheckPointSaveInterval() {
    LOG_INFO(sLogger, ("TestCheckPointSaveInterval() start", time(NULL)));
    bfs::remove_all(STRING_FLAG(check_point_filename));
    string filenames[] = {"/tmp/apsara/log.log.1", "tmp/apsara.log.2", "/apsara/tmp/log.3"};
    int64_t offsets[] = {100, 10000, 200000};
    uint64_t inodes[] = {100, 200, 300};
    string configs[] = {"config1", "config_2006", "3000008config"};
    uint64_t devs[] = {34, 56, 98};
    string sigs[] = {"jroeijgperieorwpqijpgegu",
                     "jrpehuwklxcnvujoakckghrueioxckllxjgeuin \n\t",
                     "jfuir\tjgpdg\ndfjkoepj\njfpsdfgdpgqe\tfdjksi"};
    uint64_t sigHashs[] = {0, 0, 0};
    uint32_t sigSizes[] = {0, 0, 0};

    for (size_t i = 0; i < sizeof(sigs) / sizeof(string); ++i) {
        SignatureToHash(sigs[i], sigHashs[i], sigSizes[i]);
    }
    string dirs[] = {"/tmp/1/2/3/4/5", "/tmp/1/2/3/5", "/tmp/1/2/3/4/", "/tmp/1/3"};
    //string pdirs[] = { "/tmp/1/2/3/4","/tmp/1/2/3","/tmp/1" };

    DumpInitConfigToLocal();
    CaseSetup();
    sleep(2);
    for (int i = 0; i < 4; ++i) {
        CheckPointManager::Instance()->AddDirCheckPoint(dirs[i]);
    }

    for (int i = 0; i < 3; ++i) {
        CheckPoint* checkPointPtr = new CheckPoint(
            filenames[i], offsets[i], sigSizes[i], sigHashs[i], DevInode(devs[i], inodes[i]), configs[i]);
        CheckPointManager::Instance()->AddCheckPoint(checkPointPtr);
    }

    APSARA_TEST_EQUAL(CheckPointManager::Instance()->mDevInodeCheckPointPtrMap.size(), (size_t)3);
    int32_t defaultInterval = INT32_FLAG(check_point_dump_interval);
    INT32_FLAG(check_point_dump_interval) = 5;
    int32_t curTime = (int32_t)time(NULL);
    // checkpoint add rand dump, sleep should be long enough
    sleep(68);

    // after save, not existed file's check point will be removed, so mDevInodeCheckPointPtrMap.size == 0
    APSARA_TEST_EQUAL(CheckPointManager::Instance()->mDevInodeCheckPointPtrMap.size(), (size_t)0);

    CheckPointManager::Instance()->mDevInodeCheckPointPtrMap.clear();
    CheckPointManager::Instance()->mDirNameMap.clear();

    CheckPointManager::Instance()->LoadCheckPoint();
    APSARA_TEST_TRUE(CheckPointManager::Instance()->mLastDumpTime > curTime);
    //for (int i = 0; i < 3; ++i)
    //{
    //    DirCheckPointPtr ptr;
    //    if (i == 1)
    //    {
    //        APSARA_TEST_EQUAL_FATAL(CheckPointManager::Instance()->GetDirCheckPoint(pdirs[i], ptr), true);
    //        APSARA_TEST_EQUAL(ptr.get()->mSubDir.size(), 2);
    //    }
    //    else
    //    {
    //        APSARA_TEST_EQUAL_FATAL(CheckPointManager::Instance()->GetDirCheckPoint(pdirs[i], ptr), true);
    //        APSARA_TEST_EQUAL(ptr.get()->mSubDir.size(), 1);
    //    }
    //}
    // if checkpoint dump twice in this test, mDevInodeCheckPointPtrMap will be empty
    //for (int i = 2; i >= 0; --i)
    //{
    //    CheckPointPtr checkPointSharePtr;
    //    bool result = CheckPointManager::Instance()->GetCheckPoint(DevInode(devs[i], inodes[i]), checkPointSharePtr);
    //    APSARA_TEST_EQUAL_FATAL(result, true);
    //    CheckPoint * checkPointPtr = checkPointSharePtr.get();
    //    APSARA_TEST_EQUAL(checkPointPtr->mFileName, filenames[i]);
    //    APSARA_TEST_EQUAL(checkPointPtr->mSignature, sigs[i]);
    //    APSARA_TEST_EQUAL(checkPointPtr->mOffset, offsets[i]);
    //    APSARA_TEST_EQUAL(checkPointPtr->mInode, inodes[i]);
    //}

    INT32_FLAG(check_point_dump_interval) = defaultInterval;

    CaseCleanup();
    RemoveConfigFile();
    LOG_INFO(sLogger, ("TestCheckPointSaveInterval() end", time(NULL)));
}

void ConfigUpdatorUnittest::TestCheckPointUserDefinedFilePath() {
    LOG_INFO(sLogger, ("TestCheckPointUserDefinedFilePath() start", time(NULL)));
    bfs::remove_all(STRING_FLAG(check_point_filename));

    // dump to STRING_FLAG(check_point_filename)
    CheckPointManager::Instance()->DumpCheckPointToLocal();

    string filenames[] = {PS + "tmp" + PS + "apsara" + PS + "log.log.1",
                          "tmp" + PS + "apsara.log.2",
                          PS + "apsara" + PS + "tmp" + PS + "log.3"};
    int64_t offsets[] = {100, 10000, 200000};
    uint64_t devs[] = {34, 56, 98};
    uint64_t inodes[] = {100, 200, 300};
    string configs[] = {"config1", "config_2006", "3000008config"};
    string sigs[] = {"jroeijgperieorwpqijpgegu",
                     "jrpehuwklxcnvujoakckghrueioxckllxjgeuin \n\t",
                     "jfuir\tjgpdg\ndfjkoepj\njfpsdfgdpgqe\tfdjksi"};
    uint64_t sigHashs[] = {0, 0, 0};
    uint32_t sigSizes[] = {0, 0, 0};
    for (size_t i = 0; i < sizeof(sigs) / sizeof(string); ++i) {
        SignatureToHash(sigs[i], sigHashs[i], sigSizes[i]);
    }
    string dirs[] = {PS + "tmp" + PS + "1" + PS + "2" + PS + "3" + PS + "4" + PS + "5",
                     PS + "tmp" + PS + "1" + PS + "2" + PS + "3" + PS + "5",
                     PS + "tmp" + PS + "1" + PS + "2" + PS + "3" + PS + "4" + PS,
                     PS + "tmp" + PS + "1" + PS + "3"};
    string pdirs[] = {PS + "tmp" + PS + "1" + PS + "2" + PS + "3" + PS + "4",
                      PS + "tmp" + PS + "1" + PS + "2" + PS + "3",
                      PS + "tmp" + PS + "1"};
    for (int i = 0; i < 4; ++i) {
        CheckPointManager::Instance()->AddDirCheckPoint(dirs[i]);
    }
    for (int i = 0; i < 3; ++i) {
        CheckPoint* checkPointPtr = new CheckPoint(
            filenames[i], offsets[i], sigSizes[i], sigHashs[i], DevInode(devs[i], inodes[i]), configs[i]);
        CheckPointManager::Instance()->AddCheckPoint(checkPointPtr);
    }

    std::string customCptFilePath = mRootDir + PS + "checkpoint.cpt";
    bfs::remove(customCptFilePath);

    Json::Value settingsJson;
    settingsJson["check_point_filename"] = customCptFilePath;
    string ilogtailConfig = mRootDir + PS + "checkpointfile.json";
    ofstream fout((ilogtailConfig).c_str());
    fout << settingsJson.toStyledString() << endl;
    fout.close();
    AppConfig::GetInstance()->LoadAppConfig(ilogtailConfig);
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCheckPointFilePath(), customCptFilePath);

    CheckPointManager::Instance()->DumpCheckPointToLocal();

    FILE* pCpt = fopen(customCptFilePath.c_str(), "r");
    APSARA_TEST_TRUE(pCpt != NULL);
    if (pCpt != NULL) {
        fclose(pCpt);
    }

    CheckPointManager::Instance()->LoadCheckPoint();
    for (int i = 0; i < 3; ++i) {
        DirCheckPointPtr ptr;
        if (i == 1) {
            APSARA_TEST_EQUAL_FATAL(CheckPointManager::Instance()->GetDirCheckPoint(pdirs[i], ptr), true);
            APSARA_TEST_EQUAL(ptr.get()->mSubDir.size(), 2);
        } else {
            APSARA_TEST_EQUAL_FATAL(CheckPointManager::Instance()->GetDirCheckPoint(pdirs[i], ptr), true);
            APSARA_TEST_EQUAL(ptr.get()->mSubDir.size(), 1);
        }
    }
    for (int i = 2; i >= 0; --i) {
        CheckPointPtr checkPointSharePtr;
        bool result = CheckPointManager::Instance()->GetCheckPoint(
            DevInode(devs[i], inodes[i]), configs[i], checkPointSharePtr);
        APSARA_TEST_EQUAL(result, true);
        CheckPoint* checkPointPtr = checkPointSharePtr.get();
        APSARA_TEST_EQUAL(checkPointPtr->mFileName, filenames[i]);
        APSARA_TEST_EQUAL(checkPointPtr->mSignatureHash, sigHashs[i]);
        APSARA_TEST_EQUAL(checkPointPtr->mSignatureSize, sigSizes[i]);
        APSARA_TEST_EQUAL(checkPointPtr->mOffset, offsets[i]);
        APSARA_TEST_EQUAL(checkPointPtr->mDevInode.inode, inodes[i]);
        APSARA_TEST_EQUAL(checkPointPtr->mDevInode.dev, devs[i]);
        APSARA_TEST_EQUAL(checkPointPtr->mConfigName, configs[i]);
    }
    bfs::remove(customCptFilePath);
    bfs::remove(ilogtailConfig);
    bfs::remove(STRING_FLAG(check_point_filename));

    LOG_INFO(sLogger, ("TestCheckPointUserDefinedFilePath() end", time(NULL)));
}

void ConfigUpdatorUnittest::TestCheckPointLoadDefaultFile() {
    LOG_INFO(sLogger, ("TestCheckPointLoadDefaultFile() start", time(NULL)));

    string filenames[] = {PS + "tmp" + PS + "apsara" + PS + "log.log.1",
                          "tmp" + PS + "apsara.log.2",
                          PS + "apsara" + PS + "tmp" + PS + "log.3"};
    int64_t offsets[] = {100, 10000, 200000};
    uint64_t inodes[] = {100, 200, 300};
    uint64_t devs[] = {34, 56, 98};
    string sigs[] = {"jroeijgperieorwpqijpgegu",
                     "jrpehuwklxcnvujoakckghrueioxckllxjgeuin \n\t",
                     "jfuir\tjgpdg\ndfjkoepj\njfpsdfgdpgqe\tfdjksi"};
    uint64_t sigHashs[] = {0, 0, 0};
    uint32_t sigSizes[] = {0, 0, 0};
    string configs[] = {"config1", "config_2006", "3000008config"};
    for (size_t i = 0; i < sizeof(sigs) / sizeof(string); ++i) {
        SignatureToHash(sigs[i], sigHashs[i], sigSizes[i]);
    }
    string dirs[] = {PS + "tmp" + PS + "1" + PS + "2" + PS + "3" + PS + "4" + PS + "5",
                     PS + "tmp" + PS + "1" + PS + "2" + PS + "3" + PS + "5",
                     PS + "tmp" + PS + "1" + PS + "2" + PS + "3" + PS + "4" + PS,
                     PS + "tmp" + PS + "1" + PS + "3"};
    string pdirs[] = {PS + "tmp" + PS + "1" + PS + "2" + PS + "3" + PS + "4",
                      PS + "tmp" + PS + "1" + PS + "2" + PS + "3",
                      PS + "tmp" + PS + "1"};
    for (int i = 0; i < 4; ++i) {
        CheckPointManager::Instance()->AddDirCheckPoint(dirs[i]);
    }
    for (int i = 0; i < 3; ++i) {
        CheckPoint* checkPointPtr = new CheckPoint(
            filenames[i], offsets[i], sigSizes[i], sigHashs[i], DevInode(devs[i], inodes[i]), configs[i]);
        CheckPointManager::Instance()->AddCheckPoint(checkPointPtr);
    }
    bfs::remove(STRING_FLAG(check_point_filename));
    CheckPointManager::Instance()->DumpCheckPointToLocal();

    std::string customCptFilePath = mRootDir + PS + "checkpoint.cpt";
    bfs::remove(customCptFilePath);
    Json::Value settingsJson;
    settingsJson["check_point_filename"] = customCptFilePath;
    string ilogtailConfig = mRootDir + PS + "checkpointfile.json";
    ofstream fout((ilogtailConfig).c_str());
    fout << settingsJson.toStyledString() << endl;
    fout.close();
    AppConfig::GetInstance()->LoadAppConfig(ilogtailConfig);
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCheckPointFilePath(), customCptFilePath);

    CheckPointManager::Instance()->LoadCheckPoint();
    for (int i = 0; i < 3; ++i) {
        DirCheckPointPtr ptr;
        if (i == 1) {
            APSARA_TEST_EQUAL_FATAL(CheckPointManager::Instance()->GetDirCheckPoint(pdirs[i], ptr), true);
            APSARA_TEST_EQUAL(ptr.get()->mSubDir.size(), 2);
        } else {
            APSARA_TEST_EQUAL_FATAL(CheckPointManager::Instance()->GetDirCheckPoint(pdirs[i], ptr), true);
            APSARA_TEST_EQUAL(ptr.get()->mSubDir.size(), 1);
        }
    }
    for (int i = 2; i >= 0; --i) {
        CheckPointPtr checkPointSharePtr;
        bool result = CheckPointManager::Instance()->GetCheckPoint(
            DevInode(devs[i], inodes[i]), configs[i], checkPointSharePtr);
        APSARA_TEST_EQUAL(result, true);
        CheckPoint* checkPointPtr = checkPointSharePtr.get();
        APSARA_TEST_EQUAL(checkPointPtr->mFileName, filenames[i]);
        APSARA_TEST_EQUAL(checkPointPtr->mSignatureHash, sigHashs[i]);
        APSARA_TEST_EQUAL(checkPointPtr->mSignatureSize, sigSizes[i]);
        APSARA_TEST_EQUAL(checkPointPtr->mOffset, offsets[i]);
        APSARA_TEST_EQUAL(checkPointPtr->mDevInode.inode, inodes[i]);
        APSARA_TEST_EQUAL(checkPointPtr->mDevInode.dev, devs[i]);
        APSARA_TEST_EQUAL(checkPointPtr->mConfigName, configs[i]);
    }
    bfs::remove(customCptFilePath);
    bfs::remove(STRING_FLAG(check_point_filename));
    bfs::remove(ilogtailConfig);

    LOG_INFO(sLogger, ("TestCheckPointLoadDefaultFile() end", time(NULL)));
}

bool ConfigUpdatorUnittest::MockGetLogtailSecurity(const string& targetURL,
                                                   const string& intf,
                                                   bool httpsVerifyPeer,
                                                   const string& request,
                                                   string& response,
                                                   string& errorMessage,
                                                   const string& caCert) {
    response = sSecurityResponse;
    return true;
}

bool ConfigUpdatorUnittest::MockGetAccessKey(const string& targetURL,
                                             const string& intf,
                                             bool httpsVerifyPeer,
                                             const string& request,
                                             string& response,
                                             string& errorMessage,
                                             const string& caCert) {
    response = sAccessKeyResponse;
    return true;
}

void ConfigUpdatorUnittest::SetConfigResponse(const std::string& response) {
    LOG_INFO(sLogger, ("SetConfigResponse", response));
    ScopedSpinLock lock(mResponseLock);
    sResponse = response;
}

string ConfigUpdatorUnittest::GetConfigResponse() {
    ScopedSpinLock lock(mResponseLock);
    return sResponse;
}


void ConfigUpdatorUnittest::MockAsyncSend(const std::string& projectName,
                                          const std::string& logstore,
                                          const std::string& logData,
                                          SEND_DATA_TYPE dataType,
                                          int32_t rawSize,
                                          SendClosure* sendClosure) {
    if (logstore == "logtail_alarm") {
        PostLogStoreLogsResponse* sr = new PostLogStoreLogsResponse;
        sr->statusCode = 200;
        sr->requestId = "";
        sendClosure->OnSuccess(sr);
        return;
    }
    vector<LogGroup> logGroupVec;
    Sender::ParseLogGroupFromString(logData, dataType, rawSize, logGroupVec);

    LOG_INFO(
        sLogger,
        ("send, sReplaceKey", sReplaceKey)("projectName", projectName)("category", logstore)("dataType", dataType));
    // There is a regular send in the Sender with real aliuid, whose length is longer than 7,
    // the second condition is used to avoid it changing the sReplaceKey flag.
    if (sReplaceKey && sendClosure->mDataPtr->mAliuid.length() <= 7) {
        sReplaceKey = false;
        LOG_INFO(sLogger, ("mock exception", " SLSE_UNAUTHORIZED"));
        PostLogStoreLogsResponse* sr = new PostLogStoreLogsResponse;
        sr->statusCode = 500;
        sr->requestId = "";
        sendClosure->OnFail(sr, LOGE_UNAUTHORIZED, "accesskey, accesskeyid invalid");
        return;
    }

    for (vector<LogGroup>::iterator iter = logGroupVec.begin(); iter != logGroupVec.end(); ++iter) {
        sProjectNameCountMap[projectName] += iter->logs_size();
        sProjectCategoryTopicCountMap[projectName + "_" + logstore + "_" + iter->topic()] += iter->logs_size();
        LOG_INFO(sLogger,
                 ("MockAsyncSend, projectName",
                  projectName)("logstore", logstore)("topic", iter->topic())("logs", iter->logs_size()));
        if (iter->topic().size() > 0)
            sTopicCountMap[iter->topic()] += iter->logs_size();
    }
    PostLogStoreLogsResponse* sr = new PostLogStoreLogsResponse;
    sr->statusCode = 200;
    sr->requestId = "";
    sendClosure->OnSuccess(sr);
}

void ConfigUpdatorUnittest::TestLoadIlogtailConfig() {
    LOG_INFO(sLogger, ("TestLoadIlogtailConfig() begin", time(NULL)));
    string check_point_filenametemp = AppConfig::GetInstance()->mCheckPointFilePath;
    LOG_INFO(sLogger, ("back up check_point_filenametemp", check_point_filenametemp));
    bfs::create_directories(mRootDir);
    string ilogtailConfig = mRootDir + PS + STRING_FLAG(ilogtail_config);
    string subConfigPath = mRootDir + PS + STRING_FLAG(ilogtail_config) + ".d";
    bfs::create_directories(subConfigPath);

    LOG_INFO(sLogger, ("test default settings config (global flag)", ""));

    AppConfig::GetInstance()->LoadAppConfig(ilogtailConfig);
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->mCpuUsageUpLimit, DOUBLE_FLAG(cpu_usage_up_limit));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->mMemUsageUpLimit, INT64_FLAG(memory_usage_up_limit));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMaxBytePerSec(), INT32_FLAG(default_max_send_byte_per_sec));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetBytePerSec(), INT32_FLAG(default_send_byte_per_sec));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetNumOfBufferFile(), INT32_FLAG(default_buffer_file_num));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetLocalFileSize(), INT32_FLAG(default_local_file_size));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMaxHoldedDataSize(), INT32_FLAG(max_holded_data_size));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMaxBufferNum(), INT32_FLAG(max_buffer_num));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCheckPointFilePath(), STRING_FLAG(check_point_filename));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCustomizedConfigIp(), std::string(""));

    LOG_INFO(sLogger, ("test settings config file", ""));

    Json::Value settingsJson;
    settingsJson["cpu_usage_limit"] = 0.4;
    settingsJson["mem_usage_limit"] = Json::Int64(1099511627776);
    settingsJson["max_bytes_per_sec"] = 2097152;
    settingsJson["bytes_per_sec"] = 1048576;
    settingsJson["buffer_file_num"] = 25;
    settingsJson["buffer_file_size"] = 20971520;
    settingsJson["buffer_map_num"] = 5;
    settingsJson["buffer_map_size"] = 2097152;
    settingsJson["check_point_filename"] = std::string("/etc/ilotail/checkpoint.cpt");
    settingsJson["include_config_path"] = subConfigPath;
    settingsJson["customized_config_ip"] = std::string("127.0.0.1.fuse");

    ofstream fout((ilogtailConfig).c_str());
    fout << settingsJson.toStyledString() << endl;
    fout.close();
    AppConfig::GetInstance()->LoadAppConfig(ilogtailConfig);
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->mCpuUsageUpLimit, (float)0.4);
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->mMemUsageUpLimit, 1099511627776);
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMaxBytePerSec(), 2097152);
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetBytePerSec(), 1048576);
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetNumOfBufferFile(), 25);
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetLocalFileSize(), 20971520);
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMaxHoldedDataSize(), 2097152);
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMaxBufferNum(), 5);
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCheckPointFilePath(), std::string("/etc/ilotail/checkpoint.cpt"));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCustomizedConfigIp(), std::string("127.0.0.1.fuse"));


    Json::Value subSettingsJson;
    subSettingsJson["cpu_usage_limit"] = 0.5;
    subSettingsJson["mem_usage_limit"] = Json::Int64(1099511627777);
    subSettingsJson["check_point_filename"] = std::string("/etc/ilotail/checkpoint1.cpt");

    ofstream fSubOut((subConfigPath + "/1.json").c_str());
    fSubOut << subSettingsJson.toStyledString() << endl;
    fSubOut.close();

    AppConfig::GetInstance()->LoadAppConfig(ilogtailConfig);
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->mCpuUsageUpLimit, (float)0.5);
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->mMemUsageUpLimit, 1099511627777);
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCheckPointFilePath(), std::string("/etc/ilotail/checkpoint1.cpt"));

    subSettingsJson["cpu_usage_limit"] = 0.6;
    subSettingsJson["user_config_file_path"] = std::string("/home/admin/logs/user_config_file.json");
    subSettingsJson["mem_usage_limit"] = Json::Int64(1099511627778);
    subSettingsJson["check_point_filename"] = std::string("/etc/ilotail/checkpoint2.cpt");

    ofstream fSubOut2((subConfigPath + "/2.json").c_str());
    fSubOut2 << subSettingsJson.toStyledString() << endl;
    fSubOut2.close();

    AppConfig::GetInstance()->LoadAppConfig(ilogtailConfig);
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->mCpuUsageUpLimit, (float)0.6);
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->mUserConfigPath, std::string("/home/admin/logs/user_config_file.json"));
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->mMemUsageUpLimit, 1099511627778);
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCheckPointFilePath(), std::string("/etc/ilotail/checkpoint2.cpt"));
    AppConfig::GetInstance()->mCheckPointFilePath = check_point_filenametemp;

    LOG_INFO(sLogger, ("TestLoadIlogtailConfig() end", time(NULL)));
}

void ConfigUpdatorUnittest::TestUpdateGroupTopic() {
    LOG_INFO(sLogger, ("TestUpdateGroupTopic() begin", time(NULL)));

    bfs::create_directories(bfs::path(mRootDir) / "apsara_log");
    DumpInitConfigToLocal();
    CaseSetup();
    for (int i = 0; i < 10; ++i) {
        DumpLog(10, mRootDir + PATH_SEPARATOR + gPath[0], APSARA_LOG);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    WaitForFileBeenRead();
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);

    // update group config first
    Json::Value apsara_log;
    apsara_log["project_name"] = Json::Value("8000000_proj");
    apsara_log["category"] = Json::Value("8000000_category");
    apsara_log["log_type"] = Json::Value("apsara_log");
    apsara_log["log_begin_reg"] = Json::Value("\\[\\d+-\\d+-\\d+ \\d+:\\d+:\\d+.\\d+\\].*");
    apsara_log["log_path"] = Json::Value(mRootDir + PATH_SEPARATOR + "apsara_log");
    SetConfigFilePattern(apsara_log);
    apsara_log["enable"] = Json::Value(true);
    apsara_log["preserve"] = Json::Value(false);
    apsara_log["topic_format"] = Json::Value("group_topic");
    apsara_log["group_topic"] = Json::Value("xxxx");
    apsara_log["preserve_depth"] = Json::Value(2);
    apsara_log["version"] = Json::Value(1);

    Json::Value rootJson, metrics;
    rootJson["apsara_log"] = apsara_log;
    metrics["metrics"] = rootJson;

    SetConfigResponse(metrics.toStyledString());
    sleep(WAIT_CONFIG_UPDATE_INTERVAL);
    for (int i = 0; i < 20; ++i) {
        DumpLog(10, mRootDir + PATH_SEPARATOR + gPath[0], APSARA_LOG);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    WaitForFileBeenRead();
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);

    unordered_map<string, Config*>& configMap = ConfigManager::GetInstance()->mNameConfigMap;
    unordered_map<string, Config*>::iterator it;
    it = configMap.find("apsara_log");
    APSARA_TEST_TRUE(it != configMap.end());
    Config* config = it->second;
    APSARA_TEST_EQUAL(config->mProjectName, "8000000_proj");
    APSARA_TEST_EQUAL(config->mBasePath, mRootDir + PATH_SEPARATOR + "apsara_log");
    APSARA_TEST_EQUAL(config->mVersion, 1);
    APSARA_TEST_EQUAL(config->mLogType, APSARA_LOG);
    APSARA_TEST_EQUAL(config->mIsPreserve, false);
    APSARA_TEST_EQUAL(config->mTopicFormat, "group_topic");
    APSARA_TEST_EQUAL(config->mGroupTopic, "xxxx");

    APSARA_TEST_EQUAL(configMap.size(), 2);
    APSARA_TEST_EQUAL(sProjectCategoryTopicCountMap["8000000_proj_8000000_category_"], 100);
    APSARA_TEST_EQUAL(sProjectCategoryTopicCountMap["8000000_proj_8000000_category_xxxx"], 200);

    //update group topic
    apsara_log = Json::Value();
    apsara_log["project_name"] = Json::Value("8000000_proj");
    apsara_log["category"] = Json::Value("8000000_category");
    apsara_log["log_type"] = Json::Value("apsara_log");
    apsara_log["log_begin_reg"] = Json::Value("\\[\\d+-\\d+-\\d+ \\d+:\\d+:\\d+.\\d+\\].*");
    apsara_log["log_path"] = Json::Value(mRootDir + PATH_SEPARATOR + "apsara_log");
    SetConfigFilePattern(apsara_log);
    apsara_log["enable"] = Json::Value(true);
    apsara_log["preserve"] = Json::Value(false);
    apsara_log["topic_format"] = Json::Value("group_topic");
    apsara_log["group_topic"] = Json::Value("xxxx1");
    apsara_log["preserve_depth"] = Json::Value(2);
    apsara_log["version"] = Json::Value(1);
    Json::Value rootJson1, metrics1;
    rootJson1["apsara_log"] = apsara_log;
    metrics1["metrics"] = rootJson1;
    SetConfigResponse(metrics1.toStyledString());
    sleep(WAIT_CONFIG_UPDATE_INTERVAL);
    for (int i = 0; i < 20; ++i) {
        DumpLog(10, mRootDir + PATH_SEPARATOR + gPath[0], APSARA_LOG);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    WaitForFileBeenRead();
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);
    it = configMap.find("apsara_log");
    APSARA_TEST_EQUAL(it != configMap.end(), true);
    config = it->second;
    APSARA_TEST_EQUAL(config->mProjectName, "8000000_proj");
    APSARA_TEST_EQUAL(config->mBasePath, mRootDir + PATH_SEPARATOR + "apsara_log");
    APSARA_TEST_EQUAL(config->mVersion, 1);
    APSARA_TEST_EQUAL(config->mLogType, APSARA_LOG);
    APSARA_TEST_EQUAL(config->mIsPreserve, false);
    APSARA_TEST_EQUAL(config->mTopicFormat, "group_topic");
    APSARA_TEST_EQUAL(config->mGroupTopic, "xxxx1");
    APSARA_TEST_EQUAL(sProjectCategoryTopicCountMap["8000000_proj_8000000_category_"], 100);
    APSARA_TEST_EQUAL(sProjectCategoryTopicCountMap["8000000_proj_8000000_category_xxxx"], 200);
    APSARA_TEST_EQUAL(sProjectCategoryTopicCountMap["8000000_proj_8000000_category_xxxx1"], 200);

    //disable group topic
    apsara_log = Json::Value();
    apsara_log["project_name"] = Json::Value("8000000_proj");
    apsara_log["category"] = Json::Value("8000000_category");
    apsara_log["log_type"] = Json::Value("apsara_log");
    apsara_log["log_begin_reg"] = Json::Value("\\[\\d+-\\d+-\\d+ \\d+:\\d+:\\d+.\\d+\\].*");
    apsara_log["log_path"] = Json::Value(mRootDir + PATH_SEPARATOR + "apsara_log");
    SetConfigFilePattern(apsara_log);
    apsara_log["enable"] = Json::Value(true);
    apsara_log["preserve"] = Json::Value(false);
    apsara_log["topic_format"] = Json::Value("none");
    apsara_log["preserve_depth"] = Json::Value(2);
    apsara_log["version"] = Json::Value(2);
    Json::Value rootJson2;
    rootJson2["apsara_log"] = apsara_log;
    Json::Value metrics2;
    metrics2["metrics"] = rootJson2;

    SetConfigResponse(metrics2.toStyledString());
    sleep(WAIT_CONFIG_UPDATE_INTERVAL);

    for (int i = 0; i < 20; ++i) {
        DumpLog(10, mRootDir + PATH_SEPARATOR + "apsara_log", APSARA_LOG);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    WaitForFileBeenRead();
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);

    APSARA_TEST_EQUAL(sProjectCategoryTopicCountMap["8000000_proj_8000000_category_"], 300);
    APSARA_TEST_EQUAL(sProjectCategoryTopicCountMap["8000000_proj_8000000_category_xxxx"], 200);
    APSARA_TEST_EQUAL(sProjectCategoryTopicCountMap["8000000_proj_8000000_category_xxxx1"], 200);
    APSARA_TEST_TRUE(configMap.find("apsara_log") != configMap.end());

    CaseCleanup();
    RemoveConfigFile();
    LOG_INFO(sLogger, ("TestUpdateGroupTopic() end", time(NULL)));
}

void ConfigUpdatorUnittest::TestValidWildcardPath() {
    LOG_INFO(sLogger, ("TestValidWildcardPath() begin", time(NULL)));

    auto eventDispatcher = EventDispatcher::GetInstance();
    string dirs[] = {PS + "nginx" + PS + "aa" + PS + "bb",
                     PS + "app_1" + PS + "aa",
                     PS + "app_2" + PS + "aa" + PS + "bb", //ok
                     PS + "app_2" + PS + "aa" + PS + "bb" + PS + "cc", //ok
                     PS + "app_2" + PS + "aa" + PS + "bb" + PS + "cc" + PS + "dd",
                     PS + "app_3" + PS + "aa" + PS + "bb2" + PS + "cc", //ok
                     PS + "app_3" + PS + "aa" + PS + "bb2" + PS + "cc" + PS + "dd",
                     PS + "app_13" + PS + "aa" + PS + "bb" + PS + "cc"};
    for (int i = 0; i < 8; ++i) {
        if (i != 3 && i != 4)
            bfs::create_directories(bfs::path(mRootDir) / dirs[i]);
    }

    Json::Value rootJson;
    Json::Value apsara_log;
    apsara_log["project_name"] = Json::Value("9000000_proj");
    apsara_log["category"] = Json::Value("9000000_category");
    apsara_log["log_type"] = Json::Value("apsara_log");
    apsara_log["log_begin_reg"] = Json::Value("\\[\\d+-\\d+-\\d+ \\d+:\\d+:\\d+.\\d+\\].*");
    apsara_log["log_path"] = Json::Value(mRootDir + PS + "app_?" + PS + "aa" + PS + "*");
    SetConfigFilePattern(apsara_log);
    apsara_log["enable"] = Json::Value(true);
    apsara_log["preserve"] = Json::Value(true);
    apsara_log["max_depth"] = Json::Value(1);
    apsara_log["topic_format"] = Json::Value("default");
    rootJson["apsara_log"] = apsara_log;
    Json::Value metrics;
    metrics["metrics"] = rootJson;
    ofstream fout(STRING_FLAG(user_log_config).c_str());
    fout << metrics << endl;
    fout.close();

    CaseSetup();

    LOG_INFO(sLogger, ("case", "#1"));
    sleep(1);
    bfs::create_directories(bfs::path(mRootDir) / dirs[3]);
    bfs::create_directories(bfs::path(mRootDir) / dirs[4]);
    WaitForFileBeenRead();
#if defined(_MSC_VER)
    // Because polling can't register wildcard path (it's a bug), we only have this way...
    sleep(LogInput::GetInstance()->mCheckBaseDirInterval);
#endif
    usleep(100 * 1000);
    APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[0]) == eventDispatcher->mPathWdMap.end());
    APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[1]) == eventDispatcher->mPathWdMap.end());
    APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[2]) != eventDispatcher->mPathWdMap.end());
    APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[3]) != eventDispatcher->mPathWdMap.end());
    APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[4]) == eventDispatcher->mPathWdMap.end());
    APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[5]) != eventDispatcher->mPathWdMap.end());
    APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[6]) == eventDispatcher->mPathWdMap.end());
    APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[7]) == eventDispatcher->mPathWdMap.end());
    for (int i = 0; i < 20; ++i) {
        for (int j = 0; j < 8; ++j)
            DumpLog(10, mRootDir + dirs[j], APSARA_LOG);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    WaitForFileBeenRead();
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[0] + PS + "job"], 0);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[1] + PS + "job"], 0);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[2] + PS + "job"], 200);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[3] + PS + "job"], 200);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[4] + PS + "job"], 0);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[5] + PS + "job"], 200);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[6] + PS + "job"], 0);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[7] + PS + "job"], 0);

    LOG_INFO(sLogger, ("case", "#2"));
#if defined(__linux__)
    bfs::remove_all(mRootDir + dirs[3]);
#elif defined(_MSC_VER)
    // On Windows, mRootDir + dirs[3] can't be deleted if we don't move job.log out.
    auto targetPath = mRootDir + "dir3_job.log";
    bfs::rename(mRootDir + dirs[3] + PS + "job.log", targetPath);
    bfs::remove(targetPath);
    bfs::remove_all(mRootDir + dirs[3]);
#endif
    // remove will not work when file is open
    //APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[0]) == eventDispatcher->mPathWdMap.end());
    //APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[1]) == eventDispatcher->mPathWdMap.end());
    //APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[2]) != eventDispatcher->mPathWdMap.end());
    //APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[3]) == eventDispatcher->mPathWdMap.end());
    //APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[4]) == eventDispatcher->mPathWdMap.end());
    //APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[5]) != eventDispatcher->mPathWdMap.end());
    //APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[6]) == eventDispatcher->mPathWdMap.end());
    //APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[7]) == eventDispatcher->mPathWdMap.end());
    for (int i = 0; i < 20; ++i) {
        for (int j = 0; j < 8; ++j)
            DumpLog(10, mRootDir + dirs[j], APSARA_LOG);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    WaitForFileBeenRead();
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[0] + PS + "job"], 0);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[1] + PS + "job"], 0);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[2] + PS + "job"], 400);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[3] + PS + "job"], 200);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[4] + PS + "job"], 0);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[5] + PS + "job"], 400);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[6] + PS + "job"], 0);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[7] + PS + "job"], 0);

    LOG_INFO(sLogger, ("case", "#3"));
    for (int i = 0; i < 8; ++i) {
        auto targetPath = mRootDir + "dirs" + std::to_string(i) + "_job.log";
        try {
            bfs::rename(mRootDir + dirs[i] + PS + "job.log", targetPath);
            bfs::remove(targetPath);
        } catch (...) {
        }
    }
    for (int i = 0; i < 8; ++i) {
        try {
            bfs::remove_all(mRootDir + dirs[i]);
        } catch (...) {
        }
    }
    usleep(100 * 1000);
    // remove will not work when file is open
    //APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[0]) == eventDispatcher->mPathWdMap.end());
    //APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[1]) == eventDispatcher->mPathWdMap.end());
    //APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[2]) == eventDispatcher->mPathWdMap.end());
    //APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[3]) == eventDispatcher->mPathWdMap.end());
    //APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[4]) == eventDispatcher->mPathWdMap.end());
    //APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[5]) == eventDispatcher->mPathWdMap.end());
    //APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[6]) == eventDispatcher->mPathWdMap.end());
    //APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[7]) == eventDispatcher->mPathWdMap.end());
    for (int i = 0; i < 20; ++i) {
        for (int j = 0; j < 8; ++j)
            DumpLog(10, mRootDir + dirs[j], APSARA_LOG);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
#if defined(_MSC_VER)
    sleep(INT32_FLAG(polling_modify_repush_interval));
#endif
    WaitForFileBeenRead();
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[0] + PS + "job"], 0);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[1] + PS + "job"], 0);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[2] + PS + "job"], 400);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[3] + PS + "job"], 200);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[4] + PS + "job"], 0);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[5] + PS + "job"], 400);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[6] + PS + "job"], 0);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[7] + PS + "job"], 0);

    LOG_INFO(sLogger, ("case", "#4"));
    for (int i = 0; i < 8; ++i) {
        try {
            bfs::create_directories(mRootDir + dirs[i]);
        } catch (std::exception& e) {
            LOG_WARNING(sLogger,
                        ("bfs::create_directories faile", (bfs::path(mRootDir) / dirs[i]).string())("error", e.what()));
        }
    }
    sleep(INT32_FLAG(check_base_dir_interval) + 1);
    APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[0]) == eventDispatcher->mPathWdMap.end());
    APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[1]) == eventDispatcher->mPathWdMap.end());
    APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[2]) != eventDispatcher->mPathWdMap.end());
    APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[3]) != eventDispatcher->mPathWdMap.end());
    APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[4]) == eventDispatcher->mPathWdMap.end());
    APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[5]) != eventDispatcher->mPathWdMap.end());
    APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[6]) == eventDispatcher->mPathWdMap.end());
    APSARA_TEST_TRUE(eventDispatcher->mPathWdMap.find(mRootDir + dirs[7]) == eventDispatcher->mPathWdMap.end());
    for (int i = 0; i < 20; ++i) {
        for (int j = 0; j < 8; ++j)
            DumpLog(10, mRootDir + dirs[j], APSARA_LOG);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    WaitForFileBeenRead();
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[0] + PS + "job"], 0);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[1] + PS + "job"], 0);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[2] + PS + "job"], 600);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[3] + PS + "job"], 400);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[4] + PS + "job"], 0);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[5] + PS + "job"], 600);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[6] + PS + "job"], 0);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[7] + PS + "job"], 0);

    CaseCleanup();
    RemoveConfigFile();
    LOG_INFO(sLogger, ("TestValidWildcardPath() end", time(NULL)));
}

void ConfigUpdatorUnittest::TestValidWildcardPath2() {
    LOG_INFO(sLogger, ("TestValidWildcardPath2() begin", time(NULL)));

    string dirs[] = {PS + "nginx" + PS + "aa" + PS + "bb",
                     PS + "app_1" + PS + "aa",
                     PS + "app_2" + PS + "aa" + PS + "bb", //ok
                     PS + "app_2" + PS + "aa" + PS + "bb" + PS + "cc", //ok
                     PS + "app_2" + PS + "aa" + PS + "bb" + PS + "cc" + PS + "dd",
                     PS + "app_3" + PS + "aa" + PS + "bb2" + PS + "cc",
                     PS + "app_3" + PS + "aa" + PS + "bb2" + PS + "cc" + PS + "dd",
                     PS + "app_13" + PS + "aa" + PS + "bb" + PS + "cc"};
    for (int i = 0; i < 8; ++i) {
        if (i != 3 && i != 4)
            bfs::create_directories(bfs::path(mRootDir) / dirs[i]);
    }
    Json::Value rootJson;
    Json::Value apsara_log;
    apsara_log["project_name"] = Json::Value("9000000_proj");
    apsara_log["category"] = Json::Value("9000000_category");
    apsara_log["log_type"] = Json::Value("apsara_log");
    apsara_log["log_begin_reg"] = Json::Value("\\[\\d+-\\d+-\\d+ \\d+:\\d+:\\d+.\\d+\\].*");
    apsara_log["log_path"] = Json::Value(mRootDir + PS + "app_?" + PS + "*" + PS + "bb");
    SetConfigFilePattern(apsara_log);
    apsara_log["enable"] = Json::Value(true);
    apsara_log["preserve"] = Json::Value(true);
    apsara_log["max_depth"] = Json::Value(1);
    apsara_log["topic_format"] = Json::Value("default");
    rootJson["apsara_log"] = apsara_log;
    Json::Value metrics;
    metrics["metrics"] = rootJson;
    ofstream fout(STRING_FLAG(user_log_config).c_str());
    fout << metrics << endl;
    fout.close();
    CaseSetup();

    LOG_INFO(sLogger, ("case", "#1"));
    sleep(1);
    bfs::create_directories(bfs::path(mRootDir) / dirs[3]);
    bfs::create_directories(bfs::path(mRootDir) / dirs[4]);
#if defined(_MSC_VER)
    // Because polling can't register wildcard path (it's a bug), we only have this way...
    sleep(LogInput::GetInstance()->mCheckBaseDirInterval);
#endif
    usleep(100 * 1000);
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->mPathWdMap.find(mRootDir + dirs[0])
                     == EventDispatcher::GetInstance()->mPathWdMap.end());
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->mPathWdMap.find(mRootDir + dirs[1])
                     == EventDispatcher::GetInstance()->mPathWdMap.end());
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->mPathWdMap.find(mRootDir + dirs[2])
                     != EventDispatcher::GetInstance()->mPathWdMap.end());
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->mPathWdMap.find(mRootDir + dirs[3])
                     != EventDispatcher::GetInstance()->mPathWdMap.end());
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->mPathWdMap.find(mRootDir + dirs[4])
                     == EventDispatcher::GetInstance()->mPathWdMap.end());
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->mPathWdMap.find(mRootDir + dirs[5])
                     == EventDispatcher::GetInstance()->mPathWdMap.end());
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->mPathWdMap.find(mRootDir + dirs[6])
                     == EventDispatcher::GetInstance()->mPathWdMap.end());
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->mPathWdMap.find(mRootDir + dirs[7])
                     == EventDispatcher::GetInstance()->mPathWdMap.end());
    for (int i = 0; i < 20; ++i) {
        for (int j = 0; j < 8; ++j)
            DumpLog(10, mRootDir + dirs[j], APSARA_LOG);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    WaitForFileBeenRead();
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[0] + PS + "job"], 0);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[1] + PS + "job"], 0);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[2] + PS + "job"], 200);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[3] + PS + "job"], 200);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[4] + PS + "job"], 0);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[5] + PS + "job"], 0);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[6] + PS + "job"], 0);
    APSARA_TEST_EQUAL(sTopicCountMap[mRootDir + dirs[7] + PS + "job"], 0);

    CaseCleanup();

    LOG_INFO(sLogger, ("TestValidWildcardPath2() end", time(NULL)));
}

void ConfigUpdatorUnittest::TestContainerModeNormal() {
    LOG_INFO(sLogger, ("TestContainerModeNormal() begin", time(NULL)));

    // dump config to local
    Json::Value rootJson;
    Json::Value commonreg_com;
    commonreg_com["project_name"] = Json::Value("1000000_proj");
    commonreg_com["category"] = Json::Value("1000000_cateogry");
    commonreg_com["log_type"] = Json::Value("common_reg_log");
    commonreg_com["log_path"] = Json::Value(PS + "comm");
    commonreg_com["file_pattern"] = Json::Value("*.[Ll][Oo][Gg]");
    commonreg_com["enable"] = Json::Value(true);
    commonreg_com["timeformat"] = Json::Value("%d/%b/%Y:%H:%M:%S");
    commonreg_com["topic_format"] = Json::Value("none");
    Json::Value regs;
    regs.append(Json::Value("([\\d\\.]+) \\S+ \\S+ \\[(\\S+) \\S+\\] \\\"(\\w+) ([^\"]*)\\\" (\\d+) (\\d+|-)"));
    regs.append(Json::Value("([\\d\\.]+) \\S+ \\S+ \\[(\\S+) \\S+\\] \\\"(\\w+) ([^\"]*)\\\" (\\d+) (\\d+|-) "
                            "\\\"([^\"]*)\\\" \\\"([^\"]*)\\\""));
    Json::Value keys;
    keys.append(Json::Value("ip,time,method,url,status,length"));
    keys.append(Json::Value("ip,time,method,url,status,length,ref_url,browser"));
    commonreg_com["regex"] = regs;
    commonreg_com["keys"] = keys;
    commonreg_com["preserve"] = Json::Value(true);
    rootJson["commonreg.com"] = commonreg_com;

    Json::Value apsara_log;
    apsara_log["project_name"] = Json::Value("8000000_proj");
    apsara_log["category"] = Json::Value("8000000_category");
    apsara_log["log_type"] = Json::Value("apsara_log");
    apsara_log["log_begin_reg"] = Json::Value("\\[\\d+-\\d+-\\d+ \\d+:\\d+:\\d+.\\d+\\].*");
    apsara_log["log_path"] = Json::Value(PS + "apsara_log");
    apsara_log["file_pattern"] = Json::Value("*.[Ll][Oo][Gg]");
    apsara_log["enable"] = Json::Value(true);
    apsara_log["preserve"] = Json::Value(true);
    apsara_log["topic_format"] = Json::Value("none");
    rootJson["apsara_log"] = apsara_log;

    Json::Value metrics;
    metrics["metrics"] = rootJson;
    ofstream fout(STRING_FLAG(user_log_config).c_str());
    fout << metrics << endl;
    fout.close();

    bfs::remove_all(mRootDir);
    bfs::create_directories(bfs::path(mRootDir) / "mount" / "apsara_log");
    bfs::create_directories(bfs::path(mRootDir) / "mount" / "comm");
    SetupContainerModeConfig();
    CaseSetup(false);
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 2; ++j)
            DumpLog(10, mRootDir + "/mount/" + gPath[j], (LogType)j);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);

    APSARA_TEST_EQUAL(sProjectNameCountMap["1000000_proj"], 100);
    APSARA_TEST_EQUAL(sProjectNameCountMap["8000000_proj"], 100);

    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->mPathWdMap.find("/apsara_log")
                     == EventDispatcher::GetInstance()->mPathWdMap.end());
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->mPathWdMap.find("comm")
                     == EventDispatcher::GetInstance()->mPathWdMap.end());

    // check app config
    AppConfig* pAppConfig = AppConfig::GetInstance();
    APSARA_TEST_EQUAL(pAppConfig->IsContainerMode(), true);
    APSARA_TEST_EQUAL(pAppConfig->GetConfigIP(), string("1.2.3.4"));
    APSARA_TEST_EQUAL(pAppConfig->GetConfigHostName(), string("sls-zc-test"));
    APSARA_TEST_EQUAL(pAppConfig->GetContainerMountConfigPath(), string("./container_mount_test.json"));

    // check mount config
    DockerMountPaths mountPaths = ConfigManager::GetInstance()->GetMountPaths();
    APSARA_TEST_EQUAL(mountPaths.mVersion, string("0.1.0"));
    APSARA_TEST_EQUAL(mountPaths.mContainerID, string("abcdef1234567890"));
    APSARA_TEST_EQUAL(mountPaths.mContainerName, string("logtail-docker"));
    APSARA_TEST_EQUAL(mountPaths.mHostPath, mRootDir);

    APSARA_TEST_EQUAL(mountPaths.mMountPathArray.size(), (size_t)4);

    APSARA_TEST_EQUAL(mountPaths.mMountPathArray.at(0).destination, string("/"));
    APSARA_TEST_EQUAL(mountPaths.mMountPathArray.at(0).source, string("/mount"));
    APSARA_TEST_EQUAL(mountPaths.mMountPathArray.at(3).destination, string("/app_2/xxx"));
    APSARA_TEST_EQUAL(mountPaths.mMountPathArray.at(3).source, string("/xxx"));

    // test find best match
    string realPath;
    bool result = false;

    result = mountPaths.FindBestMountPath("/", realPath);
    APSARA_TEST_EQUAL(result, true);
    APSARA_TEST_EQUAL(realPath, mRootDir + string("/mount"));
    result = mountPaths.FindBestMountPath("xxx", realPath);
    APSARA_TEST_EQUAL(result, false);
    result = mountPaths.FindBestMountPath("/root", realPath);
    APSARA_TEST_EQUAL(result, true);
    APSARA_TEST_EQUAL(realPath, mRootDir + string("/mount/root"));
    result = mountPaths.FindBestMountPath("/home/admin", realPath);
    APSARA_TEST_EQUAL(result, true);
    APSARA_TEST_EQUAL(realPath, mRootDir + string("/mount/home/admin"));
    result = mountPaths.FindBestMountPath("/home/admin/logs", realPath);
    APSARA_TEST_EQUAL(result, true);
    APSARA_TEST_EQUAL(realPath, mRootDir + string("/home/admin/t4/docker/logs"));

    result = mountPaths.FindBestMountPath("/home/admin/logs/access", realPath);
    APSARA_TEST_EQUAL(result, true);
    APSARA_TEST_EQUAL(realPath, mRootDir + string("/home/admin/t4/docker/logs/access"));

    result = mountPaths.FindBestMountPath("/home/admin/log", realPath);
    APSARA_TEST_EQUAL(result, true);
    APSARA_TEST_EQUAL(realPath, mRootDir + string("/mount/home/admin/log"));

    result = mountPaths.FindBestMountPath("/app_2/x", realPath);
    APSARA_TEST_EQUAL(result, true);
    APSARA_TEST_EQUAL(realPath, mRootDir + string("/yyyy/x"));

    CaseCleanup();
    LOG_INFO(sLogger, ("TestContainerModeNormal() end", time(NULL)));
}

void ConfigUpdatorUnittest::TestContainerModeWildcardConfig() {
    LOG_INFO(sLogger, ("TestContainerModeWildcardConfig() begin", time(NULL)));
    SetupContainerModeConfig();
    /*
    string dirs[] = {
        "/nginx/aa/bb",
        "/app_1/aa",
        "/app_2/aa/bb", //ok
        "/app_2/aa/bb/cc", //ok
        "/app_2/aa/bb/cc/dd",
        "/app_2/aa/bb2/cc",
        "/app_3/aa/bb2/cc/dd",
        "/app_13/aa/bb/cc"
    };
    */

    string mountDirs[] = {mRootDir + "/mount/nginx/aa/bb",
                          mRootDir + "/mount/app_1/aa",
                          mRootDir + "/mount/app_2/aa/bb", //ok
                          mRootDir + "/mount/app_2/aa/bb/cc", //ok
                          mRootDir + "/mount/app_2/aa/bb/cc/dd",
                          mRootDir + "/mount/app_2/aa/bb2/cc",
                          mRootDir + "/mount/app_3/aa/bb2/cc/dd",
                          mRootDir + "/mount/app_3/aa/bb/cc/dd/ee"};

    for (int i = 0; i < 8; ++i) {
        if (i != 3 && i != 4) {
            cout << "mkdir -p " << mountDirs[i] << endl;
            bfs::create_directories(mountDirs[i]);
        }
    }
    Json::Value rootJson;
    Json::Value apsara_log;
    apsara_log["project_name"] = Json::Value("9000000_proj");
    apsara_log["category"] = Json::Value("9000000_category");
    apsara_log["log_type"] = Json::Value("apsara_log");
    apsara_log["log_begin_reg"] = Json::Value("\\[\\d+-\\d+-\\d+ \\d+:\\d+:\\d+.\\d+\\].*");
    apsara_log["log_path"] = Json::Value("/app_?/*/bb");
    apsara_log["file_pattern"] = Json::Value("*.[Ll][Oo][Gg]");
    apsara_log["enable"] = Json::Value(true);
    apsara_log["preserve"] = Json::Value(true);
    apsara_log["max_depth"] = Json::Value(1);
    apsara_log["topic_format"] = Json::Value("/app_(\\d+)/(\\w+)/.*");
    rootJson["apsara_log"] = apsara_log;
    Json::Value metrics;
    metrics["metrics"] = rootJson;
    ofstream fout(STRING_FLAG(user_log_config).c_str());
    fout << metrics << endl;
    fout.close();
    CaseSetup(false);
    LOG_INFO(sLogger, ("case", "#1"));
    sleep(1);
    bfs::create_directories(mountDirs[3]);
    bfs::create_directories(mountDirs[4]);
    cout << "mkdir -p " << mountDirs[3] << endl;
    cout << "mkdir -p " << mountDirs[4] << endl;
    usleep(100 * 1000);
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->mPathWdMap.find(mountDirs[0])
                     == EventDispatcher::GetInstance()->mPathWdMap.end());
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->mPathWdMap.find(mountDirs[1])
                     == EventDispatcher::GetInstance()->mPathWdMap.end());
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->mPathWdMap.find(mountDirs[2])
                     != EventDispatcher::GetInstance()->mPathWdMap.end());
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->mPathWdMap.find(mountDirs[3])
                     != EventDispatcher::GetInstance()->mPathWdMap.end());
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->mPathWdMap.find(mountDirs[4])
                     == EventDispatcher::GetInstance()->mPathWdMap.end());
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->mPathWdMap.find(mountDirs[5])
                     == EventDispatcher::GetInstance()->mPathWdMap.end());
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->mPathWdMap.find(mountDirs[6])
                     == EventDispatcher::GetInstance()->mPathWdMap.end());
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->mPathWdMap.find(mountDirs[7])
                     == EventDispatcher::GetInstance()->mPathWdMap.end());
    for (int i = 0; i < 20; ++i) {
        for (int j = 0; j < 8; ++j)
            DumpLog(10, mountDirs[j], APSARA_LOG);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);
    APSARA_TEST_EQUAL(sTopicCountMap["2_aa"], 400);

    for (int i = 0; i < 8; ++i)
        bfs::remove_all(mountDirs[i]);

    CaseCleanup();
    LOG_INFO(sLogger, ("TestContainerModeWildcardConfig() end", time(NULL)));
}

void ConfigUpdatorUnittest::TestContainerModeConfigUpdate() {
    LOG_INFO(sLogger, ("TestContainerModeConfigUpdate() begin", time(NULL)));
    {
        // dump config to local
        Json::Value rootJson;
        Json::Value commonreg_com;
        commonreg_com["project_name"] = Json::Value("1000000_proj");
        commonreg_com["category"] = Json::Value("1000000_cateogry");
        commonreg_com["log_type"] = Json::Value("common_reg_log");
        commonreg_com["log_path"] = Json::Value("/comm");
        commonreg_com["file_pattern"] = Json::Value("*.[Ll][Oo][Gg]");
        commonreg_com["enable"] = Json::Value(true);
        commonreg_com["timeformat"] = Json::Value("%d/%b/%Y:%H:%M:%S");
        commonreg_com["topic_format"] = Json::Value("none");
        Json::Value regs;
        regs.append(Json::Value("([\\d\\.]+) \\S+ \\S+ \\[(\\S+) \\S+\\] \\\"(\\w+) ([^\"]*)\\\" (\\d+) (\\d+|-)"));
        regs.append(Json::Value("([\\d\\.]+) \\S+ \\S+ \\[(\\S+) \\S+\\] \\\"(\\w+) ([^\"]*)\\\" (\\d+) (\\d+|-) "
                                "\\\"([^\"]*)\\\" \\\"([^\"]*)\\\""));
        Json::Value keys;
        keys.append(Json::Value("ip,time,method,url,status,length"));
        keys.append(Json::Value("ip,time,method,url,status,length,ref_url,browser"));
        commonreg_com["regex"] = regs;
        commonreg_com["keys"] = keys;
        commonreg_com["preserve"] = Json::Value(true);
        rootJson["commonreg.com"] = commonreg_com;

        Json::Value apsara_log;
        apsara_log["project_name"] = Json::Value("8000000_proj");
        apsara_log["category"] = Json::Value("8000000_category");
        apsara_log["log_type"] = Json::Value("apsara_log");
        apsara_log["log_begin_reg"] = Json::Value("\\[\\d+-\\d+-\\d+ \\d+:\\d+:\\d+.\\d+\\].*");
        apsara_log["log_path"] = Json::Value("/apsara_log");
        apsara_log["file_pattern"] = Json::Value("*.[Ll][Oo][Gg]");
        apsara_log["enable"] = Json::Value(true);
        apsara_log["preserve"] = Json::Value(true);
        apsara_log["topic_format"] = Json::Value("none");
        rootJson["apsara_log"] = apsara_log;

        Json::Value metrics;
        metrics["metrics"] = rootJson;
        ofstream fout(STRING_FLAG(user_log_config).c_str());
        fout << metrics << endl;
        fout.close();
    }


    bfs::remove_all(mRootDir);
    bfs::create_directories(bfs::path(mRootDir) / "mount" / "apsara_log");
    bfs::create_directories(bfs::path(mRootDir) / "mount" / "comm");
    SetupContainerModeConfig();
    CaseSetup(false);
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 2; ++j)
            DumpLog(10, mRootDir + "/mount/" + gPath[j], (LogType)j);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);

    APSARA_TEST_EQUAL(sProjectNameCountMap["1000000_proj"], 100);
    APSARA_TEST_EQUAL(sProjectNameCountMap["8000000_proj"], 100);

    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->mPathWdMap.find("/apsara_log")
                     == EventDispatcher::GetInstance()->mPathWdMap.end());
    APSARA_TEST_TRUE(EventDispatcher::GetInstance()->mPathWdMap.find("comm")
                     == EventDispatcher::GetInstance()->mPathWdMap.end());


    // check config update
    Json::Value rootJson;
    Json::Value commonreg_com;
    commonreg_com["project_name"] = Json::Value("2000000_proj");
    commonreg_com["category"] = Json::Value("2000000_category");
    commonreg_com["log_type"] = Json::Value("common_reg_log");
    commonreg_com["log_path"] = Json::Value("/comm");
    commonreg_com["file_pattern"] = Json::Value("*.[Ll][Oo][Gg]");
    commonreg_com["enable"] = Json::Value(true);
    commonreg_com["timeformat"] = Json::Value("%d/%b/%Y:%H:%M:%S");
    Json::Value regs;
    regs.append(Json::Value("([\\d\\.]+) \\S+ \\S+ \\[(\\S+) \\S+\\] \\\"(\\w+) ([^\"]*)\\\" (\\d+) (\\d+|-)"));
    regs.append(Json::Value("([\\d\\.]+) \\S+ \\S+ \\[(\\S+) \\S+\\] \\\"(\\w+) ([^\"]*)\\\" (\\d+) (\\d+|-) "
                            "\\\"([^\"]*)\\\" \\\"([^\"]*)\\\""));
    Json::Value keys;
    keys.append(Json::Value("ip,time,method,url,status,length"));
    keys.append(Json::Value("ip,time,method,url,status,length,ref_url,browser"));
    commonreg_com["regex"] = regs;
    commonreg_com["keys"] = keys;
    commonreg_com["max_depth"] = Json::Value(0);
    commonreg_com["preserve"] = Json::Value(true);
    commonreg_com["version"] = Json::Value(1);
    rootJson["commonreg.com"] = commonreg_com;

    Json::Value apsara_log;
    apsara_log["project_name"] = Json::Value("8000000_proj");
    apsara_log["category"] = Json::Value("8000000_category");
    apsara_log["log_type"] = Json::Value("apsara_log");
    apsara_log["log_begin_reg"] = Json::Value("\\[\\d+-\\d+-\\d+ \\d+:\\d+:\\d+.\\d+\\].*");
    apsara_log["log_path"] = Json::Value("/apsara_log");
    apsara_log["file_pattern"] = Json::Value("*.[Ll][Oo][Gg]");
    apsara_log["enable"] = Json::Value(true);
    apsara_log["preserve"] = Json::Value(false);
    apsara_log["preserve_depth"] = Json::Value(2);
    apsara_log["version"] = Json::Value(1);
    rootJson["apsara_log"] = apsara_log;

    Json::Value metrics;
    metrics["metrics"] = rootJson;

    SetConfigResponse(metrics.toStyledString());
    sleep(WAIT_CONFIG_UPDATE_INTERVAL);
    for (int i = 0; i < 20; ++i) {
        for (int j = 0; j < 2; ++j)
            DumpLog(10, mRootDir + "/mount/" + gPath[j], (LogType)j);
        usleep(WRITE_LOG_SLEEP_INTERVAL);
    }
    sleep(2 * INT32_FLAG(batch_send_interval) + 2);

    unordered_map<string, Config*>& configMap = ConfigManager::GetInstance()->mNameConfigMap;
    unordered_map<string, Config*>::iterator it = configMap.find("commonreg.com");
    APSARA_TEST_TRUE(it != configMap.end());
    Config* config = it->second;
    APSARA_TEST_EQUAL(config->mProjectName, "2000000_proj");
    APSARA_TEST_EQUAL(config->mBasePath, "/comm");
    APSARA_TEST_EQUAL(config->mVersion, 1);
    APSARA_TEST_EQUAL(config->mLogType, REGEX_LOG);
    APSARA_TEST_EQUAL(config->mDockerFileFlag, true);

    it = configMap.find("apsara_log");
    APSARA_TEST_TRUE(it != configMap.end());
    config = it->second;
    APSARA_TEST_EQUAL(config->mProjectName, "8000000_proj");
    APSARA_TEST_EQUAL(config->mBasePath, "/apsara_log");
    APSARA_TEST_EQUAL(config->mVersion, 1);
    APSARA_TEST_EQUAL(config->mLogType, APSARA_LOG);
    APSARA_TEST_EQUAL(config->mIsPreserve, false);
    APSARA_TEST_EQUAL(config->mDockerFileFlag, true);

    APSARA_TEST_EQUAL(configMap.size(), 2);
    APSARA_TEST_EQUAL(sProjectNameCountMap["2000000_proj"] + sProjectNameCountMap["1000000_proj"], 300);
    APSARA_TEST_EQUAL(sProjectNameCountMap["8000000_proj"], 300);

    CaseCleanup();
    LOG_INFO(sLogger, ("TestContainerModeConfigUpdate() end", time(NULL)));
}

void ConfigUpdatorUnittest::TestWithinMaxDepth() {
    // No wildcard.
    Config* cfg_1
        = new Config(PS + "abc" + PS + "de" + PS + "f", "x.log", REGEX_LOG, "a", "", "prj", true, 0, 0, "cat");
    EXPECT_EQ(cfg_1->WithinMaxDepth(PS + "abc"), false);
    EXPECT_EQ(cfg_1->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "f"), true);
    EXPECT_EQ(cfg_1->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "fx"), false);
    EXPECT_EQ(cfg_1->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "f" + PS + "ghi"), false);
    delete cfg_1;
    // To be compatible with old settings
    Config* cfg_2
        = new Config(PS + "abc" + PS + "de" + PS + "f", "x.log", REGEX_LOG, "a", "", "prj", true, 0, -1, "cat");
    EXPECT_EQ(cfg_2->WithinMaxDepth(PS + "abc"), true);
    EXPECT_EQ(cfg_2->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "f"), true);
    EXPECT_EQ(cfg_2->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "fx"), true);
    EXPECT_EQ(cfg_2->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "f" + PS + "ghi"), true);
    EXPECT_EQ(cfg_2->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "f" + PS + "ghi" + PS + "agec" + PS + "egegt"), true);
    delete cfg_2;

    Config* cfg_3
        = new Config(PS + "abc" + PS + "de" + PS + "f", "x.log", REGEX_LOG, "a", "", "prj", true, 0, 3, "cat");
    EXPECT_EQ(cfg_3->WithinMaxDepth(PS + "abc"), false);
    EXPECT_EQ(cfg_3->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "f"), true);
    EXPECT_EQ(cfg_3->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "fx"), false);
    EXPECT_EQ(cfg_3->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "f" + PS + "ghi"), true);
    EXPECT_EQ(cfg_3->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "f" + PS + "ghi" + PS + "age"), true);
    EXPECT_EQ(cfg_3->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "f" + PS + "ghi" + PS + "age" + PS + "gege"), true);
    EXPECT_EQ(
        cfg_3->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "f" + PS + "ghi" + PS + "ageg" + PS + "agege" + PS + "gae"),
        false);
    EXPECT_EQ(cfg_3->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "f" + PS + "ghi" + PS + "agege" + PS + "gege" + PS
                                    + "gegeg" + PS + "ge"),
              false);
    delete cfg_3;

    // Wildcard.
    Config* cfg_4
        = new Config(PS + "ab?" + PS + "de" + PS + "*", "x.log", REGEX_LOG, "a", "", "prj", true, 0, 0, "cat");
    EXPECT_EQ(cfg_4->WithinMaxDepth(PS + "abc"), false);
    EXPECT_EQ(cfg_4->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "f"), true);
    EXPECT_EQ(cfg_4->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "xyz"), true);
    EXPECT_EQ(cfg_4->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "f" + PS + "ghi"), false);
    delete cfg_4;
    // To be compatible with old settings.
    Config* cfg_5 = new Config(
        PS + "abc" + PS + "de?" + PS + "f*" + PS + "xyz", "x.log", REGEX_LOG, "a", "", "prj", true, 0, -1, "cat", "");
    EXPECT_EQ(cfg_5->WithinMaxDepth(PS + "abc"), true);
    EXPECT_EQ(cfg_5->WithinMaxDepth(PS + "abc" + PS + "def" + PS + "fgz"), true);
    EXPECT_EQ(cfg_5->WithinMaxDepth(PS + "abc" + PS + "def" + PS + "fgz" + PS + "xyz0"), true);
    EXPECT_EQ(cfg_5->WithinMaxDepth(PS + "abc" + PS + "def" + PS + "fgz" + PS + "xyz"), true);
    EXPECT_EQ(cfg_5->WithinMaxDepth(PS + "abc" + PS + "deef" + PS + "fgz" + PS + "xyz" + PS + "000"), true);
    EXPECT_EQ(cfg_5->WithinMaxDepth(PS + "abc" + PS + "deef" + PS + "fgz" + PS + "xyz" + PS + "000" + PS + "111" + PS
                                    + "222"),
              true);
    delete cfg_5;

    Config* cfg_6
        = new Config(PS + "abc" + PS + "d?" + PS + "f*", "x.log", REGEX_LOG, "a", "", "prj", true, 0, 3, "cat");
    EXPECT_EQ(cfg_6->WithinMaxDepth(PS + "abc"), false);
    EXPECT_EQ(cfg_6->WithinMaxDepth(PS + "abc" + PS + "de"), false);
    EXPECT_EQ(cfg_6->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "f"), true);
    EXPECT_EQ(cfg_6->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "x"), false);
    EXPECT_EQ(cfg_6->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "xf" + PS + "g"), false);
    EXPECT_EQ(cfg_6->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "fx" + PS + "ghi"), true);
    EXPECT_EQ(cfg_6->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "fx" + PS + "ghi" + PS + "age"), true);
    EXPECT_EQ(cfg_6->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "fx" + PS + "ghi" + PS + "age" + PS + "gege"), true);
    EXPECT_EQ(cfg_6->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "fx" + PS + "ghi" + PS + "ageg" + PS + "agege" + PS
                                    + "gae"),
              false);
    EXPECT_EQ(cfg_6->WithinMaxDepth(PS + "abc" + PS + "de" + PS + "fx" + PS + "ghi" + PS + "agege" + PS + "gege" + PS
                                    + "gegeg" + PS + "ge"),
              false);
    delete cfg_6;

    // Wildcard on root path, only Windows works.
    {
#if defined(__linux__)
        Config cfg("/*", "x.log", REGEX_LOG, "a", "", "prj", true, 0, 3, "cat");
        EXPECT_TRUE(cfg.WithinMaxDepth("/var"));
        BOOL_FLAG(enable_root_path_collection) = true;
        EXPECT_TRUE(cfg.WithinMaxDepth("/var"));
#elif defined(_MSC_VER)
        Config cfg("D:\\*", "x.log", REGEX_LOG, "a", "", "prj", true, 0, 3, "cat");
        EXPECT_TRUE(!cfg.WithinMaxDepth("D:\\var"));
        BOOL_FLAG(enable_root_path_collection) = true;
        EXPECT_TRUE(cfg.WithinMaxDepth("D:\\var"));
#endif
        BOOL_FLAG(enable_root_path_collection) = false;
    }
}

void ConfigUpdatorUnittest::TestParseWildcardPath() {
    Config cfg(PS, "*.log", APSARA_LOG, "x", "", "prj", true, 0, 0, "cat");

    std::string pathRoot = "";
#if defined(_MSC_VER)
    pathRoot = "DriveLetter:";
#endif

    cfg.mBasePath = pathRoot + PS + "usr" + PS + "?";
    cfg.ParseWildcardPath();
    APSARA_TEST_EQUAL(cfg.mWildcardPaths.size(), 2);
    APSARA_TEST_EQUAL(cfg.mConstWildcardPaths.size(), 1);
    APSARA_TEST_EQUAL(cfg.mWildcardPaths[0], pathRoot + PS + "usr");
    APSARA_TEST_EQUAL(cfg.mWildcardPaths[1], pathRoot + PS + "usr" + PS + "?");
    APSARA_TEST_EQUAL(cfg.mConstWildcardPaths[0], "");

    cfg.mBasePath = pathRoot + PS + "home" + PS + "tops" + PS + "?in" + PS + "python" + PS + "*" + PS + "logs";
    cfg.ParseWildcardPath();
    APSARA_TEST_EQUAL_FATAL(cfg.mWildcardPaths.size(), 5);
    APSARA_TEST_EQUAL_FATAL(cfg.mConstWildcardPaths.size(), 4);
    APSARA_TEST_EQUAL(cfg.mWildcardPaths[0], pathRoot + PS + "home" + PS + "tops");
    APSARA_TEST_EQUAL(cfg.mWildcardPaths[1], pathRoot + PS + "home" + PS + "tops" + PS + "?in");
    APSARA_TEST_EQUAL(cfg.mWildcardPaths[2], pathRoot + PS + "home" + PS + "tops" + PS + "?in" + PS + "python");
    APSARA_TEST_EQUAL(cfg.mWildcardPaths[3],
                      pathRoot + PS + "home" + PS + "tops" + PS + "?in" + PS + "python" + PS + "*");
    APSARA_TEST_EQUAL(cfg.mWildcardPaths[4],
                      pathRoot + PS + "home" + PS + "tops" + PS + "?in" + PS + "python" + PS + "*" + PS + "logs");
    APSARA_TEST_EQUAL(cfg.mConstWildcardPaths[0], "");
    APSARA_TEST_EQUAL(cfg.mConstWildcardPaths[1], "python");
    APSARA_TEST_EQUAL(cfg.mConstWildcardPaths[2], "");
    APSARA_TEST_EQUAL(cfg.mConstWildcardPaths[3], "logs");

    cfg.mBasePath = pathRoot + PS + "*";
    cfg.ParseWildcardPath();
    APSARA_TEST_EQUAL_FATAL(cfg.mWildcardPaths.size(), 2);
    APSARA_TEST_EQUAL_FATAL(cfg.mConstWildcardPaths.size(), 1);
    APSARA_TEST_EQUAL(cfg.mWildcardPaths[0], pathRoot + PS);
    APSARA_TEST_EQUAL(cfg.mWildcardPaths[1], pathRoot + PS + "*");
    APSARA_TEST_EQUAL(cfg.mConstWildcardPaths[0], "");

    cfg.mBasePath = "h?me";
    cfg.ParseWildcardPath();
    APSARA_TEST_EQUAL_FATAL(cfg.mWildcardPaths.size(), 0);
    APSARA_TEST_EQUAL_FATAL(cfg.mConstWildcardPaths.size(), 0);

    cfg.mBasePath = "*sr";
    cfg.ParseWildcardPath();
    APSARA_TEST_EQUAL_FATAL(cfg.mWildcardPaths.size(), 0);
    APSARA_TEST_EQUAL_FATAL(cfg.mConstWildcardPaths.size(), 0);

    cfg.mBasePath = pathRoot + PS;
    cfg.ParseWildcardPath();
    APSARA_TEST_EQUAL_FATAL(cfg.mWildcardPaths.size(), 0);
    APSARA_TEST_EQUAL_FATAL(cfg.mConstWildcardPaths.size(), 0);

    cfg.mBasePath = pathRoot + PS + "home" + PS + "admin" + PS + "logs";
    cfg.ParseWildcardPath();
    APSARA_TEST_EQUAL_FATAL(cfg.mWildcardPaths.size(), 0);
    APSARA_TEST_EQUAL_FATAL(cfg.mConstWildcardPaths.size(), 0);
}

void ConfigUpdatorUnittest::TestIsWildcardPathMatch() {
    Config cfg(PS, "*.log", APSARA_LOG, "x", "", "prj", true, 100, 100, "cat");

    cfg.mBasePath = PS + "usr" + PS + "?" + PS + "abc" + PS + "*" + PS + "def";
    cfg.ParseWildcardPath();
    APSARA_TEST_EQUAL(cfg.IsWildcardPathMatch(PS + "usr" + PS + "a" + PS + "abc" + PS + "def" + PS + "def"), true);
    APSARA_TEST_EQUAL(
        cfg.IsWildcardPathMatch(PS + "usr" + PS + "a" + PS + "abc" + PS + "def" + PS + "def" + PS + "ghi"), true);
    APSARA_TEST_EQUAL(cfg.IsWildcardPathMatch(PS + "usr" + PS + "aa" + PS + "abc" + PS + "def" + PS + "def"), false);
    APSARA_TEST_EQUAL(cfg.IsWildcardPathMatch(PS + "usr" + PS + "a" + PS + "abc" + PS + "def" + PS + "adef"), false);
    APSARA_TEST_EQUAL(cfg.IsWildcardPathMatch(PS + "usr" + PS + "a" + PS + "abc" + PS + "def"), false);
    APSARA_TEST_EQUAL(cfg.IsWildcardPathMatch(PS), false);
    APSARA_TEST_EQUAL(cfg.IsWildcardPathMatch(""), false);

    cfg.mBasePath = PS + "usr*";
    cfg.ParseWildcardPath();
    APSARA_TEST_EQUAL(cfg.IsWildcardPathMatch(PS + "usr"), true);
    APSARA_TEST_EQUAL(cfg.IsWildcardPathMatch(PS + "usrx"), true);
    APSARA_TEST_EQUAL(cfg.IsWildcardPathMatch(PS + "usr" + PS + "abc"), true);
    APSARA_TEST_EQUAL(cfg.IsWildcardPathMatch(PS + "usrx" + PS + "abc" + PS + "def" + PS + "ghi"), true);
    APSARA_TEST_EQUAL(cfg.IsWildcardPathMatch(PS + "us"), false);
    APSARA_TEST_EQUAL(cfg.IsWildcardPathMatch(PS), false);

    cfg.mBasePath = PS + "usr" + PS + "?";
    cfg.ParseWildcardPath();
    APSARA_TEST_EQUAL(cfg.IsWildcardPathMatch(PS + "usr" + PS + "a"), true);
    APSARA_TEST_EQUAL(cfg.IsWildcardPathMatch(PS + "usr" + PS + "b" + PS + "cef" + PS + "geg" + PS + "gege" + PS
                                              + "gegea" + PS + "egege"),
                      true);
    APSARA_TEST_EQUAL(cfg.IsWildcardPathMatch(PS + "usr" + PS + "aa"), false);
    APSARA_TEST_EQUAL(cfg.IsWildcardPathMatch(PS + "usr" + PS + "bb" + PS + "cef"), false);
    APSARA_TEST_EQUAL(cfg.IsWildcardPathMatch(PS + "usr"), false);
    APSARA_TEST_EQUAL(cfg.IsWildcardPathMatch(""), false);

    APSARA_TEST_EQUAL(cfg.IsWildcardPathMatch(PS + "usr" + PS + "b" + PS + "cef"), true);
    cfg.mMaxDepth = 0;
    APSARA_TEST_EQUAL(cfg.IsWildcardPathMatch(PS + "usr" + PS + "b" + PS + "cef"), false);
}

#if defined(__linux__)
void ConfigUpdatorUnittest::TestLoadGlobalFuseConf() {
    LOG_INFO(sLogger, ("TestLoadGlobalFuseConf() begin", time(NULL)));
    //CaseSetup();

    // test default fuse mode
    {
        system("rm -f ilogtail_config.json");

        AppConfig::GetInstance()->LoadAppConfig(STRING_FLAG(ilogtail_config));
        bool IsFuseModeEnable = BOOL_FLAG(default_global_fuse_mode);
        APSARA_TEST_EQUAL(IsFuseModeEnable, false);
    }

    // test enable fuse mode
    {
        system("rm -f ilogtail_config.json");

        Json::Value rootJson;
        rootJson["global_fuse_mode"] = Json::Value(true);

        ofstream fout(STRING_FLAG(ilogtail_config).c_str());
        fout << rootJson.toStyledString();
        fout.close();

        AppConfig::GetInstance()->LoadAppConfig(STRING_FLAG(ilogtail_config));
        bool IsFuseModeEnable = BOOL_FLAG(default_global_fuse_mode);
        APSARA_TEST_EQUAL(IsFuseModeEnable, true);
    }

    // test disable fuse mode
    {
        system("rm -f ilogtail_config.json");

        Json::Value rootJson;
        rootJson["global_fuse_mode"] = Json::Value(false);

        ofstream fout(STRING_FLAG(ilogtail_config).c_str());
        fout << rootJson.toStyledString();
        fout.close();

        AppConfig::GetInstance()->LoadAppConfig(STRING_FLAG(ilogtail_config));
        bool IsFuseModeEnable = BOOL_FLAG(default_global_fuse_mode);
        APSARA_TEST_EQUAL(IsFuseModeEnable, false);
    }

    //CaseCleanup();
    LOG_INFO(sLogger, ("TestLoadGlobalFuseConf() end", time(NULL)));
}

void ConfigUpdatorUnittest::TestCheckUlogfsEnv() {
    LOG_INFO(sLogger, ("TestCheckUlogfsEnv() begin", time(NULL)));

    APSARA_TEST_TRUE(getenv("ULOGFS_ENABLED") == NULL);

    setenv("ULOGFS_ENABLED", "true", 1);
    APSARA_TEST_EQUAL(std::string(getenv("ULOGFS_ENABLED")), std::string("true"));

    unsetenv("ULOGFS_ENABLED");
    APSARA_TEST_TRUE(getenv("ULOGFS_ENABLED") == NULL);

    bool checkUlogfsEnv;
    bool globalFuseMode;
    bool isFuseMode;

    // env ULOGFS_ENABLED not set
    {
        system("rm -rf ilogtail_config.json");
        system("rm -rf user_log_config.json");

        unsetenv("ULOGFS_ENABLED");

        checkUlogfsEnv = false;
        globalFuseMode = false;
        isFuseMode = false;

        SetupGlobalFuseMode(globalFuseMode);
        DumpCustomizedConfigToLocal(checkUlogfsEnv, isFuseMode);

        AppConfig::GetInstance()->LoadAppConfig(STRING_FLAG(ilogtail_config));
        ConfigManager::GetInstance()->LoadConfig(STRING_FLAG(user_log_config));

        APSARA_TEST_EQUAL(BOOL_FLAG(default_global_fuse_mode), false);

        auto& nameConfigMap = ConfigManager::GetInstance()->mNameConfigMap;
        APSARA_TEST_EQUAL(nameConfigMap.size(), 1);

        for (auto it = nameConfigMap.begin(); it != nameConfigMap.end(); it++) {
            Config* config = it->second;
            APSARA_TEST_EQUAL(config->mIsFuseMode, false);
        }

        BOOL_FLAG(default_global_fuse_mode) = false;
        ConfigManager::GetInstance()->mNameConfigMap.clear();
    }

    // env ULOGFS_ENABLED not set
    {
        system("rm -rf ilogtail_config.json");
        system("rm -rf user_log_config.json");

        unsetenv("ULOGFS_ENABLED");

        checkUlogfsEnv = false;
        globalFuseMode = false;
        isFuseMode = true;

        SetupGlobalFuseMode(globalFuseMode);
        DumpCustomizedConfigToLocal(checkUlogfsEnv, isFuseMode);

        AppConfig::GetInstance()->LoadAppConfig(STRING_FLAG(ilogtail_config));
        ConfigManager::GetInstance()->LoadConfig(STRING_FLAG(user_log_config));

        APSARA_TEST_EQUAL(BOOL_FLAG(default_global_fuse_mode), false);

        auto& nameConfigMap = ConfigManager::GetInstance()->mNameConfigMap;
        APSARA_TEST_EQUAL(nameConfigMap.size(), 1);

        for (auto it = nameConfigMap.begin(); it != nameConfigMap.end(); it++) {
            Config* config = it->second;
            APSARA_TEST_EQUAL(config->mIsFuseMode, false);
        }

        BOOL_FLAG(default_global_fuse_mode) = false;
        ConfigManager::GetInstance()->mNameConfigMap.clear();
    }

    // env ULOGFS_ENABLED set "true"
    {
        system("rm -rf ilogtail_config.json");
        system("rm -rf user_log_config.json");

        setenv("ULOGFS_ENABLED", "true", 1);

        checkUlogfsEnv = true;
        globalFuseMode = false;
        isFuseMode = true;

        SetupGlobalFuseMode(globalFuseMode);
        DumpCustomizedConfigToLocal(checkUlogfsEnv, isFuseMode);

        AppConfig::GetInstance()->LoadAppConfig(STRING_FLAG(ilogtail_config));
        ConfigManager::GetInstance()->LoadConfig(STRING_FLAG(user_log_config));

        APSARA_TEST_EQUAL(BOOL_FLAG(default_global_fuse_mode), false);

        auto& nameConfigMap = ConfigManager::GetInstance()->mNameConfigMap;
        APSARA_TEST_EQUAL(nameConfigMap.size(), 0);

        BOOL_FLAG(default_global_fuse_mode) = false;
        ConfigManager::GetInstance()->mNameConfigMap.clear();
    }

    // env ULOGFS_ENABLED not set
    {
        system("rm -rf ilogtail_config.json");
        system("rm -rf user_log_config.json");

        unsetenv("ULOGFS_ENABLED");

        checkUlogfsEnv = true;
        globalFuseMode = true;
        isFuseMode = true;

        SetupGlobalFuseMode(globalFuseMode);
        DumpCustomizedConfigToLocal(checkUlogfsEnv, isFuseMode);

        AppConfig::GetInstance()->LoadAppConfig(STRING_FLAG(ilogtail_config));
        ConfigManager::GetInstance()->LoadConfig(STRING_FLAG(user_log_config));

        APSARA_TEST_EQUAL(BOOL_FLAG(default_global_fuse_mode), true);

        auto& nameConfigMap = ConfigManager::GetInstance()->mNameConfigMap;
        APSARA_TEST_EQUAL(nameConfigMap.size(), 2);

        for (auto it = nameConfigMap.begin(); it != nameConfigMap.end(); it++) {
            Config* config = it->second;
            if (config->mConfigName == STRING_FLAG(fuse_customized_config_name))
                continue;

            APSARA_TEST_EQUAL(config->mIsFuseMode, true);
        }

        BOOL_FLAG(default_global_fuse_mode) = false;
        ConfigManager::GetInstance()->mNameConfigMap.clear();
    }

    RemoveConfigFile();
    LOG_INFO(sLogger, ("TestCheckUlogfsEnv() end", time(NULL)));
}
#endif

} // namespace logtail

int main(int argc, char** argv) {
    InitUnittestMain();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
