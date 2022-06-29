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
#include <json/json.h>
#include "controller/EventDispatcher.h"
#include "config_manager/ConfigManager.h"
#include "app_config/AppConfig.h"
#include "reader/LogFileReader.h"
#include "event_handler/EventHandler.h"
#include "processor/LogFilter.h"
#include "monitor/Monitor.h"
#include "common/util.h"
#include "event/Event.h"
#include "sender/Sender.h"
#include <assert.h>
#if defined(__linux__)
#include <unistd.h>
#endif
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
#include <boost/regex.hpp>
#include "log_pb/metric.pb.h"
#include "log_pb/sls_logs.pb.h"
#include "profiler/LogtailAlarm.h"
#include "profiler/LogIntegrity.h"
#include "event_handler/LogInput.h"
#include "common/FileEncryption.h"
#include "processor/LogProcess.h"
#include "common/WaitObject.h"
#include "common/Lock.h"
#include "common/MemoryBarrier.h"
#include "common/StringTools.h"
#include "common/Thread.h"
#include "common/Constants.h"
#include "common/FileSystemUtil.h"
#include "checkpoint/CheckpointManagerV2.h"
#include "logger/Logger.h"
#include "sdk/Client.h"
#include "sdk/Common.h"
#include "sdk/Exception.h"
#include "common/LogFileCollectOffsetIndicator.h"
#include <set>
#include <vector>
#include <queue>

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
DECLARE_FLAG_INT32(logtail_alarm_interval);

DECLARE_FLAG_INT32(max_client_send_error_count);
DECLARE_FLAG_INT32(client_disable_send_retry_interval);
DECLARE_FLAG_INT32(client_disable_send_retry_interval_max);

DECLARE_FLAG_INT32(max_client_quota_exceed_count);
DECLARE_FLAG_INT32(client_quota_send_concurrency_min);
DECLARE_FLAG_INT32(client_quota_send_retry_interval);
DECLARE_FLAG_INT32(client_quota_send_retry_interval_max);
DECLARE_FLAG_BOOL(send_prefer_real_ip);
DECLARE_FLAG_INT32(send_switch_real_ip_interval);

DECLARE_FLAG_BOOL(default_global_mark_offset_flag);
DECLARE_FLAG_BOOL(default_global_fuse_mode);
DECLARE_FLAG_INT32(file_eliminate_interval);

DECLARE_FLAG_INT32(dirfile_check_interval_ms);
DECLARE_FLAG_INT32(send_request_concurrency);
DECLARE_FLAG_INT32(test_unavailable_endpoint_interval);

DECLARE_FLAG_STRING(alipay_zone);


namespace logtail {

string gRootDir = "";
int gCaseID = 0;
bool gSetTimeFlag = false;
int32_t gStartIp = 1;
int gSendFailType = 1; // 1:network error; 2:all error can write secondary; 3:all error will not write secondary
bool gLogIntegrityTestFlag = false;
bool gGlobalMarkOffsetTestFlag = false;
bool gEnableExactlyOnce = false;
const size_t kConcurrency = 8;

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

// Write {logNum} logs to {filename}
// filename: /{workpath}/{jobname}.log
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

    auto out = FileAppendOpen((path + PATH_SEPARATOR + jobname + fileExt).c_str(), "ab");
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
        fwrite(buffer, 1, size, out);
    }
    delete[] buffer;
    fclose(out);
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

void WriteConfigJson(bool isFilter = false,
                     bool hasAliuid = false,
                     bool hasEndpoint = false,
                     int projectNum = 1,
                     bool hasTopic = false,
                     int32_t maxSendRateBytes = -1,
                     int32_t expireTime = 0) {
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
        commonreg_com["max_send_rate"] = Json::Value(maxSendRateBytes);
        commonreg_com["send_rate_expire"] = Json::Value(expireTime);
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
        if (gEnableExactlyOnce) {
            Json::Value advJSON;
            advJSON["exactly_once_concurrency"] = kConcurrency;
            commonreg_com["advanced"] = advJSON;
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
    ofstream fout("user_log_config.json");
    fout << metrics << endl;
    fout.close();
}

void WriteIntegrityConfigJson(int projectNum = 1) {
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
        commonreg_com["max_send_rate"] = Json::Value(-1);
        commonreg_com["send_rate_expire"] = Json::Value(0);
        if (i == 0) {
#if defined(__linux__)
            commonreg_com["file_pattern"] = Json::Value("*.[Ll][Oo][Gg]");
#elif defined(_MSC_VER)
            commonreg_com["file_pattern"] = Json::Value("*.log");
#endif
            commonreg_com["region"] = Json::Value(projectName); // not default region here
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

        Json::Value regs;
        regs.append(Json::Value("(.*)"));
        Json::Value keys;
        keys.append(Json::Value("content"));
        commonreg_com["regex"] = regs;
        commonreg_com["keys"] = keys;

        commonreg_com["preserve"] = Json::Value(true);

        if (i >= 4) {
            Json::Value customizedFields;
            Json::Value dataIntegrity;
            //([0-9]{4})-(0[0-9]{1}|1[0-2])-(0[0-9]{1}|[12][0-9]{1}|3[01]) (0[0-9]{1}|1[0-9]{1}|2[0-3]):[0-5][0-9]{1}:([0-5][0-9]{1})
            dataIntegrity["log_time_reg"]
                = Json::Value("(0[0-9]{1}|[12][0-9]{1}|3[01])/([A-Za-z]{3})/"
                              "([0-9]{4}):(0[0-9]{1}|1[0-9]{1}|2[0-3]):[0-5][0-9]{1}:([0-5][0-9]{1})");
            dataIntegrity["logstore"] = Json::Value("test-integrity-logstore");
            dataIntegrity["project_name"] = Json::Value(projectName);
            dataIntegrity["switch"] = Json::Value(true);
            dataIntegrity["time_pos"] = Json::Value(-1);

            customizedFields["data_integrity"] = dataIntegrity;
            commonreg_com["customized_fields"] = customizedFields;
        }

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
    ofstream fout("user_log_config.json");
    fout << metrics << endl;
    fout.close();
}

bool gNetWorkStat;
bool gTestNetWorkStat = false;
int32_t gProjectNetEnableIndex = 0;
int32_t gAsynProjectSendFailCount = 0;
int32_t gSynProjectSendFailCount = 0;
int32_t gCounter; // send_success log num
int32_t gMessageSize;
LogGroup gRcvLogGroup; // the latest send_success log group
PTMutex gRecvLogGroupLock;
int32_t gStatusCount;
LogGroup gStatusLogGroup;
PTMutex gBufferLogGroupsLock;
vector<LogGroup> gBufferLogGroups; // send_success log group
std::mutex gMockExactlyOnceSendLock;
std::vector<RangeCheckpointPtr> gRangeCheckpoints;
std::set<std::string> gBlockedHashKeySet;
std::unique_ptr<std::thread> gDispatchThreadId;
ThreadPtr gRealIpSendThread;
int32_t gAsynMockLatencyMs = 0;
int32_t gSynMockLatencyMs = 0;
bool gSendThreadRunFlag = true;
class SenderUnittest;
bool gDisableIpFlag = true;
sdk::Client* gClient = NULL;
string gDisabledIp;

WaitObject mWait;
PTMutex mQueueLock;
struct AsyncRequest;
std::queue<AsyncRequest*> mAsynRequestQueue;
volatile bool mSignalFlag = false;

void RunningDispatcher() {
    LOG_INFO(sLogger, ("RunningDispatcher", "begin"));
    ConfigManager::GetInstance()->RegisterHandlers();
    EventDispatcher::GetInstance()->Dispatch();
}

struct AsyncRequest {
    const std::string mProjectName;
    const std::string mLogstore;
    const std::string mLogData;
    SEND_DATA_TYPE mDataType;
    int32_t mRawSize;
    SendClosure* mSendClosure;
    int64_t mBeginTime;
    int32_t mSendIndex;
    AsyncRequest(const std::string& projectName,
                 const std::string& logstore,
                 const std::string& logData,
                 SEND_DATA_TYPE dataType,
                 int32_t rawSize,
                 SendClosure* sendClosure)
        : mProjectName(projectName),
          mLogstore(logstore),
          mLogData(logData),
          mDataType(dataType),
          mRawSize(rawSize),
          mSendClosure(sendClosure) {
        static int32_t s_sendIndex = 0;
        mSendIndex = s_sendIndex++;
        mBeginTime = GetCurrentTimeInMilliSeconds();
    }
    ~AsyncRequest() {
        //printf("#### %d %d ms \n", mSendIndex, (int)(GetNowTime64() - mBeginTime));
    }
};

static decltype(CheckpointManagerV2::GetInstance()) sCptM = nullptr;
static decltype(QueueManager::GetInstance()) sQueueM = nullptr;
static decltype(EventDispatcher::GetInstance()) sEventDispatcher = nullptr;

class SenderUnittest : public ::testing::Test {
    static decltype(LogProcess::GetInstance()->GetQueue().mLogstoreQueueMap)* sProcessQueueMap;
    static decltype(Sender::Instance()->GetQueue().mLogstoreSenderQueueMap)* sSenderQueueMap;
    void clearGlobalResource() {
        sCptM->rebuild();
        sQueueM->clear();
        sProcessQueueMap->clear();
        sSenderQueueMap->clear();
    }

protected:
    static void StopMockSendThread() {
        gSendThreadRunFlag = false;
        {
            WaitObject::Lock lock(mWait);
            ReadWriteBarrier();
            mSignalFlag = true;
            mWait.signal();
        }
        sleep(1);
    }

    static void ClearRequestQueue() {
        PTScopedLock lock(mQueueLock);
        while (!mAsynRequestQueue.empty()) {
            delete mAsynRequestQueue.front();
            mAsynRequestQueue.pop();
        }
    }

    static vector<AsyncRequest*> GetRequest() {
        vector<AsyncRequest*> reqVec;
        WaitObject::Lock lock(mWait);
        ReadWriteBarrier();
        while (!mSignalFlag) {
            mWait.wait(lock, 1000000);
        }
        mSignalFlag = false;
        PTScopedLock mutexLock(mQueueLock);
        // printf("Wait a request %d.\n", (int)mAsynRequestQueue.size());
        while (!mAsynRequestQueue.empty()) {
            reqVec.push_back(mAsynRequestQueue.front());
            mAsynRequestQueue.pop();
        }

        return reqVec;
    }

    static void MockAsyncSendThread() {
        //printf("Thread Start.\n");
        while (gSendThreadRunFlag) {
            vector<AsyncRequest*> reqVec = GetRequest();
            for (size_t i = 0; i < reqVec.size(); ++i) {
                AsyncRequest* pReq = reqVec[i];
                if (gSynMockLatencyMs > 0) {
                    usleep(gSynMockLatencyMs * 1000);
                }
                //printf("Send Async Inner \n");
                MockAsyncSendInner(pReq->mProjectName,
                                   pReq->mLogstore,
                                   pReq->mLogData,
                                   pReq->mDataType,
                                   pReq->mRawSize,
                                   pReq->mSendClosure);
                delete pReq;
            }
        }
    }

    static sdk::GetRealIpResponse MockGetRealIp(const std::string& projectName, const std::string& logstore) {
        static char ipStr[32];
        PTScopedLock lock(gRecvLogGroupLock);
        sdk::GetRealIpResponse rsp;
        sprintf(ipStr, "10.123.32.%d", ++gStartIp);
        rsp.realIp = ipStr;
        return rsp;
    }

    static sdk::GetRealIpResponse MockGetEmptyRealIp(const std::string& projectName, const std::string& logstore) {
        PTScopedLock lock(gRecvLogGroupLock);
        sdk::GetRealIpResponse rsp;
        rsp.realIp.clear();
        return rsp;
    }

    static void MockSyncSend(const std::string& projectName,
                             const std::string& logstore,
                             const std::string& logData,
                             SEND_DATA_TYPE dataType,
                             int32_t rawSize) {
        if (projectName == string("logtail-test-network-project")) {
            //printf("test network %d.\n", gTestNetWorkStat ? 1 : 0);
            if (gTestNetWorkStat) {
                return;
            }
            throw sdk::LOGException(sdk::LOGE_REQUEST_ERROR, "Can not connect to server.");
        }
        LOG_INFO(sLogger, ("MockSyncSend, projectName", projectName)("logstore", logstore)("dataType", dataType));
        vector<LogGroup> logGroupVec;
        Sender::ParseLogGroupFromString(logData, dataType, rawSize, logGroupVec);

        for (vector<LogGroup>::iterator iter = logGroupVec.begin(); iter != logGroupVec.end(); ++iter) {
            bool projectDisabled = true;
            if (iter->logs_size() > 0) {
                if (gNetWorkStat && iter->category() == "app_log") {
                    PTScopedLock lock(gBufferLogGroupsLock);
                    gBufferLogGroups.push_back(*iter);
                }
                if (gNetWorkStat && (projectName.find("_proj") != string::npos)) /* "1000000_proj" */
                {
                    int prjIndex = atoi(projectName.substr(0, 7).c_str()) - 1000000;
                    if (prjIndex <= gProjectNetEnableIndex) {
                        projectDisabled = false;
                        gRecvLogGroupLock.lock();
                        gRcvLogGroup = *iter;
                        gCounter += gRcvLogGroup.logs_size();
                        gMessageSize += gRcvLogGroup.ByteSize();
                        gRecvLogGroupLock.unlock();
                        LOG_INFO(
                            sLogger,
                            ("gRcvLogGroup.ByteSize()", gRcvLogGroup.ByteSize())("logData.size()", logData.size()));
                    } else {
                        ++gSynProjectSendFailCount;
                    }
                }
                if (gNetWorkStat && projectName == STRING_FLAG(profile_project_name)
                    && iter->category() == "logtail_status_profile") {
                    gStatusCount++;
                    gStatusLogGroup = *iter;
                }
            }
            if (!gNetWorkStat || projectDisabled) {
                //printf("[@MockSyncSend] fail %s %s %d.\n", projectName.c_str(), logstore.c_str(), rawSize);

                if (gSendFailType == 1)
                    throw sdk::LOGException(sdk::LOGE_REQUEST_ERROR, "Can not connect to server.");
                else if (gSendFailType == 2) {
                    int randInt = rand() % 4;
                    if (randInt == 0)
                        throw sdk::LOGException(sdk::LOGE_REQUEST_ERROR, "Can not connect to server.");
                    else if (randInt == 1)
                        throw sdk::LOGException(sdk::LOGE_WRITE_QUOTA_EXCEED, "project write quota exceed.");
                    else if (randInt == 2)
                        throw sdk::LOGException(sdk::LOGE_SERVER_BUSY, "connect to server timeout.");
                    else if (randInt == 3)
                        throw sdk::LOGException(sdk::LOGE_INTERNAL_SERVER_ERROR, "connect to server timeout.");
                } else if (gSendFailType == 3) {
                    int randInt = rand() % 5;
                    if (randInt == 0)
                        throw sdk::LOGException(sdk::LOGE_UNAUTHORIZED, "LOGE_UNAUTHORIZED.");
                    else if (randInt == 1)
                        throw sdk::LOGException(sdk::LOGE_BAD_RESPONSE, "LOGE_BAD_RESPONSE.");
                    else if (randInt == 2)
                        throw sdk::LOGException(sdk::LOGE_CATEGORY_NOT_EXIST, "LOGE_CATEGORY_NOT_EXIST.");
                    else if (randInt == 3)
                        throw sdk::LOGException(sdk::LOGE_PROJECT_NOT_EXIST, "LOGE_PROJECT_NOT_EXIST.");
                    else if (randInt == 4)
                        throw sdk::LOGException(sdk::LOGE_TOPIC_NOT_EXIST, "LOGE_TOPIC_NOT_EXIST.");
                } else if (gSendFailType == 4) {
                    int randInt = rand() % 2;
                    if (randInt == 0) {
                        throw sdk::LOGException(sdk::LOGE_WRITE_QUOTA_EXCEED, "LOGE_WRITE_QUOTA_EXCEED");
                    } else if (randInt == 1) {
                        throw sdk::LOGException(sdk::LOGE_SHARD_WRITE_QUOTA_EXCEED, "LOGE_SHARD_WRITE_QUOTA_EXCEED");
                    }
                } else if (gSendFailType == 5) {
                    int randInt = rand() % 2;
                    if (randInt == 0) {
                        throw sdk::LOGException(sdk::LOGE_SERVER_BUSY, "LOGE_WRITE_QUOTA_EXCEED");
                    } else if (randInt == 1) {
                        throw sdk::LOGException(sdk::LOGE_INTERNAL_SERVER_ERROR, "LOGE_SHARD_WRITE_QUOTA_EXCEED");
                    }
                }

            } else {
                //printf("[@MockSyncSend] success %s %s %d.\n", projectName.c_str(), logstore.c_str(), rawSize);
            }
        }
    }

    static void MockAsyncSend(const std::string& projectName,
                              const std::string& logstore,
                              const std::string& logData,
                              SEND_DATA_TYPE dataType,
                              int32_t rawSize,
                              SendClosure* sendClosure) {
        // printf("Insert request.\n");
        //gSenderTest->MockAsyncSendInner(projectName, logstore, logData, dataType, rawSize, sendClosure);
        //return;
        AsyncRequest* pReq = new AsyncRequest(projectName, logstore, logData, dataType, rawSize, sendClosure);

        {
            PTScopedLock lock(mQueueLock);
            mAsynRequestQueue.push(pReq);
        }

        {
            WaitObject::Lock lock(mWait);
            ReadWriteBarrier();
            mSignalFlag = true;
            mWait.signal();
        }
    }

    static void MockAsyncSendInner(const std::string& projectName,
                                   const std::string& logstore,
                                   const std::string& logData,
                                   SEND_DATA_TYPE dataType,
                                   int32_t rawSize,
                                   SendClosure* sendClosure) {
        LOG_INFO(sLogger, ("MockAsyncSend, projectName", projectName)("logstore", logstore)("dataType", dataType));
        vector<LogGroup> logGroupVec;
        Sender::ParseLogGroupFromString(logData, dataType, rawSize, logGroupVec);

        if (gDisableIpFlag && gClient != NULL && gDisabledIp.size() > 0) {
            if (gClient->GetRawSlsHost().find(gDisabledIp) != std::string::npos) {
                auto sr = new sdk::PostLogStoreLogsResponse;
                sr->statusCode = 500;
                sr->requestId = "";
                sendClosure->OnFail(sr, sdk::LOGE_REQUEST_ERROR, "timeout to connect network");
                return;
            }
        }

        bool projectDisabled = true;

        for (vector<LogGroup>::iterator iter = logGroupVec.begin(); iter != logGroupVec.end(); ++iter) {
            // Record LogGroup information for debug.
            {
                LogGroup& logGroup = *iter;
                std::string logCase;
                if (logGroup.logs_size() > 0) {
                    auto& log = logGroup.logs(0);
                    if (log.contents_size() >= 3) {
                        logCase = log.contents(2).value();
                    }
                }
                LOG_INFO(sLogger,
                         ("LogGroupDebugInfo",
                          logCase)("Project", projectName)("Logstore", logstore)("LogCount", logGroup.logs_size()));
            }

            if (iter->logs_size() > 0) {
                if (gNetWorkStat && iter->category() == "app_log") {
                    PTScopedLock lock(gBufferLogGroupsLock);
                    gBufferLogGroups.push_back(*iter);
                }
                if (gNetWorkStat && (projectName.find("_proj") != string::npos)) /* "1000000_proj" */
                {
                    int prjIndex = atoi(projectName.substr(0, 7).c_str()) - 1000000;
                    if (prjIndex <= gProjectNetEnableIndex) {
                        projectDisabled = false;
                        gRecvLogGroupLock.lock();
                        gRcvLogGroup = *iter;
                        gCounter += gRcvLogGroup.logs_size();
                        gMessageSize += gRcvLogGroup.ByteSize();
                        gRecvLogGroupLock.unlock();
                    } else {
                        //printf("Reject %s %d\n", projectName.c_str(), gProjectNetEnableIndex);
                        ++gAsynProjectSendFailCount;
                    }
                }
                if (gNetWorkStat && projectName == STRING_FLAG(profile_project_name)
                    && iter->category() == "logtail_status_profile") {
                    gStatusCount++;
                    gStatusLogGroup = *iter;
                }
            }
        }
        // can't put this code block in for, if hava multi loggroup, the sendClosure will be destroyed multi times.
        if (gNetWorkStat && !projectDisabled) {
            //printf("[#MockAsyncSend] success %s %s %d.\n", projectName.c_str(), logstore.c_str(), rawSize);

            sdk::PostLogStoreLogsResponse* sr = new sdk::PostLogStoreLogsResponse;
            sr->statusCode = 200;
            sr->requestId = "mock_request_id";
            sendClosure->OnSuccess(sr);
        } else {
            sdk::PostLogStoreLogsResponse* sr = new sdk::PostLogStoreLogsResponse;
            sr->statusCode = 500;
            sr->requestId = "mock_request_id";

            //printf("[#MockAsyncSend] fail %s %s %d.\n", projectName.c_str(), logstore.c_str(), rawSize);
            if (gSendFailType == 1)
                sendClosure->OnFail(sr, sdk::LOGE_REQUEST_ERROR, "timeout to connect network");
            else if (gSendFailType == 2) {
                int randInt = rand() % 4;
                if (randInt == 0)
                    sendClosure->OnFail(sr, sdk::LOGE_REQUEST_ERROR, "Can not connect to server.");
                else if (randInt == 1)
                    sendClosure->OnFail(sr, sdk::LOGE_WRITE_QUOTA_EXCEED, "project write quota exceed.");
                else if (randInt == 2)
                    sendClosure->OnFail(sr, sdk::LOGE_SERVER_BUSY, "connect to server timeout.");
                else if (randInt == 3)
                    sendClosure->OnFail(sr, sdk::LOGE_INTERNAL_SERVER_ERROR, "connect to server timeout.");
            } else if (gSendFailType == 3) {
                int randInt = rand() % 5;
                if (randInt == 0)
                    sendClosure->OnFail(sr, sdk::LOGE_UNAUTHORIZED, "LOGE_UNAUTHORIZED.");
                else if (randInt == 1)
                    sendClosure->OnFail(sr, sdk::LOGE_BAD_RESPONSE, "LOGE_BAD_RESPONSE.");
                else if (randInt == 2)
                    sendClosure->OnFail(sr, sdk::LOGE_CATEGORY_NOT_EXIST, "LOGE_CATEGORY_NOT_EXIST.");
                else if (randInt == 3)
                    sendClosure->OnFail(sr, sdk::LOGE_PROJECT_NOT_EXIST, "LOGE_PROJECT_NOT_EXIST.");
                else if (randInt == 4)
                    sendClosure->OnFail(sr, sdk::LOGE_TOPIC_NOT_EXIST, "LOGE_TOPIC_NOT_EXIST.");
            } else if (gSendFailType == 4) {
                int randInt = rand() % 2;
                if (randInt == 0) {
                    sendClosure->OnFail(sr, sdk::LOGE_WRITE_QUOTA_EXCEED, "LOGE_WRITE_QUOTA_EXCEED");
                } else if (randInt == 1) {
                    sendClosure->OnFail(sr, sdk::LOGE_SHARD_WRITE_QUOTA_EXCEED, "LOGE_SHARD_WRITE_QUOTA_EXCEED");
                }
            } else if (gSendFailType == 5) {
                int randInt = rand() % 2;
                if (randInt == 0) {
                    sendClosure->OnFail(sr, sdk::LOGE_SERVER_BUSY, "LOGE_WRITE_QUOTA_EXCEED");
                } else if (randInt == 1) {
                    sendClosure->OnFail(sr, sdk::LOGE_INTERNAL_SERVER_ERROR, "LOGE_SHARD_WRITE_QUOTA_EXCEED");
                }
            }
        }
    }

    static void MockIntegritySend(LoggroupTimeValue* data) {
        const std::string& project = data->mProjectName;
        if (project[6] - '0' >= 4)
            LogIntegrity::GetInstance()->Notify(data, true);
    }

    void MockUserConfigJsonForMarkOffset() {
        // remove old suer_log_config.json
        bfs::remove_all(STRING_FLAG(user_log_config));

        // construct json
        Json::Value root;
        Json::Value metrics;
        Json::Value commonreg_com;

        commonreg_com["project_name"] = Json::Value("1000000_proj");
        commonreg_com["category"] = Json::Value("app_log");
        commonreg_com["log_path"] = Json::Value(gRootDir + "MarkOffsetTest");
        commonreg_com["file_pattern"] = Json::Value("*.log");
        commonreg_com["log_type"] = Json::Value("common_reg_log");
        commonreg_com["max_send_rate"] = Json::Value(-1);
        commonreg_com["send_rate_expire"] = Json::Value(0);
        commonreg_com["timeformat"] = Json::Value("%Y-%m-%d %H:%M:%S");

        Json::Value customized_fields;
        customized_fields["mark_offset"] = Json::Value(true);
        customized_fields["fuse_mode"] = Json::Value(false);
        commonreg_com["customized_fields"] = customized_fields;

        commonreg_com["enable"] = Json::Value(true);
        commonreg_com["preserve"] = Json::Value(true);
        commonreg_com["max_depth"] = Json::Value(10);

        Json::Value regs;
        regs.append(Json::Value("(.*)"));
        Json::Value keys;
        keys.append(Json::Value("content"));
        commonreg_com["regex"] = regs;
        commonreg_com["keys"] = keys;

        commonreg_com["version"] = Json::Value(1);

        metrics["commonreg.com"] = commonreg_com;
        root["metrics"] = metrics;

        // write json file
        std::ofstream file(STRING_FLAG(user_log_config).c_str());
        file << root.toStyledString();
        file.close();
    }

    static void EnableNetWork() { gNetWorkStat = true; }

    static void DisableNetWork() { gNetWorkStat = false; }


public:
    static void SetUpTestCase() //void Setup()
    {
        sLogger->set_level(spdlog::level::trace);
        printf("Test case setup.\n");
        srand(time(NULL));
        Sender::Instance()->AddEndpointEntry(STRING_FLAG(default_region_name), STRING_FLAG(logtail_send_address), true);
        STRING_FLAG(profile_project_name) = "sls-admin";
        INT32_FLAG(sls_host_update_interval) = 1;
        INT32_FLAG(logtail_alarm_interval) = 600;
        BOOL_FLAG(enable_mock_send) = true;
        gRootDir = GetProcessExecutionDir();
        if (PATH_SEPARATOR[0] == gRootDir.at(gRootDir.size() - 1))
            gRootDir.resize(gRootDir.size() - 1);
        gRootDir += PATH_SEPARATOR + "SenderUnittest";
        bfs::remove_all(gRootDir);

        auto const sysConfDir = gRootDir + PATH_SEPARATOR + ".ilogtail" + PATH_SEPARATOR;
        bfs::create_directories(sysConfDir);
        AppConfig::GetInstance()->SetLogtailSysConfDir(sysConfDir);
        sCptM = CheckpointManagerV2::GetInstance();
        sQueueM = QueueManager::GetInstance();
        sEventDispatcher = EventDispatcher::GetInstance();
        sProcessQueueMap = &(LogProcess::GetInstance()->GetQueue().mLogstoreQueueMap);
        sSenderQueueMap = &(Sender::Instance()->GetQueue().mLogstoreSenderQueueMap);

        new Thread(&SenderUnittest::MockAsyncSendThread);
    }
    static void TearDownTestCase() //void CleanUp()
    {
        StopMockSendThread();
        try {
            bfs::remove_all(gRootDir);
        } catch (...) {
        }
    }

    void CaseSetUp(bool isFilter = false,
                   bool hasAliuid = false,
                   bool hasEndpoint = false,
                   int projectNum = 1,
                   bool hasTopic = false,
                   int32_t maxSendRateBytes = -1,
                   int32_t expireTime = 0,
                   int32_t clientDisableRetry = 0) {
        // Test and enable container mode (if the special local file is existing).
        if (bfs::exists("LogtailContainerModeTest")) {
            cout << "replace with container config" << endl;
            ReplaceWithContainerModeConfig();
        }

        ClearRequestQueue();
        gAsynMockLatencyMs = 0;
        gSynMockLatencyMs = 0;
        INT32_FLAG(client_disable_send_retry_interval) = clientDisableRetry;
        INT32_FLAG(buffer_check_period) = 2;
        INT32_FLAG(buffer_file_alive_interval) = 3;
        gProjectNetEnableIndex = 0;
        gAsynProjectSendFailCount = 0;
        gSynProjectSendFailCount = 0;
        gSendFailType = 1;
        gTestNetWorkStat = false;
        if (gLogIntegrityTestFlag)
            WriteIntegrityConfigJson(projectNum);
        else if (gGlobalMarkOffsetTestFlag)
            MockUserConfigJsonForMarkOffset();
        else
            WriteConfigJson(isFilter, hasAliuid, hasEndpoint, projectNum, hasTopic, maxSendRateBytes, expireTime);

        {
            PTScopedLock lock(gBufferLogGroupsLock);
            gBufferLogGroups.clear();
        }
        bfs::remove("ilogtail_config.json");
        {
            fsutil::Dir dir(gRootDir);
            dir.Open();
            fsutil::Entry entry;
            while (entry = dir.ReadNext(false)) {
                auto fullPath = gRootDir + PATH_SEPARATOR + entry.Name();
                auto targetPath = gRootDir + PATH_SEPARATOR + ".." + PATH_SEPARATOR + entry.Name();
                bfs::rename(fullPath, targetPath);
                bfs::remove_all(targetPath);
            }
        }
        sCptM->close();
        bfs::remove_all(gRootDir);
        bfs::create_directories(AppConfig::GetInstance()->GetLogtailSysConfDir());
        sCptM->open();
        if (gEnableExactlyOnce) {
            clearGlobalResource();
        }
        AppConfig::GetInstance()->LoadAppConfig(STRING_FLAG(ilogtail_config));
        bool ret = ConfigManager::GetInstance()->LoadConfig(STRING_FLAG(user_log_config));
        assert(ret);
        ret = Sender::Instance()->InitSender();
        assert(ret);

        if (gLogIntegrityTestFlag) {
            Sender::Instance()->MockIntegritySend = MockIntegritySend;
        } else if (gEnableExactlyOnce) {
            gRangeCheckpoints.clear();
            gBlockedHashKeySet.clear();
            Sender::Instance()->MockIntegritySend = MockExactlyOnceSend;
        } else {
            Sender::Instance()->MockIntegritySend = NULL;
        }

        Sender::Instance()->MockAsyncSend = MockAsyncSend;
        Sender::Instance()->MockSyncSend = MockSyncSend;
        Sender::Instance()->mStopRealIpThread = false;
        gCounter = 0;
        gMessageSize = 0;
        vector<string> filesToSend;
        Sender::Instance()->LoadFileToSend(time(NULL), filesToSend);
        for (size_t i = 0; i < filesToSend.size(); ++i) {
            remove((Sender::Instance()->mBufferFilePath + filesToSend[i]).c_str());
        }
        gDispatchThreadId.reset(new std::thread(RunningDispatcher));
        sleep(1);
    }


    void ReplaceWithContainerModeConfig() {
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

    template <class T>
    void ClearMap(T& m) {
        for (auto& iter : m) {
            delete iter.second;
        }
        m.clear();
    }

    void CaseCleanUp() {
        LogInput::GetInstance()->CleanEnviroments();
        sleep(1);
        EventDispatcher::GetInstance()->CleanEnviroments();
        LogProcess::GetInstance()->CleanEnviroments();
        Sender::Instance()->RemoveSender();
        if (gRealIpSendThread) {
            Sender::Instance()->mStopRealIpThread = true;
            gRealIpSendThread.reset();
            PTScopedLock lock(Sender::Instance()->mRegionRealIpLock);
            ClearMap(Sender::Instance()->mRegionRealIpMap);
        }
        {
            PTScopedLock lock(Sender::Instance()->mRegionEndpointEntryMapLock);
            ClearMap(Sender::Instance()->mRegionEndpointEntryMap);
        }
        {
            PTScopedLock lock(Sender::Instance()->mSendClientLock);
            ClearMap(Sender::Instance()->mSendClientMap);
        }
        gRecvLogGroupLock.lock();
        gRcvLogGroup.Clear();
        gRecvLogGroupLock.unlock();
        ConfigManager::GetInstance()->CleanEnviroments();
        delete ConfigManager::GetInstance()->mSharedHandler;
        ConfigManager::GetInstance()->mSharedHandler = NULL;
        LogIntegrity::GetInstance()->ClearForTest();

        ClearRequestQueue();
        {
            PTScopedLock lock(gBufferLogGroupsLock);
            gBufferLogGroups.clear();
        }

        gDispatchThreadId->join();
        gDispatchThreadId = nullptr;

        sCptM->close();
        bfs::remove_all(gRootDir);
        PollingDirFile::GetInstance()->ClearCache();
        PollingModify::GetInstance()->ClearCache();

        gEnableExactlyOnce = false;
        gLogIntegrityTestFlag = false;
        gGlobalMarkOffsetTestFlag = false;
    }

    LogstoreSenderInfo* GetSenderInfo(int projectId) {
        char projectName[32];
        sprintf(projectName, "%d_proj", 1000000 + projectId);

        string logstore("app_log");

        LogstoreFeedBackKey key = GenerateLogstoreFeedBackKey(string(projectName), logstore);

        LogstoreSenderInfo* pInfo = Sender::Instance()->mSenderQueue.GetSenderInfo(key);
        if (pInfo == NULL) {
            APSARA_TEST_TRUE_DESC(false, string(projectName) + " " + logstore + " send queue does not exist.");
            return NULL;
        }
        return pInfo;
    }

    void CheckSendClientStatus(int projectId, bool canSend) {
        LogstoreSenderInfo* pInfo = GetSenderInfo(projectId);
        if (pInfo == NULL) {
            return;
        }
        APSARA_TEST_EQUAL_DESC(pInfo->CanSend(time(NULL)), canSend, string("projectid : ") + ToString(projectId));
        //printf("EQUAL %d %d\n", projectId, canSend ? 1 : 0);
    }

    const int PROJECT_NUM = 8;
    const int32_t ROUND_NUM = 10;
    // 100 logs per round to let log time to be seperated in different minutes,
    //   so there will be two log groups.
    const int32_t LOG_COUNT_PER_PROJECT = 100;
    const int32_t TOTAL_LOG_COUNT_PER_PROJECT = ROUND_NUM * LOG_COUNT_PER_PROJECT;
    const int32_t LOG_SEQ_NO = 5;

    void TestMultiUserSeperationAndRetryIntervalRecovery() {
        LOG_INFO(sLogger, ("TestMultiUserSeperationAndRetryIntervalRecovery() begin", time(NULL)));
        EnableNetWork();
        CaseSetUp(false, true, true, PROJECT_NUM, false, -1, 0, 900);
        int32_t defaultSendErrorCount = INT32_FLAG(max_client_send_error_count);
        INT32_FLAG(max_client_send_error_count) = 5;
        Sender::Instance()->MockTestEndpoint = MockSyncSend;
        gProjectNetEnableIndex = PROJECT_NUM / 2 - 1;

        // Generate 1000 logs for each project.
        for (int round = 0; round < ROUND_NUM; ++round) {
            for (int prjIndex = 0; prjIndex < PROJECT_NUM; ++prjIndex) {
                OneJob(LOG_COUNT_PER_PROJECT,
                       gRootDir,
                       "Job",
                       true,
                       time(NULL),
                       "TestMultiUserSeperation",
                       LOG_SEQ_NO,
                       false,
                       prjIndex);
            }
            usleep(200 * 1000);
        }
        sleep(20);

        // 4 enabeld projects (half) can send logs, so:
        // - 4000 logs are received.
        // - at least 4 projects have send failures.
        // - only 4 enabled projects' clients are normal (CanSend).
        APSARA_TEST_EQUAL(gCounter, PROJECT_NUM / 2 * TOTAL_LOG_COUNT_PER_PROJECT);
        APSARA_TEST_TRUE(gAsynProjectSendFailCount >= PROJECT_NUM / 2);
        for (int prjIndex = 0; prjIndex < PROJECT_NUM; ++prjIndex) {
            CheckSendClientStatus(prjIndex, prjIndex <= gProjectNetEnableIndex);
        }
        gRecvLogGroupLock.lock();
        const Log& log = gRcvLogGroup.logs(0);
        APSARA_TEST_EQUAL(log.contents(2).value(), "TestMultiUserSeperation");
        APSARA_TEST_EQUAL(log.contents(3).value(), ToString(LOG_SEQ_NO));
        gRecvLogGroupLock.unlock();

        // Enable the network of the other 4 projects, try to recover by resetting retry interval
        //   to give sender a change to resend (in method LogstoreSenderInfo::CanSend).
        gAsynProjectSendFailCount = 0;
        gSynProjectSendFailCount = 0;
        gCounter = 0;
        gProjectNetEnableIndex = PROJECT_NUM;
        for (int prjIndex = 0; prjIndex < PROJECT_NUM; ++prjIndex) {
            LogstoreSenderInfo* info = GetSenderInfo(prjIndex);
            if (info != NULL) {
                info->mLastNetworkErrorTime = 0;
            }
        }

        // Generate more logs (1000 per project).
        for (int round = 0; round < ROUND_NUM; ++round) {
            for (int prjIndex = 0; prjIndex < PROJECT_NUM; ++prjIndex) {
                OneJob(LOG_COUNT_PER_PROJECT,
                       gRootDir,
                       "Job",
                       true,
                       time(NULL),
                       "TestMultiUserSeperationSecond",
                       LOG_SEQ_NO,
                       false,
                       prjIndex);
            }
            usleep(100 * 1000);
        }
        sleep(20);

        // All projects can send logs, so:
        // - 4000 logs in round 1 and 8000 logs in round 2 are received.
        // - no project failure.
        // - all clients are normal.
        APSARA_TEST_EQUAL(gCounter, TOTAL_LOG_COUNT_PER_PROJECT * (PROJECT_NUM / 2 + PROJECT_NUM));
        APSARA_TEST_EQUAL(gAsynProjectSendFailCount, 0);
        APSARA_TEST_EQUAL(gSynProjectSendFailCount, 0);
        for (int prjIndex = 0; prjIndex < PROJECT_NUM; ++prjIndex) {
            CheckSendClientStatus(prjIndex, true);
        }

        // Case cleanup.
        INT32_FLAG(max_client_send_error_count) = defaultSendErrorCount;
        gProjectNetEnableIndex = 0;
        CaseCleanUp();
        Sender::Instance()->MockTestEndpoint = NULL;
        LOG_INFO(sLogger, ("TestMultiUserSeperationAndRetryIntervalRecovery() end", time(NULL)));
    }

    void TestMultiUserSeperationAndTestNetWorkRecovery() {
        LOG_INFO(sLogger, ("TestMultiUserSeperationAndTestNetWorkRecovery() begin", time(NULL)));
        EnableNetWork();
        CaseSetUp(false, true, true, PROJECT_NUM, false, -1, 0, 900);
        int32_t defaultSendErrorCount = INT32_FLAG(max_client_send_error_count);
        INT32_FLAG(max_client_send_error_count) = 5;
        int32_t defaultTestEndpointInterval = INT32_FLAG(test_unavailable_endpoint_interval);
        INT32_FLAG(test_unavailable_endpoint_interval) = 5; // Quick recover for disabled projects.
        Sender::Instance()->MockTestEndpoint = MockSyncSend;
        gProjectNetEnableIndex = PROJECT_NUM / 2 - 1;

        // Generate 1000 logs for each project.
        for (int round = 0; round < ROUND_NUM; ++round) {
            for (int prjIndex = 0; prjIndex < PROJECT_NUM; ++prjIndex) {
                OneJob(LOG_COUNT_PER_PROJECT,
                       gRootDir,
                       "Job",
                       true,
                       time(NULL),
                       "TestMultiUserSeperation",
                       LOG_SEQ_NO,
                       false,
                       prjIndex);
            }
            usleep(200 * 1000);
        }
        sleep(20);

        // 4 enabeld projects (half) can send logs, so:
        // - 4000 logs are received.
        // - at least 4 projects have send failures.
        // - only 4 enabled projects' clients are normal (CanSend).
        APSARA_TEST_EQUAL(gCounter, PROJECT_NUM / 2 * TOTAL_LOG_COUNT_PER_PROJECT);
        APSARA_TEST_TRUE(gAsynProjectSendFailCount >= PROJECT_NUM / 2);
        for (int prjIndex = 0; prjIndex < PROJECT_NUM; ++prjIndex) {
            CheckSendClientStatus(prjIndex, prjIndex <= gProjectNetEnableIndex);
        }
        gRecvLogGroupLock.lock();
        const Log& log = gRcvLogGroup.logs(0);
        APSARA_TEST_EQUAL(log.contents(2).value(), "TestMultiUserSeperation");
        APSARA_TEST_EQUAL(log.contents(3).value(), ToString(LOG_SEQ_NO));
        gRecvLogGroupLock.unlock();

        // Enable the network of the other 4 projects, try to recover by TestEndpoint.
        gAsynProjectSendFailCount = 0;
        gSynProjectSendFailCount = 0;
        gCounter = 0;
        gProjectNetEnableIndex = PROJECT_NUM;
        gTestNetWorkStat = true;
        sleep(INT32_FLAG(test_unavailable_endpoint_interval));
        for (int32_t idx = 0; idx < INT32_FLAG(test_unavailable_endpoint_interval) * 2; ++idx) {
            if (gCounter == TOTAL_LOG_COUNT_PER_PROJECT * PROJECT_NUM / 2) {
                break;
            }
            sleep(1);
        }

        // Generate more logs (1000 per project).
        for (int round = 0; round < ROUND_NUM; ++round) {
            for (int prjIndex = 0; prjIndex < PROJECT_NUM; ++prjIndex) {
                OneJob(LOG_COUNT_PER_PROJECT,
                       gRootDir,
                       "Job",
                       true,
                       time(NULL),
                       "TestMultiUserSeperationSecond",
                       LOG_SEQ_NO,
                       false,
                       prjIndex);
            }
            usleep(100 * 1000);
        }
        sleep(20);

        // All projects can send logs, so:
        // - 4000 logs in round 1 and 8000 logs in round 2 are received.
        // - no project failure.
        // - all clients are normal.
        APSARA_TEST_EQUAL(gCounter, TOTAL_LOG_COUNT_PER_PROJECT * (PROJECT_NUM / 2 + PROJECT_NUM));
        APSARA_TEST_EQUAL(gAsynProjectSendFailCount, 0);
        APSARA_TEST_EQUAL(gSynProjectSendFailCount, 0);
        for (int prjIndex = 0; prjIndex < PROJECT_NUM; ++prjIndex) {
            CheckSendClientStatus(prjIndex, true);
        }

        // Case cleanup.
        Sender::Instance()->MockTestEndpoint = NULL;
        INT32_FLAG(max_client_send_error_count) = defaultSendErrorCount;
        INT32_FLAG(test_unavailable_endpoint_interval) = defaultTestEndpointInterval;
        gProjectNetEnableIndex = 0;
        CaseCleanUp();
        Sender::Instance()->MockTestEndpoint = NULL;
        LOG_INFO(sLogger, ("TestMultiUserSeperationAndTestNetWorkRecovery() end", time(NULL)));
    }

    void TestMultiUserSeperationAndRetryQuotaRecovery() {
        LOG_INFO(sLogger, ("TestMultiUserSeperationAndRetryQuotaRecovery() begin", time(NULL)));
        EnableNetWork();
        CaseSetUp(false, true, true, PROJECT_NUM);
        auto bakClientQuotaSendRetryInterval = INT32_FLAG(client_quota_send_retry_interval);
        INT32_FLAG(client_quota_send_retry_interval) = 900;
        int32_t defaultQuotaErrorCount = INT32_FLAG(max_client_quota_exceed_count);
        INT32_FLAG(max_client_quota_exceed_count) = 5;
        int32_t seqNo = 5;

        // try to set net stat ok, this will set quota interval
        gProjectNetEnableIndex = PROJECT_NUM;
        for (int round = 0; round < 1; ++round) {
            for (int prjIndex = 0; prjIndex < PROJECT_NUM; ++prjIndex) {
                // use 100, let time to be seperated in different minutes, so there will be two packets
                OneJob(10, gRootDir, "Job", true, time(NULL), "TestMultiUserSeperation", seqNo, false, prjIndex);
            }
            usleep(200 * 1000);
        }
        WaitForFileBeenRead();
        // printf("[###] %d %d\n", gCounter, gAsynProjectSendFailCount);
        sleep(10);
        APSARA_TEST_EQUAL(gCounter, 10 * PROJECT_NUM);
        gCounter = 0;
        Sender::Instance()->MockTestEndpoint = MockSyncSend;
        gProjectNetEnableIndex = PROJECT_NUM / 2 - 1;
        // quota exceed alarm
        gSendFailType = 4;
        for (int round = 0; round < 10; ++round) {
            for (int prjIndex = 0; prjIndex < PROJECT_NUM; ++prjIndex) {
                // use 100, let time to be seperated in different minutes, so there will be two packets
                OneJob(100, gRootDir, "Job", true, time(NULL), "TestMultiUserSeperation", seqNo, false, prjIndex);
            }
            usleep(200 * 1000);
        }

        WaitForFileBeenRead();
        // printf("[###] %d %d\n", gCounter, gAsynProjectSendFailCount);
        sleep(20);
        // printf("[###] %d %d\n", gCounter, gAsynProjectSendFailCount);
        APSARA_TEST_EQUAL(gCounter, PROJECT_NUM / 2 * 1000);

        for (int prjIndex = 0; prjIndex < PROJECT_NUM; ++prjIndex) {
            CheckSendClientStatus(prjIndex, prjIndex <= gProjectNetEnableIndex);
        }
        gRecvLogGroupLock.lock();
        const Log& log = gRcvLogGroup.logs(0);
        APSARA_TEST_EQUAL(log.contents(2).value(), "TestMultiUserSeperation");
        APSARA_TEST_EQUAL(log.contents(3).value(), ToString(seqNo));
        APSARA_TEST_TRUE(gAsynProjectSendFailCount >= PROJECT_NUM / 2);
        gRecvLogGroupLock.unlock();
        //APSARA_TEST_TRUE(gSynProjectSendFailCount > 0);

        // try to recover the connection

        // set lastNetworkErrorTime 0 to give sender a chance to send again.
        for (int prjIndex = 0; prjIndex < PROJECT_NUM; ++prjIndex) {
            LogstoreSenderInfo* pInfo = GetSenderInfo(prjIndex);
            if (pInfo != NULL) {
                pInfo->mLastNetworkErrorTime = 0;
                pInfo->mLastQuotaExceedTime = 0;
            }
        }


        gAsynProjectSendFailCount = 0;
        gSynProjectSendFailCount = 0;
        gProjectNetEnableIndex = PROJECT_NUM;
        gCounter = 0;
        for (int round = 0; round < 10; ++round) {
            for (int prjIndex = 0; prjIndex < PROJECT_NUM; ++prjIndex) {
                OneJob(100, gRootDir, "Job", true, time(NULL), "TestMultiUserSeperationSecond", seqNo, false, prjIndex);
            }
            usleep(100 * 1000);
        }
        // printf("[###]\n");
        WaitForFileBeenRead();
        sleep(20);
        // printf("[###]\n");
        // 100 first send and 50 secondary
        APSARA_TEST_EQUAL(gCounter, 1500 * PROJECT_NUM);
        APSARA_TEST_TRUE(gAsynProjectSendFailCount == 0);
        APSARA_TEST_TRUE(gSynProjectSendFailCount == 0);
        for (int prjIndex = 0; prjIndex < PROJECT_NUM; ++prjIndex) {
            CheckSendClientStatus(prjIndex, true);
        }

        INT32_FLAG(max_client_quota_exceed_count) = defaultQuotaErrorCount;
        gProjectNetEnableIndex = 0;
        CaseCleanUp();

        INT32_FLAG(client_quota_send_retry_interval) = bakClientQuotaSendRetryInterval;
        Sender::Instance()->MockTestEndpoint = NULL;
        LOG_INFO(sLogger, ("TestMultiUserSeperationAndRetryQuotaRecovery() end", time(NULL)));
    }

    void TestMultiUserSeperationAndDiscardFailAndOtherFail() {
        LOG_INFO(sLogger, ("TestMultiUserSeperationAndDiscardFailAndOtherFail() begin", time(NULL)));
        EnableNetWork();
        CaseSetUp(false, true, true, PROJECT_NUM);
        INT32_FLAG(client_disable_send_retry_interval) = 900;
        int32_t defaultSendErrorCount = INT32_FLAG(max_client_send_error_count);
        INT32_FLAG(max_client_send_error_count) = 5;
        Sender::Instance()->MockTestEndpoint = MockSyncSend;
        gProjectNetEnableIndex = PROJECT_NUM / 2 - 1;
        // discard error
        gSendFailType = 3;
        int32_t seqNo = 5;
        for (int round = 0; round < ROUND_NUM; ++round) {
            for (int prjIndex = 0; prjIndex < PROJECT_NUM; ++prjIndex) {
                // use 100, let time to be seperated in different minutes, so there will be two packets
                OneJob(LOG_COUNT_PER_PROJECT,
                       gRootDir,
                       "Job",
                       true,
                       time(NULL),
                       "TestMultiUserSeperation",
                       seqNo,
                       false,
                       prjIndex);
            }
            usleep(200 * 1000);
        }

        // printf("[###] %d %d\n", gCounter, gAsynProjectSendFailCount);
        WaitForFileBeenRead();
        while (Sender::Instance()->mSenderQueue.IsEmpty() == false) {
            LOG_INFO(sLogger, ("sender queue not empty", "try again"));
            sleep(1);
        }
#if defined(_MSC_VER)
        // It's not very stable on Windows, so we have to wait for a long time.
        // TODO: Figure it out why???
        sleep(40);
#endif
        // printf("[###] %d %d\n", gCounter, gAsynProjectSendFailCount);
        // half data will be discard
        APSARA_TEST_EQUAL(gCounter, PROJECT_NUM / 2 * TOTAL_LOG_COUNT_PER_PROJECT);
        // discard data error will not change send client status
        for (int prjIndex = 0; prjIndex < PROJECT_NUM; ++prjIndex) {
            CheckSendClientStatus(prjIndex, true);
        }
        gRecvLogGroupLock.lock();
        const Log& log = gRcvLogGroup.logs(0);
        APSARA_TEST_EQUAL(log.contents(2).value(), "TestMultiUserSeperation");
        APSARA_TEST_EQUAL(log.contents(3).value(), ToString(seqNo));
        APSARA_TEST_TRUE(gAsynProjectSendFailCount >= PROJECT_NUM / 2);
        gRecvLogGroupLock.unlock();
        //APSARA_TEST_TRUE(gSynProjectSendFailCount > 0);

        // server busy error
        gSendFailType = 5;


        gAsynProjectSendFailCount = 0;
        gSynProjectSendFailCount = 0;
        gCounter = 0;
        for (int round = 0; round < ROUND_NUM; ++round) {
            for (int prjIndex = 0; prjIndex < PROJECT_NUM; ++prjIndex) {
                OneJob(LOG_COUNT_PER_PROJECT,
                       gRootDir,
                       "Job",
                       true,
                       time(NULL),
                       "TestMultiUserSeperationSecond",
                       seqNo,
                       false,
                       prjIndex);
            }
            usleep(100 * 1000);
        }
        // printf("[###]\n");
        WaitForFileBeenRead();
        sleep(10);
        // printf("[###]\n");

        // server error will send fail, so send success count is 50
        APSARA_TEST_EQUAL(gCounter, TOTAL_LOG_COUNT_PER_PROJECT * PROJECT_NUM / 2);
        // server error will not change send client status
        for (int prjIndex = 0; prjIndex < PROJECT_NUM; ++prjIndex) {
            CheckSendClientStatus(prjIndex, true);
        }

        gProjectNetEnableIndex = PROJECT_NUM;
        sleep(10);


        INT32_FLAG(max_client_send_error_count) = defaultSendErrorCount;
        gProjectNetEnableIndex = 0;
        CaseCleanUp();

        Sender::Instance()->MockTestEndpoint = NULL;
        LOG_INFO(sLogger, ("TestMultiUserSeperationAndDiscardFailAndOtherFail() end", time(NULL)));
    }

    void TestSecondaryStorage() {
        LOG_INFO(sLogger, ("TestSecondaryStorage() begin", time(NULL)));
        CaseSetUp();
        auto sender = Sender::Instance();
        gProjectNetEnableIndex = 1000000;
        int32_t defaultCheckPeriod = sender->mCheckPeriod;
        sender->mCheckPeriod = 1;
        INT32_FLAG(buffer_file_alive_interval) = 4 * INT32_FLAG(batch_send_interval) + 5;
        DisableNetWork();
        int32_t defaultMergeLimit = INT32_FLAG(merge_log_count_limit);
        INT32_FLAG(merge_log_count_limit) = 1024 * 1024 * 10;
        int32_t defaultHoldSize = INT32_FLAG(max_holded_data_size);
        INT32_FLAG(max_holded_data_size) = 20 * 1024 * 1024;
        set<string> firstLogSet;
        set<string> secondLogSet;
        set<string> thirdLogSet;
        set<string> forthLogSet;
        size_t logCount = 30;
        while (firstLogSet.size() < logCount) {
            string first = GenerateRandomStr(1, 1024 * 2) + "_$#1";
            if (firstLogSet.find(first) == firstLogSet.end())
                firstLogSet.insert(first);
        }
        while (secondLogSet.size() < logCount) {
            string second = GenerateRandomStr(1, 1024 * 2) + "_$#2";
            if (secondLogSet.find(second) == secondLogSet.end())
                secondLogSet.insert(second);
        }
        while (thirdLogSet.size() < logCount) {
            string third = GenerateRandomStr(1, 1024 * 2) + "_$#3";
            if (thirdLogSet.find(third) == thirdLogSet.end())
                thirdLogSet.insert(third);
        }
        while (forthLogSet.size() < logCount) {
            string forth = GenerateRandomStr(1, 1024 * 2) + "_$#4";
            if (forthLogSet.find(forth) == forthLogSet.end())
                forthLogSet.insert(forth);
        }

        FileEncryption::GetInstance()->mKeyMap.clear();
        FileEncryption::KeyInfo* keyV1 = new FileEncryption::KeyInfo("73056101345ceb496c5574b8c6b7fc0e", 1);
        FileEncryption::GetInstance()->mKeyMap.insert(pair<int32_t, FileEncryption::KeyInfo*>(keyV1->mVersion, keyV1));
        FileEncryption::GetInstance()->SetDefaultKey();
        APSARA_TEST_EQUAL(FileEncryption::GetInstance()->GetDefaultKeyVersion(), 1);

        // SendFailType equals to 3 means that sending error will not write to secondary.
        // Generate logs and wait enough time so that they can arrive sender queue.
        gSendFailType = 3;
        LOG_INFO(sLogger,
                 ("gNetWorkStat", gNetWorkStat)("gSendFailType", gSendFailType)("write forthLogSet", logCount));
        for (set<string>::iterator iter = forthLogSet.begin(); iter != forthLogSet.end(); ++iter) {
            OneJob(1, gRootDir, "Job", true, time(NULL), *iter);
        }
        printf("Job done\n");
        sleep(10);
        APSARA_TEST_TRUE(sender->FlushOut(5 * 1000)); // this operation will write buffer file
        sleep(INT32_FLAG(buffer_file_alive_interval) - 4);
        vector<string> filesToSend;
        // send error will be discard, so second buffer is null
        sender->LoadFileToSend(time(NULL), filesToSend);
        APSARA_TEST_EQUAL(filesToSend.size(), 0);

        // SendFailType equals to 2 means that sending error will write to secondary.
        gSendFailType = 2; // test all retryable send error
        LOG_INFO(sLogger,
                 ("gNetWorkStat", gNetWorkStat)("gSendFailType", gSendFailType)("write firstLogSet", logCount));
        for (set<string>::iterator iter = firstLogSet.begin(); iter != firstLogSet.end(); ++iter) {
            OneJob(1, gRootDir, "Job", true, time(NULL), *iter);
        }
        printf("Job done\n");
        sleep(5);
        APSARA_TEST_TRUE(sender->FlushOut(5 * 1000)); // this operation will write buffer file
        sleep(INT32_FLAG(buffer_file_alive_interval) - 4);
        // Data sent failed will be writen to secondary, so there is a local file.
        filesToSend.clear();
        sender->LoadFileToSend(time(NULL), filesToSend);
        APSARA_TEST_EQUAL(filesToSend.size(), 1);
        LOG_INFO(sLogger, ("bufferFileList", MergeVectorString(filesToSend)));
        FileEncryption::KeyInfo* keyV2 = new FileEncryption::KeyInfo("c134368ce7840a4e217ee6d8c27a7e0f", 2);
        FileEncryption::GetInstance()->mKeyMap.insert(pair<int32_t, FileEncryption::KeyInfo*>(keyV2->mVersion, keyV2));
        FileEncryption::GetInstance()->SetDefaultKey();
        APSARA_TEST_EQUAL(FileEncryption::GetInstance()->GetDefaultKeyVersion(), 2);

        // Refresh buffer file manually.
        string bufferFileName = sender->GetBufferFileName();
        if (sender->GetBufferFileName() == bufferFileName)
            sender->CreateNewFile();
        bufferFileName = sender->GetBufferFileName();

        LOG_INFO(sLogger,
                 ("gNetWorkStat", gNetWorkStat)("gSendFailType", gSendFailType)("write secondLogSet", logCount));
        gSendFailType = 1; // test network error
        for (set<string>::iterator iter = secondLogSet.begin(); iter != secondLogSet.end(); ++iter) {
            OneJob(1, gRootDir, "Job", true, time(NULL), *iter);
        }
        printf("Job done\n");
        sleep(5);
        APSARA_TEST_TRUE(Sender::Instance()->FlushOut(5 * 1000)); // this operation will write buffer file
        sleep(INT32_FLAG(buffer_file_alive_interval) - 4);
        filesToSend.clear();
        Sender::Instance()->LoadFileToSend(time(NULL), filesToSend);
        APSARA_TEST_EQUAL(filesToSend.size(), 2);
        LOG_INFO(sLogger, ("bufferFileList", MergeVectorString(filesToSend)));

        FileEncryption::KeyInfo* keyV3 = new FileEncryption::KeyInfo("e1b07c24b3340c94945c058db193d8f4", 3);
        FileEncryption::GetInstance()->mKeyMap.insert(pair<int32_t, FileEncryption::KeyInfo*>(keyV3->mVersion, keyV3));
        FileEncryption::GetInstance()->SetDefaultKey();
        APSARA_TEST_EQUAL(FileEncryption::GetInstance()->GetDefaultKeyVersion(), 3);
        if (Sender::Instance()->GetBufferFileName() == bufferFileName)
            Sender::Instance()->CreateNewFile();
        bufferFileName = Sender::Instance()->GetBufferFileName();
        LOG_INFO(sLogger,
                 ("gNetWorkStat", gNetWorkStat)("gSendFailType", gSendFailType)("write thirdLogSet", logCount));
        for (set<string>::iterator iter = thirdLogSet.begin(); iter != thirdLogSet.end(); ++iter) {
            OneJob(1, gRootDir, "Job", true, time(NULL), *iter);
        }
        printf("Job done\n");

        // move delete key behind sleep
        // delete key 2, so second log will be discard
        FileEncryption::GetInstance()->mKeyMap.erase(2);
        FileEncryption::GetInstance()->SetDefaultKey();
        LOG_INFO(sLogger, ("delete key version 2, networkstat", gNetWorkStat));

        sleep(5);
        APSARA_TEST_TRUE(Sender::Instance()->FlushOut(5 * 1000)); // this operation will write buffer file
        sleep(INT32_FLAG(buffer_file_alive_interval) - 4); //debug
        filesToSend.clear();
        Sender::Instance()->LoadFileToSend(time(NULL), filesToSend);
        // buffer file with version2 will be deleted
        APSARA_TEST_EQUAL(filesToSend.size(), 2);
        LOG_INFO(sLogger, ("bufferFileList", MergeVectorString(filesToSend)));

        EnableNetWork();
        sleep(INT32_FLAG(buffer_file_alive_interval) + 1);
        LOG_INFO(sLogger, ("begin check, gNetWorkStat", gNetWorkStat));

        APSARA_TEST_EQUAL(gCounter, int32_t(2 * logCount));
        filesToSend.clear();
        Sender::Instance()->LoadFileToSend(time(NULL), filesToSend);
        APSARA_TEST_EQUAL(filesToSend.size(), 0);

        size_t firstLogCount = 0;
        size_t secondLogCount = 0;
        size_t thirdLogCount = 0;
        size_t forthLogCount = 0;
        APSARA_TEST_EQUAL(gBufferLogGroups.size(), 2);
        for (size_t i = 0; i < (size_t)gBufferLogGroups.size(); ++i) {
            LogGroup& logGroup = gBufferLogGroups[i];
            if (logGroup.category() != "app_log")
                continue;
            for (size_t j = 0; j < (size_t)logGroup.logs_size(); ++j) {
                const Log log = logGroup.logs(j);
                string ip = "";
                string nothing = "";
                for (size_t k = 0; k < (size_t)log.contents_size(); ++k) {
                    const Log_Content content = log.contents(k);
                    if (content.key() == "ip")
                        ip = content.value();
                    else if (content.key() == "nothing")
                        nothing = content.value();
                }
                if (ip != "10.7.241.21")
                    continue;
                LOG_INFO(sLogger, ("nothing", nothing)("substr", nothing.substr(nothing.size() - 4)));
                if (nothing.substr(nothing.size() - 4) == "_$#1") {
                    if (firstLogSet.find(nothing) != firstLogSet.end()) {
                        firstLogCount++;
                        firstLogSet.erase(nothing);
                    }
                } else if (nothing.substr(nothing.size() - 4) == "_$#3") {
                    if (thirdLogSet.find(nothing) != thirdLogSet.end()) {
                        thirdLogCount++;
                        thirdLogSet.erase(nothing);
                    }
                } else if (nothing.substr(nothing.size() - 4) == "_$#4") {
                    if (forthLogSet.find(nothing) != forthLogSet.end()) {
                        forthLogCount++;
                        forthLogSet.erase(nothing);
                    }
                } else if (nothing.substr(nothing.size() - 4) == "_$#2") {
                    if (secondLogSet.find(nothing) != secondLogSet.end()) {
                        secondLogCount++;
                        secondLogSet.erase(nothing);
                    }
                }
            }
        }

        APSARA_TEST_EQUAL(firstLogSet.size(), 0);
        APSARA_TEST_EQUAL(firstLogCount, logCount);
        APSARA_TEST_EQUAL(secondLogSet.size(), logCount);
        APSARA_TEST_EQUAL(secondLogCount, 0);
        APSARA_TEST_EQUAL(thirdLogSet.size(), 0);
        APSARA_TEST_EQUAL(thirdLogCount, logCount);
        APSARA_TEST_EQUAL(forthLogSet.size(), logCount);
        APSARA_TEST_EQUAL(forthLogCount, 0);

        FileEncryption::GetInstance()->mKeyMap.clear();
        FileEncryption::GetInstance()->LoadKeyInfo();
        FileEncryption::GetInstance()->SetDefaultKey();
        delete keyV1;
        delete keyV2;
        delete keyV3;
        INT32_FLAG(merge_log_count_limit) = defaultMergeLimit;
        INT32_FLAG(max_holded_data_size) = defaultHoldSize;
        Sender::Instance()->mCheckPeriod = defaultCheckPeriod;
        gProjectNetEnableIndex = 0;
        CaseCleanUp();
        LOG_INFO(sLogger, ("TestSecondaryStorage() end", time(NULL)));
    }

    void TestEncryptAndDecrypt() {
        LOG_INFO(sLogger, ("TestEncryptAndDecrypt() begin", time(NULL)));
        for (size_t i = 0; i < 100; ++i) {
            string data = GenerateRandomStr(1024 * 16, 1024 * 64);
            char* des;
            int32_t srcLength = (int32_t)data.size();
            int32_t encryptionLength;
            FileEncryption::GetInstance()->Encrypt(data.c_str(), srcLength, des, encryptionLength);
            string encryption = string(des, encryptionLength);
            delete[] des;
            char* src = new char[srcLength];
            FileEncryption::GetInstance()->Decrypt(encryption.c_str(), encryptionLength, src, srcLength, 1);
            string decryption = string(src, srcLength);
            delete[] src;
            APSARA_TEST_TRUE(data == decryption);
            APSARA_TEST_TRUE(encryption != decryption);

            int32_t blockBytes = FileEncryption::GetInstance()->mKeyMap[1]->mBlockBytes;
            APSARA_TEST_TRUE_DESC((encryptionLength >= srcLength && encryptionLength < (srcLength + blockBytes)),
                                  "encryption.size:" + ToString(encryptionLength) + ";decryption.size:"
                                      + ToString(srcLength) + ";blockBytes:" + ToString(blockBytes));
        }
        LOG_INFO(sLogger, ("TestEncryptAndDecrypt() end", time(NULL)));
    }

    // Wait several seconds to make sure test log files have been read.
    static void WaitForFileBeenRead() {
#if defined(_MSC_VER)
        // Because of lackness of event based discovery, on Windows, we can only use
        // polling, so wait for polling interval plus several process time (2s here).
        logtail::sleep(INT32_FLAG(dirfile_check_interval_ms) / 1000 + 2);
#endif
    }

    void TestDiscardOldData() {
        LOG_INFO(sLogger, ("TestDiscardOldData() begin", time(NULL)));
        CaseSetUp();
        EnableNetWork();
        BOOL_FLAG(ilogtail_discard_old_data) = true;
        //case#1
        gCounter = 0;
        OneJob(10, gRootDir, "Job", true, time(NULL));
        WaitForFileBeenRead();
        sleep(2 * INT32_FLAG(batch_send_interval) + 1);
        APSARA_TEST_EQUAL(gCounter, 10);

        //case#2
        gCounter = 0;
        OneJob(10, gRootDir, "Job", true, time(NULL) - INT32_FLAG(ilogtail_discard_interval) - 100);
        WaitForFileBeenRead();
        sleep(2 * INT32_FLAG(batch_send_interval) + 1);
        APSARA_TEST_EQUAL(gCounter, 0);

        //case#3
        gCounter = 0;
        time_t timeBefore1970 = -100000;
#if defined(__linux__)
        OneJob(10, gRootDir, "Job", true, timeBefore1970);
        WaitForFileBeenRead();
        sleep(2 * INT32_FLAG(batch_send_interval) + 1);
        APSARA_TEST_EQUAL(gCounter, 0);
#endif

        //case#4
        gCounter = 0;
        OneJob(1, gRootDir, "Job", true, time(NULL));
#if defined(__linux__)
        OneJob(1, gRootDir, "Job", true, timeBefore1970);
#endif
        OneJob(1, gRootDir, "Job", true, time(NULL));
        OneJob(1, gRootDir, "Job", true, time(NULL) - INT32_FLAG(ilogtail_discard_interval) - 100);
        OneJob(1, gRootDir, "Job", true, time(NULL));
#if defined(__linux__)
        OneJob(1, gRootDir, "Job", true, timeBefore1970);
#endif
        OneJob(1, gRootDir, "Job", true, time(NULL));
        OneJob(1, gRootDir, "Job", true, time(NULL) - INT32_FLAG(ilogtail_discard_interval) - 100);
        WaitForFileBeenRead();
        sleep(2 * INT32_FLAG(batch_send_interval) + 1);
        APSARA_TEST_EQUAL(gCounter, 4);

        BOOL_FLAG(ilogtail_discard_old_data) = false;
        //case#5
        gCounter = 0;
        OneJob(10, gRootDir, "Job", true, time(NULL) - INT32_FLAG(ilogtail_discard_interval) - 100);
        WaitForFileBeenRead();
        sleep(2 * INT32_FLAG(batch_send_interval) + 1);
        APSARA_TEST_EQUAL(gCounter, 10);

        //case#6
        gCounter = 0;
#if defined(__linux__)
        OneJob(10, gRootDir, "Job", true, timeBefore1970);
        WaitForFileBeenRead();
        sleep(2 * INT32_FLAG(batch_send_interval) + 1);
        APSARA_TEST_EQUAL(gCounter, 0);
#endif

        //case#7
        gCounter = 0;
        OneJob(1, gRootDir, "Job", true, time(NULL));
#if defined(__linux__)
        OneJob(1, gRootDir, "Job", true, timeBefore1970);
#endif
        OneJob(1, gRootDir, "Job", true, time(NULL));
        OneJob(1, gRootDir, "Job", true, time(NULL) - INT32_FLAG(ilogtail_discard_interval) - 100);
        OneJob(1, gRootDir, "Job", true, time(NULL));
#if defined(__linux__)
        OneJob(1, gRootDir, "Job", true, timeBefore1970);
#endif
        OneJob(1, gRootDir, "Job", true, time(NULL));
        OneJob(1, gRootDir, "Job", true, time(NULL) - INT32_FLAG(ilogtail_discard_interval) - 100);
        WaitForFileBeenRead();
        sleep(2 * INT32_FLAG(batch_send_interval) + 1);
        APSARA_TEST_EQUAL(gCounter, 6);

        CaseCleanUp();
        LOG_INFO(sLogger, ("TestDiscardOldData() end", time(NULL)));
    }

    void TestConnect() {
        LOG_INFO(sLogger, ("TestConnect() begin", time(NULL)));
        CaseSetUp();
        EnableNetWork();
        OneJob(1, gRootDir, "Job", true, time(NULL));
        WaitForFileBeenRead();
        sleep(2 * INT32_FLAG(batch_send_interval) + 1);
        APSARA_TEST_EQUAL(gCounter, 1);
        CaseCleanUp();
        LOG_INFO(sLogger, ("TestConnect() end", time(NULL)));
    }

    void TestDisConnect() {
        LOG_INFO(sLogger, ("TestDisConnect() begin", time(NULL)));
        CaseSetUp();
        LogGroup logGroup;
        logGroup.set_category("app_log");
        Log* logPtr = logGroup.add_logs();
        Log_Content* contentPtr = logPtr->add_contents();
        contentPtr->set_key("ip");
        contentPtr->set_value("10.7.241.21");
        contentPtr = logPtr->add_contents();
        contentPtr->set_key("time");
        contentPtr->set_value(ToString(time(NULL)));
        contentPtr = logPtr->add_contents();
        contentPtr->set_key("nothing");
        contentPtr->set_value(ToString(
            "abcdefghijklmnopqrsputskjueiguwdhruwldirudsjhdklguejsldiuuwjskldgsksjdkdjfksjsdkfjsksdjfksjdkfuujss"));
        contentPtr = logPtr->add_contents();
        contentPtr->set_key("seq");
        contentPtr->set_value(ToString("0"));

        DisableNetWork();
        OneJob(1, gRootDir, "Job", true, time(NULL));
        WaitForFileBeenRead();
        sleep(INT32_FLAG(batch_send_interval) * 2 + 2);
        vector<string> filesToSend;
        Sender::Instance()->LoadFileToSend(time(NULL), filesToSend);
        APSARA_TEST_EQUAL(gCounter, 0);
        //APSARA_TEST_EQUAL(filesToSend.size(), 1);
        EnableNetWork();
        sleep(INT32_FLAG(batch_send_interval) * 3 + 1);
        gRecvLogGroupLock.lock();
        APSARA_TEST_EQUAL(gRcvLogGroup.category(), logGroup.category());
        APSARA_TEST_EQUAL(gRcvLogGroup.logs_size(), logGroup.logs_size());
        for (int i = 0; i < gRcvLogGroup.logs_size() && i < logGroup.logs_size(); i++) {
            const Log& log1 = gRcvLogGroup.logs(i);
            const Log& log2 = logGroup.logs(i);
            APSARA_TEST_EQUAL_FATAL(log1.contents_size(), log2.contents_size());
            for (int j = 0; j < log1.contents_size(); j++) {
                const Log_Content& content1 = log1.contents(j);
                const Log_Content& content2 = log2.contents(j);
                APSARA_TEST_EQUAL(content1.key(), content2.key());
                APSARA_TEST_TRUE_DESC(content1.key() == "time" || content1.value() == content2.value(),
                                      content1.key() + content1.value() + content2.key() + content2.value());
            }
        }
        gRecvLogGroupLock.unlock();
        APSARA_TEST_EQUAL(gCounter, 1);
        filesToSend.clear();
        Sender::Instance()->LoadFileToSend(time(NULL), filesToSend);
        APSARA_TEST_EQUAL(filesToSend.size(), 0);
        CaseCleanUp();
        LOG_INFO(sLogger, ("TestDisConnect() end", time(NULL)));
    }

    void TestChangeStat() {
        LOG_INFO(sLogger, ("TestChangeStat() begin", time(NULL)));
        CaseSetUp();
        DisableNetWork();
        int32_t num = 10;
        for (int32_t i = 0; i < num; ++i) {
            OneJob(1, gRootDir, "job", true, time(NULL));
            WaitForFileBeenRead();
            sleep(INT32_FLAG(batch_send_interval) * 2 + 1);
            if (i % 2 == 0)
                EnableNetWork();
            else
                DisableNetWork();
        }
        EnableNetWork();
        sleep(INT32_FLAG(batch_send_interval) * 4 + 1);
        APSARA_TEST_EQUAL(gCounter, num);
        vector<string> filesToSend;
        Sender::Instance()->LoadFileToSend(time(NULL), filesToSend);
        APSARA_TEST_EQUAL(filesToSend.size(), 0);
        CaseCleanUp();
        LOG_INFO(sLogger, ("TestChangeStat() end", time(NULL)));
    }
    void TestFlowControl() {
        LOG_INFO(sLogger, ("TestFlowControl() begin", time(NULL)));
        CaseSetUp();
        DisableNetWork();
        AppConfig::GetInstance()->mBytePerSec = 1 * 1024 * 1024;
        int32_t MAX_BUFFER_FILE_SIZE = 50 * 1024 * 1024;
        while (true) {
            // if not use fixed time, then there may be many 10 seconds logs
            // if the 10 seconds have different minutes, then
            // ((value->mLogGroup).logs(0).time() / 60 != (*(mutableLogPtr + logIdx))->time() / 60)
            // is true, so there will be many little packages and send time will take very long time
            // may be one hour....................................
            OneJob(10, gRootDir, "job", true, time(NULL), "", 0, true);
            ifstream fin((gRootDir + PATH_SEPARATOR + "job.log").c_str());
            fin.seekg(0, ios::end);
            int32_t pos = fin.tellg();
            if (pos > MAX_BUFFER_FILE_SIZE) {
                fin.close();
                break;
            }
            fin.close();
        }
        WaitForFileBeenRead();
        sleep(3 * INT32_FLAG(batch_send_interval) + 1);
        //vector<string> filesToSend;
        //Sender::Instance() -> LoadFileToSend(time(NULL),filesToSend);
        //LOG_INFO(sLogger,("buffer file num",filesToSend.size()));
        //APSARA_TEST_TRUE_FATAL(filesToSend.size() > 1);
        int32_t defaultMaxBps = AppConfig::GetInstance()->mMaxBytePerSec;
        AppConfig::GetInstance()->mMaxBytePerSec = 2 * 1024 * 1024;
        EnableNetWork();
        Sender::Instance()->TestEndpoint(STRING_FLAG(default_region_name), "");
        //{
        //    PTScopedLock lock(Sender::Instance()->mBufferWait);
        //    Sender::Instance()->mBufferWait.signal();
        //}
        uint64_t start = GetCurrentTimeInMicroSeconds();
        sleep(20);
        while (true) {
            sleep(1);
            Sender* pSender = Sender::Instance();
            Aggregator* pAgg = Aggregator::GetInstance();
            if (LogProcess::GetInstance()->mLogFeedbackQueue.IsEmpty() && pAgg->IsMergeMapEmpty()
                && pSender->IsBatchMapEmpty() && pSender->GetSendingCount() == 0 && pSender->IsSecondaryBufferEmpty()) {
                break;
            }
            //pSender->mSenderQueue.PrintStatus();
            //filesToSend.clear();
            //Sender::Instance() -> LoadFileToSend(time(NULL),filesToSend);
            //if(filesToSend.size() == 0)
            //    break;
        }
        double interval = (GetCurrentTimeInMicroSeconds() - start) / 1000000.0;
        double speed = gMessageSize * 1.0 / interval;
        double diffRatio = speed / AppConfig::GetInstance()->GetMaxBytePerSec();
        APSARA_TEST_TRUE_DESC(diffRatio >= 0.9 && diffRatio <= 1.3, diffRatio);
        LOG_INFO(sLogger, ("actual speed", speed)("diffRatio", diffRatio));
        AppConfig::GetInstance()->mMaxBytePerSec = defaultMaxBps;

        if (interval < 40.) {
            sleep((uint32_t)(40. - interval));
        }
        CaseCleanUp();
        LOG_INFO(sLogger, ("TestFlowControl() end", time(NULL)));
    }

    void TestLogstoreFlowControl() {
        LOG_INFO(sLogger, ("TestLogstoreFlowControl() begin", time(NULL)));
        CaseSetUp(false, false, false, 1, false, 2 * 1024 * 1024, 0);
        DisableNetWork();
        int32_t MAX_BUFFER_FILE_SIZE = 50 * 1024 * 1024;
        while (true) {
            // if not use fixed time, then there may be many 10 seconds logs
            // if the 10 seconds have different minutes, then
            // ((value->mLogGroup).logs(0).time() / 60 != (*(mutableLogPtr + logIdx))->time() / 60)
            // is true, so there will be many little packages and send time will take very long time
            // may be one hour....................................
            OneJob(10, gRootDir, "job", true, time(NULL), "", 0, true);
            ifstream fin((gRootDir + PATH_SEPARATOR + "job.log").c_str());
            fin.seekg(0, ios::end);
            int32_t pos = fin.tellg();
            if (pos > MAX_BUFFER_FILE_SIZE) {
                fin.close();
                break;
            }
            fin.close();
        }
        WaitForFileBeenRead();
        sleep(3 * INT32_FLAG(batch_send_interval) + 1);
        //vector<string> filesToSend;
        //Sender::Instance() -> LoadFileToSend(time(NULL),filesToSend);
        //LOG_INFO(sLogger,("buffer file num",filesToSend.size()));
        //APSARA_TEST_TRUE_FATAL(filesToSend.size() > 1);
        EnableNetWork();
        Sender::Instance()->TestEndpoint(STRING_FLAG(default_region_name), "");
        //{
        //    PTScopedLock lock(Sender::Instance()->mBufferWait);
        //    Sender::Instance()->mBufferWait.signal();
        //}
        uint64_t start = GetCurrentTimeInMicroSeconds();
        sleep(20);
        while (true) {
            sleep(1);
            Sender* pSender = Sender::Instance();
            Aggregator* pAgg = Aggregator::GetInstance();
            if (LogProcess::GetInstance()->mLogFeedbackQueue.IsEmpty() && pAgg->IsMergeMapEmpty()
                && pSender->IsBatchMapEmpty() && pSender->GetSendingCount() == 0 && pSender->IsSecondaryBufferEmpty()) {
                break;
            }
            //pSender->mSenderQueue.PrintStatus();
            //filesToSend.clear();
            //Sender::Instance() -> LoadFileToSend(time(NULL),filesToSend);
            //if(filesToSend.size() == 0)
            //    break;
        }
        double interval = (GetCurrentTimeInMicroSeconds() - start) / 1000000.0;
        double speed = gMessageSize * 1.0 / interval;
        double diffRatio = speed / (2 * 1024 * 1024);
        APSARA_TEST_TRUE_DESC(diffRatio >= 0.8 && diffRatio <= 1.3, diffRatio);
        LOG_INFO(sLogger, ("actual speed", speed)("diffRatio", diffRatio));

        if (interval < 40.) {
            sleep((uint32_t)(40. - interval));
        }
        CaseCleanUp();
        LOG_INFO(sLogger, ("TestLogstoreFlowControl() end", time(NULL)));
    }

    void TestLogstoreFlowControlPause() {
        LOG_INFO(sLogger, ("TestLogstoreFlowControlPause() begin", time(NULL)));
        CaseSetUp(false, false, false, 1, false, 0, 0);
        EnableNetWork();
        for (int i = 0; i < 100; ++i) {
            OneJob(10, gRootDir, "job", true, time(NULL), "", 0, true);
        }
        WaitForFileBeenRead();
        sleep(3 * INT32_FLAG(batch_send_interval) + 1);
        APSARA_TEST_EQUAL(gCounter, 0);
        string project = "1000000_proj";
        string logstore = "app_log";
        LogstoreFeedBackKey key = GenerateLogstoreFeedBackKey(project, logstore);
        Sender::Instance()->SetLogstoreFlowControl(key, -1, 0);

        sleep(3 * INT32_FLAG(batch_send_interval) + 1);
        APSARA_TEST_EQUAL(gCounter, 1000);
        CaseCleanUp();
        LOG_INFO(sLogger, ("TestLogstoreFlowControlPause() end", time(NULL)));
    }


    void TestLogstoreFlowControlExpire() {
        LOG_INFO(sLogger, ("TestLogstoreFlowControlExpire() begin", time(NULL)));
        CaseSetUp(false, false, false, 1, false, 0, time(NULL) + 20);
        EnableNetWork();
        for (int i = 0; i < 100; ++i) {
            OneJob(10, gRootDir, "job", true, time(NULL), "", 0, true);
        }
        sleep(10);
        APSARA_TEST_EQUAL(gCounter, 0);
        sleep(20);
        APSARA_TEST_EQUAL(gCounter, 1000);
        CaseCleanUp();
        LOG_INFO(sLogger, ("TestLogstoreFlowControlExpire() end", time(NULL)));
    }

    char GetnoneUTF8Char() {
        char noneUTF8Chars[] = {char(0xff), char(0xc0), char(0xe0), char(0xf0)};
        static int index = 0;
        index++;
        index = index % 4;
        return noneUTF8Chars[index];
    }
    void GenerateNoneUTF8Char(LogGroup& logGroup) {
        for (int i = 0; i < 10; ++i) {
            Log* logPtr = logGroup.add_logs();
            logPtr->set_time(time(NULL));
            for (int j = 0; j < 10; ++j) {
                Log_Content* contentPtr = logPtr->add_contents();
                if (j == i) {
                    contentPtr->set_key("key" + ToString(j));
                    contentPtr->set_value(string(i, 'v') + string(i + 1, GetnoneUTF8Char()) + string(i, 'v'));
                } else if (j == (i + 1)) {
                    contentPtr->set_key(string(i, 'v') + string(i + 1, GetnoneUTF8Char()) + string(i, 'v'));
                    contentPtr->set_value("value" + ToString(j));
                } else if (j == (i + 2)) {
                    contentPtr->set_key("key" + ToString(j));
                    contentPtr->set_value(string(i, 'v') + string(i + 1, GetnoneUTF8Char()));
                } else if (j == (i + 3)) {
                    contentPtr->set_key("key" + ToString(j));
                    contentPtr->set_value(string(i + 1, GetnoneUTF8Char()) + string(i, 'v'));
                } else {
                    contentPtr->set_key("key" + ToString(j));
                    contentPtr->set_value("value" + ToString(j));
                }
            }
        }
    }

    void TestFilterUTF8() {
        LOG_INFO(sLogger, ("TestFilterUTF8() begin", time(NULL)));
        CaseSetUp();
        EnableNetWork();

        string category = "app_log";
        LogGroup srcLog;
        srcLog.set_category(category);
        GenerateNoneUTF8Char(srcLog);
        Config* config = new Config("", "", APSARA_LOG, "", "", "1000000_proj", false, 2, -1, category);
        config->mFilterRule = NULL;
        config->mDiscardNoneUtf8 = true;
        Sender::Instance()->Send("1000000_proj", "", srcLog, config, MERGE_BY_TOPIC, srcLog.ByteSize());
        sleep(INT32_FLAG(batch_send_interval) * 2 + 1);
        gRecvLogGroupLock.lock();
        const LogGroup& mg = gRcvLogGroup;
        char noneUTF8Char = ' ';

        APSARA_TEST_EQUAL(mg.logs_size(), 10);

        for (int i = 0; i < mg.logs_size(); ++i) {
            const Log& log = mg.logs(i);
            APSARA_TEST_EQUAL(log.contents_size(), 10);
            for (int k = 0; k < log.contents_size() && k < 10; ++k) {
                int j = k;
                const Log_Content& content = log.contents(k);
                string key = content.key();
                string value = content.value();
                if (j == i) {
                    APSARA_TEST_EQUAL(key, "key" + ToString(j));
                    APSARA_TEST_EQUAL(value, string(i, 'v') + string(i + 1, noneUTF8Char) + string(i, 'v'));
                } else if (j == (i + 1)) {
                    APSARA_TEST_EQUAL(key, string(i, 'v') + string(i + 1, noneUTF8Char) + string(i, 'v'));
                    APSARA_TEST_EQUAL(value, "value" + ToString(j));
                } else if (j == (i + 2)) {
                    APSARA_TEST_EQUAL(key, "key" + ToString(j));
                    APSARA_TEST_EQUAL(value, string(i, 'v') + string(i + 1, noneUTF8Char));
                } else if (j == (i + 3)) {
                    APSARA_TEST_EQUAL(key, "key" + ToString(j));
                    APSARA_TEST_EQUAL(value, string(i + 1, noneUTF8Char) + string(i, 'v'));
                } else {
                    APSARA_TEST_EQUAL(key, "key" + ToString(j));
                    APSARA_TEST_EQUAL(value, "value" + ToString(j));
                }
            }
        }
        delete config;
        gRcvLogGroup.Clear();
        gRecvLogGroupLock.unlock();
        CaseCleanUp();
        LOG_INFO(sLogger, ("TestFilterUTF8() end", time(NULL)));
    }

    void TestMergeByCount() {
        LOG_INFO(sLogger, ("TestMergeByCount() begin", time(NULL)));
        int32_t defaultMergeLimit = INT32_FLAG(merge_log_count_limit);
        INT32_FLAG(merge_log_count_limit) = rand() % 100 + 1;
        LOG_INFO(sLogger, ("merge_log_count_limit", INT32_FLAG(merge_log_count_limit)));
        CaseSetUp();
        EnableNetWork();

        for (size_t caseId = 0; caseId < 3; ++caseId) {
            gBufferLogGroups.clear();

            size_t totalSendLogNum = 0;
            for (size_t j = 0; j < 10; j++) {
                size_t randSendLogNum = rand() % INT32_FLAG(merge_log_count_limit);
                totalSendLogNum += randSendLogNum;
                OneJob(randSendLogNum, gRootDir, "Job", true, time(NULL), "");
                LOG_INFO(sLogger, ("send loop", j)("totalSendLogNum", totalSendLogNum)("current size", randSendLogNum));
            }
            WaitForFileBeenRead();
            sleep(INT32_FLAG(batch_send_interval) + 3);

            size_t totalRecieveLogNum = 0;
            for (size_t j = 0; j < (size_t)gBufferLogGroups.size(); ++j) {
                LogGroup& logGroup = gBufferLogGroups[j];
                APSARA_TEST_TRUE_DESC(logGroup.logs_size() <= INT32_FLAG(merge_log_count_limit), logGroup.logs_size());
                totalRecieveLogNum += logGroup.logs_size();
                LOG_INFO(
                    sLogger,
                    ("recv loop", j)("totalRecieveLogNum", totalRecieveLogNum)("current size", logGroup.logs_size())(
                        "merge_log_count_limit", INT32_FLAG(merge_log_count_limit)));
            }
            APSARA_TEST_EQUAL(totalSendLogNum, totalRecieveLogNum);
        }

        CaseCleanUp();
        INT32_FLAG(merge_log_count_limit) = defaultMergeLimit;
        LOG_INFO(sLogger, ("TestMergeByCount() end", time(NULL)));
    }

    void TestFilterRule() {
        LOG_INFO(sLogger, ("TestFilterRule() begin", time(NULL)));
        CaseSetUp(true);
        EnableNetWork();
        OneJob(1, gRootDir, "Job", true, time(NULL), "filter", 23234); //not match
        OneJob(1, gRootDir, "Job", true, time(NULL), "testfilter", 5); //not match
        OneJob(1, gRootDir, "Job", true, time(NULL), "filtersdfas", 5); //match
        OneJob(1, gRootDir, "Job", true, time(NULL), "sdfsds", 5); //not match
        WaitForFileBeenRead();
        sleep(INT32_FLAG(batch_send_interval) + 3);

        APSARA_TEST_EQUAL(gCounter, 1);

        for (size_t i = 0; i < gBufferLogGroups.size(); i++) {
            LogGroup& loggroup = gBufferLogGroups[i];
            for (int32_t j = 0; j < loggroup.logs_size(); j++) {
                const Log& log = loggroup.logs(j);
                for (int k = 0; k < log.contents_size(); k++) {
                    const Log_Content& content = log.contents(k);
                    if (content.key() == "nothing") {
                        APSARA_TEST_EQUAL(content.value(), "filtersdfas");
                    }
                    if (content.key() == "seq") {
                        APSARA_TEST_EQUAL(content.value(), "5");
                    }
                }
            }
        }

        CaseCleanUp();
        LOG_INFO(sLogger, ("TestFilterRule() end", time(NULL)));
    }

    void TestMonitor() {
        LOG_INFO(sLogger, ("TestMonitor() begin", time(NULL)));
        int32_t defaultMonitorInterval = INT32_FLAG(monitor_interval);
        INT32_FLAG(monitor_interval) = 3;
        CaseSetUp();
        LogtailMonitor::Instance()->InitMonitor();
        gStatusLogGroup.Clear();
        gStatusCount = 0;
        EnableNetWork();

        sleep(INT32_FLAG(monitor_interval) * 5 + 1);

        APSARA_TEST_TRUE_DESC(gStatusCount >= 2, gStatusCount);
        APSARA_TEST_EQUAL(gStatusLogGroup.category(), "logtail_status_profile");
        const Log& log = gStatusLogGroup.logs(0);
        int idx = 0;
        APSARA_TEST_EQUAL(log.contents(idx++).key(), "cpu");
#if defined(__linux__)
        APSARA_TEST_EQUAL(log.contents(idx++).key(), "os_cpu");
#endif
        APSARA_TEST_EQUAL(log.contents(idx++).key(), "mem");
        APSARA_TEST_EQUAL(log.contents(idx++).key(), "version");
        APSARA_TEST_EQUAL(log.contents(idx++).key(), "uuid");
        APSARA_TEST_EQUAL(log.contents(idx++).key(), "user_defined_id");
        APSARA_TEST_EQUAL(log.contents(idx++).key(), "aliuids");
        APSARA_TEST_EQUAL(log.contents(idx++).key(), "projects");
        APSARA_TEST_EQUAL(log.contents(idx++).key(), "instance_id");
        APSARA_TEST_EQUAL(log.contents(idx++).key(), "syslog_open");
        APSARA_TEST_EQUAL(log.contents(idx++).key(), "ip");
        APSARA_TEST_EQUAL(log.contents(idx++).key(), "hostname");
        APSARA_TEST_EQUAL(log.contents(idx++).key(), "os");
        APSARA_TEST_EQUAL(log.contents(idx++).key(), "os_detail");
        APSARA_TEST_EQUAL(log.contents(idx++).key(), "user");
#if defined(__linux__)
        APSARA_TEST_EQUAL(log.contents(idx++).key(), "load");
#endif
        APSARA_TEST_EQUAL(log.contents(idx++).key(), "metric_json");
        APSARA_TEST_EQUAL(log.contents(idx++).key(), "status");
        LogtailMonitor::Instance()->RemoveMonitor();
        CaseCleanUp();
        INT32_FLAG(monitor_interval) = defaultMonitorInterval;
        LOG_INFO(sLogger, ("TestMonitor() end", time(NULL)));
    }

    void TestFlushOut() {
        LOG_INFO(sLogger, ("TestFlushOut() begin", time(NULL)));
        AppConfig::GetInstance()->mMaxBufferNum = 500;
        EnableNetWork();
        CaseSetUp();

        int32_t seqNo = 5;
        for (int round = 0; round < 10; ++round) {
            OneJob(100, gRootDir, "Job", true, time(NULL), "testflushout", seqNo);
            usleep(100 * 1000);
        }
        WaitForFileBeenRead();
        LOG_INFO(sLogger, ("Flush out", "begin"));
        APSARA_TEST_TRUE(Sender::Instance()->FlushOut(5 * 1000)); // this operation will write buffer file
        LOG_INFO(sLogger, ("Flush out", "end"));
        sleep(2);
        {
            WaitObject::Lock lock(Sender::Instance()->mBufferWait);
            Sender::Instance()->mBufferWait.signal();
        }
        sleep(10);
        APSARA_TEST_EQUAL(gCounter, 1000);
        gRecvLogGroupLock.lock();
        const Log& log = gRcvLogGroup.logs(0);
        APSARA_TEST_EQUAL(log.contents(2).value(), "testflushout");
        APSARA_TEST_EQUAL(log.contents(3).value(), ToString(seqNo));
        gRecvLogGroupLock.unlock();

        CaseCleanUp();
        AppConfig::GetInstance()->mMaxBufferNum = INT32_FLAG(max_buffer_num);
        LOG_INFO(sLogger, ("TestFlushOut() end", time(NULL)));
    }

    void TestDumpSnapshot() {
        LOG_INFO(sLogger, ("TestDumpSnapshot() begin", time(NULL)));
        CaseSetUp();
        EnableNetWork();
        LogFileProfiler::GetInstance()->mSendInterval = 3600;
        LogFileProfiler::GetInstance()->SendProfileData(true); // Update mLastSendTime.

        OneJob(10, gRootDir, "Job", true, time(NULL));
        WaitForFileBeenRead();
        sleep(5);
        LogFileProfiler::GetInstance()->SendProfileData(true);
        Json::Value root;
        ParseConfig(STRING_FLAG(logtail_profile_snapshot), root);
        LOG_INFO(sLogger, ("snapshot", root.toStyledString()));
        APSARA_TEST_TRUE(root["version"].asString().size() > 1);
        APSARA_TEST_EQUAL(root["ip"].asString(), LogFileProfiler::mIpAddr);
        int32_t timeInterval = root["end_time"].asInt64() - root["begin_time"].asInt64();
#if defined(__linux__)
        APSARA_TEST_TRUE_DESC(timeInterval >= 5 && timeInterval <= 7, timeInterval);
#elif defined(_MSC_VER)
        EXPECT_TRUE(timeInterval >= 5 && timeInterval <= 20);
#endif
        APSARA_TEST_EQUAL(root["detail"].size(), 1);
        Json::Value categoryDetail = root["detail"][0];
        APSARA_TEST_EQUAL(categoryDetail["project"].asString(), "1000000_proj");
        APSARA_TEST_EQUAL(categoryDetail["logstore"].asString(), "app_log");
        APSARA_TEST_EQUAL(StringTo<int64_t>(categoryDetail["split_lines"].asString()), 10);
        APSARA_TEST_EQUAL(StringTo<int64_t>(categoryDetail["parse_fail_lines"].asString()), 0);
        APSARA_TEST_TRUE(StringTo<int64_t>(categoryDetail["read_bytes"].asString()) > 0);

        OneJob(100, gRootDir, "Job", true, time(NULL));
        WaitForFileBeenRead();
        sleep(5);
        LogFileProfiler::GetInstance()->SendProfileData(true);

        ParseConfig(STRING_FLAG(logtail_profile_snapshot), root);
        LOG_INFO(sLogger, ("snapshot", root.toStyledString()));
        APSARA_TEST_TRUE(root["version"].asString().size() > 1);
        APSARA_TEST_EQUAL(root["ip"].asString(), LogFileProfiler::mIpAddr);
        timeInterval = root["end_time"].asInt64() - root["begin_time"].asInt64();
        APSARA_TEST_TRUE_DESC(timeInterval >= 5 && timeInterval <= 20, timeInterval);
        APSARA_TEST_EQUAL(root["detail"].size(), 1);
        categoryDetail = root["detail"][0];
        APSARA_TEST_EQUAL(categoryDetail["project"].asString(), "1000000_proj");
        APSARA_TEST_EQUAL(categoryDetail["logstore"].asString(), "app_log");
        APSARA_TEST_EQUAL(StringTo<int64_t>(categoryDetail["split_lines"].asString()), 100);
        APSARA_TEST_EQUAL(StringTo<int64_t>(categoryDetail["parse_fail_lines"].asString()), 0);
        APSARA_TEST_TRUE(StringTo<int64_t>(categoryDetail["read_bytes"].asString()) > 0);

        CaseCleanUp();
        LOG_INFO(sLogger, ("TestDumpSnapshot() end", time(NULL)));
    }

    void TestMergeByMinute() {
        LOG_INFO(sLogger, ("TestMergeByMinute() begin", time(NULL)));
        CaseSetUp();
        EnableNetWork();
        for (size_t caseId = 0; caseId < 3; ++caseId) {
            {
                PTScopedLock lock(gBufferLogGroupsLock);
                gBufferLogGroups.clear();
            }
            int32_t curTime = (time(NULL) / 60) * 60;
            int32_t logTimes[10] = {
                curTime - 300, //new loggroup
                curTime - 150, //new loggroup
                curTime - 121,
                curTime - 120, //new loggroup
                curTime - 60, //new loggroup
                curTime - 59,
                curTime - 3,
                curTime, //new loggroup
                curTime + 59,
                curTime + 60 //new loggroup
            };

            int32_t logNums[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            size_t totalSendLogNum = 0;
            LOG_INFO(sLogger, ("caseId", caseId));
            for (size_t j = 0; j < 10; j++) {
                logNums[j] = (rand() % (INT32_FLAG(merge_log_count_limit) / 50)) + 1;
                OneJob(logNums[j], gRootDir, "Job", true, logTimes[j], "", 0, true);
                LOG_INFO(sLogger, ("logTime", logTimes[j])("logNum", logNums[j]));
                totalSendLogNum += logNums[j];
            }
            int32_t mergeLogNums[6] = {logNums[0],
                                       logNums[1] + logNums[2],
                                       logNums[3],
                                       logNums[4] + logNums[5] + logNums[6],
                                       logNums[7] + logNums[8],
                                       logNums[9]};
            WaitForFileBeenRead();
            sleep(INT32_FLAG(batch_send_interval) * 2 + 1);
            size_t totalRecieveLogNum = 0;
            map<int32_t, int32_t> recvLogCountMap; //<timestamp, count>
            gBufferLogGroupsLock.lock();
            APSARA_TEST_EQUAL_DESC(gBufferLogGroups.size(), 6, string("caseId:") + ToString(caseId));
            if (gBufferLogGroups.size() == 6) {
                for (size_t idx = 0; idx < gBufferLogGroups.size(); ++idx) {
                    const LogGroup& logGroup = gBufferLogGroups[idx];
                    APSARA_TEST_EQUAL_DESC(logGroup.logs_size(),
                                           mergeLogNums[idx],
                                           string("caseId:") + ToString(caseId) + ", idx:" + ToString(idx));
                    totalRecieveLogNum += logGroup.logs_size();
                    for (int32_t logIdx = 0; logIdx < logGroup.logs_size(); ++logIdx) {
                        int32_t logtime = logGroup.logs(logIdx).time();
                        if (recvLogCountMap.find(logtime) == recvLogCountMap.end())
                            recvLogCountMap[logtime] = 1;
                        else
                            recvLogCountMap[logtime] += 1;
                    }
                }
            }
            gBufferLogGroupsLock.unlock();
            for (map<int32_t, int32_t>::iterator iter = recvLogCountMap.begin(); iter != recvLogCountMap.end(); ++iter)
                LOG_INFO(sLogger, ("recvLogCountMap, time", iter->first)("count", iter->second));

            APSARA_TEST_EQUAL_DESC(totalSendLogNum, totalRecieveLogNum, string("caseId:") + ToString(caseId));
            for (int32_t idx = 0; idx < 10; ++idx) {
                if (recvLogCountMap.find(logTimes[idx]) != recvLogCountMap.end())
                    APSARA_TEST_EQUAL_DESC(recvLogCountMap[logTimes[idx]],
                                           logNums[idx],
                                           string("caseId:") + ToString(caseId) + ", idx:" + ToString(idx));
                else
                    APSARA_TEST_TRUE_DESC(recvLogCountMap.find(logTimes[idx]) != recvLogCountMap.end(),
                                          string("caseId:") + ToString(caseId) + ", idx:" + ToString(idx));
            }
        }
        CaseCleanUp();
        LOG_INFO(sLogger, ("TestMergeByMinute() end", time(NULL)));
    }

    void TestRealIpSend() {
        LOG_INFO(sLogger, ("TestRealIpSend() begin", time(NULL)));
        CaseSetUp(false, true, true);
        EnableNetWork();

        //  enable real ip
        BOOL_FLAG(send_prefer_real_ip) = true;
        INT32_FLAG(send_switch_real_ip_interval) = 10;

        Sender::Instance()->MockGetRealIp = SenderUnittest::MockGetRealIp;
        // start real ip update thread
        gRealIpSendThread = CreateThread(std::bind(&Sender::RealIpUpdateThread, Sender::Instance()));

        string aliuid = "1234567890";
        string region = AppConfig::GetInstance()->GetDefaultRegion();
        LOG_INFO(sLogger, (aliuid, region));

        sleep(1);
        LOG_INFO(sLogger, ("start generate log", ""));

        int32_t seqNo = 5;
        for (int round = 0; round < 10; ++round) {
            OneJob(100, gRootDir, "Job", true, time(NULL), "TestRealIpSend", seqNo);
            usleep(100 * 1000);
        }

        sleep(10);

        // check data
        APSARA_TEST_EQUAL(gCounter, 1000);
        gRecvLogGroupLock.lock();
        const Log& log = gRcvLogGroup.logs(0);
        APSARA_TEST_EQUAL(log.contents(2).value(), "TestRealIpSend");
        APSARA_TEST_EQUAL(log.contents(3).value(), ToString(seqNo));
        gRecvLogGroupLock.unlock();

        // check send host
        auto client = Sender::Instance()->GetSendClient(region, aliuid);

        APSARA_TEST_TRUE(client->GetRawSlsHostFlag());
        APSARA_TEST_EQUAL(client->GetRawSlsHost().substr(0, strlen("10.123.32.")), string("10.123.32."));

        CaseCleanUp();
        // reset flag
        BOOL_FLAG(send_prefer_real_ip) = false;
        LOG_INFO(sLogger, ("TestRealIpSend() end", time(NULL)));
    }

    void TestEmptyRealIp() {
        LOG_INFO(sLogger, ("TestEmptyRealIp() begin", time(NULL)));

        CaseSetUp(false, true, false);
        EnableNetWork();

        //  enable real ip
        BOOL_FLAG(send_prefer_real_ip) = true;
        INT32_FLAG(send_switch_real_ip_interval) = 10;

        Sender::Instance()->MockGetRealIp = SenderUnittest::MockGetEmptyRealIp;
        // start real ip update thread
        gRealIpSendThread = CreateThread(std::bind(&Sender::RealIpUpdateThread, Sender::Instance()));

        string aliuid = "1234567890";
        string region = AppConfig::GetInstance()->GetDefaultRegion();
        LOG_INFO(sLogger, (aliuid, region));

        sleep(1);
        LOG_INFO(sLogger, ("start generate log", ""));

        int32_t seqNo = 5;
        for (int round = 0; round < 10; ++round) {
            OneJob(100, gRootDir, "Job", true, time(NULL), "TestRealIpSend", seqNo);
            usleep(100 * 1000);
        }

        sleep(10);

        // check data
        APSARA_TEST_EQUAL(gCounter, 1000);
        gRecvLogGroupLock.lock();
        const Log& log = gRcvLogGroup.logs(0);
        APSARA_TEST_EQUAL(log.contents(2).value(), "TestRealIpSend");
        APSARA_TEST_EQUAL(log.contents(3).value(), ToString(seqNo));
        gRecvLogGroupLock.unlock();

        // check send host
        sdk::Client* client = Sender::Instance()->GetSendClient(region, aliuid);

        APSARA_TEST_EQUAL(client->GetRawSlsHostFlag(), false);

        CaseCleanUp();
        // reset flag
        BOOL_FLAG(send_prefer_real_ip) = false;

        LOG_INFO(sLogger, ("TestEmptyRealIp() end", time(NULL)));
    }

    void TestRealIpSendFailAndRecover() {
        LOG_INFO(sLogger, ("TestRealIpSendFailAndRecover() begin", time(NULL)));
        CaseSetUp(false, true, true);
        DisableNetWork();
        gSendFailType = 1;

        //  enable real ip
        BOOL_FLAG(send_prefer_real_ip) = true;
        INT32_FLAG(send_switch_real_ip_interval) = 60;

        Sender::Instance()->MockGetRealIp = SenderUnittest::MockGetRealIp;
        // start real ip update thread
        gStartIp = 0;
        gRealIpSendThread = CreateThread(std::bind(&Sender::RealIpUpdateThread, Sender::Instance()));

        string aliuid = "1234567890";
        string region = AppConfig::GetInstance()->GetDefaultRegion();
        LOG_INFO(sLogger, (aliuid, region));

        sleep(1);
        LOG_INFO(sLogger, ("start generate log", ""));

        int32_t seqNo = 5;
        for (int round = 0; round < 10; ++round) {
            OneJob(100, gRootDir, "Job", true, time(NULL), "TestRealIpSendFailAndRecover", seqNo);
            usleep(10 * 1000);
        }
        LOG_INFO(sLogger, ("generate end", ""));
        sleep(10);
        APSARA_TEST_EQUAL(gCounter, 0);
        auto client = Sender::Instance()->GetSendClient(region, aliuid);

        APSARA_TEST_TRUE(client->GetRawSlsHostFlag());
        APSARA_TEST_TRUE_DESC(gStartIp >= 2, gStartIp);

        LOG_INFO(sLogger, ("enable network", ""));
        EnableNetWork();
        sleep(8);

        // check data
        APSARA_TEST_EQUAL(gCounter, 1000);
        gRecvLogGroupLock.lock();
        const Log& log = gRcvLogGroup.logs(0);
        APSARA_TEST_EQUAL(log.contents(2).value(), "TestRealIpSendFailAndRecover");
        APSARA_TEST_EQUAL(log.contents(3).value(), ToString(seqNo));
        gRecvLogGroupLock.unlock();
        // make sure real ip is enabled
        int moreLogNum = 0;
        for (int i = 0; i < 10; ++i) {
            if (client->GetRawSlsHostFlag() != true) {
                LOG_INFO(sLogger, ("write log to switch real ip", ""));
                moreLogNum += 10;
                OneJob(10, gRootDir, "Job", true, time(NULL), "TestRealIpSendFailAndRecover", seqNo);
                sleep(1);
            } else
                break;
        }

        // check send host
        APSARA_TEST_TRUE(client->GetRawSlsHostFlag());
        APSARA_TEST_EQUAL(client->GetRawSlsHost().substr(0, strlen("10.123.32.")), string("10.123.32."));
        int32_t lastTime = time(NULL);
        INT32_FLAG(send_switch_real_ip_interval) = 1000;
        sleep(5);
        string lastIp = client->GetRawSlsHost();
        gClient = client;
        gDisabledIp = "10.123.32.";
        gDisableIpFlag = true;
        LOG_INFO(sLogger, ("disable ip", lastIp));
        for (int round = 0; round < 10; ++round) {
            OneJob(100, gRootDir, "Job", true, time(NULL), "TestRealIpSendFailAndRecover", seqNo);
            usleep(1000 * 1000);
        }
        WaitForFileBeenRead();
        sleep(5);
        LOG_INFO(sLogger, ("check ip switch", lastIp));

        Sender::Instance()->mRegionRealIpLock.lock();
        APSARA_TEST_NOT_EQUAL(lastIp, Sender::Instance()->mRegionRealIpMap[region]->mRealIp);
        APSARA_TEST_TRUE(Sender::Instance()->mRegionRealIpMap[region]->mLastUpdateTime >= lastTime);
        Sender::Instance()->mRegionRealIpLock.unlock();

        gDisableIpFlag = false;
        sleep(10);

        APSARA_TEST_EQUAL(gCounter, 2000 + moreLogNum);

        sleep(3);
        gDisableIpFlag = false;
        gClient = NULL;
        INT32_FLAG(send_switch_real_ip_interval) = 10;

        CaseCleanUp();
        // reset flag
        BOOL_FLAG(send_prefer_real_ip) = false;
        LOG_INFO(sLogger, ("TestRealIpSendFailAndRecover() end", time(NULL)));
    }

    // TestRegionConcurreny tests if the send concurrency control on region level works.
    // 1. Set the send_request_concurrency to 30 and create 4 projects.
    // 2. Generate some datas and then assert the number of received data.
    // 3. Disable network and generate some datas, then assert the send concurrency of each region.
    // 4. Enable network but only enable one project, generate lots of data of another three
    //    projects, and then assert that data to the enabled project can be received.
    void TestRegionConcurreny() {
        LOG_INFO(sLogger, ("TestRegionConcurreny() begin", time(NULL)));

        // There are three built-in regions, and project 0 will use the __default__ region, so
        // the number of regions is 6 (3+3).
        LOG_INFO(sLogger,
                 ("Set send_request_concurrency to 30 and "
                  "create 4 projects",
                  time(NULL)));
        auto bakSendRequestConcurrency = INT32_FLAG(send_request_concurrency);
        INT32_FLAG(send_request_concurrency) = 30;
        CaseSetUp(false, false, true, 4);
        EnableNetWork();
        gProjectNetEnableIndex = 3;

        LOG_INFO(sLogger,
                 ("Generate some data and "
                  "assert the number of received data",
                  ""));
        for (int prjIndex = 0; prjIndex < 4; ++prjIndex) {
            OneJob(60 * 10, gRootDir, "Job", true, time(NULL), "TestRegionConcurreny", 0, false, prjIndex);
        }
        WaitForFileBeenRead();
        sleep(INT32_FLAG(batch_send_interval) * 2 + 2);
        sleep(10);
        auto counter = gCounter;
        EXPECT_EQ(counter, 60 * 10 * 4);

        LOG_INFO(sLogger,
                 ("Disable network and generate more data, "
                  "assert the send concurrency of each region",
                  ""));
        sleep(1);
        gSendFailType = 1;
        DisableNetWork();
        sleep(1);
        for (int prjIndex = 0; prjIndex < 4; ++prjIndex) {
            OneJob(60 * 10, gRootDir, "Job", true, time(NULL), "TestRegionConcurreny", 0, false, prjIndex);
        }
        WaitForFileBeenRead();
        sleep(INT32_FLAG(batch_send_interval) * 2 + 2);
        EXPECT_EQ(gCounter, counter);
        {
            PTScopedLock lock(Sender::Instance()->mRegionEndpointEntryMapLock);
            auto& regionMap = Sender::Instance()->mRegionEndpointEntryMap;
            LOG_INFO(sLogger, ("Access region map", regionMap.size()));
            for (auto iter = regionMap.begin(); iter != regionMap.end(); ++iter) {
                auto& regionName = iter->first;
                LOG_INFO(sLogger, (regionName, iter->second->mConcurrency));

                // Only tests __default__ and 100000[1-3]_proj.
                int32_t projectIndex = -1;
                if (0 == regionName.find("__default"))
                    projectIndex = 0;
                else if (0 == regionName.find("1000")) {
                    projectIndex = StringTo<int32_t>(regionName.substr(std::string("100000").length(), 1));
                }
                if (0 <= projectIndex && projectIndex <= 3) {
                    EXPECT_EQ(iter->second->mConcurrency, 30 / regionMap.size());
                }
            }
        }

        LOG_INFO(sLogger,
                 ("Enable network but only enable one project, "
                  "generate lots of data of another three projects, and then "
                  "assert that data to the enabled project can be received on time.",
                  ""));
        sleep(1);
        gProjectNetEnableIndex = 0;
        EnableNetWork();
        sleep(INT32_FLAG(batch_send_interval) * 2 + 2);
        counter += 60 * 10;
        EXPECT_EQ(gCounter, counter);
        for (int prjIndex = 1; prjIndex < 4; ++prjIndex) {
            OneJob(60 * 300, gRootDir, "Job", true, time(NULL), "TestRegionConcurreny", 0, false, prjIndex);
        }
        OneJob(60 * 100, gRootDir, "Job", true, time(NULL), "TestRegionConcurreny", 0, false, 0);
        WaitForFileBeenRead();
        sleep(INT32_FLAG(batch_send_interval) * 2 + 2);
        counter += 60 * 100;
        EXPECT_EQ(gCounter, counter);

        INT32_FLAG(send_request_concurrency) = bakSendRequestConcurrency;
        CaseCleanUp();

        LOG_INFO(sLogger, ("TestRegionConcurreny() end", time(NULL)));
    }

    void TestTooOldFilesIntegrity() {
        LOG_INFO(sLogger, ("TestTooOldFilesIntegrity() begin", time(NULL)));
        const int PROJECT_NUM = 8;
        EnableNetWork();
        gLogIntegrityTestFlag = true;
        auto bakFileEliminateInterval = INT32_FLAG(file_eliminate_interval);
        CaseSetUp(false, true, true, PROJECT_NUM, false, -1, 0, 900);
        INT32_FLAG(file_eliminate_interval) = 3;
        for (int round = 0; round < 10; ++round) {
            for (int prjIndex = 0; prjIndex < PROJECT_NUM; ++prjIndex) {
                // use 100, let time to be separated in different minutes, so there will be two packets
                OneJob(100, gRootDir, "Job", true, time(NULL), "TestTooOldFilesIntegrity", 0, false, prjIndex);
            }
            usleep(200 * 1000);
        }
        WaitForFileBeenRead();

        // wait 10s
        sleep(10);

        // test
        const LogIntegrity::RegionOutDatedFileMap& regionOutDatedFileMap
            = LogIntegrity::GetInstance()->mRegionOutDatedFileMap;
        APSARA_TEST_EQUAL(regionOutDatedFileMap.size(), 4);

        for (LogIntegrity::RegionOutDatedFileMap::const_iterator regionIter = regionOutDatedFileMap.begin();
             regionIter != regionOutDatedFileMap.end();
             ++regionIter) {
            // get seq
            const std::string& getRegion = regionIter->first;
            int seq = getRegion[6] - '0';

            char region[16] = {0};
            char projectName[16] = {0};
            char logFileName[16] = {0};
            snprintf(region, sizeof(region), "%d_proj", 1000000 + seq);
            snprintf(projectName, sizeof(projectName), "%d_proj", 1000000 + seq);
            if (seq == 0)
                strcpy(logFileName, "Job.log");
            else
                snprintf(logFileName, sizeof(logFileName), "Job.log%d", seq);

            const LogIntegrity::OutDatedFileMap* outDatedFileMap = regionIter->second;
            APSARA_TEST_EQUAL(regionIter->first, std::string(region));
            APSARA_TEST_EQUAL(outDatedFileMap->size(), (size_t)1);

            for (LogIntegrity::OutDatedFileMap::const_iterator fileIter = outDatedFileMap->begin();
                 fileIter != outDatedFileMap->end();
                 ++fileIter) {
                const OutDatedFile* outDatedFile = fileIter->second;
                APSARA_TEST_EQUAL(outDatedFile->mRegion, std::string(region));
                APSARA_TEST_EQUAL(outDatedFile->mProjectName, std::string(projectName));
                APSARA_TEST_EQUAL(outDatedFile->mLogstore, "app_log");
                APSARA_TEST_EQUAL(outDatedFile->mFilename, gRootDir + PATH_SEPARATOR + std::string(logFileName));
                APSARA_TEST_EQUAL(outDatedFile->mIntegrityProject, std::string(projectName));
                APSARA_TEST_EQUAL(outDatedFile->mIntegrityLogstore, "test-integrity-logstore");
            }
        }

        gLogIntegrityTestFlag = false;
        CaseCleanUp();
        INT32_FLAG(file_eliminate_interval) = bakFileEliminateInterval;
        LOG_INFO(sLogger, ("TestTooOldFilesIntegrity() end", time(NULL)));
    }

    void TestMergeTruncateInfo() {
        LOG_INFO(sLogger, ("TestMergeTruncateInfo() begin", time(NULL)));

        std::string truInfo_1("[000-111]");
        std::string truInfo_2("[222-333]");
        std::string truInfo_3("[444-555]");
        std::string truInfo_4("[666-777]");
        std::string truInfo_5("[888-999]");

        std::string truInfo;
        truInfo.append(truInfo_1);
        truInfo.append("," + truInfo_2);
        truInfo.append("," + truInfo_3);
        truInfo.append("," + truInfo_4);
        truInfo.append("," + truInfo_5);

        MergeItem mergeItem(std::string("test_project"),
                            std::string("test_config_name"),
                            std::string("test_filename"),
                            true,
                            std::string("test_aliuid"),
                            std::string("test_region"),
                            123456,
                            MERGE_BY_LOGSTORE,
                            std::string("test_shardhashkey"),
                            123456);

        sls_logs::LogGroup logGroup_1;
        sls_logs::LogTag* logTag_1 = logGroup_1.add_logtags();
        logTag_1->set_key(LOG_RESERVED_KEY_TRUNCATE_INFO);
        logTag_1->set_value(truInfo_1);
        Aggregator::GetInstance()->MergeTruncateInfo(logGroup_1, &mergeItem);

        sls_logs::LogGroup logGroup_2;
        sls_logs::LogTag* logTag_2 = logGroup_2.add_logtags();
        logTag_2->set_key(LOG_RESERVED_KEY_TRUNCATE_INFO);
        logTag_2->set_value(truInfo_2);
        Aggregator::GetInstance()->MergeTruncateInfo(logGroup_2, &mergeItem);

        sls_logs::LogGroup logGroup_3;
        sls_logs::LogTag* logTag_3 = logGroup_3.add_logtags();
        logTag_3->set_key(LOG_RESERVED_KEY_TRUNCATE_INFO);
        logTag_3->set_value(truInfo_3);
        Aggregator::GetInstance()->MergeTruncateInfo(logGroup_3, &mergeItem);

        sls_logs::LogGroup logGroup_4;
        sls_logs::LogTag* logTag_4 = logGroup_4.add_logtags();
        logTag_4->set_key(LOG_RESERVED_KEY_TRUNCATE_INFO);
        logTag_4->set_value(truInfo_4);
        Aggregator::GetInstance()->MergeTruncateInfo(logGroup_4, &mergeItem);

        sls_logs::LogGroup logGroup_5;
        sls_logs::LogTag* logTag_5 = logGroup_5.add_logtags();
        logTag_5->set_key(LOG_RESERVED_KEY_TRUNCATE_INFO);
        logTag_5->set_value(truInfo_5);
        Aggregator::GetInstance()->MergeTruncateInfo(logGroup_5, &mergeItem);

        APSARA_TEST_EQUAL(mergeItem.mLogGroup.logtags_size(), 1);

        const sls_logs::LogTag& logTag = mergeItem.mLogGroup.logtags(0);
        APSARA_TEST_EQUAL(logTag.key(), LOG_RESERVED_KEY_TRUNCATE_INFO);
        APSARA_TEST_EQUAL(logTag.value(), truInfo);

        LOG_INFO(sLogger, ("TestMergeTruncateInfo() end", time(NULL)));
    }

    void TestGlobalMarkOffset() {
        LOG_INFO(sLogger, ("TestGlobalMarkOffset() begin", time(NULL)));
        // prepare
        auto bakDefaultGlobalMarkOffsetFlag = BOOL_FLAG(default_global_mark_offset_flag);
        auto bakDefaultGlobalFuseMode = BOOL_FLAG(default_global_fuse_mode);
        BOOL_FLAG(default_global_mark_offset_flag) = true;
        BOOL_FLAG(default_global_fuse_mode) = false;

        gGlobalMarkOffsetTestFlag = true;
        std::string dir = gRootDir + "MarkOffsetTest";
        bfs::create_directories(dir);
        auto& PS = PATH_SEPARATOR;

        // test
        EnableNetWork();
        CaseSetUp(false, true, true, 1, false, -1, 0, 900);

        auto PrintMapInfo = []() {
            auto inst = LogFileCollectOffsetIndicator::GetInstance();
            LOG_INFO(sLogger, ("mLogFileOffsetInfoMap", ""));
            for (auto& i : inst->mLogFileOffsetInfoMap) {
                LOG_INFO(sLogger, (i.first.ToString(), i.second->mLogFileInfo.mFilename));
            }
            LOG_INFO(sLogger, ("mLogFileOffsetInfoMap", "done"));
            LOG_INFO(sLogger, ("mLogFileCollectProgressMap", ""));
            for (auto& i : inst->mLogFileCollectProgressMap) {
                LOG_INFO(sLogger, (i.first.ToString(), i.second.IsValid()));
            }
            LOG_INFO(sLogger, ("mLogFileCollectProgressMap", "done"));
        };

        LOG_INFO(sLogger, ("write a.log", ""));
        for (int round = 0; round < 10; ++round) {
            OneJob(100, dir, "a", true, time(NULL));
            usleep(200 * 1000);
        }
        WaitForFileBeenRead();
        sleep(5);
        LOG_INFO(sLogger, ("print map info", "after writing a.log"));
        PrintMapInfo();

        LOG_INFO(sLogger, ("check map status", "1"));
        const LogFileCollectProgressMap& m1 = LogFileCollectOffsetIndicator::GetInstance()->mLogFileCollectProgressMap;
        APSARA_TEST_EQUAL(m1.size(), 1);
        fsutil::PathStat buf_a;
        bool statRet_a = fsutil::PathStat::stat(std::string(dir + PS + "a.log").c_str(), buf_a);
        APSARA_TEST_TRUE_DESC(statRet_a, "a.log must exists");
        const LogFileInfo& info_a = m1.begin()->first;
        const LogFileCollectProgress& progress_a = m1.begin()->second;
        APSARA_TEST_EQUAL(info_a.mFilename, dir + PS + "a.log");
        APSARA_TEST_EQUAL(progress_a.mFileSize, buf_a.GetFileSize());
        APSARA_TEST_EQUAL(progress_a.mFileLastPos, progress_a.mFileSize);
        APSARA_TEST_EQUAL(progress_a.mFileReadPos, progress_a.mFileSize);
        APSARA_TEST_TRUE(progress_a.IsFinished());

        LOG_INFO(sLogger, ("write b.log", ""));
        for (int round = 0; round < 20; ++round) {
            OneJob(100, dir, "b", true, time(NULL));
            usleep(200 * 1000);
        }
        WaitForFileBeenRead();
        sleep(11);
        LOG_INFO(sLogger, ("print map info", "after writing b.log"));
        PrintMapInfo();

        LOG_INFO(sLogger, ("rotate b.log and write new b.log", ""));
        bfs::rename(std::string(dir + PS + "b.log").c_str(), std::string(dir + PS + "b.log.1").c_str());
        for (int round = 0; round < 10; ++round) {
            OneJob(100, dir, "b", true, time(NULL));
            usleep(200 * 1000);
        }
        WaitForFileBeenRead();
        sleep(11);
        LOG_INFO(sLogger, ("print map info", "after rotating b.log"));
        PrintMapInfo();

        LOG_INFO(sLogger, ("check map status", "2"));
        const LogFileCollectProgressMap& m2 = LogFileCollectOffsetIndicator::GetInstance()->mLogFileCollectProgressMap;
        APSARA_TEST_EQUAL(m2.size(), 2);
        fsutil::PathStat buf_b;
        bool statRet_b = fsutil::PathStat::stat(std::string(dir + PS + "b.log").c_str(), buf_b);
        APSARA_TEST_TRUE_DESC(statRet_b, "b.log must exists");
        for (LogFileCollectProgressMap::const_iterator iter = m2.begin(); iter != m2.end(); ++iter) {
            const LogFileInfo& info_b = iter->first;
            const LogFileCollectProgress& progress_b = iter->second;
            if (info_b.mFilename == dir + PS + "b.log") {
                APSARA_TEST_EQUAL(info_b.mFilename, dir + PS + "b.log");
                APSARA_TEST_EQUAL(progress_b.mFileSize, buf_b.GetFileSize());
                APSARA_TEST_EQUAL(progress_b.mFileLastPos, progress_b.mFileSize);
                APSARA_TEST_EQUAL(progress_b.mFileReadPos, progress_b.mFileSize);
                APSARA_TEST_TRUE(progress_b.IsFinished());
            }
        }

        // write file, c.log, rotate, then delete c.log
        LOG_INFO(sLogger, ("write c.log", ""));
        for (int round = 0; round < 20; ++round) {
            OneJob(100, dir, "c", true, time(NULL));
            usleep(200 * 1000);
        }
        WaitForFileBeenRead();
        sleep(11);
        LOG_INFO(sLogger, ("print map info", "after writing c.log"));
        PrintMapInfo();

        LOG_INFO(sLogger, ("rotate c.log and write new c.log", ""));
        bfs::rename(std::string(dir + PS + "c.log").c_str(), std::string(dir + PS + "c.log.1").c_str());
        for (int round = 0; round < 10; ++round) {
            OneJob(100, dir, "c", true, time(NULL));
            usleep(200 * 1000);
        }
        WaitForFileBeenRead();
        sleep(11);
        LOG_INFO(sLogger, ("print map info", "after rorating c.log"));
        PrintMapInfo();

        LOG_INFO(sLogger, ("check map status", "3"));
        const LogFileCollectProgressMap& m3 = LogFileCollectOffsetIndicator::GetInstance()->mLogFileCollectProgressMap;
        APSARA_TEST_EQUAL(m3.size(), 3);

        LOG_INFO(sLogger, ("delete c.log", ""));
        bfs::remove(std::string(dir + PS + "c.log").c_str());
        sleep(11);
        LOG_INFO(sLogger, ("print map info", "after deleting c.log"));
        PrintMapInfo();

        LOG_INFO(sLogger, ("check map status", "4"));
        const LogFileCollectProgressMap& m4 = LogFileCollectOffsetIndicator::GetInstance()->mLogFileCollectProgressMap;
        APSARA_TEST_EQUAL(m4.size(), 2);
        bool found = false;
        for (LogFileCollectProgressMap::const_iterator iter = m2.begin(); iter != m2.end(); ++iter) {
            const LogFileInfo& info_c = iter->first;
            if (info_c.mFilename == dir + PS + "c.log") {
                found = true;
            }
        }
        APSARA_TEST_TRUE(!found);

        // case clean up
        CaseCleanUp();
        bfs::remove_all(dir);
        gGlobalMarkOffsetTestFlag = false;
        BOOL_FLAG(default_global_fuse_mode) = bakDefaultGlobalFuseMode;
        BOOL_FLAG(default_global_mark_offset_flag) = bakDefaultGlobalMarkOffsetFlag;
        LOG_INFO(sLogger, ("TestGlobalMarkOffset() end", time(NULL)));
    }

    void TestLogGroupAlipayZoneInfo() {
        LOG_INFO(sLogger, ("TestLogGroupAlipayZoneInfo() begin", time(NULL)));

        EnableNetWork();
        // set env
#if defined(_MSC_VER)
        _putenv_s(STRING_FLAG(alipay_zone).c_str(), "GZ00C");
#else
        setenv(STRING_FLAG(alipay_zone).c_str(), "GZ00C", 1);
#endif
        CaseSetUp(false, true, true, 1, false, -1, 0, 900);

        // write file, a.log
        for (int round = 0; round < 10; ++round) {
            OneJob(5, gRootDir, "a", true, time(NULL));
            usleep(200 * 1000);
        }
        sleep(5);

        bool hasZoneInfoFlag = false;
        int logTagSize = gRcvLogGroup.logtags_size();
        for (int i = 0; i < logTagSize; ++i) {
            const ::sls_logs::LogTag logTag = gRcvLogGroup.logtags(i);
            if (logTag.key() == LOG_RESERVED_KEY_ALIPAY_ZONE) {
                APSARA_TEST_EQUAL(logTag.value(), "GZ00C");
                hasZoneInfoFlag = true;
            }
        }
        APSARA_TEST_TRUE(hasZoneInfoFlag);

        // unset env
#if defined(_MSC_VER)
        _putenv((STRING_FLAG(alipay_zone) + "=").c_str());
#else
        unsetenv(STRING_FLAG(alipay_zone).c_str());
#endif

        CaseCleanUp();
        LOG_INFO(sLogger, ("TestLogGroupAlipayZoneInfo() end", time(NULL)));
    }

    static void MockExactlyOnceSend(LoggroupTimeValue* data);

    void TestExactlyOnceDataSendSequence();

    void TestExactlyOncePartialBlockConcurrentSend();

    void TestExactlyOnceCompleteBlockConcurrentSend();
};

APSARA_UNIT_TEST_CASE(SenderUnittest, TestSecondaryStorage, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestEncryptAndDecrypt, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestFilterUTF8, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestDiscardOldData, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestConnect, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestDisConnect, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestChangeStat, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestFlowControl, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestMergeByCount, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestFlushOut, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestMonitor, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestFilterRule, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestDumpSnapshot, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestMergeByMinute, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestMultiUserSeperationAndDiscardFailAndOtherFail, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestMultiUserSeperationAndRetryQuotaRecovery, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestMultiUserSeperationAndTestNetWorkRecovery, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestMultiUserSeperationAndRetryIntervalRecovery, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestLogstoreFlowControl, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestLogstoreFlowControlPause, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestLogstoreFlowControlExpire, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestTooOldFilesIntegrity, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestMergeTruncateInfo, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestGlobalMarkOffset, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestRealIpSend, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestEmptyRealIp, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestRealIpSendFailAndRecover, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestRegionConcurreny, gCaseID);
APSARA_UNIT_TEST_CASE(SenderUnittest, TestLogGroupAlipayZoneInfo, gCaseID);
UNIT_TEST_CASE(SenderUnittest, TestExactlyOnceDataSendSequence);
UNIT_TEST_CASE(SenderUnittest, TestExactlyOncePartialBlockConcurrentSend);
UNIT_TEST_CASE(SenderUnittest, TestExactlyOnceCompleteBlockConcurrentSend);

decltype(SenderUnittest::sProcessQueueMap) SenderUnittest::sProcessQueueMap = nullptr;
decltype(SenderUnittest::sSenderQueueMap) SenderUnittest::sSenderQueueMap = nullptr;

// Record checkpoint and forward to MockAsyncSend.
void SenderUnittest::MockExactlyOnceSend(LoggroupTimeValue* data) {
    auto& cpt = data->mLogGroupContext.mExactlyOnceCheckpoint;
    if (!cpt) {
        return;
    }

    auto closure = new SendClosure;
    closure->mDataPtr = data;

    {
        auto hashKey = cpt->data.hash_key();
        std::lock_guard<std::mutex> lock(gMockExactlyOnceSendLock);
        if (gBlockedHashKeySet.find(hashKey) != gBlockedHashKeySet.end()) {
            LOG_INFO(sLogger, ("hash key is blocked", hashKey)("checkpoint", cpt->data.DebugString()));
            auto res = new sdk::PostLogStoreLogsResponse;
            res->statusCode = 403;
            res->requestId = "RequestIDInMockExactlyOnceSend";
            closure->OnFail(res, sdk::LOGE_WRITE_QUOTA_EXCEED, "reject by exactly once send");
            return;
        }
    }

    gRangeCheckpoints.push_back(RangeCheckpointPtr(new RangeCheckpoint(*(cpt.get()))));
    LOG_INFO(sLogger, ("checkpoint key", cpt->key)("checkpoint", cpt->data.DebugString()));
    MockAsyncSend(data->mProjectName, data->mLogstore, data->mLogData, data->mDataType, data->mRawSize, closure);
}

// Test if data's sequence is generated orderly:
// - Incremental sequence ID.
// - Incremental checkpoint index.
//
// Procedure: generate N logs continuously, use MockExactlyOnceSend to catch checkpoints,
//  after N logs are sent, test if these logs break order.
void SenderUnittest::TestExactlyOnceDataSendSequence() {
    LOG_INFO(sLogger, ("TestExactlyOnceDataSendSequence() begin", time(NULL)));

    gEnableExactlyOnce = true;
    EnableNetWork();
    CaseSetUp(false, true, true, 1, false, -1, 0, 900);

    std::string dir = gRootDir + PATH_SEPARATOR + "ExactlyOnceDataSendSequence";
    bfs::create_directories(dir);

    LOG_INFO(sLogger, ("write a.log", dir));
    const size_t kRound = 3;
    for (size_t round = 0; round < kConcurrency * kRound; ++round) {
        OneJob(100, dir, "a", true, time(NULL));
        usleep(200 * 1000);
    }
    WaitForFileBeenRead();
    sleep(5);

    LOG_INFO(sLogger, ("test sequence", ""));
    ModifyHandler* handler = nullptr;
    {
        auto& pathMap = sEventDispatcher->mPathWdMap;
        auto iter = pathMap.find(dir);
        ASSERT_TRUE(iter != pathMap.end());
        auto dirHandler = static_cast<CreateModifyHandler*>(sEventDispatcher->mWdDirInfoMap[iter->second]->mHandler);
        EXPECT_EQ(dirHandler->mModifyHandlerPtrMap.size(), 1);
        handler = dirHandler->mModifyHandlerPtrMap.begin()->second;
    }
    auto& readerMap = handler->mNameReaderMap;
    EXPECT_EQ(readerMap.size(), 1);
    auto& readerArray = readerMap.begin()->second;
    EXPECT_EQ(readerArray.size(), 1);
    auto& reader = readerArray.front();
    auto& eo = reader->mEOOption;
    EXPECT_TRUE(eo);

    std::set<std::string> hashKeySet;
    for (auto& cpt : eo->rangeCheckpointPtrs) {
        hashKeySet.insert(cpt->data.hash_key());
    }
    EXPECT_EQ(hashKeySet.size(), kConcurrency);
    EXPECT_GE(gRangeCheckpoints.size(), kConcurrency);
    std::unordered_map<std::string, std::vector<uint64_t> > hashKeySeqIDs;
    for (auto& cpt : gRangeCheckpoints) {
        hashKeySeqIDs[cpt->data.hash_key()].push_back(cpt->data.sequence_id());
    }
    for (auto& iter : hashKeySeqIDs) {
        auto sortedSequenceIDs = iter.second;
        std::sort(sortedSequenceIDs.begin(), sortedSequenceIDs.end());
        EXPECT_EQ(iter.second, sortedSequenceIDs);
    }

    CaseCleanUp();
    gEnableExactlyOnce = false;
    LOG_INFO(sLogger, ("TestExactlyOnceDataSendSequence() end", time(NULL)));
}

// Test if data read will block as expected when send is blocked.
// Reader will be blocked only when all concurrency is blocked.
//
// Procedure: generate N logs continuously, use MockExactlyOnceSend to catch checkpoints,
//  after N logs are sent, block specified concurrency in MockExactlyOnceSend.
void SenderUnittest::TestExactlyOncePartialBlockConcurrentSend() {
    LOG_INFO(sLogger, ("TestExactlyOncePartialBlockConcurrentSend() begin", time(NULL)));

    auto bakClientQuotaSendRetryIntervalMax = INT32_FLAG(client_quota_send_retry_interval_max);
    auto bakClientQuotaSendConcurrencyMin = INT32_FLAG(client_quota_send_concurrency_min);
    auto bakClientQuotaSendRetryInterval = INT32_FLAG(client_quota_send_retry_interval);
    INT32_FLAG(client_quota_send_retry_interval_max) = 1;
    INT32_FLAG(client_quota_send_concurrency_min) = kConcurrency;
    INT32_FLAG(client_quota_send_retry_interval) = 1;
    gEnableExactlyOnce = true;
    EnableNetWork();
    CaseSetUp(false, true, true, 1, false, -1, 0, 900);

    std::string dir = gRootDir + PATH_SEPARATOR + "ExactlyOncePartialBlockConcurrentSend";
    bfs::create_directories(dir);

    auto globalRangeCheckpoints2HashKeySet = [](std::vector<std::string>* hashKeys = nullptr) {
        std::set<std::string> hashKeySet;
        for (auto& cpt : gRangeCheckpoints) {
            hashKeySet.insert(cpt->data.hash_key());
            if (hashKeys) {
                hashKeys->push_back(cpt->data.hash_key());
            }
        }
        return hashKeySet;
    };

    LOG_INFO(sLogger, ("send round 1 data", "collect checkpoint keys"));
    for (size_t round = 0; round < kConcurrency * 2; ++round) {
        OneJob(100, dir, "a", true, time(NULL));
        usleep(100 * 1000);
    }
    WaitForFileBeenRead();
    sleep(5);
    std::vector<std::string> hashKeys;
    {
        // All concurrency have been used.
        auto hashKeySet = globalRangeCheckpoints2HashKeySet(&hashKeys);
        EXPECT_EQ(hashKeySet.size(), kConcurrency);
        gRangeCheckpoints.clear();
    }

    const auto blockedIndex = kConcurrency / 2;
    const auto blockedHashKey = hashKeys[blockedIndex];
    LOG_INFO(sLogger, ("block hash key", blockedHashKey)("send round 2 data", ""));
    gBlockedHashKeySet.insert(blockedHashKey);
    for (size_t round = 0; round < kConcurrency * 2; ++round) {
        OneJob(100, dir, "a", true, time(NULL));
        usleep(100 * 1000);
    }
    WaitForFileBeenRead();
    sleep(5);
    {
        // Concurrency except for bloced one have been used.
        auto hashKeySet = globalRangeCheckpoints2HashKeySet();
        EXPECT_EQ(hashKeySet.size(), kConcurrency - 1);
        EXPECT_TRUE(hashKeySet.find(blockedHashKey) == hashKeySet.end());
        gRangeCheckpoints.clear();
    }

    LOG_INFO(sLogger, ("unblock hash key", blockedHashKey));
    gBlockedHashKeySet.clear();
    for (size_t round = 0; round < kConcurrency * 2; ++round) {
        OneJob(100, dir, "a", true, time(NULL));
        usleep(100 * 1000);
    }
    WaitForFileBeenRead();
    sleep(5);
    {
        // The blocked data can be sent now.
        auto hashKeySet = globalRangeCheckpoints2HashKeySet();
        EXPECT_GE(hashKeySet.size(), 1);
        EXPECT_TRUE(hashKeySet.find(blockedHashKey) != hashKeySet.end());
        gRangeCheckpoints.clear();
    }

    LOG_INFO(sLogger, ("send round 4 data", "recovery"));
    for (size_t round = 0; round < kConcurrency * 2; ++round) {
        OneJob(100, dir, "a", true, time(NULL));
        usleep(100 * 1000);
    }
    WaitForFileBeenRead();
    sleep(5);
    {
        // All concurrency have been used.
        auto hashKeySet = globalRangeCheckpoints2HashKeySet();
        EXPECT_EQ(hashKeySet.size(), kConcurrency);
        gRangeCheckpoints.clear();
    }

    CaseCleanUp();
    gEnableExactlyOnce = false;
    INT32_FLAG(client_quota_send_retry_interval_max) = bakClientQuotaSendRetryIntervalMax;
    INT32_FLAG(client_quota_send_concurrency_min) = bakClientQuotaSendConcurrencyMin;
    INT32_FLAG(client_quota_send_retry_interval) = bakClientQuotaSendRetryInterval;
    LOG_INFO(sLogger, ("TestExactlyOncePartialBlockConcurrentSend() end", time(NULL)));
}

// Test if data read will block as expected when send is blocked.
// Reader will be blocked only when all concurrency is blocked.
//
// Procedure: generate N logs continuously, use MockExactlyOnceSend to catch checkpoints,
//  after N logs are sent, block all concurrency in MockExactlyOnceSend.
void SenderUnittest::TestExactlyOnceCompleteBlockConcurrentSend() {
    LOG_INFO(sLogger, ("TestExactlyOnceCompleteBlockConcurrentSend() begin", time(NULL)));

    auto bakClientQuotaSendRetryIntervalMax = INT32_FLAG(client_quota_send_retry_interval_max);
    auto bakClientQuotaSendConcurrencyMin = INT32_FLAG(client_quota_send_concurrency_min);
    auto bakClientQuotaSendRetryInterval = INT32_FLAG(client_quota_send_retry_interval);
    INT32_FLAG(client_quota_send_retry_interval_max) = 1;
    INT32_FLAG(client_quota_send_concurrency_min) = kConcurrency;
    INT32_FLAG(client_quota_send_retry_interval) = 1;
    gEnableExactlyOnce = true;
    EnableNetWork();
    CaseSetUp(false, true, true, 1, false, -1, 0, 900);

    std::string dir = gRootDir + PATH_SEPARATOR + "ExactlyOnceCompleteBlockConcurrentSend";
    bfs::create_directories(dir);

    auto globalRangeCheckpoints2HashKeySet = [](std::vector<std::string>* hashKeys = nullptr) {
        std::set<std::string> hashKeySet;
        for (auto& cpt : gRangeCheckpoints) {
            hashKeySet.insert(cpt->data.hash_key());
            if (hashKeys) {
                hashKeys->push_back(cpt->data.hash_key());
            }
        }
        return hashKeySet;
    };

    LOG_INFO(sLogger, ("send round 1 data", "collect checkpoint keys"));
    for (size_t round = 0; round < kConcurrency * 2; ++round) {
        OneJob(100, dir, "a", true, time(NULL));
        usleep(100 * 1000);
    }
    WaitForFileBeenRead();
    sleep(5);
    std::vector<std::string> hashKeys;
    {
        // All concurrency have been used.
        auto hashKeySet = globalRangeCheckpoints2HashKeySet(&hashKeys);
        EXPECT_EQ(hashKeySet.size(), kConcurrency);
        gRangeCheckpoints.clear();
    }

    LOG_INFO(sLogger, ("block all hash keys", "send round 2 data"));
    gBlockedHashKeySet.insert(hashKeys.begin(), hashKeys.end());
    for (size_t round = 0; round < kConcurrency * 2; ++round) {
        OneJob(100, dir, "a", true, time(NULL));
        usleep(100 * 1000);
    }
    WaitForFileBeenRead();
    sleep(5);
    {
        // No data can be sent.
        auto hashKeySet = globalRangeCheckpoints2HashKeySet();
        EXPECT_EQ(hashKeySet.size(), 0);
        EXPECT_EQ(gRangeCheckpoints.size(), 0);
    }

    const auto unlockedIndex = kConcurrency / 2;
    const auto unlockedHashKey = hashKeys[unlockedIndex];
    LOG_INFO(sLogger, ("unblock one hash key", unlockedHashKey));
    {
        std::lock_guard<std::mutex> lock(gMockExactlyOnceSendLock);
        gBlockedHashKeySet.erase(unlockedHashKey);
    }
    sleep(INT32_FLAG(client_quota_send_retry_interval_max) * 2);
    {
        // All data is send through the concurrency.
        auto hashKeySet = globalRangeCheckpoints2HashKeySet();
        EXPECT_GE(hashKeySet.size(), 1);
        EXPECT_TRUE(hashKeySet.find(unlockedHashKey) != hashKeySet.end());
        gRangeCheckpoints.clear();
    }

    LOG_INFO(sLogger, ("unblocked all hash keys", "send round 3 data"));
    gBlockedHashKeySet.clear();
    for (size_t round = 0; round < kConcurrency * 2; ++round) {
        OneJob(100, dir, "a", true, time(NULL));
        usleep(100 * 1000);
    }
    WaitForFileBeenRead();
    sleep(5);
    {
        // All concurrency have been used.
        auto hashKeySet = globalRangeCheckpoints2HashKeySet();
        EXPECT_EQ(hashKeySet.size(), kConcurrency);
        gRangeCheckpoints.clear();
    }

    CaseCleanUp();
    gEnableExactlyOnce = false;
    INT32_FLAG(client_quota_send_retry_interval_max) = bakClientQuotaSendRetryIntervalMax;
    INT32_FLAG(client_quota_send_concurrency_min) = bakClientQuotaSendConcurrencyMin;
    INT32_FLAG(client_quota_send_retry_interval) = bakClientQuotaSendRetryInterval;
    LOG_INFO(sLogger, ("TestExactlyOnceCompleteBlockConcurrentSend() end", time(NULL)));
}

} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
