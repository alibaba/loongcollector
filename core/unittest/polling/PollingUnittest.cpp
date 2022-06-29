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
#include "common/Flags.h"
#include <json/json.h>
#include "controller/EventDispatcher.h"
#include "config_manager/ConfigManager.h"
#include "app_config/AppConfig.h"
#include "reader/LogFileReader.h"
#include "event_handler/EventHandler.h"
#include "processor/LogFilter.h"
#include "monitor/Monitor.h"
#include "common/util.h"
#include "common/StringTools.h"
#include "logger/Logger.h"
#include "event/Event.h"
#include "sender/Sender.h"
#if defined(__linux__)
#include <pthread.h>
#include <unistd.h>
#endif
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <string>
#include <errno.h>
#include <typeinfo>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <boost/regex.hpp>
#include "log_pb/metric.pb.h"
#include "log_pb/sls_logs.pb.h"
#include "profiler/LogtailAlarm.h"
#include "event_handler/LogInput.h"
#include "common/FileEncryption.h"
#include "common/FileSystemUtil.h"
#include <set>
#include <vector>
#include <queue>
#include "polling/PollingDirFile.h"
#include "polling/PollingModify.h"
#include "polling/PollingEventQueue.h"

using namespace std;
using namespace sls_logs;

DECLARE_FLAG_INT32(buffer_file_alive_interval);
DECLARE_FLAG_STRING(profile_project_name);
DECLARE_FLAG_BOOL(enable_mock_send);
DECLARE_FLAG_INT32(max_holded_data_size);
DECLARE_FLAG_INT32(ilogtail_discard_interval);
DECLARE_FLAG_BOOL(ilogtail_discard_old_data);
DECLARE_FLAG_INT32(merge_log_count_limit);
DECLARE_FLAG_INT32(first_read_endure_bytes);
DECLARE_FLAG_STRING(ilogtail_config);
DECLARE_FLAG_STRING(user_log_config);
DECLARE_FLAG_STRING(logtail_profile_snapshot);
DECLARE_FLAG_INT32(buffer_check_period);
DECLARE_FLAG_INT32(monitor_interval);
DECLARE_FLAG_INT32(max_buffer_num);
DECLARE_FLAG_INT32(sls_host_update_interval);
DECLARE_FLAG_STRING(default_region_name);
DECLARE_FLAG_INT32(max_client_send_error_count);
DECLARE_FLAG_INT32(client_disable_send_retry_interval);

DECLARE_FLAG_INT32(stat_notify_check_round);
DECLARE_FLAG_INT32(check_not_exist_file_dir_round);
DECLARE_FLAG_INT32(delete_dir_file_round);
DECLARE_FLAG_INT32(dirfile_check_interval_ms);
DECLARE_FLAG_INT32(dirfile_stat_count);
DECLARE_FLAG_INT32(dirfile_stat_sleep);

DECLARE_FLAG_INT32(modify_check_interval);
DECLARE_FLAG_INT32(ignore_file_modify_timeout);

DECLARE_FLAG_INT32(max_file_not_exist_times);
DECLARE_FLAG_INT32(polling_dir_upperlimit);
DECLARE_FLAG_INT32(polling_file_upperlimit);
DECLARE_FLAG_INT32(polling_dir_timeout);
DECLARE_FLAG_INT32(polling_file_timeout);
DECLARE_FLAG_INT32(polling_dir_first_watch_timeout);
DECLARE_FLAG_INT32(polling_file_first_watch_timeout);
DECLARE_FLAG_INT32(polling_check_timeout_interval);
DECLARE_FLAG_INT32(polling_modify_repush_interval);

namespace logtail {

string gRootDir = "";
int gCaseID = 0;
bool gSetTimeFlag = false;
int gSendFailType = 1; // 1:network error; 2:all error can write secondary; 3:all error will not write secondary

//warning: if you want to modify these cases, pay attention to the order
void getLogContent(char* buffer, time_t logTime, string content = "", int32_t seq = 0) {
    char timeBuffer[50];
    struct tm timeInfo;
#if defined(__linux__)
    localtime_r(&logTime, &timeInfo);
#elif defined(_MSC_VER)
    localtime_s(&timeInfo, &logTime);
#endif
    char buffer1[] = "10.7.241.21"; // - - [05/Mar/2012:15:10:59 +0800] ";
    char buffer2[]
        = "abcdefghijklmnopqrsputskjueiguwdhruwldirudsjhdklguejsldiuuwjskldgsksjdkdjfksjsdkfjsksdjfksjdkfuujss ";
    strftime(timeBuffer, sizeof(timeBuffer), " - - [%d/%b/%Y:%R:%S +0800] ", &timeInfo);
    if (content == "")
        sprintf(buffer, "%s%s%s%d\n", buffer1, timeBuffer, buffer2, seq);
    else
        sprintf(buffer, "%s%s%s%d\n", buffer1, timeBuffer, (content + " ").c_str(), seq);
}
unsigned OneJob(int logNum,
                string path,
                string jobname,
                bool jobOrNot,
                time_t logTime,
                string content = "",
                int32_t seq = 0,
                bool fixedTime = false,
                int projectID = 0) {
    //$1=num of log $2 path+name ;$3 job or not a job
    char fileExt[32];
    if (jobOrNot) {
        if (projectID == 0) {
            strcpy(fileExt, ".log");
        } else {
            sprintf(fileExt, ".log%d", projectID);
        }
    } else {
        strcpy(fileExt, ".xlog");
    }

    std::ofstream out(path + logtail::PATH_SEPARATOR + jobname + fileExt, std::ios_base::app);
    if (!out) {
        return 0;
    }
    const static unsigned BUFFER_SIZE = 1024 * 20;
    char* buffer;
    unsigned lines = 0;
    buffer = new char[BUFFER_SIZE + 1024];
    unsigned size = 0;
    for (int i = 0; i < logNum; ++i) {
        size = 0;
        memset(buffer, 0, BUFFER_SIZE + 1024);
        if (fixedTime)
            getLogContent(buffer, logTime, content, seq);
        else
            getLogContent(buffer, logTime + i, content, seq);
        size += strlen(buffer);
        ++lines;
        out.write(buffer, size);
    }
    delete[] buffer;
    return lines;
}

string GenerateRandomStr(int32_t minLength, int32_t maxLength) {
    int32_t length = (rand() % (maxLength - minLength + 1)) + minLength;
    string randomStr = "";
    for (int32_t i = 0; i < length; ++i) {
        //ascii: 33 - 126
        int temp = (rand() % (126 - 33 + 1)) + 33;
        randomStr += (char)temp;
    }
    return randomStr;
}

string MergeVectorString(vector<string>& input) {
    string output;
    for (size_t i = 0; i < input.size(); i++) {
        if (i != 0)
            output.append(" | ");
        output.append(input[i]);
    }
    return output;
}

void WriteMetrics(const Json::Value& metrics) {
    std::ofstream(STRING_FLAG(user_log_config)) << metrics << std::endl;
}

Json::Value MakeConfigJson(bool isFilter = false,
                           bool hasAliuid = false,
                           bool hasEndpoint = false,
                           int projectNum = 1,
                           bool hasTopic = false) {
    Json::Value rootJson;
    //"slb_aliyun"
    for (int i = 0; i < projectNum; ++i) {
        Json::Value commonreg_com;
        char projectName[32];
        sprintf(projectName, "%d_proj", 1000000 + i);
        commonreg_com["project_name"] = Json::Value(projectName);
        commonreg_com["category"] = Json::Value("app_log");
        commonreg_com["log_type"] = Json::Value("common_reg_log");
        commonreg_com["log_path"] = Json::Value(gRootDir);
        commonreg_com["max_depth"] = Json::Value(3);
        if (i == 0) {
#if defined(__linux__)
            commonreg_com["file_pattern"] = Json::Value("*.[Ll][Oo][Gg]");
#elif defined(_MSC_VER)
            commonreg_com["file_pattern"] = Json::Value("*.log");
#endif
        } else {
            char filePattern[32];
#if defined(__linux__)
            sprintf(filePattern, "*.[Ll][Oo][Gg]%d", i);
#elif defined(_MSC_VER)
            sprintf(filePattern, "*.log%d", i);
#endif
            commonreg_com["file_pattern"] = Json::Value(filePattern);
            commonreg_com["region"] = Json::Value(projectName);
        }
        commonreg_com["enable"] = Json::Value(true);
        commonreg_com["timeformat"] = Json::Value("%d/%b/%Y:%H:%M:%S");
        commonreg_com["local_storage"] = Json::Value(true);
        if (hasEndpoint) {
            commonreg_com["defaultEndpoint"] = Json::Value("cn-yungu-test-intranet.log.aliyuntest.com");
        }
        if (hasAliuid) {
            char aliuid[32];
            sprintf(aliuid, "123456789%d", i);
            commonreg_com["aliuid"] = Json::Value(aliuid);
        }
        Json::Value regs;
        regs.append(Json::Value("([\\d\\.]+) \\S+ \\S+ \\[(\\S+) \\S+\\] (\\S+) (\\d+)"));
        Json::Value keys;
        keys.append(Json::Value("ip,time,nothing,seq"));
        commonreg_com["regex"] = regs;
        commonreg_com["keys"] = keys;
        if (isFilter) {
            Json::Value filter_keys;
            filter_keys.append(Json::Value("nothing"));
            filter_keys.append(Json::Value("seq"));
            Json::Value filter_regs;
            filter_regs.append(Json::Value("filter.*"));
            filter_regs.append(Json::Value("5"));
            commonreg_com["filter_keys"] = filter_keys;
            commonreg_com["filter_regs"] = filter_regs;
        }
        if (hasTopic) {
            commonreg_com["topic_format"] = Json::Value(".*/(.*)");
        }
        commonreg_com["preserve"] = Json::Value(true);
        if (i == 0) {
            rootJson["commonreg.com"] = commonreg_com;
        } else {
            char title[32];
            sprintf(title, "commomreg.com_%d", i);
            rootJson[title] = commonreg_com;
        }
    }

    Json::Value secondary;
    secondary["max_flow_byte_per_sec"] = Json::Value("10240000");
    secondary["max_num_of_file"] = Json::Value("10");
    secondary["enable_secondady"] = Json::Value(true);

    Json::Value metrics;
    metrics["metrics"] = rootJson;
    metrics["local_storage"] = secondary;
    return metrics;
}

void WriteConfigJson(bool isFilter = false,
                     bool hasAliuid = false,
                     bool hasEndpoint = false,
                     int projectNum = 1,
                     bool hasTopic = false) {
    WriteMetrics(MakeConfigJson(isFilter, hasAliuid, hasEndpoint, projectNum, hasTopic));
}

bool gNetWorkStat;
bool gTestNetWorkStat = false;
int32_t gProjectNetEnableIndex = 0;
int32_t gAsynProjectSendFailCount = 0;
int32_t gSynProjectSendFailCount = 0;
int32_t gCounter;
int32_t gMessageSize;
LogGroup gRcvLogGroup;
PTMutex gRecvLogGroupLock;
int32_t gStatusCount;
LogGroup gStatusLogGroup;
PTMutex gBufferLogGroupsLock;
vector<LogGroup> gBufferLogGroups;
int32_t gAsynMockLatencyMs = 0;
int32_t gSynMockLatencyMs = 0;
bool gSendThreadRunFlag = true;

class PollingUnittest;
PollingUnittest* gPollingTest = NULL;

void RotateLogFileRecursive(const string& filePath, const string& oldSuffix, int index) {
    string newSuffix = "." + ToString(index);
    FILE* pFile = fopen((filePath + newSuffix).c_str(), "r");
    if (pFile != NULL) {
        fclose(pFile);
        RotateLogFileRecursive(filePath, newSuffix, index + 1);
    }
    rename((filePath + oldSuffix).c_str(), (filePath + newSuffix).c_str());
}


void RotateLogFile(const string& filePath) {
    RotateLogFileRecursive(filePath, "", 1);
}

class PollingUnittest : public ::testing::Test {
    const std::string& PS = PATH_SEPARATOR;

public:
    PollingUnittest() { gPollingTest = this; }

    static void SetUpTestCase() {
        printf("Test case setup.\n");
        sLogger->set_level(spdlog::level::trace);
        srand(time(NULL));
        gRootDir = GetProcessExecutionDir();
        if (PATH_SEPARATOR[0] == gRootDir.at(gRootDir.size() - 1))
            gRootDir.resize(gRootDir.size() - 1);
        gRootDir += PATH_SEPARATOR + "PollingUnittest";
    }
    static void TearDownTestCase() {}

    void CaseSetUp(bool isFilter = false,
                   bool hasAliuid = false,
                   bool hasEndpoint = false,
                   int projectNum = 1,
                   bool hasTopic = false) {
        if (bfs::exists(gRootDir)) {
            bfs::remove_all(gRootDir);
        }
        bfs::create_directories(gRootDir);
        WriteConfigJson(isFilter, hasAliuid, hasEndpoint, projectNum, hasTopic);
        commonCaseSetUp();
    }

    void commonCaseSetUp() {
        {
            PTScopedLock lock(gBufferLogGroupsLock);
            gBufferLogGroups.clear();
        }
        bfs::remove(STRING_FLAG(ilogtail_config));
        AppConfig::GetInstance()->LoadAppConfig(STRING_FLAG(ilogtail_config));
        APSARA_TEST_TRUE_FATAL(ConfigManager::GetInstance()->LoadConfig(STRING_FLAG(user_log_config)));
        sleep(1);
    }

    void CaseCleanUp() {
        ConfigManager::GetInstance()->RemoveAllConfigs();
        PollingDirFile::GetInstance()->ClearCache();
        PollingModify::GetInstance()->ClearCache();
        PollingEventQueue::GetInstance()->Clear();
        bfs::remove_all(gRootDir);
    }

    void TestPollingDirFile() {
        LOG_INFO(sLogger, ("TestPollingDirFile() begin", time(NULL)));
        CaseSetUp(false, true, true, 8);
        int32_t dirfileInterval = INT32_FLAG(dirfile_check_interval_ms);
        INT32_FLAG(dirfile_check_interval_ms) = 1000;
        PollingDirFile::GetInstance()->Start();
        sleep(INT32_FLAG(dirfile_check_interval_ms) / 1000);

        int32_t checkRound = INT32_FLAG(check_not_exist_file_dir_round);
        int32_t deleteRound = INT32_FLAG(delete_dir_file_round);
        int32_t seqNo = 5;
        string logFileDirs[] = {"a",
                                "a" + PS + "a",
                                "a" + PS + "b",
                                "b",
                                "b" + PS + "a",
                                "b" + PS + "c",
                                "b" + PS + "d",
                                "b" + PS + "a" + PS + "a"};
        string logSrcDirs[] = {"", PS + "a", PS + "a", "", PS + "b", PS + "b", PS + "b", PS + "b" + PS + "a"};
        string logObjDirs[] = {"a", "a", "b", "b", "a", "c", "d", "a"};
        string logFileNames[]
            = {"job.log", "job.log1", "job.log2", "job.log3", "job.log4", "job.log5", "job.log6", "job.log7"};
        string disableFileName = "job.xlog";

        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            bfs::create_directories(bfs::path(gRootDir) / logFileDirs[prjIndex]);
        }

        for (int round = 0; round < 10; ++round) {
            for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
                OneJob(10,
                       gRootDir + PS + logFileDirs[prjIndex],
                       "job",
                       true,
                       time(NULL),
                       "TestPollingDirFile",
                       seqNo,
                       false,
                       prjIndex);
                OneJob(10,
                       gRootDir + PS + logFileDirs[prjIndex],
                       "job",
                       false,
                       time(NULL),
                       "TestPollingDirFile",
                       seqNo,
                       false,
                       prjIndex);
            }
            usleep(20 * 1000);
        }

        sleep(3);

        APSARA_TEST_TRUE(PollingEventQueue::GetInstance()->FindEvent(gRootDir, "") == NULL);

        // Dir create events.
        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            Event* pEvent
                = PollingEventQueue::GetInstance()->FindEvent(gRootDir + logSrcDirs[prjIndex], logObjDirs[prjIndex]);
            APSARA_TEST_TRUE(pEvent != NULL);
            if (pEvent != NULL) {
                APSARA_TEST_TRUE(pEvent->IsDir());
                APSARA_TEST_TRUE(pEvent->IsCreate());
            }
            APSARA_TEST_TRUE(PollingModify::GetInstance()->FindNewFile(gRootDir + PS + logFileDirs[prjIndex],
                                                                       logFileNames[prjIndex]));
            APSARA_TEST_TRUE(
                PollingModify::GetInstance()->FindNewFile(gRootDir + PS + logFileDirs[prjIndex], disableFileName)
                == false);
        }

        APSARA_TEST_EQUAL(PollingModify::GetInstance()->mNewFileNameQueue.size(), (size_t)8);
        APSARA_TEST_EQUAL(PollingEventQueue::GetInstance()->mEventQueue.size(), (size_t)8);
        APSARA_TEST_EQUAL(PollingDirFile::GetInstance()->mDirCacheMap.size(), (size_t)9);
        APSARA_TEST_EQUAL(PollingDirFile::GetInstance()->mFileCacheMap.size(), (size_t)8);


        INT32_FLAG(check_not_exist_file_dir_round) = 1;
        INT32_FLAG(delete_dir_file_round) = 1;

        PollingEventQueue::GetInstance()->Clear();

        APSARA_TEST_EQUAL(PollingEventQueue::GetInstance()->mEventQueue.size(), (size_t)0);

        for (int prjIndex = 0; prjIndex < 3; ++prjIndex) {
            bfs::remove_all(bfs::path(gRootDir) / logFileDirs[prjIndex]);
        }
        sleep(5);
        APSARA_TEST_EQUAL(PollingEventQueue::GetInstance()->mEventQueue.size(), (size_t)3);
        APSARA_TEST_EQUAL(PollingDirFile::GetInstance()->mDirCacheMap.size(), (size_t)6);
        APSARA_TEST_EQUAL(PollingDirFile::GetInstance()->mFileCacheMap.size(), (size_t)5);
        bfs::remove_all(gRootDir);
        sleep(5);


        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            Event* pEvent
                = PollingEventQueue::GetInstance()->FindEvent(gRootDir + PS + logFileDirs[prjIndex], string());
            APSARA_TEST_TRUE(pEvent != NULL);
            if (pEvent != NULL) {
                APSARA_TEST_TRUE(pEvent->IsDir());
                APSARA_TEST_TRUE(pEvent->IsTimeout());
            }
        }
        APSARA_TEST_EQUAL(PollingEventQueue::GetInstance()->mEventQueue.size(), (size_t)9);
        APSARA_TEST_EQUAL(PollingDirFile::GetInstance()->mDirCacheMap.size(), (size_t)0);
        APSARA_TEST_EQUAL(PollingDirFile::GetInstance()->mFileCacheMap.size(), (size_t)0);

        INT32_FLAG(dirfile_check_interval_ms) = dirfileInterval;
        INT32_FLAG(check_not_exist_file_dir_round) = checkRound;
        INT32_FLAG(delete_dir_file_round) = deleteRound;

        PollingDirFile::GetInstance()->Stop();

        CaseCleanUp();
        LOG_INFO(sLogger, ("TestPollingDirFile() end", time(NULL)));
    }

    void TestPollingModify() {
        LOG_INFO(sLogger, ("TestPollingModify() begin", time(NULL)));
        CaseSetUp(false, true, true, 8);
        int32_t dirfileInterval = INT32_FLAG(dirfile_check_interval_ms);
        INT32_FLAG(dirfile_check_interval_ms) = 1000;
        PollingDirFile::GetInstance()->Start();
        PollingModify::GetInstance()->Start();
        sleep(INT32_FLAG(dirfile_check_interval_ms) / 1000);

        int32_t seqNo = 5;
        string logFileDirs[] = {"a",
                                "a" + PS + "a",
                                "a" + PS + "b",
                                "b",
                                "b" + PS + "a",
                                "b" + PS + "c",
                                "b" + PS + "d",
                                "b" + PS + "a" + PS + "a"};
        string logSrcDirs[] = {"", PS + "a", PS + "a", "", PS + "b", PS + "b", PS + "b", PS + "b" + PS + "a"};
        string logObjDirs[] = {"a", "a", "b", "b", "a", "c", "d", "a"};
        string logFileNames[]
            = {"job.log", "job.log1", "job.log2", "job.log3", "job.log4", "job.log5", "job.log6", "job.log7"};
        string disableFileName = "job.xlog";
#if defined(__linux__)
        string fifoFileName = "fifo.log";
        string softLinkFileName = "lnk.log";

        // create fifo file
        string cmd = "mkfifo " + gRootDir + "/" + fifoFileName;
        system(cmd.c_str());
        // soft link
        cmd = string("mkdir -p ") + gRootDir + "/" + "x/x/x/x";
        system(cmd.c_str());
        cmd = string("touch ") + gRootDir + "/" + "x/x/x/x/job.log";
        system(cmd.c_str());
        cmd = string("ln -s ") + gRootDir + "/" + "x/x/x/x/job.log " + gRootDir + "/" + softLinkFileName;
        system(cmd.c_str());
#endif

        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            bfs::create_directories(bfs::path(gRootDir) / logFileDirs[prjIndex]);
        }

        for (int round = 0; round < 10; ++round) {
            for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
                OneJob(10,
                       gRootDir + PS + logFileDirs[prjIndex],
                       "job",
                       true,
                       time(NULL),
                       "TestPollingDirFile",
                       seqNo,
                       false,
                       prjIndex);
                OneJob(10,
                       gRootDir + PS + logFileDirs[prjIndex],
                       "job",
                       false,
                       time(NULL),
                       "TestPollingDirFile",
                       seqNo,
                       false,
                       prjIndex);
            }
#if defined(__linux__)
            OneJob(
                10, gRootDir + "/" + softLinkFileName, "job", true, time(NULL), "TestPollingDirFile", seqNo, false, 0);
#endif
            usleep(20 * 1000);
        }

        sleep(5);

        APSARA_TEST_TRUE(PollingEventQueue::GetInstance()->FindEvent(gRootDir, "") == NULL);

        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            Event* pEvent
                = PollingEventQueue::GetInstance()->FindEvent(gRootDir + logSrcDirs[prjIndex], logObjDirs[prjIndex]);
            APSARA_TEST_TRUE(pEvent != NULL);
            if (pEvent != NULL) {
                APSARA_TEST_TRUE(pEvent->IsDir());
                APSARA_TEST_TRUE(pEvent->IsCreate());
            }
            // modify event
            pEvent = PollingEventQueue::GetInstance()->FindEvent(gRootDir + PS + logFileDirs[prjIndex],
                                                                 logFileNames[prjIndex]);
            APSARA_TEST_TRUE(pEvent != NULL);
            if (pEvent != NULL) {
                APSARA_TEST_TRUE(!pEvent->IsDir());
                APSARA_TEST_TRUE(pEvent->IsModify());
                string filePath = gRootDir + PS + logFileDirs[prjIndex] + PS + logFileNames[prjIndex];
                fsutil::PathStat stat;
                fsutil::PathStat::stat(filePath, stat);
                auto devInode = stat.GetDevInode();
                APSARA_TEST_EQUAL((uint64_t)devInode.dev, pEvent->GetDev());
                APSARA_TEST_EQUAL((uint64_t)devInode.inode, pEvent->GetInode());
            }
        }
#if defined(__linux__)
        // fifo file
        Event* pEvent = PollingEventQueue::GetInstance()->FindEvent(gRootDir, fifoFileName);
        APSARA_TEST_TRUE(pEvent == NULL);
        // soft link
        pEvent = PollingEventQueue::GetInstance()->FindEvent(gRootDir, softLinkFileName);
        APSARA_TEST_TRUE(pEvent != NULL);
        if (pEvent != NULL) {
            APSARA_TEST_TRUE(!pEvent->IsDir());
            APSARA_TEST_TRUE(pEvent->IsModify());
            string filePath = gRootDir + "/" + softLinkFileName;
            struct stat fStat;
            stat(filePath.c_str(), &fStat);
            APSARA_TEST_EQUAL((uint64_t)fStat.st_dev, pEvent->GetDev());
            APSARA_TEST_EQUAL((uint64_t)fStat.st_ino, pEvent->GetInode());
        }
#endif

        PollingEventQueue::GetInstance()->Clear();

#if defined(__linux__)
        APSARA_TEST_EQUAL(PollingModify::GetInstance()->mModifyCacheMap.size(), (size_t)8 + 1); // lnk.log has event
#else
        APSARA_TEST_EQUAL(PollingModify::GetInstance()->mModifyCacheMap.size(), (size_t)8);
#endif

        int32_t fileNotExistTimes = INT32_FLAG(max_file_not_exist_times);
        INT32_FLAG(max_file_not_exist_times) = 1;

        for (int prjIndex = 0; prjIndex < 3; ++prjIndex) {
            bfs::remove_all(bfs::path(gRootDir) / logFileDirs[prjIndex]);
        }
        sleep(5);
        APSARA_TEST_EQUAL(PollingEventQueue::GetInstance()->mEventQueue.size(), (size_t)3);
#if defined(__linux__)
        APSARA_TEST_EQUAL(PollingModify::GetInstance()->mModifyCacheMap.size(), (size_t)5 + 1); // lnk.log has event
#else
        APSARA_TEST_EQUAL(PollingModify::GetInstance()->mModifyCacheMap.size(), (size_t)5);
#endif


        bfs::remove_all(gRootDir);
        sleep(5);
#if defined(__linux__)
        APSARA_TEST_EQUAL(PollingEventQueue::GetInstance()->mEventQueue.size(), (size_t)8 + 1); // lnk.log has event
#else
        APSARA_TEST_EQUAL(PollingEventQueue::GetInstance()->mEventQueue.size(), (size_t)8);
#endif
        APSARA_TEST_EQUAL(PollingModify::GetInstance()->mModifyCacheMap.size(), (size_t)0);


        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            // Delete event
            Event* pEvent = PollingEventQueue::GetInstance()->FindEvent(gRootDir + PS + logFileDirs[prjIndex],
                                                                        logFileNames[prjIndex]);
            APSARA_TEST_TRUE(pEvent != NULL);
            if (pEvent != NULL) {
                APSARA_TEST_TRUE(!pEvent->IsDir());
                APSARA_TEST_TRUE(pEvent->IsDeleted());
            }
        }
#if defined(__linux__)
        // fifo file doesnot have event
        pEvent = PollingEventQueue::GetInstance()->FindEvent(gRootDir, fifoFileName);
        APSARA_TEST_TRUE(pEvent == NULL);
        // soft link has event when deleted
        pEvent = PollingEventQueue::GetInstance()->FindEvent(gRootDir, softLinkFileName);
        APSARA_TEST_TRUE(pEvent != NULL);
        if (pEvent != NULL) {
            APSARA_TEST_TRUE(!pEvent->IsDir());
            APSARA_TEST_TRUE(pEvent->IsDeleted());
        }
#endif

        INT32_FLAG(dirfile_check_interval_ms) = dirfileInterval;
        INT32_FLAG(max_file_not_exist_times) = fileNotExistTimes;

        PollingModify::GetInstance()->Stop();
        PollingDirFile::GetInstance()->Stop();

        CaseCleanUp();
        LOG_INFO(sLogger, ("TestPollingModify() end", time(NULL)));
    }

    void TestPollingSoftLink() {
        LOG_INFO(sLogger, ("TestPollingSoftLink() begin", time(NULL)));
#if defined(__linux__) // TODO: Add for Windows.
        CaseSetUp(false, true, true, 8);
        PollingModify::GetInstance()->Start();
        PollingDirFile::GetInstance()->Start();
        usleep(100);
        int32_t dirfileInterval = INT32_FLAG(dirfile_check_interval_ms);
        int32_t checkRound = INT32_FLAG(check_not_exist_file_dir_round);
        int32_t deleteRound = INT32_FLAG(delete_dir_file_round);
        INT32_FLAG(dirfile_check_interval_ms) = 1000;
        int32_t seqNo = 5;
        string linkFilePrefixDir("PollingLink");
        string logFileDirs[] = {"a", "a/a", "a/b", "b", "b/a", "b/c", "b/d", "b/a/a"};
        string logSrcDirs[] = {"", "/a", "/a", "", "/b", "/b", "/b", "/b/a"};
        string logObjDirs[] = {"a", "a", "b", "b", "a", "c", "d", "a"};
        string logFileNames[]
            = {"job.log", "job.log1", "job.log2", "job.log3", "job.log4", "job.log5", "job.log6", "job.log7"};
        string disableFileName = "job.xlog";

        string exeDir = GetProcessExecutionDir();
        exeDir.resize(exeDir.size() - 1);
        system(string("mkdir " + exeDir + "/" + linkFilePrefixDir).c_str());
        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            string cmd("mkdir ");
            system((cmd + exeDir + "/" + linkFilePrefixDir + "/" + logFileDirs[prjIndex]).c_str());
        }
        usleep(100 * 1000);
        system(string("ln -s " + exeDir + "/" + linkFilePrefixDir + "/a" + "  " + gRootDir + "/a").c_str());
        system(string("ln -s " + exeDir + "/" + linkFilePrefixDir + "/b" + "  " + gRootDir + "/b").c_str());

        for (int round = 0; round < 1; ++round) {
            for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
                OneJob(10,
                       exeDir + "/" + linkFilePrefixDir,
                       "job",
                       true,
                       time(NULL),
                       "TestPollingDirFile",
                       seqNo,
                       false,
                       prjIndex);
                OneJob(10,
                       exeDir + "/" + linkFilePrefixDir,
                       "job",
                       false,
                       time(NULL),
                       "TestPollingDirFile",
                       seqNo,
                       false,
                       prjIndex);
            }
            usleep(10 * 1000);
        }
        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            string cmd("ln -s ");
            system((cmd + exeDir + "/" + linkFilePrefixDir + "/" + logFileNames[prjIndex] + " " + gRootDir + "/"
                    + logFileDirs[prjIndex] + "/" + logFileNames[prjIndex])
                       .c_str());
            system((cmd + exeDir + "/" + linkFilePrefixDir + "/" + disableFileName + " " + gRootDir + "/"
                    + logFileDirs[prjIndex] + "/" + disableFileName)
                       .c_str());
        }

        sleep(3);


        APSARA_TEST_TRUE(PollingEventQueue::GetInstance()->FindEvent(gRootDir, "") == NULL);

        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            Event* pEvent
                = PollingEventQueue::GetInstance()->FindEvent(gRootDir + logSrcDirs[prjIndex], logObjDirs[prjIndex]);
            APSARA_TEST_TRUE(pEvent != NULL);
            if (pEvent != NULL) {
                APSARA_TEST_TRUE(pEvent->IsDir());
                APSARA_TEST_TRUE(pEvent->IsCreate());
            }
            // modify event
            pEvent = PollingEventQueue::GetInstance()->FindEvent(gRootDir + "/" + logFileDirs[prjIndex],
                                                                 logFileNames[prjIndex]);
            APSARA_TEST_TRUE(pEvent != NULL);
            if (pEvent != NULL) {
                APSARA_TEST_TRUE(!pEvent->IsDir());
                APSARA_TEST_TRUE(pEvent->IsModify());
                string filePath = exeDir + "/" + linkFilePrefixDir + "/" + logFileNames[prjIndex];
                struct stat fStat;
                stat(filePath.c_str(), &fStat);
                APSARA_TEST_EQUAL((uint64_t)fStat.st_dev, pEvent->GetDev());
                APSARA_TEST_EQUAL((uint64_t)fStat.st_ino, pEvent->GetInode());
            }
        }

        PollingEventQueue::GetInstance()->Clear();

        APSARA_TEST_EQUAL(PollingModify::GetInstance()->mModifyCacheMap.size(), (size_t)8);

        APSARA_TEST_EQUAL(PollingDirFile::GetInstance()->mDirCacheMap.size(), (size_t)9);
        APSARA_TEST_EQUAL(PollingDirFile::GetInstance()->mFileCacheMap.size(), (size_t)16);

        int32_t fileNotExistTimes = INT32_FLAG(max_file_not_exist_times);
        INT32_FLAG(max_file_not_exist_times) = 1;
        INT32_FLAG(check_not_exist_file_dir_round) = 1;
        INT32_FLAG(delete_dir_file_round) = 1;

        for (int prjIndex = 0; prjIndex < 3; ++prjIndex) {
            string cmd("rm -fr ");
            system((cmd + exeDir + "/" + linkFilePrefixDir + "/" + logFileDirs[prjIndex]).c_str());
        }
        sleep(5);
        APSARA_TEST_EQUAL(PollingEventQueue::GetInstance()->mEventQueue.size(), (size_t)6);
        APSARA_TEST_EQUAL(PollingModify::GetInstance()->mModifyCacheMap.size(), (size_t)5);

        APSARA_TEST_EQUAL(PollingDirFile::GetInstance()->mDirCacheMap.size(), (size_t)6);
        APSARA_TEST_EQUAL(PollingDirFile::GetInstance()->mFileCacheMap.size(), (size_t)10);


        string cmd("rm -fr ");
        system((cmd + exeDir + "/" + linkFilePrefixDir).c_str());
        sleep(5);
        APSARA_TEST_EQUAL(PollingEventQueue::GetInstance()->mEventQueue.size(), (size_t)16);
        APSARA_TEST_EQUAL(PollingModify::GetInstance()->mModifyCacheMap.size(), (size_t)0);

        APSARA_TEST_EQUAL(PollingDirFile::GetInstance()->mDirCacheMap.size(), (size_t)1);
        APSARA_TEST_EQUAL(PollingDirFile::GetInstance()->mFileCacheMap.size(), (size_t)0);

        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            // Delete event
            Event* pEvent = PollingEventQueue::GetInstance()->FindEvent(gRootDir + "/" + logFileDirs[prjIndex],
                                                                        logFileNames[prjIndex]);
            APSARA_TEST_TRUE(pEvent != NULL);
            if (pEvent != NULL) {
                APSARA_TEST_TRUE(!pEvent->IsDir());
                APSARA_TEST_TRUE(pEvent->IsDeleted());
            }
            pEvent = PollingEventQueue::GetInstance()->FindEvent(gRootDir + "/" + logFileDirs[prjIndex], string());
            APSARA_TEST_TRUE(pEvent != NULL);
            if (pEvent != NULL) {
                APSARA_TEST_TRUE(pEvent->IsDir());
                APSARA_TEST_TRUE(pEvent->IsTimeout());
            }
        }

        INT32_FLAG(dirfile_check_interval_ms) = dirfileInterval;
        INT32_FLAG(check_not_exist_file_dir_round) = checkRound;
        INT32_FLAG(delete_dir_file_round) = deleteRound;
        INT32_FLAG(max_file_not_exist_times) = fileNotExistTimes;

        PollingDirFile::GetInstance()->Stop();
        PollingModify::GetInstance()->Stop();
        CaseCleanUp();
#endif
        LOG_INFO(sLogger, ("TestPollingSoftLink() end", time(NULL)));
    }

    void TestPollingMultiLevelSoftLink() {
        LOG_INFO(sLogger, ("TestPollingMultiLevelSoftLink() begin", time(NULL)));
#if defined(__linux__) // TODO: Add for Windows.
        CaseSetUp(false, true, true, 8);
        PollingModify::GetInstance()->Start();
        PollingDirFile::GetInstance()->Start();
        usleep(100);
        int32_t dirfileInterval = INT32_FLAG(dirfile_check_interval_ms);
        int32_t checkRound = INT32_FLAG(check_not_exist_file_dir_round);
        int32_t deleteRound = INT32_FLAG(delete_dir_file_round);
        INT32_FLAG(dirfile_check_interval_ms) = 1000;
        int32_t seqNo = 5;
        string linkFilePrefixDir("PollingLink");
        string innerLinkFilePrefixDir("Inner");
        string logFileDirs[] = {"a", "a/a", "a/b", "b", "b/a", "b/c", "b/d", "b/a/a"};
        string logSrcDirs[] = {"", "/a", "/a", "", "/b", "/b", "/b", "/b/a"};
        string logObjDirs[] = {"a", "a", "b", "b", "a", "c", "d", "a"};
        string logFileNames[]
            = {"job.log", "job.log1", "job.log2", "job.log3", "job.log4", "job.log5", "job.log6", "job.log7"};
        string disableFileName = "job.xlog";

        string exeDir = GetProcessExecutionDir();
        exeDir.resize(exeDir.size() - 1);
        system(string("mkdir " + exeDir + "/" + linkFilePrefixDir).c_str());
        system(string("mkdir " + exeDir + "/" + linkFilePrefixDir + "/" + innerLinkFilePrefixDir).c_str());
        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            string cmd("mkdir ");
            system((cmd + exeDir + "/" + linkFilePrefixDir + "/" + logFileDirs[prjIndex]).c_str());
        }
        usleep(100 * 1000);
        system(string("ln -s " + exeDir + "/" + linkFilePrefixDir + "/a" + "  " + gRootDir + "/a").c_str());
        system(string("ln -s " + exeDir + "/" + linkFilePrefixDir + "/b" + "  " + gRootDir + "/b").c_str());

        for (int round = 0; round < 1; ++round) {
            for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
                OneJob(10,
                       exeDir + "/" + linkFilePrefixDir + "/" + innerLinkFilePrefixDir,
                       "job",
                       true,
                       time(NULL),
                       "TestPollingDirFile",
                       seqNo,
                       false,
                       prjIndex);
                OneJob(10,
                       exeDir + "/" + linkFilePrefixDir + "/" + innerLinkFilePrefixDir,
                       "job",
                       false,
                       time(NULL),
                       "TestPollingDirFile",
                       seqNo,
                       false,
                       prjIndex);
            }
            usleep(10 * 1000);
        }

        system((string("ln -s ") + exeDir + "/" + linkFilePrefixDir + "/" + innerLinkFilePrefixDir + "/"
                + disableFileName + " " + exeDir + "/" + linkFilePrefixDir + "/" + disableFileName)
                   .c_str());
        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            string cmd("ln -s ");

            system((cmd + exeDir + "/" + linkFilePrefixDir + "/" + innerLinkFilePrefixDir + "/" + logFileNames[prjIndex]
                    + " " + exeDir + "/" + linkFilePrefixDir + "/" + logFileNames[prjIndex])
                       .c_str());

            usleep(100 * 1000);
            system((cmd + exeDir + "/" + linkFilePrefixDir + "/" + logFileNames[prjIndex] + " " + gRootDir + "/"
                    + logFileDirs[prjIndex] + "/" + logFileNames[prjIndex])
                       .c_str());
            system((cmd + exeDir + "/" + linkFilePrefixDir + "/" + disableFileName + " " + gRootDir + "/"
                    + logFileDirs[prjIndex] + "/" + disableFileName)
                       .c_str());
        }

        sleep(3);


        APSARA_TEST_TRUE(PollingEventQueue::GetInstance()->FindEvent(gRootDir, "") == NULL);

        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            Event* pEvent
                = PollingEventQueue::GetInstance()->FindEvent(gRootDir + logSrcDirs[prjIndex], logObjDirs[prjIndex]);
            APSARA_TEST_TRUE(pEvent != NULL);
            if (pEvent != NULL) {
                APSARA_TEST_TRUE(pEvent->IsDir());
                APSARA_TEST_TRUE(pEvent->IsCreate());
            }
            // modify event
            pEvent = PollingEventQueue::GetInstance()->FindEvent(gRootDir + "/" + logFileDirs[prjIndex],
                                                                 logFileNames[prjIndex]);
            APSARA_TEST_TRUE(pEvent != NULL);
            if (pEvent != NULL) {
                APSARA_TEST_TRUE(!pEvent->IsDir());
                APSARA_TEST_TRUE(pEvent->IsModify());
                string filePath
                    = exeDir + "/" + linkFilePrefixDir + "/" + innerLinkFilePrefixDir + "/" + logFileNames[prjIndex];
                struct stat fStat;
                stat(filePath.c_str(), &fStat);
                APSARA_TEST_EQUAL((uint64_t)fStat.st_dev, pEvent->GetDev());
                APSARA_TEST_EQUAL((uint64_t)fStat.st_ino, pEvent->GetInode());
            }
        }

        PollingEventQueue::GetInstance()->Clear();


        // test : when real file write, polling link file will generate modify event
        APSARA_TEST_EQUAL(PollingEventQueue::GetInstance()->mEventQueue.size(), (size_t)0);

        for (int round = 0; round < 10; ++round) {
            for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
                OneJob(10,
                       exeDir + "/" + linkFilePrefixDir + "/" + innerLinkFilePrefixDir,
                       "job",
                       true,
                       time(NULL),
                       "TestPollingDirFile",
                       seqNo,
                       false,
                       prjIndex);
                OneJob(10,
                       exeDir + "/" + linkFilePrefixDir + "/" + innerLinkFilePrefixDir,
                       "job",
                       false,
                       time(NULL),
                       "TestPollingDirFile",
                       seqNo,
                       false,
                       prjIndex);
            }
            usleep(10 * 1000);
        }

        sleep(2);

        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            Event* pEvent = PollingEventQueue::GetInstance()->FindEvent(gRootDir + "/" + logFileDirs[prjIndex],
                                                                        logFileNames[prjIndex]);
            APSARA_TEST_TRUE(pEvent != NULL);
            if (pEvent != NULL) {
                APSARA_TEST_TRUE(!pEvent->IsDir());
                APSARA_TEST_TRUE(pEvent->IsModify());
                string filePath
                    = exeDir + "/" + linkFilePrefixDir + "/" + innerLinkFilePrefixDir + "/" + logFileNames[prjIndex];
                struct stat fStat;
                stat(filePath.c_str(), &fStat);
                APSARA_TEST_EQUAL((uint64_t)fStat.st_dev, pEvent->GetDev());
                APSARA_TEST_EQUAL((uint64_t)fStat.st_ino, pEvent->GetInode());
            }
        }


        PollingEventQueue::GetInstance()->Clear();

        APSARA_TEST_EQUAL(PollingModify::GetInstance()->mModifyCacheMap.size(), (size_t)8);

        APSARA_TEST_EQUAL(PollingDirFile::GetInstance()->mDirCacheMap.size(), (size_t)9);
        APSARA_TEST_EQUAL(PollingDirFile::GetInstance()->mFileCacheMap.size(), (size_t)16);

        int32_t fileNotExistTimes = INT32_FLAG(max_file_not_exist_times);
        INT32_FLAG(max_file_not_exist_times) = 1;
        INT32_FLAG(check_not_exist_file_dir_round) = 1;
        INT32_FLAG(delete_dir_file_round) = 1;

        for (int prjIndex = 0; prjIndex < 3; ++prjIndex) {
            string cmd("rm -fr ");
            system((cmd + exeDir + "/" + linkFilePrefixDir + "/" + logFileDirs[prjIndex]).c_str());
        }
        sleep(5);
        APSARA_TEST_EQUAL(PollingEventQueue::GetInstance()->mEventQueue.size(), (size_t)6);
        APSARA_TEST_EQUAL(PollingModify::GetInstance()->mModifyCacheMap.size(), (size_t)5);

        APSARA_TEST_EQUAL(PollingDirFile::GetInstance()->mDirCacheMap.size(), (size_t)6);
        APSARA_TEST_EQUAL(PollingDirFile::GetInstance()->mFileCacheMap.size(), (size_t)10);


        string cmd("rm -fr ");
        system((cmd + exeDir + "/" + linkFilePrefixDir).c_str());
        sleep(5);
        APSARA_TEST_EQUAL(PollingEventQueue::GetInstance()->mEventQueue.size(), (size_t)16);
        APSARA_TEST_EQUAL(PollingModify::GetInstance()->mModifyCacheMap.size(), (size_t)0);

        APSARA_TEST_EQUAL(PollingDirFile::GetInstance()->mDirCacheMap.size(), (size_t)1);
        APSARA_TEST_EQUAL(PollingDirFile::GetInstance()->mFileCacheMap.size(), (size_t)0);

        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            // Delete event
            Event* pEvent = PollingEventQueue::GetInstance()->FindEvent(gRootDir + "/" + logFileDirs[prjIndex],
                                                                        logFileNames[prjIndex]);
            APSARA_TEST_TRUE(pEvent != NULL);
            if (pEvent != NULL) {
                APSARA_TEST_TRUE(!pEvent->IsDir());
                APSARA_TEST_TRUE(pEvent->IsDeleted());
            }
            pEvent = PollingEventQueue::GetInstance()->FindEvent(gRootDir + "/" + logFileDirs[prjIndex], string());
            APSARA_TEST_TRUE(pEvent != NULL);
            if (pEvent != NULL) {
                APSARA_TEST_TRUE(pEvent->IsDir());
                APSARA_TEST_TRUE(pEvent->IsTimeout());
            }
        }

        INT32_FLAG(dirfile_check_interval_ms) = dirfileInterval;
        INT32_FLAG(check_not_exist_file_dir_round) = checkRound;
        INT32_FLAG(delete_dir_file_round) = deleteRound;
        INT32_FLAG(max_file_not_exist_times) = fileNotExistTimes;

        PollingDirFile::GetInstance()->Stop();
        PollingModify::GetInstance()->Stop();
        CaseCleanUp();
#endif
        LOG_INFO(sLogger, ("TestPollingMultiLevelSoftLink() end", time(NULL)));
    }

    void TestPollingRollBack() {
        LOG_INFO(sLogger, ("TestPollingRollBack() begin", time(NULL)));
        CaseSetUp(false, true, true, 8);
        int32_t dirfileInterval = INT32_FLAG(dirfile_check_interval_ms);
        INT32_FLAG(dirfile_check_interval_ms) = 1000;
        PollingDirFile::GetInstance()->Start();
        PollingModify::GetInstance()->Start();
        sleep(INT32_FLAG(dirfile_check_interval_ms) / 1000);

        int32_t seqNo = 5;
        string logFileDirs[] = {"a",
                                "a" + PS + "a",
                                "a" + PS + "b",
                                "b",
                                "b" + PS + "a",
                                "b" + PS + "c",
                                "b" + PS + "d",
                                "b" + PS + "a" + PS + "a"};
        string logSrcDirs[] = {"", PS + "a", PS + "a", "", PS + "b", PS + "b", PS + "b", PS + "b" + PS + "a"};
        string logObjDirs[] = {"a", "a", "b", "b", "a", "c", "d", "a"};
        string logFileNames[]
            = {"job.log", "job.log1", "job.log2", "job.log3", "job.log4", "job.log5", "job.log6", "job.log7"};

        // Create subdirectories and files.
        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            bfs::create_directories(bfs::path(gRootDir) / logFileDirs[prjIndex]);
        }
        for (int round = 0; round < 10; ++round) {
            for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
                OneJob(10,
                       gRootDir + PS + logFileDirs[prjIndex],
                       "job",
                       true,
                       time(NULL),
                       "TestPollingDirFile",
                       seqNo,
                       false,
                       prjIndex);
                OneJob(10,
                       gRootDir + PS + logFileDirs[prjIndex],
                       "job",
                       false,
                       time(NULL),
                       "TestPollingDirFile",
                       seqNo,
                       false,
                       prjIndex);
            }
            usleep(20 * 1000);
        }
        sleep(5);

        // Test events of subdirectories and files.
        APSARA_TEST_TRUE(PollingEventQueue::GetInstance()->FindEvent(gRootDir, "") == NULL);
        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            Event* pEvent
                = PollingEventQueue::GetInstance()->FindEvent(gRootDir + logSrcDirs[prjIndex], logObjDirs[prjIndex]);
            APSARA_TEST_TRUE(pEvent != NULL);
            if (pEvent != NULL) {
                APSARA_TEST_TRUE(pEvent->IsDir());
                APSARA_TEST_TRUE(pEvent->IsCreate());
            }
            // modify event
            pEvent = PollingEventQueue::GetInstance()->FindEvent(gRootDir + PS + logFileDirs[prjIndex],
                                                                 logFileNames[prjIndex]);
            APSARA_TEST_TRUE(pEvent != NULL);
            if (pEvent != NULL) {
                APSARA_TEST_TRUE(!pEvent->IsDir());
                APSARA_TEST_TRUE(pEvent->IsModify());
                string filePath = gRootDir + PS + logFileDirs[prjIndex] + PS + logFileNames[prjIndex];
                fsutil::PathStat stat;
                fsutil::PathStat::stat(filePath, stat);
                auto devInode = stat.GetDevInode();
                APSARA_TEST_EQUAL((uint64_t)devInode.dev, pEvent->GetDev());
                APSARA_TEST_EQUAL((uint64_t)devInode.inode, pEvent->GetInode());
            }
        }
        PollingEventQueue::GetInstance()->Clear();
        sleep(1);

        // Rotate each files 10 rounds.
        for (int round = 0; round < 10; ++round) {
            for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
                RotateLogFile(gRootDir + PS + logFileDirs[prjIndex] + PS + logFileNames[prjIndex]);
                OneJob(1,
                       gRootDir + PS + logFileDirs[prjIndex],
                       "job",
                       true,
                       time(NULL),
                       "TestPollingDirFile",
                       seqNo,
                       false,
                       prjIndex);
            }
            sleep(2);
        }

        // check newest dev inode
        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            string filePath = gRootDir + PS + logFileDirs[prjIndex] + PS + logFileNames[prjIndex];
            fsutil::PathStat stat;
            fsutil::PathStat::stat(filePath, stat);
            auto devInode = stat.GetDevInode();
            Event* pEvent = PollingEventQueue::GetInstance()->FindEvent(
                gRootDir + PS + logFileDirs[prjIndex], logFileNames[prjIndex], devInode.dev, devInode.inode);
            APSARA_TEST_TRUE(pEvent != NULL);
        }

        APSARA_TEST_TRUE(PollingEventQueue::GetInstance()->mEventQueue.size() >= (size_t)80);
        PollingEventQueue::GetInstance()->Clear();

        // Delete 3 subdirectories and test they will be removed from cache
        // and delete events will be generated.
        int32_t fileNotExistTimes = INT32_FLAG(max_file_not_exist_times);
        INT32_FLAG(max_file_not_exist_times) = 1;
        sleep(1);
        for (int prjIndex = 0; prjIndex < 3; ++prjIndex) {
            bfs::remove_all(bfs::path(gRootDir) / logFileDirs[prjIndex]);
        }
        sleep(10);
        for (int prjIndex = 0; prjIndex < 3; ++prjIndex) {
            auto evt = PollingEventQueue::GetInstance()->FindEvent(
                gRootDir + PS + logFileDirs[prjIndex], logFileNames[prjIndex], EVENT_DELETE);
            APSARA_TEST_TRUE(evt != NULL);
            APSARA_TEST_TRUE(!evt->IsDir());
        }
        APSARA_TEST_EQUAL(PollingModify::GetInstance()->mModifyCacheMap.size(), (size_t)5);

        // Delete root directory, all cache will be cleared and delete events
        // of files can be found.
        bfs::remove_all(gRootDir);
        sleep(10);
        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            auto evt = PollingEventQueue::GetInstance()->FindEvent(
                gRootDir + PS + logFileDirs[prjIndex], logFileNames[prjIndex], EVENT_DELETE);
            APSARA_TEST_TRUE(evt != NULL);
            APSARA_TEST_TRUE(!evt->IsDir());
        }
        APSARA_TEST_EQUAL(PollingModify::GetInstance()->mModifyCacheMap.size(), (size_t)0);
        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            // Delete event
            Event* pEvent = PollingEventQueue::GetInstance()->FindEvent(gRootDir + PS + logFileDirs[prjIndex],
                                                                        logFileNames[prjIndex]);
            APSARA_TEST_TRUE(pEvent != NULL);
            if (pEvent != NULL) {
                APSARA_TEST_TRUE(!pEvent->IsDir());
                APSARA_TEST_TRUE(pEvent->IsDeleted());
            }
        }

        INT32_FLAG(dirfile_check_interval_ms) = dirfileInterval;
        INT32_FLAG(max_file_not_exist_times) = fileNotExistTimes;
        PollingModify::GetInstance()->Stop();
        PollingDirFile::GetInstance()->Stop();
        CaseCleanUp();
        LOG_INFO(sLogger, ("TestPollingRollBack() end", time(NULL)));
    }

    void TestPollingUpperLimit() {
        LOG_INFO(sLogger, ("TestPollingUpperLimit() begin", time(NULL)));

        CaseSetUp(false, true, true, 8);
        int32_t dirfileInterval = INT32_FLAG(dirfile_check_interval_ms);
        INT32_FLAG(dirfile_check_interval_ms) = 1000;
        PollingDirFile::GetInstance()->Start();
        PollingModify::GetInstance()->Start();
        sleep(INT32_FLAG(dirfile_check_interval_ms) / 1000);

        int32_t seqNo = 5;
        string logFileDirs[] = {"a",
                                "a" + PS + "a",
                                "a" + PS + "b",
                                "b",
                                "b" + PS + "a",
                                "b" + PS + "c",
                                "b" + PS + "d",
                                "b" + PS + "a" + PS + "a"};
        string logSrcDirs[] = {"", PS + "a", PS + "a", "", PS + "b", PS + "b", PS + "b", PS + "b" + PS + "a"};
        string logObjDirs[] = {"a", "a", "b", "b", "a", "c", "d", "a"};
        string logFileNames[]
            = {"job.log", "job.log1", "job.log2", "job.log3", "job.log4", "job.log5", "job.log6", "job.log7"};

        LOG_INFO(sLogger, ("Create directories and logs", ""));
        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            bfs::create_directories(bfs::path(gRootDir) / logFileDirs[prjIndex]);
        }
        for (int round = 0; round < 10; ++round) {
            for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
                OneJob(10,
                       gRootDir + PS + logFileDirs[prjIndex],
                       "job",
                       true,
                       time(NULL),
                       "TestPollingDirFile",
                       seqNo,
                       false,
                       prjIndex);
                OneJob(10,
                       gRootDir + PS + logFileDirs[prjIndex],
                       "job",
                       false,
                       time(NULL),
                       "TestPollingDirFile",
                       seqNo,
                       false,
                       prjIndex);
            }
            usleep(20 * 1000);
        }
        LOG_INFO(sLogger, ("Create directories and logs", "done"));
        sleep(5);

        LOG_INFO(sLogger, ("Test PollingEventQueue", "LogInput is not running, all events will be kept"));
        // The first time to read root directory, no event generated.
        APSARA_TEST_TRUE(PollingEventQueue::GetInstance()->FindEvent(gRootDir, "") == NULL);
        auto TestSubdirectoriesAndFiles = [&]() {
            for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
                // Subdirectories create event.
                Event* pEvent = PollingEventQueue::GetInstance()->FindEvent(gRootDir + logSrcDirs[prjIndex],
                                                                            logObjDirs[prjIndex]);
                APSARA_TEST_TRUE(pEvent != NULL);
                if (pEvent != NULL) {
                    APSARA_TEST_TRUE(pEvent->IsDir());
                    APSARA_TEST_TRUE(pEvent->IsCreate());
                }

                // File modify event.
                pEvent = PollingEventQueue::GetInstance()->FindEvent(gRootDir + PS + logFileDirs[prjIndex],
                                                                     logFileNames[prjIndex]);
                APSARA_TEST_TRUE(pEvent != NULL);
                if (pEvent != NULL) {
                    APSARA_TEST_TRUE(!pEvent->IsDir());
                    APSARA_TEST_TRUE(pEvent->IsModify());
                    string filePath = gRootDir + PS + logFileDirs[prjIndex] + PS + logFileNames[prjIndex];
                    fsutil::PathStat stat;
                    fsutil::PathStat::stat(filePath, stat);
                    auto devInode = stat.GetDevInode();
                    APSARA_TEST_EQUAL((uint64_t)devInode.dev, pEvent->GetDev());
                    APSARA_TEST_EQUAL((uint64_t)devInode.inode, pEvent->GetInode());
                }
            }
        };
        TestSubdirectoriesAndFiles();
        PollingEventQueue::GetInstance()->Clear();
        LOG_INFO(sLogger, ("Test modify cache", "8 files"));
        APSARA_TEST_EQUAL(PollingModify::GetInstance()->mModifyCacheMap.size(), (size_t)8);

        LOG_INFO(sLogger, ("Backup flags", ""));
        int32_t defaultDirLimit = INT32_FLAG(polling_dir_upperlimit);
        int32_t defaultFileLimit = INT32_FLAG(polling_file_upperlimit);
        int32_t defaultDirTimeOut = INT32_FLAG(polling_dir_timeout);
        int32_t defaultFileTimeOut = INT32_FLAG(polling_file_timeout);
        int32_t defaultFirstDirTimeOut = INT32_FLAG(polling_dir_first_watch_timeout);
        int32_t defaultFirstFileTimeOut = INT32_FLAG(polling_file_first_watch_timeout);
        int32_t defaultCheckTimeout = INT32_FLAG(polling_check_timeout_interval);
        int32_t defaultModifyCheckInterval = INT32_FLAG(modify_check_interval);
        LOG_INFO(sLogger, ("Set upperlimit flags to clear cache", ""));
        INT32_FLAG(polling_dir_upperlimit) = 0;
        INT32_FLAG(polling_file_upperlimit) = 0;
        INT32_FLAG(polling_dir_timeout) = 0;
        INT32_FLAG(polling_file_timeout) = 0;
        INT32_FLAG(polling_dir_first_watch_timeout) = 0;
        INT32_FLAG(polling_file_first_watch_timeout) = 0;
        INT32_FLAG(polling_check_timeout_interval) = 0;
        INT32_FLAG(modify_check_interval) = 500;

        // Make sure that PollingDirFile only ran once and PollingModify ran at least once.
        // PollingDirFile ran once: clear dir and file caches because of upperlimit.
        // PollingModify ran once: clear modify cache because of PollingDirFile cleared file cache.
        LOG_INFO(sLogger, ("Wait for clearing caches", ""));
        usleep(1000 * (500 + std::max(INT32_FLAG(dirfile_check_interval_ms), INT32_FLAG(modify_check_interval))));

        APSARA_TEST_EQUAL(PollingModify::GetInstance()->mModifyCacheMap.size(), (size_t)0);
        APSARA_TEST_EQUAL(PollingDirFile::GetInstance()->mDirCacheMap.size(), (size_t)0);
        APSARA_TEST_EQUAL(PollingDirFile::GetInstance()->mFileCacheMap.size(), (size_t)0);

        INT32_FLAG(polling_dir_upperlimit) = defaultDirLimit;
        INT32_FLAG(polling_file_upperlimit) = defaultFileLimit;
        INT32_FLAG(polling_dir_timeout) = defaultDirTimeOut;
        INT32_FLAG(polling_file_timeout) = defaultFileTimeOut;
        INT32_FLAG(polling_dir_first_watch_timeout) = defaultFirstDirTimeOut;
        INT32_FLAG(polling_file_first_watch_timeout) = defaultFirstFileTimeOut;
        INT32_FLAG(polling_check_timeout_interval) = defaultCheckTimeout;
        INT32_FLAG(modify_check_interval) = defaultModifyCheckInterval;
        sleep(10);

        LOG_INFO(sLogger, ("Second time to read root directory", "generate event"));
        APSARA_TEST_TRUE(PollingEventQueue::GetInstance()->FindEvent(gRootDir, "") != NULL);
        TestSubdirectoriesAndFiles();

        PollingModify::GetInstance()->Stop();
        PollingDirFile::GetInstance()->Stop();
        INT32_FLAG(dirfile_check_interval_ms) = dirfileInterval;
        CaseCleanUp();
        LOG_INFO(sLogger, ("TestPollingUpperLimit() end", time(NULL)));
    }

    void TestPollingFileDeleted() {
        LOG_INFO(sLogger, ("TestPollingFileDeleted() begin", time(NULL)));

        CaseSetUp(false, true, true, 8);
        int32_t dirfileInterval = INT32_FLAG(dirfile_check_interval_ms);
        INT32_FLAG(dirfile_check_interval_ms) = 1000;
        PollingDirFile::GetInstance()->Start();
        PollingModify::GetInstance()->Start();
        sleep(INT32_FLAG(dirfile_check_interval_ms) / 1000);

        int32_t seqNo = 5;
        string logFileDirs[] = {"a",
                                "a" + PS + "a",
                                "a" + PS + "b",
                                "b",
                                "b" + PS + "a",
                                "b" + PS + "c",
                                "b" + PS + "d",
                                "b" + PS + "a" + PS + "a"};
        string logSrcDirs[] = {"", PS + "a", PS + "a", "", PS + "b", PS + "b", PS + "b", PS + "b" + PS + "a"};
        string logObjDirs[] = {"a", "a", "b", "b", "a", "c", "d", "a"};
        string logFileNames[]
            = {"job.log", "job.log1", "job.log2", "job.log3", "job.log4", "job.log5", "job.log6", "job.log7"};
        string disableFileName = "job.xlog";

        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            bfs::create_directories(bfs::path(gRootDir) / logFileDirs[prjIndex]);
        }

        for (int round = 0; round < 10; ++round) {
            for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
                OneJob(10,
                       gRootDir + PS + logFileDirs[prjIndex],
                       "job",
                       true,
                       time(NULL),
                       "TestPollingDirFile",
                       seqNo,
                       false,
                       prjIndex);
                OneJob(10,
                       gRootDir + PS + logFileDirs[prjIndex],
                       "job",
                       false,
                       time(NULL),
                       "TestPollingDirFile",
                       seqNo,
                       false,
                       prjIndex);
            }
            usleep(20 * 1000);
        }

        sleep(5);

        auto pollingEventQueue = PollingEventQueue::GetInstance();
        APSARA_TEST_TRUE(pollingEventQueue->FindEvent(gRootDir, "") == NULL);

        EXPECT_EQ(pollingEventQueue->mEventQueue.size(), 16);
        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            Event* pEvent = pollingEventQueue->FindEvent(gRootDir + logSrcDirs[prjIndex], logObjDirs[prjIndex]);
            APSARA_TEST_TRUE(pEvent != NULL);
            if (pEvent != NULL) {
                APSARA_TEST_TRUE(pEvent->IsDir());
                APSARA_TEST_TRUE(pEvent->IsCreate());
            }
            // modify event
            pEvent = pollingEventQueue->FindEvent(gRootDir + PS + logFileDirs[prjIndex], logFileNames[prjIndex]);
            APSARA_TEST_TRUE(pEvent != NULL);
            if (pEvent != NULL) {
                APSARA_TEST_TRUE(!pEvent->IsDir());
                APSARA_TEST_TRUE(pEvent->IsModify());
                string filePath = gRootDir + PS + logFileDirs[prjIndex] + PS + logFileNames[prjIndex];
                fsutil::PathStat stat;
                fsutil::PathStat::stat(filePath, stat);
                auto devInode = stat.GetDevInode();
                APSARA_TEST_EQUAL((uint64_t)devInode.dev, pEvent->GetDev());
                APSARA_TEST_EQUAL((uint64_t)devInode.inode, pEvent->GetInode());
            }
        }


        APSARA_TEST_EQUAL(PollingModify::GetInstance()->mModifyCacheMap.size(), (size_t)8);

        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            bfs::remove_all(bfs::path(gRootDir) / logFileDirs[prjIndex] / logFileNames[prjIndex]);
        }

        sleep(10);

        PollingEventQueue::GetInstance()->Clear();

        APSARA_TEST_EQUAL(PollingModify::GetInstance()->mModifyCacheMap.size(), (size_t)0);


        int32_t polling_modify_repush_interval = INT32_FLAG(polling_modify_repush_interval);


        INT32_FLAG(polling_modify_repush_interval) = 0;


        for (int round = 0; round < 10; ++round) {
            for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
                OneJob(10,
                       gRootDir + PS + logFileDirs[prjIndex],
                       "job",
                       true,
                       time(NULL),
                       "TestPollingDirFile",
                       seqNo,
                       false,
                       prjIndex);
                OneJob(10,
                       gRootDir + PS + logFileDirs[prjIndex],
                       "job",
                       false,
                       time(NULL),
                       "TestPollingDirFile",
                       seqNo,
                       false,
                       prjIndex);
            }
            usleep(20 * 1000);
        }
        sleep(10);


        for (int prjIndex = 0; prjIndex < 8; ++prjIndex) {
            Event* pEvent = PollingEventQueue::GetInstance()->FindEvent(gRootDir + PS + logFileDirs[prjIndex],
                                                                        logFileNames[prjIndex]);
            APSARA_TEST_TRUE(pEvent != NULL);
            if (pEvent != NULL) {
                APSARA_TEST_TRUE(!pEvent->IsDir());
                APSARA_TEST_TRUE(pEvent->IsModify());
                string filePath = gRootDir + PS + logFileDirs[prjIndex] + PS + logFileNames[prjIndex];
                fsutil::PathStat stat;
                fsutil::PathStat::stat(filePath, stat);
                auto devInode = stat.GetDevInode();
                APSARA_TEST_EQUAL((uint64_t)devInode.dev, pEvent->GetDev());
                APSARA_TEST_EQUAL((uint64_t)devInode.inode, pEvent->GetInode());
            }
        }

        APSARA_TEST_EQUAL(PollingModify::GetInstance()->mModifyCacheMap.size(), (size_t)8);

        PollingModify::GetInstance()->Stop();
        PollingDirFile::GetInstance()->Stop();
        INT32_FLAG(polling_modify_repush_interval) = polling_modify_repush_interval;
        INT32_FLAG(dirfile_check_interval_ms) = dirfileInterval;
        CaseCleanUp();
        LOG_INFO(sLogger, ("TestPollingFileDeleted() end", time(NULL)));
    }

    void testPollingWindowsRootPathCaseSetUp(bool enable, bool wildcard, std::vector<bfs::path>& directories);
    void testPollingWindowsRootPathCaseCleanUp();

    void TestPollingWindowsRootPath();
    void TestPollingWindowsRootPathNew();

    void TestPollingWindowsWildcardRootPath();
    void TestPollingWindowsWildcardRootPathNew();
};

APSARA_UNIT_TEST_CASE(PollingUnittest, TestPollingFileDeleted, 1);
APSARA_UNIT_TEST_CASE(PollingUnittest, TestPollingUpperLimit, 2);
APSARA_UNIT_TEST_CASE(PollingUnittest, TestPollingDirFile, 3);
APSARA_UNIT_TEST_CASE(PollingUnittest, TestPollingModify, 4);
APSARA_UNIT_TEST_CASE(PollingUnittest, TestPollingSoftLink, 5);
APSARA_UNIT_TEST_CASE(PollingUnittest, TestPollingMultiLevelSoftLink, 6);
APSARA_UNIT_TEST_CASE(PollingUnittest, TestPollingRollBack, 7);

#if defined(_MSC_VER)
const std::string kRootPath = "C:";
const std::string kFileExt = "logtailtestlog";
const std::string kDirPrefix = "logtailtestdir";

void PollingUnittest::testPollingWindowsRootPathCaseSetUp(bool enable,
                                                          bool wildcard,
                                                          std::vector<bfs::path>& directories) {
    if (bfs::exists(gRootDir)) {
        bfs::remove_all(gRootDir);
    }
    bfs::create_directories(gRootDir);

    {
        fsutil::Dir dir(kRootPath);
        ASSERT_TRUE(dir.Open());
        while (fsutil::Entry e = dir.ReadNext()) {
            if ((e.IsRegFile() && EndWith(e.Name(), kFileExt)) || (e.IsDir() && StartWith(e.Name(), kDirPrefix))) {
                bfs::remove_all(kRootPath + PATH_SEPARATOR + e.Name());
            }
        }
    }

    Json::Value metrics = MakeConfigJson();
    Json::Value& cfg = *metrics["metrics"].begin();
    cfg["log_path"] = kRootPath + (wildcard ? "\\" + kDirPrefix + "*" : "");
    cfg["file_pattern"] = "*" + kFileExt;
    if (enable) {
        cfg["advanced"]["enable_root_path_collection"] = true;
    }
    if (!wildcard) {
        std::vector<std::string> kBlacklistedDirs
            = {"Windows",      "gopath",    "logtail", "logtail_host", "LogtailData*", "MSOCache", "logtailplugin",
               "*Cloud*",      "openssl-*", "local",   "Intel",        "Users",        "*Re*",     "A*",
               "GitlabRunner", "Elevation", "P*",      "S*",           "D*",           "C*"};
        Json::Value& dirBlacklist = cfg["advanced"]["blacklist"]["dir_blacklist"];
        for (auto& p : kBlacklistedDirs) {
            dirBlacklist.append(kRootPath + "\\" + p);
        }
    }
    WriteMetrics(metrics);
    commonCaseSetUp();
    ASSERT_EQ(BOOL_FLAG(enable_root_path_collection), enable);

    PollingDirFile::GetInstance()->Start();
    sleep(INT32_FLAG(dirfile_check_interval_ms) / 1000 * 2);

    // Subdirectories: d1, d2, d3, d3/d4, d3/d4/d5/d6, 2 files in each directory.
    directories = std::vector<bfs::path>{bfs::path(kRootPath + "\\"),
                                         bfs::path(kRootPath + "\\") / bfs::path(kDirPrefix + "1"),
                                         bfs::path(kRootPath + "\\") / bfs::path(kDirPrefix + "2"),
                                         bfs::path(kRootPath + "\\") / bfs::path(kDirPrefix + "3")};
    {
        bfs::path p = directories.back();
        for (size_t idx = 4; idx <= 6; ++idx) {
            p /= (kDirPrefix + std::to_string(idx));
            directories.push_back(p);
        }
    }
    for (auto& d : directories) {
        if (!bfs::exists(d)) {
            bfs::create_directories(d);
        }
    }
    sleep(1);

    for (int round = 0; round < 10; ++round) {
        for (auto& d : directories) {
            auto name = d.filename().string();
            if (name == "\\") {
                name.clear();
            }
            std::ofstream((d / (name + "0" + kFileExt)).string()) << "round " << round << "0" << std::endl;
            std::ofstream((d / (name + "1" + kFileExt)).string()) << "round " << round << "0" << std::endl;
        }
        usleep(20 * 1000);
    }
    LOG_WARNING(sLogger, ("message", "create done"));
    sleep(3);
}

void PollingUnittest::testPollingWindowsRootPathCaseCleanUp() {
    PollingDirFile::GetInstance()->Stop();
    CaseCleanUp();
    BOOL_FLAG(enable_root_path_collection) = false;
}

void PollingUnittest::TestPollingWindowsRootPath() {
    LOG_INFO(sLogger, ("TestPollingWindowsRootPath() begin", time(NULL)));
    std::vector<bfs::path> directories;
    testPollingWindowsRootPathCaseSetUp(false, false, directories);

    auto eventQueue = PollingEventQueue::GetInstance();
    auto& modifyFileQueue = PollingModify::GetInstance()->mNewFileNameQueue;
    EXPECT_EQ(eventQueue->mEventQueue.size(), 0);
    EXPECT_EQ(modifyFileQueue.size(), 0);

    testPollingWindowsRootPathCaseCleanUp();
    LOG_INFO(sLogger, ("TestPollingWindowsRootPath() end", time(NULL)));
}
APSARA_UNIT_TEST_CASE(PollingUnittest, TestPollingWindowsRootPath, 0);

void PollingUnittest::TestPollingWindowsRootPathNew() {
    LOG_INFO(sLogger, ("TestPollingWindowsRootPathNew() begin", time(NULL)));
    std::vector<bfs::path> directories;
    testPollingWindowsRootPathCaseSetUp(true, false, directories);

    auto eventQueue = PollingEventQueue::GetInstance();
    auto& modifyFileQueue = PollingModify::GetInstance()->mNewFileNameQueue;
    // Dir create events, ignore root and last directories.
    for (size_t idx = 1; idx < directories.size() - 1; ++idx) {
        const auto& d = directories[idx];
        std::string parent = d.parent_path().string();
        if (parent.back() == '\\') {
            parent.pop_back();
        }
        auto evt = eventQueue->FindEvent(parent, d.filename().string());
        EXPECT_TRUE(evt != NULL);
        if (evt != NULL) {
            EXPECT_TRUE(evt->IsDir());
            EXPECT_TRUE(evt->IsCreate());
        }
    }

    // File events in modify manager: ignore files under last directory.
    EXPECT_EQ(modifyFileQueue.size(), (directories.size() - 1) * 2);
    for (size_t idx = 0; idx < directories.size() - 1; ++idx) {
        const auto& d = directories[idx];
        auto name = d.filename().string();
        if (name == "\\") {
            name.clear();
        }
        auto dir = name.empty() ? kRootPath : d.string();
        for (size_t num = 0; num < 2; ++num) {
            const auto& f = modifyFileQueue[idx * 2 + num];
            EXPECT_EQ(f.mFileDir, dir);
            EXPECT_EQ(f.mFileName, name + std::to_string(num) + kFileExt);
        }
    }

    testPollingWindowsRootPathCaseCleanUp();
    LOG_INFO(sLogger, ("TestPollingWindowsRootPathNew() end", time(NULL)));
}
APSARA_UNIT_TEST_CASE(PollingUnittest, TestPollingWindowsRootPathNew, 0);

void PollingUnittest::TestPollingWindowsWildcardRootPath() {
    LOG_INFO(sLogger, ("TestPollingWindowsWildcardRootPath() begin", time(NULL)));
    std::vector<bfs::path> directories;
    testPollingWindowsRootPathCaseSetUp(false, true, directories);

    auto eventQueue = PollingEventQueue::GetInstance();
    auto& modifyFileQueue = PollingModify::GetInstance()->mNewFileNameQueue;
    EXPECT_EQ(eventQueue->mEventQueue.size(), 0);
    EXPECT_EQ(modifyFileQueue.size(), 0);

    testPollingWindowsRootPathCaseCleanUp();
    LOG_INFO(sLogger, ("TestPollingWindowsWildcardRootPath() end", time(NULL)));
}
APSARA_UNIT_TEST_CASE(PollingUnittest, TestPollingWindowsWildcardRootPath, 0);

void PollingUnittest::TestPollingWindowsWildcardRootPathNew() {
    LOG_INFO(sLogger, ("TestPollingWindowsWildcardRootPathNew() begin", time(NULL)));
    std::vector<bfs::path> directories;
    testPollingWindowsRootPathCaseSetUp(true, true, directories);

    auto eventQueue = PollingEventQueue::GetInstance();
    auto& modifyFileQueue = PollingModify::GetInstance()->mNewFileNameQueue;
    // Dir create events, ignore root directory.
    for (size_t idx = 1; idx < directories.size(); ++idx) {
        const auto& d = directories[idx];
        const auto dirPath = d.string();
        const auto isFirstLevel = dirPath.rfind('\\') == kRootPath.length();
        const auto parent = isFirstLevel ? dirPath : d.parent_path().string();
        const auto object = isFirstLevel ? "" : d.filename().string();
        auto evt = eventQueue->FindEvent(parent, object);
        EXPECT_TRUE(evt != NULL);
        if (evt != NULL) {
            EXPECT_TRUE(evt->IsDir());
            EXPECT_TRUE(evt->IsCreate());
        }
    }

    // File events in modify manager: ignore files under root directory.
    EXPECT_EQ(modifyFileQueue.size(), (directories.size() - 1) * 2);
    for (size_t idx = 1; idx < directories.size(); ++idx) {
        const auto& d = directories[idx];
        auto name = d.filename().string();
        if (name == "\\") {
            name.clear();
        }
        auto dir = name.empty() ? kRootPath : d.string();
        for (size_t num = 0; num < 2; ++num) {
            const auto& f = modifyFileQueue[(idx - 1) * 2 + num];
            EXPECT_EQ(f.mFileDir, dir);
            EXPECT_EQ(f.mFileName, name + std::to_string(num) + kFileExt);
        }
    }

    testPollingWindowsRootPathCaseCleanUp();
    LOG_INFO(sLogger, ("TestPollingWindowsWildcardRootPathNew() end", time(NULL)));
}
APSARA_UNIT_TEST_CASE(PollingUnittest, TestPollingWindowsWildcardRootPathNew, 0);
#endif

} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
