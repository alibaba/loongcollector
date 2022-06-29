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
#include <assert.h>
#if defined(__linux__)
#include <unistd.h>
#include <signal.h>
#include <sys/inotify.h>
#include <fnmatch.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <string>
#include <errno.h>
#include <typeinfo>
#include <iostream>
#include <fstream>
#include <memory>
#include <thread>
#include <set>
#include <json/json.h>
#include "common/Flags.h"
#include "common/FileSystemUtil.h"
#include "common/util.h"
#include "event_handler/EventHandler.h"
#include "polling/PollingEventQueue.h"
#include "controller/EventDispatcher.h"
#include "app_config/AppConfig.h"
#include "config_manager/ConfigManager.h"
#include "reader/LogFileReader.h"
#include "event_handler/LogInput.h"
#include "event/Event.h"
#include "logger/Logger.h"
using namespace std;

DECLARE_FLAG_STRING(ilogtail_config);
DECLARE_FLAG_STRING(user_log_config);
DECLARE_FLAG_INT32(batch_send_interval);

namespace logtail {

string gRootDir = "";
int gCaseID = 0;

std::unique_ptr<std::thread> gDispatchThreadId;

void RunningDispatcher() {
    ConfigManager::GetInstance()->RegisterHandlers();
    EventDispatcher::GetInstance()->Dispatch();
}

class ConfigMatchUnittest : public ::testing::Test {
public:
    static void SetUpTestCase() {
        gRootDir = GetProcessExecutionDir();
        if (gRootDir.at(gRootDir.size() - 1) == PATH_SEPARATOR[0])
            gRootDir.resize(gRootDir.size() - 1);
        gRootDir += PATH_SEPARATOR + "ilogtailConfigMatchTest";
        bfs::remove_all(gRootDir);
        bfs::create_directories(gRootDir);
        srand(time(NULL));
    }
    static void TearDownTestCase() { bfs::remove_all(gRootDir); }

    void CaseSetUp() {
        bfs::remove("log_file_out");
        AppConfig::GetInstance()->LoadAppConfig(STRING_FLAG(ilogtail_config));
        ConfigManager::GetInstance()->LoadConfig(STRING_FLAG(user_log_config));
        gDispatchThreadId.reset(new std::thread(RunningDispatcher));
        sleep(1);
    }
    void CaseCleanUp() {
        EventDispatcher::GetInstance()->CleanEnviroments();
        gDispatchThreadId->join();
        gDispatchThreadId = nullptr;
        ConfigManager::GetInstance()->CleanEnviroments();
        bfs::remove("user_log_config.json");
        bfs::remove("ilogtail_config.json");
        bfs::remove_all(gRootDir);
    }

    string GenerateRandomStr(int32_t minLength, int32_t maxLength) {
        int32_t length = (rand() % (maxLength - minLength + 1)) + minLength;
        string randomStr = "";
        for (int32_t i = 0; i < length; ++i) {
            //A-Z: 65-90
            int temp = (rand() % (90 - 65 + 1)) + 65;
            randomStr += (char)temp;
        }
        return randomStr;
    }

    string GenerateDirWithDepth(int32_t minDepth, int32_t maxDepth) {
        int32_t depth = (rand() % (maxDepth - minDepth + 1)) + minDepth;
        string randomDir = "";
        for (int32_t i = 0; i < depth; ++i) {
            randomDir += PATH_SEPARATOR + GenerateRandomStr(1, 1);
        }
        return randomDir;
    }

    void MockDefaultConfigs() {
        Json::Value rootJson;

        //commonreg1.com -- logpath: A/B;  preserve: false;  preserve_depth: 1
        Json::Value commonreg_com;
        commonreg_com["project_name"] = Json::Value("1000000_proj");
        commonreg_com["category"] = Json::Value("app_log");
        commonreg_com["log_type"] = Json::Value("common_reg_log");
        commonreg_com["log_path"] = Json::Value(gRootDir + "/A/B");
        commonreg_com["file_pattern"] = Json::Value("*.[Ll][Oo][Gg]");
        commonreg_com["enable"] = Json::Value(true);
        commonreg_com["timeformat"] = Json::Value("%d/%b/%Y:%H:%M:%S");
        commonreg_com["local_storage"] = Json::Value(true);
        // added by xianzhi(bowen.gbw@antfin.com)
        Json::Value customizedFieldsValue, dataIntegrityValue, lineCountValue;
        dataIntegrityValue["switch"] = Json::Value(true);
        dataIntegrityValue["project_name"] = Json::Value("data_integrity");
        dataIntegrityValue["switch"] = Json::Value("data_integrity");
        dataIntegrityValue["log_time_reg"] = Json::Value("([0-9]{4})-(0[0-9]{1}|1[0-2])-(0[0-9]{1}|[12][0-9]{1}|3[01]) "
                                                         "(0[0-9]{1}|1[0-9]{1}|2[0-3]):[0-5][0-9]{1}:([0-5][0-9]{1})");
        customizedFieldsValue["data_integrity"] = dataIntegrityValue;

        lineCountValue["switch"] = Json::Value(true);
        lineCountValue["project_name"] = Json::Value("line_count");
        lineCountValue["switch"] = Json::Value("line_count");
        customizedFieldsValue["line_count"] = lineCountValue;
        commonreg_com["customized"] = customizedFieldsValue;

        Json::Value regs;
        regs.append(Json::Value("([\\d\\.]+) \\S+ \\S+ \\[(\\S+) \\S+\\] (\\S+) (\\d+)"));
        Json::Value keys;
        keys.append(Json::Value("ip,time,nothing,seq"));
        commonreg_com["regex"] = regs;
        commonreg_com["keys"] = keys;
        commonreg_com["preserve"] = Json::Value(false);
        commonreg_com["preserve_depth"] = Json::Value(1);
        rootJson["commonreg1.com"] = commonreg_com;

        //commonreg2.com -- logpath: A/B/C/Service;  preserve: true
        Json::Value commonreg_com2;
        commonreg_com2["project_name"] = Json::Value("1000000_proj");
        commonreg_com2["category"] = Json::Value("app_log");
        commonreg_com2["log_type"] = Json::Value("common_reg_log");
        commonreg_com2["log_path"] = Json::Value(gRootDir + "/A/B/C/Service");
        commonreg_com2["file_pattern"] = Json::Value("*.[Ll][Oo][Gg]");
        commonreg_com2["enable"] = Json::Value(true);
        commonreg_com2["timeformat"] = Json::Value("%d/%b/%Y:%H:%M:%S");
        commonreg_com2["local_storage"] = Json::Value(true);

        Json::Value regs2;
        regs2.append(Json::Value("([\\d\\.]+) \\S+ \\S+ \\[(\\S+) \\S+\\] (\\S+) (\\d+)"));
        Json::Value keys2;
        keys2.append(Json::Value("ip,time,nothing,seq"));
        commonreg_com2["regex"] = regs2;
        commonreg_com2["keys"] = keys2;
        commonreg_com2["preserve"] = Json::Value(true);

        rootJson["commonreg2.com"] = commonreg_com2;

        Json::Value secondary;
        secondary["max_flow_byte_per_sec"] = Json::Value("10240000");
        secondary["max_num_of_file"] = Json::Value("10");
        secondary["enable_secondady"] = Json::Value(true);

        Json::Value metrics;
        metrics["metrics"] = rootJson;
        metrics["local_storage"] = secondary;

        ofstream fout("user_log_config.json");
        fout << metrics << endl;
        fout.close();
    }

    void MockFoceMultiConfigs() {
        Json::Value rootJson;

        //commonreg1.com -- logpath: A/B;  preserve: false;  preserve_depth: 1
        Json::Value commonreg_com;
        commonreg_com["project_name"] = Json::Value("1000000_proj");
        commonreg_com["category"] = Json::Value("app_log");
        commonreg_com["log_type"] = Json::Value("common_reg_log");
        commonreg_com["log_path"] = Json::Value(gRootDir + PATH_SEPARATOR + "A" + PATH_SEPARATOR + "B");
#if defined(_MSC_VER)
        commonreg_com["file_pattern"] = Json::Value("*.log");
#else
        commonreg_com["file_pattern"] = Json::Value("*.[Ll][Oo][Gg]");
#endif
        commonreg_com["enable"] = Json::Value(true);
        commonreg_com["timeformat"] = Json::Value("%d/%b/%Y:%H:%M:%S");
        commonreg_com["local_storage"] = Json::Value(true);

        Json::Value regs;
        regs.append(Json::Value("([\\d\\.]+) \\S+ \\S+ \\[(\\S+) \\S+\\] (\\S+) (\\d+)"));
        Json::Value keys;
        keys.append(Json::Value("ip,time,nothing,seq"));
        commonreg_com["regex"] = regs;
        commonreg_com["keys"] = keys;
        commonreg_com["preserve"] = Json::Value(false);
        commonreg_com["preserve_depth"] = Json::Value(1);
        rootJson["commonreg1.com"] = commonreg_com;

        Json::Value commonreg_com2;
        commonreg_com2 = commonreg_com;
        Json::Value advancedConfig;
        advancedConfig["force_multiconfig"] = Json::Value(false);
        commonreg_com2["advanced"] = advancedConfig;
        rootJson["commonreg2.com"] = commonreg_com2;


        Json::Value commonreg_com3;
        commonreg_com3 = commonreg_com;
        advancedConfig["force_multiconfig"] = Json::Value(true);
        commonreg_com3["advanced"] = advancedConfig;
        rootJson["commonreg3.com"] = commonreg_com3;


        Json::Value commonreg_com4;
        commonreg_com4 = commonreg_com;
        commonreg_com4["log_path"] = Json::Value("/no_this_path");
        rootJson["commonreg4.com"] = commonreg_com4;

        Json::Value metrics;
        metrics["metrics"] = rootJson;

        ofstream fout("user_log_config.json");
        fout << metrics << endl;
        fout.close();
    }

    void TestForceMultiConfig() {
        LOG_INFO(sLogger, ("TestForceMultiConfig() begin", time(NULL)));
        MockFoceMultiConfigs();

        auto& PS = PATH_SEPARATOR;

        CaseSetUp();
        {
            vector<Config*> allConfig;
            ConfigManager::GetInstance()->FindMatchWithForceFlag(allConfig, gRootDir + PS + "A" + PS + "B", "");
            APSARA_TEST_EQUAL(allConfig.size(), (size_t)2);
        }
        {
            vector<Config*> allConfig;
            ConfigManager::GetInstance()->FindMatchWithForceFlag(allConfig, gRootDir + PS + "A" + PS + "B", "test.Log");
            APSARA_TEST_EQUAL(allConfig.size(), (size_t)2);
        }
        ConfigManager::GetInstance()->mCacheFileAllConfigMap.clear();
        {
            vector<Config*> allConfig;
            ConfigManager::GetInstance()->FindAllMatch(allConfig, gRootDir + PS + "A" + PS + "B", "test.Log");
            APSARA_TEST_EQUAL(allConfig.size(), (size_t)3);
        }
        {
            vector<Config*> allConfig;
            ConfigManager::GetInstance()->FindMatchWithForceFlag(allConfig, gRootDir + PS, "");
            APSARA_TEST_EQUAL(allConfig.size(), (size_t)0);
        }

        LOG_INFO(sLogger, ("TestForceMultiConfig() done", time(NULL)));
        CaseCleanUp();
    }

    void TestConfigLongestMatch() {
        LOG_INFO(sLogger, ("TestConfigLongestMatch() begin", time(NULL)));
        // TODO: Add for Windows.
#if defined(__linux__)
        MockDefaultConfigs();

        CaseSetUp();

        string path[] = {
            //timeout
            "/A/B", //false
            "/A/B/Job", //false
            "/A/B/C", //false
            "/A/B/C/Job", //true
            "/A/B/C/Service", //false
            "/A/B/C/Service/s1", //false
            "/A/B/C/Service/s2" //false
        };

        bool result[] = {false, false, false, true, false, false, false};

        bfs::create_directories(gRootDir + path[0]);
        bfs::create_directories(gRootDir + path[4]);
        sleep(65);

        for (size_t i = 1; i < 7; i++) {
            if (i == 4)
                continue;
            bfs::create_directories(gRootDir + path[i]);
        }

        sleep(1);
        for (size_t i = 0; i < 7; i++) {
            string dir = gRootDir + path[i];
            std::unordered_map<std::string, int>::iterator pwItr
                = (EventDispatcher::GetInstance()->mPathWdMap).find(dir);
            APSARA_TEST_TRUE_DESC(pwItr != EventDispatcher::GetInstance()->mPathWdMap.end(), dir);
            if (pwItr != EventDispatcher::GetInstance()->mPathWdMap.end()) {
                unordered_map<int, time_t>::iterator wuItr
                    = EventDispatcher::GetInstance()->mWdUpdateTimeMap.find(pwItr->second);
                APSARA_TEST_EQUAL_DESC(wuItr != EventDispatcher::GetInstance()->mWdUpdateTimeMap.end(), result[i], dir);
            }
        }

        CaseCleanUp();
#endif
        LOG_INFO(sLogger, ("TestConfigLongestMatch() end", time(NULL)));
    }

    void MockLotsOfConfigs() {
        Json::Value rootJson;
        //common regex log
        for (size_t i = 0; i < 1000; i++) {
            string logName = "common_regex_log_" + ToString(i);
            Json::Value commonreg_com;
            commonreg_com["project_name"] = Json::Value("1000000_proj");
            commonreg_com["category"] = Json::Value("app_log");
            commonreg_com["log_type"] = Json::Value("common_reg_log");
            commonreg_com["log_path"] = Json::Value(gRootDir + "/common_reg_log" + GenerateDirWithDepth(1, 4));
            commonreg_com["file_pattern"] = Json::Value("*.[Ll][Oo][Gg]");
            commonreg_com["enable"] = Json::Value(true);
            commonreg_com["timeformat"] = Json::Value("%d/%b/%Y:%H:%M:%S");
            commonreg_com["local_storage"] = Json::Value(true);
            Json::Value regs;
            regs.append(Json::Value("([\\d\\.]+) \\S+ \\S+ \\[(\\S+) \\S+\\] (\\S+) (\\d+)"));
            Json::Value keys;
            keys.append(Json::Value("ip,time,nothing,seq"));
            commonreg_com["regex"] = regs;
            commonreg_com["keys"] = keys;
            commonreg_com["preserve"] = Json::Value(false);
            commonreg_com["preserve_depth"] = Json::Value(1);
            rootJson[logName] = commonreg_com;
        }
        // apsara log
        for (size_t i = 0; i < 1000; ++i) {
            string logName = "apsara_log_" + ToString(i);
            Json::Value apsara_log;
            apsara_log["project_name"] = Json::Value("9000000_proj");
            apsara_log["category"] = Json::Value(string("9000000_category_") + ToString(i));
            apsara_log["log_type"] = Json::Value("apsara_log");
            apsara_log["log_begin_reg"] = Json::Value("\\[\\d+-\\d+-\\d+ \\d+:\\d+:\\d+.\\d+\\].*");
            apsara_log["log_path"] = Json::Value(gRootDir + "/apsara_log" + GenerateDirWithDepth(1, 4));
            apsara_log["file_pattern"] = Json::Value("*.[Ll][Oo][Gg]");
            apsara_log["enable"] = Json::Value(true);
            apsara_log["preserve"] = Json::Value(true);
            apsara_log["topic_format"] = Json::Value("default");
            rootJson[logName] = apsara_log;
        }

        Json::Value secondary;
        secondary["max_flow_byte_per_sec"] = Json::Value("10240000");
        secondary["max_num_of_file"] = Json::Value("10");
        secondary["enable_secondady"] = Json::Value(true);

        Json::Value metrics;
        metrics["metrics"] = rootJson;
        metrics["local_storage"] = secondary;

        ofstream fout("user_log_config.json");
        fout << metrics << endl;
        fout.close();
    }
    void TestLotsOfConfigs() {
        LOG_INFO(sLogger, ("TestLotsOfConfigs() begin", time(NULL)));
        MockLotsOfConfigs();
        CaseSetUp();
        sleep(1);
        APSARA_TEST_EQUAL(int(ConfigManager::GetInstance()->mNameConfigMap.size()), 2000);
        CaseCleanUp();
        LOG_INFO(sLogger, ("TestLotsOfConfigs() end", time(NULL)));
    }

    void MockMaxDepthConfigs() {
        Json::Value rootJson;

        //commonreg1.com
        Json::Value commonreg_com;
        commonreg_com["project_name"] = Json::Value("1000000_proj");
        commonreg_com["category"] = Json::Value("app_log");
        commonreg_com["log_type"] = Json::Value("common_reg_log");
        commonreg_com["log_path"] = Json::Value(gRootDir + PATH_SEPARATOR + "A");
        commonreg_com["file_pattern"] = Json::Value("*.[Ll][Oo][Gg]");
        commonreg_com["enable"] = Json::Value(true);
        commonreg_com["timeformat"] = Json::Value("%d/%b/%Y:%H:%M:%S");
        commonreg_com["local_storage"] = Json::Value(true);
        Json::Value regs;
        regs.append(Json::Value("([\\d\\.]+) \\S+ \\S+ \\[(\\S+) \\S+\\] (\\S+) (\\d+)"));
        Json::Value keys;
        keys.append(Json::Value("ip,time,nothing,seq"));
        commonreg_com["regex"] = regs;
        commonreg_com["keys"] = keys;
        commonreg_com["preserve"] = Json::Value(true);
        commonreg_com["max_depth"] = -1; //default
        rootJson["commonreg1.com"] = commonreg_com;

        //commonreg2.com
        Json::Value commonreg_com2;
        commonreg_com2["project_name"] = Json::Value("1000000_proj");
        commonreg_com2["category"] = Json::Value("app_log_2");
        commonreg_com2["log_type"] = Json::Value("common_reg_log");
        commonreg_com2["log_path"] = Json::Value(gRootDir + PATH_SEPARATOR + "B");
        commonreg_com2["file_pattern"] = Json::Value("*.[Ll][Oo][Gg]");
        commonreg_com2["enable"] = Json::Value(true);
        commonreg_com2["timeformat"] = Json::Value("%d/%b/%Y:%H:%M:%S");
        commonreg_com2["local_storage"] = Json::Value(true);
        Json::Value regs2;
        regs2.append(Json::Value("([\\d\\.]+) \\S+ \\S+ \\[(\\S+) \\S+\\] (\\S+) (\\d+)"));
        Json::Value keys2;
        keys2.append(Json::Value("ip,time,nothing,seq"));
        commonreg_com2["regex"] = regs2;
        commonreg_com2["keys"] = keys2;
        commonreg_com2["preserve"] = Json::Value(true);
        commonreg_com2["max_depth"] = 0;
        rootJson["commonreg2.com"] = commonreg_com2;

        //commonreg3.com
        Json::Value commonreg_com3;
        commonreg_com3["project_name"] = Json::Value("1000000_proj");
        commonreg_com3["category"] = Json::Value("app_log_3");
        commonreg_com3["log_type"] = Json::Value("common_reg_log");
        commonreg_com3["log_path"] = Json::Value(gRootDir + PATH_SEPARATOR + "C");
        commonreg_com3["file_pattern"] = Json::Value("*.[Ll][Oo][Gg]");
        commonreg_com3["enable"] = Json::Value(true);
        commonreg_com3["timeformat"] = Json::Value("%d/%b/%Y:%H:%M:%S");
        commonreg_com3["local_storage"] = Json::Value(true);
        Json::Value regs3;
        regs3.append(Json::Value("([\\d\\.]+) \\S+ \\S+ \\[(\\S+) \\S+\\] (\\S+) (\\d+)"));
        Json::Value keys3;
        keys3.append(Json::Value("ip,time,nothing,seq"));
        commonreg_com3["regex"] = regs2;
        commonreg_com3["keys"] = keys2;
        commonreg_com3["preserve"] = Json::Value(true);
        commonreg_com3["max_depth"] = 2;
        rootJson["commonreg3.com"] = commonreg_com3;

        Json::Value secondary;
        secondary["max_flow_byte_per_sec"] = Json::Value("10240000");
        secondary["max_num_of_file"] = Json::Value("10");
        secondary["enable_secondady"] = Json::Value(true);

        Json::Value metrics;
        metrics["metrics"] = rootJson;
        metrics["local_storage"] = secondary;

        ofstream fout("user_log_config.json");
        fout << metrics << endl;
        fout.close();
    }
    void TestMaxWatchDepth() {
        LOG_INFO(sLogger, ("TestMaxWatchDepth() begin", time(NULL)));
        auto const& PS = PATH_SEPARATOR;
        string path[12] = {PS + "A",
                           PS + "B",
                           PS + "C",
                           PS + "A" + PS + "a",
                           PS + "A" + PS + "a" + PS + "aa",
                           PS + "A" + PS + "a" + PS + "aa" + PS + "aaa",
                           PS + "A" + PS + "a" + PS + "aa" + PS + "aaa" + PS + "aaaa",
                           PS + "B" + PS + "b",
                           PS + "B" + PS + "bb",
                           PS + "C" + PS + "c",
                           PS + "C" + PS + "c" + PS + "cc",
                           PS + "C" + PS + "c" + PS + "cc" + PS + "ccc"};
        bool result[12] = {true, true, true, true, true, true, true, false, false, true, true, false};

        LOG_INFO(sLogger, ("case 1", "child directory create dynamicly"));
        MockMaxDepthConfigs();
        int32_t idx = 0;
        for (; idx < 3; idx++) {
            string cmd("mkdir -p ");
            cmd.append(gRootDir).append(path[idx]);
            bfs::create_directories(bfs::path(gRootDir) / path[idx]);
            LOG_INFO(sLogger, ("cmd", cmd));
        }

        CaseSetUp();

        for (; idx < 12; idx++) {
            string cmd("mkdir -p ");
            cmd.append(gRootDir).append(path[idx]);
            bfs::create_directories(bfs::path(gRootDir) / path[idx]);
            LOG_INFO(sLogger, ("cmd", cmd));
            usleep(300 * 1000);
        }

        sleep(3);
#if defined(_MSC_VER)
        // On Windows, only polling is available, so we have to wait for a longer time.
        sleep(LogInput::GetInstance()->mCheckBaseDirInterval);
#endif
        for (idx = 0; idx < 12; ++idx) {
            string dir = gRootDir + path[idx];
            bool registered = EventDispatcher::GetInstance()->mPathWdMap.find(dir)
                != EventDispatcher::GetInstance()->mPathWdMap.end();
            APSARA_TEST_EQUAL_DESC(registered, result[idx], dir);
        }
        int32_t bakInterval = LogInput::GetInstance()->mCheckBaseDirInterval;
        LogInput::GetInstance()->mCheckBaseDirInterval = 86400;
        sleep(1);
        for (idx = 0; idx < 3; idx++) {
            EventDispatcher::GetInstance()->UnregisterAllDir(gRootDir + path[idx]);
            LOG_INFO(sLogger, ("UnregisterAllDir", gRootDir + path[idx]));
        }

        LOG_INFO(sLogger, ("case 2", "UnregisterAllDir"));

        sleep(1);
        for (idx = 0; idx < 12; ++idx) {
            string dir = gRootDir + path[idx];
            bool registered = EventDispatcher::GetInstance()->mPathWdMap.find(dir)
                == EventDispatcher::GetInstance()->mPathWdMap.end();
            APSARA_TEST_TRUE_DESC(registered, dir);
        }

        CaseCleanUp();
        sleep(3);

        LOG_INFO(sLogger, ("case 3", "directory already exists"));
        for (idx = 0; idx < 12; idx++) {
            string cmd("mkdir -p ");
            cmd.append(gRootDir).append(path[idx]);
            bfs::create_directories(bfs::path(gRootDir) / path[idx]);
            LOG_INFO(sLogger, ("cmd", cmd));
        }
        MockMaxDepthConfigs();

        CaseSetUp();
        sleep(1);
        for (idx = 0; idx < 12; ++idx) {
            string dir = gRootDir + path[idx];
            bool registered = EventDispatcher::GetInstance()->mPathWdMap.find(dir)
                != EventDispatcher::GetInstance()->mPathWdMap.end();
            APSARA_TEST_EQUAL_DESC(registered, result[idx], dir);
        }

        LogInput::GetInstance()->mCheckBaseDirInterval = bakInterval;
        CaseCleanUp();
        LOG_INFO(sLogger, ("TestMaxWatchDepth() end", time(NULL)));
    }

    void TestChinesePathAndFilePattern();

    void TestBlacklistControlInIsMatch();

    void TestBlacklistControl();

    void TestBlacklistControlWildcardBasePath();

private:
    void GenerateUserLogConfigForTestingBlacklistControl(const std::string& pathRoot, const std::string& logPath = "");

    void TestBlacklistControlCommon(const std::string& pathRoot, const std::string& logPath = "") {
        GenerateUserLogConfigForTestingBlacklistControl(pathRoot);
        std::ofstream out("ilogtail_config.json");
        out << std::string("{") << std::string("\"config_server_address\" : \"file\",")
            << std::string("\"data_server_address\" : \"file\",") << std::string("\"domain\" : \"\"")
            << std::string("}");
        out.close();
        CaseSetUp();

        // Generate log for each case.
        auto GenerateLog = [](const bfs::path& logPath) {
            bfs::create_directories(bfs::path(logPath).parent_path());

            static int32_t lastLogID = 0;
            std::ofstream(logPath.string(), std::ios_base::app) << "LOGID:" << lastLogID << std::endl;
            return lastLogID++;
        };
        std::vector<int32_t> acceptedLogIDs;

        // Rule 1 in directory blacklist.
        GenerateLog(pathRoot + "/1" + "/1.log");
        GenerateLog(pathRoot + "/1" + "/2.log");
        GenerateLog(pathRoot + "/1" + "/2" + "/3.log");
        GenerateLog(pathRoot + "/1" + "/3" + "/4" + "/4.log");
        // Rule 2 in directory blacklist.
        GenerateLog(pathRoot + "/4" + "/1" + "/1.log");
        GenerateLog(pathRoot + "/4" + "/1" + "/1" + "/1.log");
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/4" + "/1.log"));
        // Rule 3 in directory blacklist.
        GenerateLog(pathRoot + "/5" + "/a" + "/1" + "/1.log");
        GenerateLog(pathRoot + "/5" + "/a" + "/1" + "/2" + "/1.log");
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/5" + "/1" + "/1.log"));
#if defined(_MSC_VER)
        GenerateLog(pathRoot + "/5" + "/a" + "/b" + "/1" + "/1.log");
#else
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/5" + "/a" + "/b" + "/1" + "/1.log"));
#endif
        // Rule 4 in directory blacklist.
        GenerateLog(pathRoot + "/6" + "/a" + "/b" + "/1" + "/1.log");
        GenerateLog(pathRoot + "/6" + "/a" + "/b" + "/1" + "/2" + "/1.log");
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/6" + "/1" + "/1.log"));
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/6" + "/a" + "/1" + "/1.log"));
#if defined(_MSC_VER)
        GenerateLog(pathRoot + "/6" + "/a" + "/b" + "/c" + "/1" + "/1.log");
#else
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/6" + "/a" + "/b" + "/c" + "/1" + "/1.log"));
#endif
        // Rule 5 in directory blacklist.
        GenerateLog(pathRoot + "/10" + "/a" + "/1.log");
        GenerateLog(pathRoot + "/10" + "/a" + "/b" + "/1" + "/1.log");
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/10" + "/1.log"));
        // Rule 6 in directory blacklist.
        GenerateLog(pathRoot + "/11" + "/a" + "/1" + "/1.log");
        GenerateLog(pathRoot + "/11" + "/a" + "/b" + "/1" + "/1.log");
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/11" + "/1" + "/1.log"));
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/11" + "/2" + "/1.log"));
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/11" + "/a" + "/2" + "/1.log"));
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/11" + "/a" + "/b" + "/3" + "/1.log"));

        // Rule 1 in file path blacklist.
        GenerateLog(pathRoot + "/2" + "/2" + "/ignore.log");
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/2" + "/2" + "/1.log"));
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/2" + "/2" + "/2.log"));
        // Rule 2 in file path blacklist.
        GenerateLog(pathRoot + "/2" + "/3" + "/ignore.log");
        GenerateLog(pathRoot + "/2" + "/3" + "/ignore100.log");
        GenerateLog(pathRoot + "/2" + "/3" + "/ignore100_xx.log");
        acceptedLogIDs.push_back(GenerateLog((pathRoot + "/2" + "/3" + "/lignore100_xx.log")));
        // Rule 3 in file path blacklist.
        GenerateLog(pathRoot + "/7" + "/3" + "/ignore.log");
        GenerateLog(pathRoot + "/7" + "/4" + "/ignore100.log");
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/7" + "/ignore.log"));
#if defined(_MSC_VER)
        GenerateLog(pathRoot + "/7" + "/4" + "/a" + "/ignore.log");
#else
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/7" + "/4" + "/a" + "/ignore.log"));
#endif
        // Rule 4 in file path blacklist.
        GenerateLog(pathRoot + "/8" + "/3" + "/1" + "/ignore.log");
        GenerateLog(pathRoot + "/8" + "/b" + "/1" + "/ignore100.log");
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/8" + "/1" + "/ignore.log"));
#if defined(_MSC_VER)
        GenerateLog(pathRoot + "/8" + "/a" + "/b" + "/1" + "/ignore.log");
#else
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/8" + "/a" + "/b" + "/1" + "/ignore.log"));
#endif
        // Rule 5 in file path blacklist.
        GenerateLog(pathRoot + "/9" + "/a" + "/b" + "/1" + "/ignore.log");
        GenerateLog(pathRoot + "/9" + "/a" + "/d" + "/1" + "/ignore100.log");
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/9" + "/1" + "/ignore.log"));
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/9" + "/a" + "/1" + "/ignore.log"));
#if defined(_MSC_VER)
        GenerateLog(pathRoot + "/9" + "/a" + "/b" + "/c" + "/1" + "/ignore.log");
#else
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/9" + "/a" + "/b" + "/c" + "/1" + "/ignore.log"));
#endif
        // Rule 6 in file path blacklist.
        GenerateLog(pathRoot + "/12" + "/a" + "/ignore.log");
        GenerateLog(pathRoot + "/12" + "/a" + "/b" + "/c" + "/ignore100.log");
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/12" + "/ignore.log"));
        // Rule 7 in file path blacklist.
        GenerateLog(pathRoot + "/13" + "/a" + "/1" + "/ignore.log");
        GenerateLog(pathRoot + "/13" + "/a" + "/b" + "/1" + "/ignore100.log");
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/13" + "/1" + "/ignore.log"));
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/13" + "/2" + "/ignore.log"));
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/13" + "/a" + "/2" + "/ignore.log"));
        acceptedLogIDs.push_back(GenerateLog(pathRoot + "/13" + "/a" + "/b" + "/3" + "/ignore.log"));

        // Rule 1 in file name blacklist.
        GenerateLog((pathRoot + "/ignoreEverywhere.log"));
        GenerateLog((pathRoot + "/ignoreEverywhere100.log"));
        GenerateLog((pathRoot + "/1" + "/ignoreEverywhere200.log"));
        GenerateLog((pathRoot + "/2" + "/ignoreEverywhere100_xx.log"));
        GenerateLog((pathRoot + "/2" + "/3" + "/ignoreEverywhere100_xxyy.log"));

        // Normal logs.
        acceptedLogIDs.push_back(GenerateLog((pathRoot + "/lignore100_xx.log")));
        acceptedLogIDs.push_back(GenerateLog((pathRoot + "/lignore100_xx.log")));
        acceptedLogIDs.push_back(GenerateLog((pathRoot + "/2.log")));
        acceptedLogIDs.push_back(GenerateLog((pathRoot + "/2.log")));

        // Wait for a while, then parse log_file_out.
        sleep(3 * INT32_FLAG(batch_send_interval) + 1);
        std::vector<int32_t> collectedLogIDs;
        std::ifstream in("log_file_out");
        std::string line;
        while (std::getline(in, line)) {
            size_t pos = line.find("LOGID:");
            if (std::string::npos == pos)
                continue;
            collectedLogIDs.push_back(StringTo<int32_t>(SplitString(line.substr(line.find("LOGID:")), ":")[1]));
        }
        APSARA_TEST_EQUAL(acceptedLogIDs.size(), collectedLogIDs.size());
        std::sort(collectedLogIDs.begin(), collectedLogIDs.end());
        EXPECT_EQ(acceptedLogIDs, collectedLogIDs);

        CaseCleanUp();
    }
};

TEST_F(ConfigMatchUnittest, TestConfigLongestMatchWrapper) {
    TestConfigLongestMatch();
}

TEST_F(ConfigMatchUnittest, TestLotsOfConfigsWrapper) {
    TestLotsOfConfigs();
}

TEST_F(ConfigMatchUnittest, TestMaxWatchDepth) {
    TestMaxWatchDepth();
}

APSARA_UNIT_TEST_CASE(ConfigMatchUnittest, TestForceMultiConfig, 0);

// @pathRoot: used to set blacklist rule, if @logPath is empty, set log_path to it.
// @logPath: set log_path.
void ConfigMatchUnittest::GenerateUserLogConfigForTestingBlacklistControl(const std::string& pathRoot,
                                                                          const std::string& logPath) {
    const std::string PS = PATH_SEPARATOR;

    Json::Value configJSON;
    configJSON["project_name"] = Json::Value("test_project");
    configJSON["category"] = Json::Value("test_category");
    configJSON["log_type"] = Json::Value("common_reg_log");
    configJSON["log_begin_reg"] = Json::Value(".*");
    configJSON["log_path"] = Json::Value(logPath.empty() ? pathRoot : logPath);
    configJSON["file_pattern"] = "*.log";
    configJSON["enable"] = Json::Value(true);
    configJSON["preserve"] = Json::Value(true);
    configJSON["preserve_depth"] = Json::Value(0);
    configJSON["topic_format"] = Json::Value("none");
    configJSON["timeformat"] = Json::Value("%d/%b/%Y:%H:%M:%S");
    configJSON["version"] = Json::Value(1);
    Json::Value keys;
    keys.append(Json::Value("content"));
    configJSON["keys"] = keys;
    Json::Value regs;
    regs.append(Json::Value("(.*)"));
    configJSON["regex"] = regs;
    Json::Value advancedConfig;
    Json::Value blacklist;
    Json::Value dirBlacklist;
    dirBlacklist.append(Json::Value(pathRoot + PS + "1"));
    dirBlacklist.append(Json::Value(pathRoot + PS + "4" + PS + "*"));
    dirBlacklist.append(Json::Value(pathRoot + PS + "5" + PS + "*" + PS + "1"));
    dirBlacklist.append(Json::Value(pathRoot + PS + "6" + PS + "*" + PS + "*" + PS + "1"));
    dirBlacklist.append(Json::Value(pathRoot + PS + "10" + PS + "**"));
    dirBlacklist.append(Json::Value(pathRoot + PS + "11" + PS + "**" + PS + "1"));
    blacklist["dir_blacklist"] = dirBlacklist;
    Json::Value fileNameBlacklist;
    fileNameBlacklist.append(Json::Value("ignoreEverywhere*.log"));
    blacklist["filename_blacklist"] = fileNameBlacklist;
    Json::Value filePathBlacklist;
    filePathBlacklist.append(Json::Value(pathRoot + PS + "2" + PS + "2" + PS + "ignore.log"));
    filePathBlacklist.append(Json::Value(pathRoot + PS + "2" + PS + "3" + PS + "ignore*.log"));
    filePathBlacklist.append(Json::Value(pathRoot + PS + "7" + PS + "*" + PS + "ignore*.log"));
    filePathBlacklist.append(Json::Value(pathRoot + PS + "8" + PS + "*" + PS + "1" + PS + "ignore*.log"));
    filePathBlacklist.append(Json::Value(pathRoot + PS + "9" + PS + "*" + PS + "*" + PS + "1" + PS + "ignore*.log"));
    filePathBlacklist.append(Json::Value(pathRoot + PS + "12" + PS + "**" + PS + "ignore*.log"));
    filePathBlacklist.append(Json::Value(pathRoot + PS + "13" + PS + "**" + PS + "1" + PS + "ignore*.log"));
    blacklist["filepath_blacklist"] = filePathBlacklist;
    advancedConfig["blacklist"] = blacklist;
    configJSON["advanced"] = advancedConfig;
    Json::Value rootJSON, metricsJSON;
    rootJSON["common_reg_log"] = configJSON;
    metricsJSON["metrics"] = rootJSON;
    std::ofstream out;
    out.open(STRING_FLAG(user_log_config).c_str());
    out << metricsJSON.toStyledString() << std::endl;
}

// File not in docker and with normal base path.
void ConfigMatchUnittest::TestBlacklistControlInIsMatch() {
    LOG_INFO(sLogger, ("TestBlacklistControlInIsMatch() begin", time(NULL)));

    auto& PS = PATH_SEPARATOR;
    auto& pathRoot = gRootDir;
    GenerateUserLogConfigForTestingBlacklistControl(pathRoot);
    CaseSetUp();

    auto configManagerInst = ConfigManager::GetInstance();
    auto& configMap = configManagerInst->GetAllConfig();
    ASSERT_EQ(configMap.size(), 1);
    auto beginIter = configMap.begin();
    EXPECT_EQ(beginIter->first, "common_reg_log");
    auto& config = *(beginIter->second);
    EXPECT_EQ(config.mDirPathBlacklist.size(), 1);
    EXPECT_EQ(config.mWildcardDirPathBlacklist.size(), 3);
    EXPECT_EQ(config.mMLWildcardDirPathBlacklist.size(), 2);
    EXPECT_EQ(config.mFilePathBlacklist.size(), 5);
    EXPECT_EQ(config.mMLFilePathBlacklist.size(), 2);
    EXPECT_EQ(config.mFileNameBlacklist.size(), 1);

    struct Case {
        std::string path;
        std::string name;
        bool expected;

        std::string String() const {
            std::ostringstream oss;
            oss << "Case{\n"
                << "\tPath:" << path << "\n"
                << "\tName:" << name << "\n"
                << "\tExpected:" << expected << "\n"
                << "}";
            return oss.str();
        }
    };

    std::vector<Case> blacklistedCases;
#define ADD_CASE(dirPath, fileName) \
    do { \
        blacklistedCases.push_back({dirPath, fileName, false}); \
        if (std::string(fileName).empty()) \
            blacklistedCases.push_back({dirPath, "1.log", false}); \
    } while (0);

    // Rule 1 in directory blacklist.
    ADD_CASE(pathRoot + PS + "1", "");
    ADD_CASE(pathRoot + PS + "1" + PS + "2", "");
    ADD_CASE(pathRoot + PS + "1" + PS + "3" + PS + "4", "");
    // Rule 2 in directory blacklist.
    ADD_CASE(pathRoot + PS + "4" + PS + "1", "");
    // It's insignificant that IsMatch can match following two cases, because if their
    //   parent directory is filtered, they will not be discovered, no IsMatch will
    //   be called on them.
    // - {pathRoot + PS + "4" + PS + "1" + PS + "1", "", true}
    // - {pathRoot + PS + "4" + PS + "1" + PS + "1", "3.log", true}
    // Rule 3 in directory blacklist.
    ADD_CASE(pathRoot + PS + "5" + PS + "a" + PS + "1", "");
    // Rule 4 in directory blacklist.
    ADD_CASE(pathRoot + PS + "6" + PS + "a" + PS + "b" + PS + "1", "");
    // Rule 5 in directory blacklist.
    ADD_CASE(pathRoot + PS + "10" + PS + "a", "");
    ADD_CASE(pathRoot + PS + "10" + PS + "a" + PS + "b" + PS + "1", "");
    // Rule 6 in directory blacklist.
    ADD_CASE(pathRoot + PS + "11" + PS + "a" + PS + "1", "");
    ADD_CASE(pathRoot + PS + "11" + PS + "a" + PS + "b" + PS + "1", "");

    // Rule 1 in file path blacklist.
    ADD_CASE(pathRoot + PS + "2" + PS + "2", "ignore.log");
    // Rule 2 in file path blacklist.
    ADD_CASE(pathRoot + PS + "2" + PS + "3", "ignore.log");
    ADD_CASE(pathRoot + PS + "2" + PS + "3", "ignore100.log");
    ADD_CASE(pathRoot + PS + "2" + PS + "3", "ignore100_xx.log");
    // Rule 3 in file path blacklist.
    ADD_CASE(pathRoot + PS + "7" + PS + "3", "ignore.log");
    ADD_CASE(pathRoot + PS + "7" + PS + "4", "ignore100.log");
    // Rule 4 in file path blacklist.
    ADD_CASE(pathRoot + PS + "8" + PS + "3" + PS + "1", "ignore.log");
    ADD_CASE(pathRoot + PS + "8" + PS + "b" + PS + "1", "ignore100.log");
    // Rule 5 in file path blacklist.
    ADD_CASE(pathRoot + PS + "9" + PS + "a" + PS + "b" + PS + "1", "ignore.log");
    ADD_CASE(pathRoot + PS + "9" + PS + "a" + PS + "d" + PS + "1", "ignore100.log");
    // Rule 6 in file path blacklist.
    ADD_CASE(pathRoot + PS + "12" + PS + "a", "ignore.log");
    ADD_CASE(pathRoot + PS + "12" + PS + "a" + PS + "b" + PS + "c", "ignore100.log");
    // Rule 7 in file path blacklist.
    ADD_CASE(pathRoot + PS + "13" + PS + "a" + PS + "1", "ignore.log");
    ADD_CASE(pathRoot + PS + "13" + PS + "a" + PS + "b" + PS + "1", "ignore100.log");

    // Rule 1 in file name blacklist.
    ADD_CASE(pathRoot, "ignoreEverywhere.log");
    ADD_CASE(pathRoot + PS + "3" + PS + "3", "ignoreEverywhere100.log");
    ADD_CASE(pathRoot + PS + "4" + PS + "5", "ignoreEverywhere100.log");

    for (size_t i = 0; i < blacklistedCases.size(); ++i) {
        auto& c = blacklistedCases[i];
        auto result = config.IsMatch(c.path, c.name);
        LOG_INFO(sLogger, ("Blacklist case", i)("detail", c.String())("actual", result));
        APSARA_TEST_EQUAL_DESC(c.expected, result, c.String());
    }
#undef ADD_CASE

    std::vector<Case> acceptedCases;
    std::string tmpPath;
#define ADD_CASE(dirPath, fileName) \
    do { \
        acceptedCases.push_back({dirPath, fileName, true}); \
        if (std::string(fileName).empty()) \
            acceptedCases.push_back({dirPath, "1.log", true}); \
    } while (0);

    // Won't block by rule 2 in directory blacklist.
    tmpPath = pathRoot + PS + "4";
    APSARA_TEST_EQUAL(fnmatch((pathRoot + PS + "4" + PS + "*").c_str(), tmpPath.c_str(), FNM_PATHNAME), 1);
    ADD_CASE(tmpPath, "");
    // Won't block by rule 3 in directory blacklist.
    tmpPath = pathRoot + PS + "5" + PS + "1";
    APSARA_TEST_EQUAL(fnmatch((pathRoot + PS + "5" + PS + "*" + PS + "1").c_str(), tmpPath.c_str(), FNM_PATHNAME), 1);
    ADD_CASE(tmpPath, "");
    tmpPath = pathRoot + PS + "5" + PS + "a" + PS + "b" + PS + "1";
#if defined(_MSC_VER)
    APSARA_TEST_EQUAL(fnmatch((pathRoot + PS + "5" + PS + "*" + PS + "1").c_str(), tmpPath.c_str(), FNM_PATHNAME), 0);
#else
    APSARA_TEST_EQUAL(fnmatch((pathRoot + PS + "5" + PS + "*" + PS + "1").c_str(), tmpPath.c_str(), FNM_PATHNAME), 1);
    ADD_CASE(tmpPath, "");
#endif
    // Won't block by rule 4 in directory blacklist.
    tmpPath = pathRoot + PS + "6" + PS + "1";
    APSARA_TEST_EQUAL(
        fnmatch((pathRoot + PS + "6" + PS + "*" + PS + "*" + PS + "1").c_str(), tmpPath.c_str(), FNM_PATHNAME), 1);
    ADD_CASE(tmpPath, "");
    tmpPath = pathRoot + PS + "6" + PS + "a" + PS + "1";
    APSARA_TEST_EQUAL(
        fnmatch((pathRoot + PS + "6" + PS + "*" + PS + "*" + PS + "1").c_str(), tmpPath.c_str(), FNM_PATHNAME), 1);
    ADD_CASE(tmpPath, "");
    tmpPath = pathRoot + PS + "6" + PS + "a" + PS + "b" + PS + "c" + PS + "1";
#if defined(_MSC_VER)
    APSARA_TEST_EQUAL(
        fnmatch((pathRoot + PS + "6" + PS + "*" + PS + "*" + PS + "1").c_str(), tmpPath.c_str(), FNM_PATHNAME), 0);
#else
    APSARA_TEST_EQUAL(
        fnmatch((pathRoot + PS + "6" + PS + "*" + PS + "*" + PS + "1").c_str(), tmpPath.c_str(), FNM_PATHNAME), 1);
    ADD_CASE(tmpPath, "");
#endif
    // Won't block by rule 5 in directory blacklist.
    tmpPath = pathRoot + PS + "10";
    ADD_CASE(tmpPath, "");
    // Won't block by rule 6 in directory blacklist.
    tmpPath = pathRoot + PS + "11" + PS + "1";
    ADD_CASE(tmpPath, "");
    tmpPath = pathRoot + PS + "11" + PS + "2";
    ADD_CASE(tmpPath, "");
    tmpPath = pathRoot + PS + "11" + PS + "a" + PS + "2";
    ADD_CASE(tmpPath, "");
    tmpPath = pathRoot + PS + "11" + PS + "a" + PS + "b" + PS + "3";
    ADD_CASE(tmpPath, "");

    // Won't block by rule 3 in file path blacklist.
    tmpPath = pathRoot + PS + "7";
    ADD_CASE(tmpPath, "ignore.log");
#if !defined(_MSC_VER)
    tmpPath = pathRoot + PS + "7" + PS + "4" + PS + "a";
    ADD_CASE(tmpPath, "ignore.log");
#endif
    // Won't block by rule 4 in file path blacklist.
    tmpPath = pathRoot + PS + "8" + PS + "1";
    ADD_CASE(tmpPath, "ignore.log");
#if !defined(_MSC_VER)
    tmpPath = pathRoot + PS + "8" + PS + "a" + PS + "b" + PS + "1";
    ADD_CASE(tmpPath, "ignore.log");
#endif
    // Won't block by rule 5 in file path blacklist.
    tmpPath = pathRoot + PS + "9" + PS + "1";
    ADD_CASE(tmpPath, "ignore.log");
    tmpPath = pathRoot + PS + "9" + PS + "a" + PS + "1";
    ADD_CASE(tmpPath, "ignore.log");
#if !defined(_MSC_VER)
    tmpPath = pathRoot + PS + "9" + PS + "a" + PS + "b" + PS + "c" + PS + "1";
    ADD_CASE(tmpPath, "ignore.log");
#endif
    // Won't block by rule 6 in file path blacklist.
    tmpPath = pathRoot + PS + "12";
    APSARA_TEST_EQUAL(fnmatch((pathRoot + PS + "12" + PS + "**" + PS + "ignore*.log").c_str(),
                              (tmpPath + PS + "ignore.log").c_str(),
                              0),
                      1);
    ADD_CASE(tmpPath, "ignore.log");
    // Won't block by rule 7 in file path blacklist.
    tmpPath = pathRoot + PS + "13" + PS + "1";
    APSARA_TEST_EQUAL(fnmatch((pathRoot + PS + "13" + PS + "**" + PS + "1" + PS + "ignore*.log").c_str(),
                              (tmpPath + PS + "ignore.log").c_str(),
                              0),
                      1);
    ADD_CASE(tmpPath, "ignore.log");
    tmpPath = pathRoot + PS + "13" + PS + "2";
    ADD_CASE(tmpPath, "ignore.log");
    tmpPath = pathRoot + PS + "13" + PS + "a" + PS + "2";
    ADD_CASE(tmpPath, "ignore.log");
    tmpPath = pathRoot + PS + "13" + PS + "a" + PS + "b" + PS + "3";
    ADD_CASE(tmpPath, "ignore.log");

    acceptedCases.push_back({pathRoot + PS + "2", "", true});
    acceptedCases.push_back({pathRoot + PS + "3", "", true});
    acceptedCases.push_back({pathRoot + PS + "3" + PS + "5", "", true});
    acceptedCases.push_back({pathRoot + PS + "3" + PS + "5", "1.log", true});
    acceptedCases.push_back({pathRoot + PS + "3" + PS + "5", "2.log", true});
    acceptedCases.push_back({pathRoot + PS + "2" + PS + "2", "2.log", true});
    acceptedCases.push_back({pathRoot + PS + "2" + PS + "3", "3.log", true});
    acceptedCases.push_back({pathRoot + PS + "2" + PS + "3", "lignore.log", true});

    for (size_t i = 0; i < acceptedCases.size(); ++i) {
        Case& c = acceptedCases[i];
        bool result = config.IsMatch(c.path, c.name);
        LOG_INFO(sLogger, ("Accept case", i)("detail", c.String())("actual", result));
        APSARA_TEST_EQUAL_DESC(c.expected, result, c.String());
    }
#undef ADD_CASE

    CaseCleanUp();

    LOG_INFO(sLogger, ("TestBlacklistControlInIsMatch() end", time(NULL)));
}
APSARA_UNIT_TEST_CASE(ConfigMatchUnittest, TestBlacklistControlInIsMatch, 0);

// File not in docker and with normal base path.
void ConfigMatchUnittest::TestBlacklistControl() {
    LOG_INFO(sLogger, ("TestBlacklistControl() begin", time(NULL)));

    TestBlacklistControlCommon(gRootDir);

    LOG_INFO(sLogger, ("TestBlacklistControl() end", time(NULL)));
}
APSARA_UNIT_TEST_CASE(ConfigMatchUnittest, TestBlacklistControl, 0);

// File not in docker but with wildcard path.
void ConfigMatchUnittest::TestBlacklistControlWildcardBasePath() {
    LOG_INFO(sLogger, ("TestBlacklistControlWildcardBasePath() begin", time(NULL)));

    auto& PS = PATH_SEPARATOR;
    TestBlacklistControlCommon(gRootDir + PS + "test" + PS + "x", gRootDir + PS + "test*" + PS + "?");

    LOG_INFO(sLogger, ("TestBlacklistControlWildcardBasePath() end", time(NULL)));
}
APSARA_UNIT_TEST_CASE(ConfigMatchUnittest, TestBlacklistControlWildcardBasePath, 0);

void ConfigMatchUnittest::TestChinesePathAndFilePattern() {
    LOG_INFO(sLogger, ("TestChinesePathAndFilePattern() begin", time(NULL)));
    CaseSetUp();
    auto const pathRoot = gRootDir + PATH_SEPARATOR + "中文路径";
    auto const fileNames
        = std::vector<std::string>{"中文文件a.txt", "中文文件b中文.txt", "中文文件c.txt中", "中文文件d.txtt", "a.txt"};
    auto const matchResults = std::vector<bool>{true, true, false, false, false};
    bfs::create_directories(pathRoot);
    for (auto& fn : fileNames) {
        std::ofstream(pathRoot + PATH_SEPARATOR + fn) << "content";
    }

    std::string basePath = pathRoot;
    std::string filePattern = "中文文件*.txt";
#if defined(_MSC_VER)
    basePath = EncodingConverter::GetInstance()->FromACPToUTF8(basePath);
    filePattern = EncodingConverter::GetInstance()->FromACPToUTF8(filePattern);
#endif
    {
        Config cfg(basePath, filePattern, LogType::REGEX_LOG, "log", ".*", "project", true, 3, 3, "logstore");
        fsutil::Dir dir(pathRoot);
        APSARA_TEST_TRUE(dir.Open());
        while (fsutil::Entry ent = dir.ReadNext()) {
            auto fn = ent.Name();
            auto r = cfg.IsMatch(pathRoot, fn);
            for (size_t i = 0; i < fileNames.size(); ++i) {
                if (fileNames[i] == fn) {
                    APSARA_TEST_EQUAL_DESC(matchResults[i], r, fn);
                }
            }
        }
    }

    CaseCleanUp();
    LOG_INFO(sLogger, ("TestChinesePathAndFilePattern() end", time(NULL)));
}
APSARA_UNIT_TEST_CASE(ConfigMatchUnittest, TestChinesePathAndFilePattern, 0);

} // end of namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
