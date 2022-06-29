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

#include "Sender.h"
#include "sls_control/SLSControl.h"
#include <fstream>
#include <string>
#include <atomic>
#if defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#endif
#include "common/Constants.h"
#include "common/StringTools.h"
#include "common/CompressTools.h"
#include "common/FileEncryption.h"
#include "common/ExceptionBase.h"
#include "common/FileSystemUtil.h"
#include "common/LogtailCommonFlags.h"
#include "common/TimeUtil.h"
#include "common/RuntimeUtil.h"
#include "common/HashUtil.h"
#include "common/FileSystemUtil.h"
#include "common/util.h"
#include "common/ErrorUtil.h"
#include "common/RandomUtil.h"
#include "common/SlidingWindowCounter.h"
#include "sdk/Client.h"
#include "sdk/Exception.h"
#include "config/Config.h"
#include "processor/LogProcess.h"
#include "processor/LogFilter.h"
#include "profiler/LogtailAlarm.h"
#include "profiler/LogIntegrity.h"
#include "profiler/LogLineCount.h"
#include "profiler/LogFileProfiler.h"
#include "app_config/AppConfig.h"
#include "monitor/Monitor.h"
#include "config_manager/ConfigManager.h"
#include "common/LogFileCollectOffsetIndicator.h"
#include "fuse/UlogfsHandler.h"

#ifdef LOGTAIL_RUNTIME_PLUGIN
#include "LogtailRuntimePlugin.h"
#endif

using namespace std;
using namespace sls_logs;

DECLARE_FLAG_INT32(buffer_check_period);
DEFINE_FLAG_INT32(quota_exceed_wait_interval, "when daemon buffer thread get quotaExceed error, sleep 5 seconds", 5);
DEFINE_FLAG_INT32(secondary_buffer_count_limit, "data ready for write buffer file", 20);
DEFINE_FLAG_BOOL(enable_mock_send, "if enable mock send in ut", false);
DEFINE_FLAG_INT32(merge_log_count_limit, "log count in one logGroup at most", 4000);
DEFINE_FLAG_INT32(buffer_file_alive_interval, "the max alive time of a bufferfile, 5 minutes", 300);
DEFINE_FLAG_INT32(write_secondary_wait_timeout, "interval of dump seconary buffer from memory to file, seconds", 2);
DEFINE_FLAG_BOOL(e2e_send_throughput_test, "dump file for e2e throughpt test", false);
DEFINE_FLAG_INT32(send_client_timeout_interval, "recycle clients avoid memory increment", 12 * 3600);
DEFINE_FLAG_INT32(check_send_client_timeout_interval, "", 600);
DEFINE_FLAG_INT32(send_retry_sleep_interval, "sleep microseconds when sync send fail, 50ms", 50000);
DEFINE_FLAG_INT32(unauthorized_send_retrytimes,
                  "how many times should retry if PostLogStoreLogs operation return UnAuthorized",
                  5);
DEFINE_FLAG_INT32(profile_data_send_retrytimes, "how many times should retry if profile data send fail", 5);
// Logtail will drop data with Unauthorized error (401) if it happened after
// allowed delay (compare to the time when AK was acquired).
DEFINE_FLAG_INT32(unauthorized_allowed_delay_after_reset, "allowed delay to retry for unauthorized error, 30s", 30);
DEFINE_FLAG_DOUBLE(send_server_error_retry_ratio, "", 0.3);
DEFINE_FLAG_DOUBLE(send_server_error_ratio_smoothing_factor, "", 5.0);
DEFINE_FLAG_INT32(send_statistic_entry_timeout, "seconds", 7200);
DEFINE_FLAG_INT32(sls_host_update_interval, "seconds", 5);
DEFINE_FLAG_INT32(max_send_log_group_size, "bytes", 5 * 1024 * 1024);
DEFINE_FLAG_BOOL(dump_reduced_send_result, "for performance test", false);
DEFINE_FLAG_INT32(test_network_normal_interval, "if last check is normal, test network again after seconds ", 30);
DEFINE_FLAG_INT32(same_topic_merge_send_count,
                  "merge package if topic is same and package count per send is greater than",
                  10);
DEFINE_FLAG_INT32(discard_send_fail_interval, "discard data when send fail after 6 * 3600 seconds", 6 * 3600);
DEFINE_FLAG_BOOL(send_prefer_real_ip, "use real ip to send data", false);
DEFINE_FLAG_INT32(send_switch_real_ip_interval, "seconds", 60);
DEFINE_FLAG_INT32(send_check_real_ip_interval, "seconds", 2);
DEFINE_FLAG_BOOL(global_network_success, "global network success flag, default false", false);
DEFINE_FLAG_INT32(reset_region_concurrency_error_count,
                  "reset region concurrency if the number of continuous error exceeds, 5",
                  5);
DEFINE_FLAG_INT32(unknow_error_try_max, "discard data when try times > this value", 5);
DEFINE_FLAG_INT32(test_unavailable_endpoint_interval, "test unavailable endpoint interval", 60);
DEFINE_FLAG_INT32(sending_cost_time_alarm_interval, "sending log group cost too much time, second", 10);
DEFINE_FLAG_INT32(log_group_wait_in_queue_alarm_interval,
                  "log group wait in queue alarm interval, may blocked by concurrency or quota, second",
                  10);

namespace logtail {
const string Sender::BUFFER_FILE_NAME_PREFIX = "logtail_buffer_file_";
const int32_t Sender::BUFFER_META_BASE_SIZE = 65536;

std::atomic_int gNetworkErrorCount{0};

void SendClosure::OnSuccess(sdk::Response* response) {
    BOOL_FLAG(global_network_success) = true;
    Sender::Instance()->SubSendingBufferCount();
    Sender::Instance()->DescSendingCount();

    LOG_DEBUG(
        sLogger,
        ("StatusCode", response->statusCode)("RequestId", response->requestId)("projectName", mDataPtr->mProjectName)(
            "logstore", mDataPtr->mLogstore)("logs", mDataPtr->mLogLines)("bytes", mDataPtr->mLogData.size()));
    if (BOOL_FLAG(e2e_send_throughput_test))
        Sender::Instance()->DumpDebugFile(mDataPtr, true);

    if (mDataPtr->mLogGroupContext.mIntegrityConfigPtr.get() != NULL
        && mDataPtr->mLogGroupContext.mIntegrityConfigPtr->mIntegritySwitch)
        LogIntegrity::GetInstance()->Notify(mDataPtr, true);

    if (mDataPtr->mLogGroupContext.mLineCountConfigPtr.get() != NULL
        && mDataPtr->mLogGroupContext.mLineCountConfigPtr->mLineCountSwitch)
        LogLineCount::GetInstance()->NotifySuccess(mDataPtr);

    if (mDataPtr->mLogGroupContext.mMarkOffsetFlag) {
        LogFileCollectOffsetIndicator::GetInstance()->NotifySuccess(mDataPtr);
    }

    auto& cpt = mDataPtr->mLogGroupContext.mExactlyOnceCheckpoint;
    if (cpt) {
        cpt->Commit();
        cpt->IncreaseSequenceID();
        LOG_DEBUG(sLogger, ("increase sequence id", cpt->key)("checkpoint", cpt->data.DebugString()));
    }

    Sender::Instance()->IncreaseRegionConcurrency(mDataPtr->mRegion);
    Sender::Instance()->IncTotalSendStatistic(mDataPtr->mProjectName, mDataPtr->mLogstore, time(NULL));
    Sender::Instance()->OnSendDone(mDataPtr, LogstoreSenderInfo::SendResult_OK); // mDataPtr is released here

    delete this;
}

void SendClosure::OnFail(sdk::Response* response, const string& errorCode, const string& errorMessage) {
    // test
    LOG_DEBUG(sLogger, ("send failed, error code", errorCode)("error msg", errorMessage));

    // added by xianzhi(bowen.gbw@antfin.com)
    if (mDataPtr->mLogGroupContext.mIntegrityConfigPtr.get() != NULL
        && mDataPtr->mLogGroupContext.mIntegrityConfigPtr->mIntegritySwitch)
        LogIntegrity::GetInstance()->Notify(mDataPtr, false);

    mDataPtr->mSendRetryTimes++;
    int32_t curTime = time(NULL);
    OperationOnFail operation;
    LogstoreSenderInfo::SendResult recordRst = LogstoreSenderInfo::SendResult_OtherFail;
    SendResult sendResult = ConvertErrorCode(errorCode);
    if (sendResult == SEND_NETWORK_ERROR || sendResult == SEND_SERVER_ERROR) {
        if (SEND_NETWORK_ERROR == sendResult) {
            gNetworkErrorCount++;
        }

        if (BOOL_FLAG(send_prefer_real_ip) && mDataPtr->mRealIpFlag) {
            LOG_WARNING(sLogger,
                        ("connect refused, use vip directly", mDataPtr->mRegion)(
                            mDataPtr->mProjectName, mDataPtr->mLogstore)("endpoint", mDataPtr->mCurrentEndpoint));
            // just set force update flag
            //mDataPtr->mRealIpFlag = false;
            //Sender::Instance()->ResetSendClientEndpoint(mDataPtr->mAliuid, mDataPtr->mRegion, curTime);
            Sender::Instance()->ForceUpdateRealIp(mDataPtr->mRegion);
        }
        double serverErrorRatio
            = Sender::Instance()->IncSendServerErrorStatistic(mDataPtr->mProjectName, mDataPtr->mLogstore, curTime);
        if (serverErrorRatio < DOUBLE_FLAG(send_server_error_retry_ratio)
            && mDataPtr->mSendRetryTimes < INT32_FLAG(send_retrytimes)) {
            operation = RETRY_ASYNC_WHEN_FAIL;
        } else {
            if (sendResult == SEND_NETWORK_ERROR) {
                // only set network stat when no real ip
                if (!BOOL_FLAG(send_prefer_real_ip) || !mDataPtr->mRealIpFlag) {
                    Sender::Instance()->SetNetworkStat(mDataPtr->mRegion, mDataPtr->mCurrentEndpoint, false);
                    recordRst = LogstoreSenderInfo::SendResult_NetworkFail;
                    Sender::Instance()->ResetSendClientEndpoint(mDataPtr->mAliuid, mDataPtr->mRegion, curTime);
                }
            }
            operation = mDataPtr->mBufferOrNot ? RECORD_ERROR_WHEN_FAIL : DISCARD_WHEN_FAIL;
        }
        Sender::Instance()->ResetRegionConcurrency(mDataPtr->mRegion);
    } else if (sendResult == SEND_QUOTA_EXCEED) {
        BOOL_FLAG(global_network_success) = true;
        Sender::Instance()->IncTotalSendStatistic(mDataPtr->mProjectName, mDataPtr->mLogstore, curTime);
        LogtailAlarm::GetInstance()->SendAlarm(SEND_QUOTA_EXCEED_ALARM,
                                               "error_code: " + errorCode + ", error_message: " + errorMessage
                                                   + ", request_id:" + response->requestId,
                                               mDataPtr->mProjectName,
                                               mDataPtr->mLogstore,
                                               mDataPtr->mRegion);
        operation = RECORD_ERROR_WHEN_FAIL;
        recordRst = LogstoreSenderInfo::SendResult_QuotaFail;
    } else if (sendResult == SEND_UNAUTHORIZED
               && mDataPtr->mSendRetryTimes < INT32_FLAG(unauthorized_send_retrytimes)) {
        BOOL_FLAG(global_network_success) = true;
        Sender::Instance()->IncTotalSendStatistic(mDataPtr->mProjectName, mDataPtr->mLogstore, curTime);
        if (mDataPtr->mAliuid.empty() && ConfigManager::GetInstance()->GetRegionType() == REGION_CORP) {
            operation = RETRY_ASYNC_WHEN_FAIL;
        } else {
            int32_t lastUpdateTime;
            sdk::Client* sendClient = Sender::Instance()->GetSendClient(mDataPtr->mRegion, mDataPtr->mAliuid);
            if (SLSControl::Instance()->SetSlsSendClientAuth(mDataPtr->mAliuid, false, sendClient, lastUpdateTime))
                operation = RETRY_ASYNC_WHEN_FAIL;
            else if (curTime - lastUpdateTime < INT32_FLAG(unauthorized_allowed_delay_after_reset))
                operation = RETRY_ASYNC_WHEN_FAIL;
            else
                operation = DISCARD_WHEN_FAIL;
        }
    } else if (sendResult == SEND_INVALID_SEQUENCE_ID) {
        do {
#define LOG_PATTERN ("config", mDataPtr->mConfigName)("request_id", response->requestId)
            auto& cpt = mDataPtr->mLogGroupContext.mExactlyOnceCheckpoint;
            if (!cpt) {
                LOG_ERROR(sLogger,
                          LOG_PATTERN("message", "unexpected result when exactly once checkpoint is not found"));
                operation = DISCARD_WHEN_FAIL;
                break;
            }

            // Because hash key is generated by UUID library, we consider that
            //  the possibility of hash key conflict is very low, so data is
            //  dropped here.
            cpt->Commit();
            LOG_WARNING(
                sLogger,
                LOG_PATTERN("checkpoint key", cpt->key)("message", "drop exactly once log group and commit checkpoint")(
                    "checkpoint", cpt->data.DebugString()));
            LogtailAlarm::GetInstance()->SendAlarm(EXACTLY_ONCE_ALARM,
                                                   "drop exactly once log group because of invalid sequence ID, cpt:"
                                                       + cpt->key + ", data:" + cpt->data.DebugString()
                                                       + "request id:" + response->requestId);
            operation = DISCARD_WHEN_FAIL;
            cpt->IncreaseSequenceID();
#undef LOG_PATTERN
        } while (0);
    } else if (AppConfig::GetInstance()->EnableLogTimeAutoAdjust() && sdk::LOGE_REQUEST_TIME_EXPIRED == errorCode) {
        operation = RETRY_ASYNC_WHEN_FAIL;
        LOG_INFO(sLogger,
                 ("retry expired data", response->requestId)("project", mDataPtr->mProjectName)(
                     "logstore", mDataPtr->mLogstore)("error", errorMessage));
    } else {
        Sender::Instance()->IncTotalSendStatistic(mDataPtr->mProjectName, mDataPtr->mLogstore, curTime);
        // when unknown error such as SignatureNotMatch happens, we should retry several times
        // first time, we will retry immediately
        // then we record error and retry latter
        // when retry times > unknow_error_try_max, we will drop this data
        if (mDataPtr->mSendRetryTimes == 1) {
            operation = RETRY_ASYNC_WHEN_FAIL;
        } else if (mDataPtr->mSendRetryTimes > INT32_FLAG(unknow_error_try_max)) {
            operation = DISCARD_WHEN_FAIL;
        } else {
            operation = RECORD_ERROR_WHEN_FAIL;
        }
    }
    if (curTime - mDataPtr->mLastUpdateTime > INT32_FLAG(discard_send_fail_interval)) {
        operation = DISCARD_WHEN_FAIL;
    }
    bool isProfileData = Sender::IsProfileData(mDataPtr->mRegion, mDataPtr->mProjectName, mDataPtr->mLogstore);
    if (isProfileData && mDataPtr->mSendRetryTimes >= INT32_FLAG(profile_data_send_retrytimes)) {
        operation = DISCARD_WHEN_FAIL;
    }
    switch (operation) {
        case RETRY_ASYNC_WHEN_FAIL:
            LOG_DEBUG(sLogger,
                      ("send data to SLS fail", "will retry soon")("StatusCode", response->statusCode)(
                          "RequestId", response->requestId)("ErrorCode", errorCode)("ErrorMessage", errorMessage)(
                          "region", mDataPtr->mRegion)("projectName", mDataPtr->mProjectName)(
                          "logstore", mDataPtr->mLogstore)("RetryTimes",
                                                           mDataPtr->mSendRetryTimes)("LogLines", mDataPtr->mLogLines)(
                          "bytes", mDataPtr->mLogData.size())("endpoint", mDataPtr->mCurrentEndpoint));

            if (curTime - mDataPtr->mLastUpdateTime > INT32_FLAG(sending_cost_time_alarm_interval)) {
                LOG_WARNING(sLogger,
                            ("continuous network error, will retry soon, region",
                             mDataPtr->mRegion)("projectName", mDataPtr->mProjectName)("logstore", mDataPtr->mLogstore)(
                                "RetryTimes", mDataPtr->mSendRetryTimes)("LogLines", mDataPtr->mLogLines)(
                                "bytes", mDataPtr->mLogData.size())("endpoint", mDataPtr->mCurrentEndpoint));
            }
            Sender::Instance()->SendToNetAsync(mDataPtr);
            break;
        case RECORD_ERROR_WHEN_FAIL:
            if (!isProfileData) {
                LOG_WARNING(sLogger,
                            ("send data to SLS fail", "repush into send buffer and retry later")(
                                "StatusCode", response->statusCode)("RequestId", response->requestId)(
                                "ErrorCode", errorCode)("ErrorMessage", errorMessage)("region", mDataPtr->mRegion)(
                                "projectName", mDataPtr->mProjectName)("logstore", mDataPtr->mLogstore)(
                                "RetryTimes", mDataPtr->mSendRetryTimes)("LogLines", mDataPtr->mLogLines)(
                                "bytes", mDataPtr->mLogData.size())("endpoint", mDataPtr->mCurrentEndpoint));
            }
            //Sender::Instance()->PutIntoSecondaryBuffer(mDataPtr, 10);
            Sender::Instance()->SubSendingBufferCount();
            // record error
            Sender::Instance()->OnSendDone(mDataPtr, recordRst);
            Sender::Instance()->DescSendingCount();
            break;
        case DISCARD_WHEN_FAIL:
        default:
            if (isProfileData)
                LOG_DEBUG(sLogger,
                          ("report Logtail profiling data fail",
                           "discard data, this will not affect user's log data")("StatusCode", response->statusCode)(
                              "RequestId", response->requestId)("ErrorCode", errorCode)("ErrorMessage", errorMessage)(
                              "projectName", mDataPtr->mProjectName)("logstore", mDataPtr->mLogstore)(
                              "RetryTimes", mDataPtr->mSendRetryTimes)("LogLines", mDataPtr->mLogLines)(
                              "bytes", mDataPtr->mLogData.size())("endpoint", mDataPtr->mCurrentEndpoint));
            else {
                LOG_ERROR(sLogger,
                          ("send data to SLS fail", "discard data")("StatusCode", response->statusCode)(
                              "RequestId", response->requestId)("ErrorCode", errorCode)("ErrorMessage", errorMessage)(
                              "region", mDataPtr->mRegion)("projectName", mDataPtr->mProjectName)("logstore",
                                                                                                  mDataPtr->mLogstore)(
                              "RetryTimes", mDataPtr->mSendRetryTimes)("LogLines", mDataPtr->mLogLines)(
                              "bytes", mDataPtr->mLogData.size())("endpoint", mDataPtr->mCurrentEndpoint));
                LogtailAlarm::GetInstance()->SendAlarm(SEND_DATA_FAIL_ALARM,
                                                       "logs:" + ToString(mDataPtr->mLogLines) + ", errorCode:"
                                                           + errorCode + ", errorMessage:" + errorMessage
                                                           + ", requestId:" + response->requestId
                                                           + ", endponit:" + mDataPtr->mCurrentEndpoint,
                                                       mDataPtr->mProjectName,
                                                       mDataPtr->mLogstore,
                                                       mDataPtr->mRegion);
            }
            Sender::Instance()->SubSendingBufferCount();
            // set ok to delete data
            Sender::Instance()->OnSendDone(mDataPtr, LogstoreSenderInfo::SendResult_DiscardFail);
            Sender::Instance()->DescSendingCount();
    }

    delete this;
}

SendResult ConvertErrorCode(const std::string& errorCode) {
    if (errorCode == sdk::LOGE_REQUEST_ERROR || errorCode == sdk::LOGE_CLIENT_OPERATION_TIMEOUT
        || errorCode == sdk::LOGE_REQUEST_TIMEOUT)
        return SEND_NETWORK_ERROR;
    else if (errorCode == sdk::LOGE_SERVER_BUSY || errorCode == sdk::LOGE_INTERNAL_SERVER_ERROR)
        return SEND_SERVER_ERROR;
    else if (errorCode == sdk::LOGE_WRITE_QUOTA_EXCEED || errorCode == sdk::LOGE_SHARD_WRITE_QUOTA_EXCEED)
        return SEND_QUOTA_EXCEED;
    else if (errorCode == sdk::LOGE_UNAUTHORIZED)
        return SEND_UNAUTHORIZED;
    else if (errorCode == sdk::LOGE_INVALID_SEQUENCE_ID) {
        return SEND_INVALID_SEQUENCE_ID;
    } else
        return SEND_DISCARD_ERROR;
}

Sender::Sender() {
    srand(time(NULL));
    mFlushLog = false;
    SetBufferFilePath(AppConfig::GetInstance()->GetBufferFilePath());
    mTestNetworkClient = new sdk::Client("",
                                         STRING_FLAG(default_access_key_id),
                                         STRING_FLAG(default_access_key),
                                         INT32_FLAG(sls_client_send_timeout),
                                         LogFileProfiler::mIpAddr,
                                         BOOL_FLAG(sls_client_send_compress),
                                         AppConfig::GetInstance()->GetBindInterface());
    SLSControl::Instance()->SetSlsSendClientCommonParam(mTestNetworkClient);

    mUpdateRealIpClient = new sdk::Client("",
                                          STRING_FLAG(default_access_key_id),
                                          STRING_FLAG(default_access_key),
                                          INT32_FLAG(sls_client_send_timeout),
                                          LogFileProfiler::mIpAddr,
                                          BOOL_FLAG(sls_client_send_compress),
                                          AppConfig::GetInstance()->GetBindInterface());
    SLSControl::Instance()->SetSlsSendClientCommonParam(mUpdateRealIpClient);
    SetSendingBufferCount(0);
    size_t concurrencyCount = (size_t)AppConfig::GetInstance()->GetSendRequestConcurrency();
    if (concurrencyCount < 10) {
        concurrencyCount = 10;
    }
    if (concurrencyCount > 50) {
        concurrencyCount = 50;
    }
    mSenderQueue.SetParam((size_t)(concurrencyCount * 1.5), (size_t)(concurrencyCount * 2), 200);
    LOG_INFO(sLogger, ("Set sender queue param depend value", concurrencyCount));
    new Thread(bind(&Sender::TestNetwork, this)); // be careful: this thread will not stop until process exit
    if (BOOL_FLAG(send_prefer_real_ip)) {
        LOG_INFO(sLogger, ("start real ip update thread", ""));
        new Thread(bind(&Sender::RealIpUpdateThread, this)); // be careful: this thread will not stop until process exit
    }
    new Thread(bind(&Sender::DaemonSender, this)); // be careful: this thread will not stop until process exit
    new Thread(bind(&Sender::WriteSecondary, this)); // be careful: this thread will not stop until process exit
}

Sender* Sender::Instance() {
    static Sender* senderPtr = new Sender();
    return senderPtr;
}

///////////////////////////////for debug & ut//////////////////////////////////
bool Sender::ParseLogGroupFromLZ4(const std::string& logData, int32_t rawSize, LogGroup& logGroupPb) {
    string uncompressed;
    if (!UncompressLz4(logData, rawSize, uncompressed)) {
        LOG_ERROR(sLogger, ("uncompress logData", "fail")("rawSize", rawSize));
        return false;
    }
    if (!logGroupPb.ParseFromString(uncompressed)) {
        LOG_ERROR(sLogger, ("logGroup parseFromString", "fail")("rawSize", rawSize));
        return false;
    }
    return true;
}

void Sender::ParseLogGroupFromString(const std::string& logData,
                                     SEND_DATA_TYPE dataType,
                                     int32_t rawSize,
                                     std::vector<sls_logs::LogGroup>& logGroupVec) {
    if (dataType == LOGGROUP_LZ4_COMPRESSED) {
        LogGroup logGroupPb;
        if (Sender::ParseLogGroupFromLZ4(logData, rawSize, logGroupPb))
            logGroupVec.push_back(logGroupPb);
        else
            LOG_ERROR(sLogger, ("ParseLogGroupFromLZ4", "fail")("dataType", dataType));
    } else {
        SlsLogPackageList logPackageList;
        if (logPackageList.ParseFromString(logData)) {
            for (int32_t pIdx = 0; pIdx < logPackageList.packages_size(); ++pIdx) {
                LogGroup logGroupPb;
                if (Sender::ParseLogGroupFromLZ4(logPackageList.packages(pIdx).data(),
                                                 logPackageList.packages(pIdx).uncompress_size(),
                                                 logGroupPb))
                    logGroupVec.push_back(logGroupPb);
                else
                    LOG_ERROR(sLogger, ("ParseLogGroupFromLZ4", "fail")("dataType", dataType));
            }
        } else
            LOG_ERROR(sLogger, ("logPackageList parseFromString", "fail")("dataType", dataType));
    }
}

bool Sender::WriteToFile(const std::string& projectName, const LogGroup& logGroup, bool sendPerformance) {
    string path = GetProcessExecutionDir() + "log_file_out";
    if (sendPerformance)
        path.append("_send_performance");
    std::ofstream outfile(path.c_str(), std::ofstream::app | std::ofstream::binary);

    static uint32_t lines = 0;
    if (outfile.good()) {
        if (sendPerformance)
            outfile << projectName << "\t" << logGroup.category() << "\t" << time(NULL) << "\t" << logGroup.ByteSize()
                    << "\t" << logGroup.logs_size() << endl;
        else {
            outfile << "project_name : " << projectName;
            outfile << "\t category : " << logGroup.category();
            outfile << "\t topic : " << logGroup.topic() << endl;
            for (int i = 0; i < logGroup.logs_size(); i++) {
                const Log& log = logGroup.logs(i);
                outfile << "__lines__ : " << lines++;
                outfile << "\t time : " << log.time();
                for (int j = 0; j < log.contents_size(); j++) {
                    const Log_Content& content = log.contents(j);
                    outfile << "\t" << content.key() << ":" << content.value();
                }
                outfile << "\n";
            }
        }
        outfile.close();
        return true;
    } else {
        LOG_ERROR(sLogger, ("write to file", "fail")("projectName", projectName));
        return false;
    }
}

bool Sender::WriteToFile(LoggroupTimeValue* value, bool sendPerformance) {
    string path = GetProcessExecutionDir() + "log_file_out";
    if (sendPerformance)
        path.append("_send_performance");
    std::ofstream outfile(path.c_str(), std::ofstream::app);

    if (outfile.good()) {
        if (sendPerformance)
            outfile << value->mProjectName << "\t" << value->mLogstore << "\t" << time(NULL) << "\t" << value->mRawSize
                    << "\t" << value->mLogLines << endl;
        else
            outfile << value->mProjectName << "\t" << value->mLogstore << "\t" << value->mLastUpdateTime << "\t"
                    << value->mRawSize << "\t" << value->mLogLines << endl;
        outfile.close();
        return true;
    } else {
        LOG_ERROR(sLogger, ("write to file", "fail")("projectName", value->mProjectName));
        return false;
    }
}

bool Sender::DumpDebugFile(LoggroupTimeValue* value, bool sendPerformance) {
    if (BOOL_FLAG(dump_reduced_send_result))
        return WriteToFile(value, sendPerformance);
    else {
        vector<LogGroup> logGroupVec;
        Sender::ParseLogGroupFromString(value->mLogData, value->mDataType, value->mRawSize, logGroupVec);
        for (vector<LogGroup>::iterator iter = logGroupVec.begin(); iter != logGroupVec.end(); ++iter) {
            if (!WriteToFile(value->mProjectName, *iter, sendPerformance))
                return false;
        }
        return true;
    }
}

bool Sender::LZ4CompressLogGroup(const sls_logs::LogGroup& logGroup, std::string& compressed, int32_t& rawSize) {
    string rawData;
    logGroup.SerializeToString(&rawData);
    rawSize = rawData.size();
    if (!CompressLz4(rawData, compressed)) {
        LOG_ERROR(sLogger, ("lz4 compress data", "fail")("logstore", logGroup.category()));
        return false;
    }
    return true;
}
/////////////////////////////////////////////////////////////////////////////////////

bool Sender::InitSender(void) {
    static Aggregator* aggregator = Aggregator::GetInstance();
    aggregator->CleanLogPackSeqMap();


    SetBufferFilePath(AppConfig::GetInstance()->GetBufferFilePath());
    MockAsyncSend = NULL;
    MockSyncSend = NULL;
    MockTestEndpoint = NULL;
    MockIntegritySend = NULL;
    MockGetRealIp = NULL;
    mFlushLog = false;
    mBufferDivideTime = time(NULL);
    mCheckPeriod = INT32_FLAG(buffer_check_period);
    mSendBufferThreadId = CreateThread([this]() { DaemonBufferSender(); });
    mSendLastTime[0] = 0;
    mSendLastTime[1] = 0;
    mSendLastByte[0] = 0;
    mSendLastByte[1] = 0;
    ResetSendingCount();
    SetSendingBufferCount(0);
    mLastCheckSendClientTime = time(NULL);
    ResetFlush();
    mIsSendingBuffer = false;
    return true;
}

void Sender::SetBufferFilePath(const std::string& bufferfilepath) {
    ScopedSpinLock lock(mBufferFileLock);
    if (bufferfilepath == "") {
        mBufferFilePath = GetProcessExecutionDir();
    } else
        mBufferFilePath = bufferfilepath;

    if (mBufferFilePath[mBufferFilePath.size() - 1] != PATH_SEPARATOR[0])
        mBufferFilePath += PATH_SEPARATOR;
    mBufferFileName = "";
}

void Sender::SetNetworkStat(const std::string& region, const std::string& endpoint, bool status, int32_t latency) {
    PTScopedLock lock(mRegionEndpointEntryMapLock);
    std::unordered_map<std::string, RegionEndpointEntry*>::iterator iter = mRegionEndpointEntryMap.find(region);
    // should not create endpoint when set net work stat
    if (iter != mRegionEndpointEntryMap.end())
        (iter->second)->UpdateEndpointDetail(endpoint, status, latency, false);
}

std::string Sender::GetRegionCurrentEndpoint(const std::string& region) {
    PTScopedLock lock(mRegionEndpointEntryMapLock);
    std::unordered_map<std::string, RegionEndpointEntry*>::iterator iter = mRegionEndpointEntryMap.find(region);
    if (iter != mRegionEndpointEntryMap.end())
        return (iter->second)->GetCurrentEndpoint();
    else
        return "";
}

std::string Sender::GetRegionFromEndpoint(const std::string& endpoint) {
    PTScopedLock lock(mRegionEndpointEntryMapLock);
    for (std::unordered_map<std::string, RegionEndpointEntry*>::iterator iter = mRegionEndpointEntryMap.begin();
         iter != mRegionEndpointEntryMap.end();
         ++iter) {
        for (std::unordered_map<std::string, EndpointDetail>::iterator epIter
             = ((iter->second)->mEndpointDetailMap).begin();
             epIter != ((iter->second)->mEndpointDetailMap).end();
             ++epIter) {
            if (epIter->first == endpoint)
                return iter->first;
        }
    }
    return STRING_FLAG(default_region_name);
}


LogstoreFeedBackInterface* Sender::GetSenderFeedBackInterface() {
    return (LogstoreFeedBackInterface*)&mSenderQueue;
}


void Sender::SetFeedBackInterface(LogstoreFeedBackInterface* pProcessInterface) {
    mSenderQueue.SetFeedBackObject(pProcessInterface);
}

void Sender::OnSendDone(LoggroupTimeValue* mDataPtr, LogstoreSenderInfo::SendResult sendRst) {
    mSenderQueue.OnLoggroupSendDone(mDataPtr, sendRst);
}

bool Sender::HasNetworkAvailable() {
    static int32_t lastCheckTime = time(NULL);
    int32_t curTime = time(NULL);
    if (curTime - lastCheckTime >= 3600) {
        lastCheckTime = curTime;
        return true;
    }
    {
        PTScopedLock lock(mRegionEndpointEntryMapLock);
        for (std::unordered_map<std::string, RegionEndpointEntry*>::iterator iter = mRegionEndpointEntryMap.begin();
             iter != mRegionEndpointEntryMap.end();
             ++iter) {
            for (std::unordered_map<std::string, EndpointDetail>::iterator epIter
                 = ((iter->second)->mEndpointDetailMap).begin();
                 epIter != ((iter->second)->mEndpointDetailMap).end();
                 ++epIter) {
                if ((epIter->second).mStatus)
                    return true;
            }
        }
    }
    return false;
}

sdk::Client* Sender::GetSendClient(const std::string& region, const std::string& aliuid) {
    string key = region + "_" + aliuid;
    {
        PTScopedLock lock(mSendClientLock);
        unordered_map<string, SlsClientInfo*>::iterator iter = mSendClientMap.find(key);
        if (iter != mSendClientMap.end()) {
            (iter->second)->lastUsedTime = time(NULL);

            return (iter->second)->sendClient;
        }
    }

    int32_t lastUpdateTime;
    sdk::Client* sendClient = new sdk::Client(GetRegionCurrentEndpoint(region),
                                              "",
                                              "",
                                              INT32_FLAG(sls_client_send_timeout),
                                              LogFileProfiler::mIpAddr,
                                              BOOL_FLAG(sls_client_send_compress),
                                              AppConfig::GetInstance()->GetBindInterface());
    SLSControl::Instance()->SetSlsSendClientCommonParam(sendClient);
    SLSControl::Instance()->SetSlsSendClientAuth(aliuid, true, sendClient, lastUpdateTime);
    SlsClientInfo* clientInfo = new SlsClientInfo(sendClient, time(NULL));
    {
        PTScopedLock lock(mSendClientLock);
        mSendClientMap.insert(pair<string, SlsClientInfo*>(key, clientInfo));
    }
    return sendClient;
}

bool Sender::ResetSendClientEndpoint(const std::string aliuid, const std::string region, int32_t curTime) {
    sdk::Client* sendClient = GetSendClient(region, aliuid);
    if (curTime - sendClient->GetSlsHostUpdateTime() < INT32_FLAG(sls_host_update_interval))
        return false;
    sendClient->SetSlsHostUpdateTime(curTime);
    string endpoint = GetRegionCurrentEndpoint(region);
    if (endpoint.empty())
        return false;
    if (sendClient->GetRawSlsHost() != endpoint) {
        mSenderQueue.OnRegionRecover(region);
    }
    sendClient->SetSlsHost(endpoint);
    return true;
}

void Sender::CleanTimeoutSendClient() {
    PTScopedLock lock(mSendClientLock);
    int32_t curTime = time(NULL);
    std::unordered_map<string, SlsClientInfo*>::iterator iter = mSendClientMap.begin();
    for (; iter != mSendClientMap.end();) {
        if ((curTime - (iter->second)->lastUsedTime) > INT32_FLAG(send_client_timeout_interval)) {
            delete (iter->second)->sendClient;
            delete iter->second;
            iter = mSendClientMap.erase(iter);
        } else
            iter++;
    }
}

std::string Sender::GetBufferFilePath() {
    ScopedSpinLock lock(mBufferFileLock);
    return mBufferFilePath;
}
void Sender::SetBufferFileName(const std::string& filename) {
    ScopedSpinLock lock(mBufferFileLock);
    mBufferFileName = filename;
}
std::string Sender::GetBufferFileName() {
    ScopedSpinLock lock(mBufferFileLock);
    return mBufferFileName;
}
bool Sender::LoadFileToSend(time_t timeLine, std::vector<std::string>& filesToSend) {
    string bufferFilePath = GetBufferFilePath();
    if (!CheckExistance(bufferFilePath)) {
        if (GetProcessExecutionDir().find(bufferFilePath) != 0) {
            LOG_WARNING(sLogger,
                        ("buffer file path not exist", bufferFilePath)("logtail will not recreate external path",
                                                                       "local secondary does not work"));
            LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                                   string("buffer file directory:") + bufferFilePath + " not exist");
            return false;
        }
        string errorMessage;
        if (!RebuildExecutionDir(AppConfig::GetInstance()->GetIlogtailConfigJson(), errorMessage)) {
            LOG_ERROR(sLogger, ("failed to rebuild buffer file path", bufferFilePath)("errorMessage", errorMessage));
            LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM, errorMessage);
            return false;
        } else
            LOG_INFO(sLogger, ("rebuild buffer file path success", bufferFilePath));
    }

    fsutil::Dir dir(bufferFilePath);
    if (!dir.Open()) {
        string errorStr = ErrnoToString(GetErrno());
        LOG_ERROR(sLogger, ("open dir error", bufferFilePath)("reason", errorStr));
        LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                               string("open dir error,dir:") + bufferFilePath + ",error:" + errorStr);
        return false;
    }
    fsutil::Entry ent;
    while (ent = dir.ReadNext()) {
        string filename = ent.Name();
        if (filename.find(BUFFER_FILE_NAME_PREFIX) == 0) {
            try {
                int32_t filetime = StringTo<int32_t>(filename.substr(BUFFER_FILE_NAME_PREFIX.size()));
                if (filetime < timeLine)
                    filesToSend.push_back(filename);
            } catch (...) {
                LOG_INFO(sLogger, ("can not get file time from file name", filename));
            }
        }
    }
    sort(filesToSend.begin(), filesToSend.end());
    return true;
}
bool Sender::ReadNextEncryption(int32_t& pos,
                                const std::string& filename,
                                std::string& encryption,
                                EncryptionStateMeta& meta,
                                bool& readResult,
                                LogtailBufferMeta& bufferMeta) {
    bufferMeta.Clear();
    readResult = false;
    encryption.clear();
    int retryTimes = 0;

    FILE* fin = NULL;
    while (true) {
        retryTimes++;
        fin = FileReadOnlyOpen(filename.c_str(), "rb");
        if (fin)
            break;
        if (retryTimes >= 3) {
            string errorStr = ErrnoToString(GetErrno());
            LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                                   string("open file error:") + filename + ",error:" + errorStr);
            LOG_ERROR(sLogger, ("open file error", filename)("error", errorStr));
            return false;
        }
        usleep(5000);
    }
    fseek(fin, 0, SEEK_END);
    auto const currentSize = ftell(fin);
    if (currentSize == pos) {
        fclose(fin);
        return false;
    }
    fseek(fin, pos, SEEK_SET);
    auto nbytes = fread(static_cast<void*>(&meta), sizeof(char), sizeof(meta), fin);
    if (nbytes != sizeof(meta)) {
        string errorStr = ErrnoToString(GetErrno());
        LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                               string("read encryption file meta error:") + filename
                                                   + ", error:" + errorStr + ", meta.mEncryptionSize:"
                                                   + ToString(meta.mEncryptionSize) + ", nbytes: " + ToString(nbytes)
                                                   + ", pos: " + ToString(pos) + ", ftell: " + ToString(currentSize));
        LOG_ERROR(sLogger,
                  ("read encryption file meta error",
                   filename)("error", errorStr)("nbytes", nbytes)("pos", pos)("ftell", currentSize));
        fclose(fin);
        return false;
    }

    bool pbMeta = false;
    int32_t encodedInfoSize = meta.mEncodedInfoSize;
    if (encodedInfoSize > BUFFER_META_BASE_SIZE) {
        encodedInfoSize -= BUFFER_META_BASE_SIZE;
        pbMeta = true;
    }

    if (meta.mEncryptionSize < 0 || encodedInfoSize < 0) {
        LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                               string("meta of encryption file invalid:" + filename
                                                      + ", meta.mEncryptionSize:" + ToString(meta.mEncryptionSize)
                                                      + ", meta.mEncodedInfoSize:" + ToString(meta.mEncodedInfoSize)));
        LOG_ERROR(sLogger,
                  ("meta of encryption file invalid", filename)("meta.mEncryptionSize", meta.mEncryptionSize)(
                      "meta.mEncodedInfoSize", meta.mEncodedInfoSize));
        fclose(fin);
        return false;
    }

    pos += sizeof(meta) + encodedInfoSize + meta.mEncryptionSize;
    if ((time(NULL) - meta.mTimeStamp) > INT32_FLAG(log_expire_time) || meta.mHandled == 1) {
        fclose(fin);
        if (meta.mHandled != 1) {
            LOG_WARNING(sLogger, ("timeout buffer file, meta.mTimeStamp", meta.mTimeStamp));
            LogtailAlarm::GetInstance()->SendAlarm(DISCARD_SECONDARY_ALARM,
                                                   "buffer file timeout (1day), delete file: " + filename);
        }
        return true;
    }

    char* buffer = new char[encodedInfoSize + 1];
    nbytes = fread(buffer, sizeof(char), encodedInfoSize, fin);
    if (nbytes != static_cast<size_t>(encodedInfoSize)) {
        fclose(fin);
        string errorStr = ErrnoToString(GetErrno());
        LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                               string("read projectname from file error:") + filename
                                                   + ", error:" + errorStr + ", meta.mEncodedInfoSize:"
                                                   + ToString(meta.mEncodedInfoSize) + ", nbytes:" + ToString(nbytes));
        LOG_ERROR(sLogger,
                  ("read encodedInfo from file error",
                   filename)("error", errorStr)("meta.mEncodedInfoSize", meta.mEncodedInfoSize)("nbytes", nbytes));
        delete[] buffer;
        return true;
    }
    string encodedInfo = string(buffer, encodedInfoSize);
    delete[] buffer;
    if (pbMeta) {
        if (!bufferMeta.ParseFromString(encodedInfo)) {
            fclose(fin);
            LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                                   string("parse buffer meta from file error:") + filename);
            LOG_ERROR(sLogger, ("parse buffer meta from file error", filename)("buffer meta", encodedInfo));
            bufferMeta.Clear();
            return true;
        }
    } else {
        bufferMeta.set_project(encodedInfo);
        bufferMeta.set_endpoint(AppConfig::GetInstance()->GetDefaultRegion()); //new mode
        bufferMeta.set_aliuid("");
    }

    buffer = new char[meta.mEncryptionSize + 1];
    nbytes = fread(buffer, sizeof(char), meta.mEncryptionSize, fin);
    if (nbytes != static_cast<size_t>(meta.mEncryptionSize)) {
        fclose(fin);
        string errorStr = ErrnoToString(GetErrno());
        LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                               string("read encryption from file error:") + filename
                                                   + ",error:" + errorStr + ",meta.mEncryptionSize:"
                                                   + ToString(meta.mEncryptionSize) + ", nbytes:" + ToString(nbytes));
        LOG_ERROR(sLogger,
                  ("read encryption from file error",
                   filename)("error", errorStr)("meta.mEncryptionSize", meta.mEncryptionSize)("nbytes", nbytes));
        delete[] buffer;
        return true;
    }
    encryption = string(buffer, meta.mEncryptionSize);
    readResult = true;
    delete[] buffer;
    fclose(fin);
    return true;
}

void Sender::DaemonBufferSender() {
    mBufferSenderThreadIsRunning = true;
    LOG_DEBUG(sLogger, ("SendBufferThread", "start"));
    while (mBufferSenderThreadIsRunning) {
        if (!HasNetworkAvailable()) {
            if (!mBufferSenderThreadIsRunning)
                break;
            sleep(mCheckPeriod);
            continue;
        }
        vector<string> filesToSend;
        if (!LoadFileToSend(mBufferDivideTime, filesToSend)) {
            if (!mBufferSenderThreadIsRunning)
                break;
            sleep(mCheckPeriod);
            continue;
        }
        mIsSendingBuffer = true;
        int32_t fileToSendCount = int32_t(filesToSend.size());
        int32_t bufferFileNumValue = AppConfig::GetInstance()->GetNumOfBufferFile();
        for (int32_t i = (fileToSendCount > bufferFileNumValue ? fileToSendCount - bufferFileNumValue : 0);
             i < fileToSendCount && mBufferSenderThreadIsRunning;
             ++i) {
            string fileName = GetBufferFilePath() + filesToSend[i];
            unordered_map<string, string> kvMap;
            if (FileEncryption::CheckHeader(fileName, kvMap)) {
                int32_t keyVersion = -1;
                if (kvMap.find(STRING_FLAG(file_encryption_field_key_version)) != kvMap.end()) {
                    try {
                        keyVersion = StringTo<int32_t>(kvMap[STRING_FLAG(file_encryption_field_key_version)]);
                    } catch (...) {
                        LOG_ERROR(sLogger,
                                  ("convert key_version to int32_t fail",
                                   kvMap[STRING_FLAG(file_encryption_field_key_version)]));
                        keyVersion = -1;
                    }
                }
                if (keyVersion >= 1 && keyVersion <= FileEncryption::GetInstance()->GetDefaultKeyVersion()) {
                    LOG_INFO(sLogger, ("check local encryption file", fileName)("key_version", keyVersion));
                    SendEncryptionBuffer(fileName, keyVersion);
                } else {
                    remove(fileName.c_str());
                    LOG_ERROR(sLogger,
                              ("invalid key_version in header",
                               kvMap[STRING_FLAG(file_encryption_field_key_version)])("delete bufffer file", fileName));
                    LogtailAlarm::GetInstance()->SendAlarm(
                        DISCARD_SECONDARY_ALARM, "key version in buffer file invalid, delete file: " + fileName);
                }
            } else {
                remove(fileName.c_str());
                LOG_WARNING(sLogger, ("check header of buffer file failed, delete file", fileName));
                LogtailAlarm::GetInstance()->SendAlarm(DISCARD_SECONDARY_ALARM,
                                                       "check header of buffer file failed, delete file: " + fileName);
            }
        }
        mIsSendingBuffer = false;
        if (!mBufferSenderThreadIsRunning)
            break;
        {
            WaitObject::Lock lock(mBufferWait);
            mBufferWait.wait(lock, mCheckPeriod * 1000000);
        }
    }
    LOG_INFO(sLogger, ("SendBufferThread", "exit"));
}

void Sender::SendEncryptionBuffer(const std::string& filename, int32_t keyVersion) {
    string encryption;
    string logData;
    EncryptionStateMeta meta;
    bool readResult;
    bool writeBack = false;
    int32_t pos = INT32_FLAG(file_encryption_header_length);
    LogtailBufferMeta bufferMeta;
    int32_t discardCount = 0;
    while (ReadNextEncryption(pos, filename, encryption, meta, readResult, bufferMeta)) {
        logData.clear();
        bool sendResult = false;
        if (!readResult || bufferMeta.project().empty()) {
            if (meta.mHandled == 1)
                continue;
            sendResult = true;
            discardCount++;
        }
        if (!sendResult) {
            char* des = new char[meta.mLogDataSize];
            if (!FileEncryption::GetInstance()->Decrypt(
                    encryption.c_str(), meta.mEncryptionSize, des, meta.mLogDataSize, keyVersion)) {
                sendResult = true;
                discardCount++;
                LOG_ERROR(sLogger,
                          ("decrypt error, project_name",
                           bufferMeta.project())("key_version", keyVersion)("meta.mLogDataSize", meta.mLogDataSize));
                LogtailAlarm::GetInstance()->SendAlarm(ENCRYPT_DECRYPT_FAIL_ALARM,
                                                       string("decrypt error, project_name:" + bufferMeta.project()
                                                              + ", key_version:" + ToString(keyVersion)
                                                              + ", meta.mLogDataSize:" + ToString(meta.mLogDataSize)));
            } else {
                if (bufferMeta.has_logstore())
                    logData = string(des, meta.mLogDataSize);
                else {
                    // compatible to old buffer file (logGroup string), convert to LZ4 compressed
                    string logGroupStr = string(des, meta.mLogDataSize);
                    LogGroup logGroup;
                    if (!logGroup.ParseFromString(logGroupStr)) {
                        sendResult = true;
                        LOG_ERROR(sLogger,
                                  ("parse error from string to loggroup, projectName is", bufferMeta.project()));
                        discardCount++;
                        LogtailAlarm::GetInstance()->SendAlarm(
                            LOG_GROUP_PARSE_FAIL_ALARM,
                            string("projectName is:" + bufferMeta.project() + ", fileName is:" + filename));
                    } else if (!CompressLz4(logGroupStr, logData)) {
                        sendResult = true;
                        LOG_ERROR(sLogger, ("LZ4 compress loggroup fail, projectName is", bufferMeta.project()));
                        discardCount++;
                        LogtailAlarm::GetInstance()->SendAlarm(
                            LZ4_COMPRESS_FAIL_ALARM,
                            string("projectName is:" + bufferMeta.project() + ", fileName is:" + filename));
                    } else {
                        bufferMeta.set_logstore(logGroup.category());
                        bufferMeta.set_datatype(LOGGROUP_LZ4_COMPRESSED);
                        bufferMeta.set_rawsize(meta.mLogDataSize);
                    }
                }
                if (!sendResult) {
                    string errorCode;
                    SendResult res = SendBufferFileData(bufferMeta, logData, errorCode);
                    if (res == SEND_OK)
                        sendResult = true;
                    else if (res == SEND_DISCARD_ERROR || res == SEND_UNAUTHORIZED) {
                        LogtailAlarm::GetInstance()->SendAlarm(SEND_DATA_FAIL_ALARM,
                                                               string("send buffer file fail, rawsize:")
                                                                   + ToString(bufferMeta.rawsize())
                                                                   + "errorCode: " + errorCode,
                                                               bufferMeta.project(),
                                                               bufferMeta.logstore(),
                                                               "");
                        sendResult = true;
                        discardCount++;
                    } else if (res == SEND_QUOTA_EXCEED && INT32_FLAG(quota_exceed_wait_interval) > 0)
                        sleep(INT32_FLAG(quota_exceed_wait_interval));
                }
            }
            delete[] des;
        }
        if (sendResult)
            meta.mHandled = 1;
        LOG_DEBUG(sLogger,
                  ("send LogGroup from local buffer file", filename)("rawsize", bufferMeta.rawsize())("sendResult",
                                                                                                      sendResult));
        WriteBackMeta(pos - meta.mEncryptionSize - sizeof(meta)
                          - (meta.mEncodedInfoSize > BUFFER_META_BASE_SIZE
                                 ? (meta.mEncodedInfoSize - BUFFER_META_BASE_SIZE)
                                 : meta.mEncodedInfoSize),
                      (char*)&meta,
                      sizeof(meta),
                      filename);
        if (!sendResult)
            writeBack = true;
    }
    if (!writeBack) {
        remove(filename.c_str());
        if (discardCount > 0) {
            LOG_ERROR(sLogger, ("send buffer file, discard LogGroup count", discardCount)("delete file", filename));
            LogtailAlarm::GetInstance()->SendAlarm(DISCARD_SECONDARY_ALARM,
                                                   "delete buffer file: " + filename + ", discard "
                                                       + ToString(discardCount) + " logGroups");
        } else
            LOG_INFO(sLogger, ("send buffer file success, delete buffer file", filename));
    }
}

// file is not really created when call CreateNewFile(), file created happened when SendToBufferFile() first called
bool Sender::CreateNewFile() {
    vector<string> filesToSend;
    int64_t currentTime = time(NULL);
    if (!LoadFileToSend(currentTime, filesToSend))
        return false;
    int32_t bufferFileNumValue = AppConfig::GetInstance()->GetNumOfBufferFile();
    for (int32_t i = 0; i < (int32_t)filesToSend.size() - bufferFileNumValue; ++i) {
        string fileName = GetBufferFilePath() + filesToSend[i];
        if (CheckExistance(fileName)) {
            remove(fileName.c_str());
            LOG_ERROR(sLogger,
                      ("buffer file count exceed limit",
                       "file created earlier will be cleaned, and new file will create for new log data")("delete file",
                                                                                                          fileName));
            LogtailAlarm::GetInstance()->SendAlarm(DISCARD_SECONDARY_ALARM,
                                                   "buffer file count exceed, delete file: " + fileName);
        }
    }
    mBufferDivideTime = currentTime;
    SetBufferFileName(GetBufferFilePath() + BUFFER_FILE_NAME_PREFIX + ToString(currentTime));
    return true;
}

bool Sender::WriteBackMeta(int32_t pos, const void* buf, int32_t length, const string& filename) {
    // TODO: Why not use fopen or fstream???
    // TODO: Make sure and merge them.
#if defined(__linux__)
    int fd = open(filename.c_str(), O_WRONLY);
    if (fd < 0) {
        string errorStr = ErrnoToString(GetErrno());
        LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                               string("open secondary file for write meta fail:") + filename
                                                   + ",reason:" + errorStr);
        LOG_ERROR(sLogger, ("open file error", filename));
        return false;
    }
    lseek(fd, pos, SEEK_SET);
    if (write(fd, buf, length) < 0) {
        string errorStr = ErrnoToString(GetErrno());
        LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                               string("write secondary file for write meta fail:") + filename
                                                   + ",reason:" + errorStr);
        LOG_ERROR(sLogger, ("can not write back meta", filename));
    }
    close(fd);
    return true;
#elif defined(_MSC_VER)
    FILE* f = FileWriteOnlyOpen(filename.c_str(), "wb");
    if (!f) {
        string errorStr = ErrnoToString(GetErrno());
        LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                               string("open secondary file for write meta fail:") + filename
                                                   + ",reason:" + errorStr);
        LOG_ERROR(sLogger, ("open file error", filename));
        return false;
    }
    fseek(f, pos, SEEK_SET);
    auto nbytes = fwrite(buf, 1, length, f);
    if (nbytes != length) {
        string errorStr = ErrnoToString(GetErrno());
        LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                               string("write secondary file for write meta fail:") + filename
                                                   + ",reason:" + errorStr);
        LOG_ERROR(sLogger, ("can not write back meta", filename));
    }
    fclose(f);
    return true;
#endif
}
bool Sender::RemoveSender() {
    mBufferSenderThreadIsRunning = false;
    mSenderQueue.Signal();
    {
        WaitObject::Lock lock(mBufferWait);
        mBufferWait.signal();
    }
    {
        WaitObject::Lock lock(mWriteSecondaryWait);
        mWriteSecondaryWait.signal();
    }

    try {
        if (mSendBufferThreadId.get() != NULL) {
            mSendBufferThreadId->GetValue(200 * 1000);
        }
        mSenderQueue.RemoveAll();
    } catch (const ExceptionBase& e) {
        LOG_ERROR(sLogger,
                  ("Remove SendBufferThread fail, message", e.GetExceptionMessage())("exception", e.ToString()));
    } catch (...) {
        LOG_ERROR(sLogger, ("Remove SendBufferThread fail", "unknown exception"));
    }

    ResetSendingCount();
    SetSendingBufferCount(0);
    return true;
}

void Sender::WriteSecondary() {
    vector<LoggroupTimeValue*> logGroupToDump;
    LOG_DEBUG(sLogger, ("DumpSecondaryThread", "start"));
    while (true) {
        {
            WaitObject::Lock lock(mWriteSecondaryWait);
            mWriteSecondaryWait.wait(lock, INT32_FLAG(write_secondary_wait_timeout) * 1000000);
        }
        // update bufferDiveideTime to flush data; buffer file before bufferDiveideTime will be ready for read
        if (time(NULL) - mBufferDivideTime > INT32_FLAG(buffer_file_alive_interval))
            CreateNewFile();

        {
            PTScopedLock lock(mSecondaryMutexLock);
            if (mSecondaryBuffer.size() > 0) {
                logGroupToDump.insert(logGroupToDump.end(), mSecondaryBuffer.begin(), mSecondaryBuffer.end());
                mSecondaryBuffer.clear();
            }
        }

        if (logGroupToDump.size() > 0) {
            for (vector<LoggroupTimeValue*>::iterator itr = logGroupToDump.begin(); itr != logGroupToDump.end();
                 ++itr) {
                SendToBufferFile(*itr);
                LOG_DEBUG(sLogger, ("Write LogGroup to Secondary File, logs", (*itr)->mLogLines));
                delete *itr;
            }
            logGroupToDump.clear();
        }
    }
    LOG_INFO(sLogger, ("DumpSecondaryThread", "exit"));
}

bool Sender::IsBatchMapEmpty() {
    return mSenderQueue.IsEmpty();
}

bool Sender::IsSecondaryBufferEmpty() {
    PTScopedLock lock(mSecondaryMutexLock);
    if (mSecondaryBuffer.size() > 0)
        return false;
    return true;
}

void Sender::DaemonSender() {
    LOG_INFO(sLogger, ("SendThread", "start"));
    int32_t lastUpdateMetricTime = time(NULL);
    int32_t sendBufferCount = 0;
    size_t sendBufferBytes = 0;
    size_t sendLines = 0;
    size_t sendNetBodyBytes = 0;
    mLastDaemonRunTime = lastUpdateMetricTime;
    mLastSendDataTime = lastUpdateMetricTime;
    Aggregator* aggregator = Aggregator::GetInstance();
    while (true) {
        vector<LoggroupTimeValue*> logGroupToSend;
        mSenderQueue.Wait(1000);

        uint32_t bufferPackageCount = 0;
        bool singleBatchMapFull = false;
        int32_t curTime = time(NULL);
        if (IsFlush()) {
            mSenderQueue.PopAllItem(logGroupToSend, curTime, singleBatchMapFull);
        } else {
            std::unordered_map<std::string, int32_t> regionConcurrencyLimits;
            {
                PTScopedLock lock(mRegionEndpointEntryMapLock);
                for (auto iter = mRegionEndpointEntryMap.begin(); iter != mRegionEndpointEntryMap.end(); ++iter) {
                    regionConcurrencyLimits.insert(std::make_pair(iter->first, iter->second->mConcurrency));
                }
            }

            mSenderQueue.CheckAndPopAllItem(logGroupToSend, curTime, singleBatchMapFull, regionConcurrencyLimits);

#ifdef LOGTAIL_DEBUG_FLAG
            if (logGroupToSend.size() > 0) {
                static size_t s_totalCount = 0;
                static int32_t s_lastTime = 0;
                s_totalCount += logGroupToSend.size();
                LOG_DEBUG(sLogger, ("CheckAndPopAllItem logs", logGroupToSend.size()));
                if (curTime - s_lastTime > 60) {
                    LOG_INFO(sLogger,
                             ("CheckAndPopAllItem logs", s_totalCount)("singleBatchMap", ToString(singleBatchMapFull)));
                    s_lastTime = curTime;
                    s_totalCount = 0;
                }
            }
#endif
        }
        mLastDaemonRunTime = curTime;
        IncSendingCount((int32_t)logGroupToSend.size());
        bufferPackageCount = logGroupToSend.size();

        sendBufferCount += logGroupToSend.size();
        if (curTime - lastUpdateMetricTime >= 40) {
            static auto sMonitor = LogtailMonitor::Instance();

            sMonitor->UpdateMetric("send_tps", 1.0 * sendBufferCount / (curTime - lastUpdateMetricTime));
            sMonitor->UpdateMetric("send_bytes_ps", 1.0 * sendBufferBytes / (curTime - lastUpdateMetricTime));
            sMonitor->UpdateMetric("send_net_bytes_ps", 1.0 * sendNetBodyBytes / (curTime - lastUpdateMetricTime));
            sMonitor->UpdateMetric("send_lines_ps", 1.0 * sendLines / (curTime - lastUpdateMetricTime));
            lastUpdateMetricTime = curTime;
            sendBufferCount = 0;
            sendLines = 0;
            sendNetBodyBytes = 0;
            sendBufferBytes = 0;

            // update process queue status
            int32_t invalidCount = 0;
            int32_t invalidSenderCount = 0;
            int32_t totalCount = 0;
            int32_t eoInvalidCount = 0;
            int32_t eoInvalidSenderCount = 0;
            int32_t eoTotalCount = 0;
            mSenderQueue.GetStatus(
                invalidCount, invalidSenderCount, totalCount, eoInvalidCount, eoInvalidSenderCount, eoTotalCount);
            sMonitor->UpdateMetric("send_queue_full", invalidCount);
            sMonitor->UpdateMetric("send_queue_total", totalCount);
            sMonitor->UpdateMetric("sender_invalid", invalidSenderCount);
            if (eoTotalCount > 0) {
                sMonitor->UpdateMetric("eo_send_queue_full", eoInvalidCount);
                sMonitor->UpdateMetric("eo_send_queue_total", eoTotalCount);
                sMonitor->UpdateMetric("eo_sender_invalid", eoInvalidSenderCount);
            }

            // Collect at most 15 stats, similar to Linux load 1,5,15.
            static SlidingWindowCounter sNetErrCounter = CreateLoadCounter();
            sMonitor->UpdateMetric("net_err_stat", sNetErrCounter.Add(gNetworkErrorCount.exchange(0)));
        }

        ///////////////////////////////////////
        // smoothing send tps, walk around webserver load burst
        if (!IsFlush() && AppConfig::GetInstance()->IsSendRandomSleep()) {
            int64_t sleepMicroseconds = 0;
            if (bufferPackageCount < 10)
                sleepMicroseconds = (rand() % 40) * 10000; // 0ms ~ 400ms
            else if (!singleBatchMapFull) {
                if (bufferPackageCount < 20)
                    sleepMicroseconds = (rand() % 30) * 10000; // 0ms ~ 300ms
                else if (bufferPackageCount < 30)
                    sleepMicroseconds = (rand() % 20) * 10000; // 0ms ~ 200ms
                else if (bufferPackageCount < 40)
                    sleepMicroseconds = (rand() % 10) * 10000; // 0ms ~ 100ms
            }
            if (sleepMicroseconds > 0)
                usleep(sleepMicroseconds);
        }
        ///////////////////////////////////////

        for (vector<LoggroupTimeValue*>::iterator itr = logGroupToSend.begin(); itr != logGroupToSend.end(); ++itr) {
            LoggroupTimeValue* data = *itr;
            int32_t logGroupWaitTime = curTime - data->mLastUpdateTime;

            if (logGroupWaitTime > INT32_FLAG(log_group_wait_in_queue_alarm_interval)) {
                LOG_WARNING(sLogger,
                            ("log group wait in queue for too long, may blocked by concurrency or quota, region",
                             data->mRegion)("project", data->mProjectName)("logstore", data->mLogstore)(
                                "log group wait time", logGroupWaitTime));
                LogtailAlarm::GetInstance()->SendAlarm(LOG_GROUP_WAIT_TOO_LONG_ALARM,
                                                       "log group wait in queue for too long, log group wait time "
                                                           + ToString(logGroupWaitTime),
                                                       data->mProjectName,
                                                       data->mLogstore,
                                                       data->mRegion);
            }

            mLastSendDataTime = curTime;
            if (BOOST_UNLIKELY(AppConfig::GetInstance()->IsDebugMode())) {
                DumpDebugFile(data);
                OnSendDone(data, LogstoreSenderInfo::SendResult_OK);
                DescSendingCount();
            } else {
                if (!IsFlush() && AppConfig::GetInstance()->IsSendFlowControl()) {
                    FlowControl(data->mRawSize, REALTIME_SEND_THREAD);
                }

                int32_t beforeSleepTime = time(NULL);
                while (!IsFlush() && GetSendingBufferCount() >= AppConfig::GetInstance()->GetSendRequestConcurrency()) {
                    usleep(10 * 1000);
                }
                int32_t afterSleepTime = time(NULL);
                int32_t sendCostTime = afterSleepTime - beforeSleepTime;
                if (sendCostTime > INT32_FLAG(sending_cost_time_alarm_interval)) {
                    LOG_WARNING(sLogger,
                                ("sending log group blocked too long because of on flying buffer",
                                 "")("send cost time", sendCostTime)("sending buffer count", GetSendingBufferCount())(
                                    "send request concurrency", AppConfig::GetInstance()->GetSendRequestConcurrency()));
                    LogtailAlarm::GetInstance()->SendAlarm(SENDING_COSTS_TOO_MUCH_TIME_ALARM,
                                                           "sending log group costs too much time, send cost time "
                                                               + ToString(sendCostTime),
                                                           data->mProjectName,
                                                           data->mLogstore,
                                                           data->mRegion);
                }

                AddSendingBufferCount();
                sendBufferBytes += data->mRawSize;
                sendNetBodyBytes += data->mLogData.size();
                sendLines += data->mLogLines;
                SendToNetAsync(data);
            }
        }
        logGroupToSend.clear();

        if ((time(NULL) - mLastCheckSendClientTime) > INT32_FLAG(check_send_client_timeout_interval)) {
            CleanTimeoutSendClient();
            CleanTimeoutSendStatistic();
            aggregator->CleanTimeoutLogPackSeq();
            mLastCheckSendClientTime = time(NULL);
        }

        if (IsFlush() && aggregator->IsMergeMapEmpty() && IsBatchMapEmpty() && GetSendingCount() == 0
            && IsSecondaryBufferEmpty())
            ResetFlush();
    }
    LOG_INFO(sLogger, ("SendThread", "exit"));
}

void Sender::PutIntoSecondaryBuffer(LoggroupTimeValue* dataPtr, int32_t retryTimes) {
    int32_t retry = 0;
    bool writeDone = false;
    while (true) {
        ++retry;
        {
            PTScopedLock lock(mSecondaryMutexLock);
            if (IsFlush() || (mSecondaryBuffer.size() < (uint32_t)INT32_FLAG(secondary_buffer_count_limit))) {
                mSecondaryBuffer.push_back(dataPtr);
                writeDone = true;
                break;
            }
        }

        if (retry >= retryTimes)
            break;
        {
            WaitObject::Lock lock(mWriteSecondaryWait);
            mWriteSecondaryWait.signal();
        }
        usleep(50 * 1000);
    }

    if (writeDone) {
        WaitObject::Lock lock(mWriteSecondaryWait);
        mWriteSecondaryWait.signal();
    } else {
        LOG_ERROR(sLogger,
                  ("write to secondary buffer fail", "discard data")("projectName", dataPtr->mProjectName)(
                      "logstore", dataPtr->mLogstore)("RetryTimes", dataPtr->mSendRetryTimes)(
                      "LogLines", dataPtr->mLogLines)("bytes", dataPtr->mLogData.size()));
        LogtailAlarm::GetInstance()->SendAlarm(DISCARD_DATA_ALARM,
                                               "write to buffer file fail, logs:" + ToString(dataPtr->mLogLines),
                                               dataPtr->mProjectName,
                                               dataPtr->mLogstore,
                                               dataPtr->mRegion);
        delete dataPtr;
    }
}

string Sender::GetBufferFileHeader() {
    string reserve = STRING_FLAG(file_encryption_field_key_version) + STRING_FLAG(file_encryption_key_value_splitter)
        + ToString(FileEncryption::GetInstance()->GetDefaultKeyVersion());
    string nullHeader = string(
        (INT32_FLAG(file_encryption_header_length) - STRING_FLAG(file_encryption_magic_number).size() - reserve.size()),
        '\0');
    return (STRING_FLAG(file_encryption_magic_number) + reserve + nullHeader);
}

bool Sender::SendToBufferFile(LoggroupTimeValue* dataPtr) {
    string bufferFileName = GetBufferFileName();
    if (bufferFileName.empty()) {
        CreateNewFile();
        bufferFileName = GetBufferFileName();
    }
    // if file not exist, create it new
    FILE* fout = FileAppendOpen(bufferFileName.c_str(), "ab");
    if (!fout) {
        string errorStr = ErrnoToString(GetErrno());
        LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                               string("open file error:") + bufferFileName + ",error:" + errorStr);
        LOG_ERROR(sLogger, ("open buffer file error", bufferFileName));
        return false;
    }

    if (ftell(fout) == (streampos)0) {
        string header = GetBufferFileHeader();
        auto nbytes = fwrite(header.c_str(), 1, header.size(), fout);
        if (header.size() != nbytes) {
            string errorStr = ErrnoToString(GetErrno());
            LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                                   string("write file error:") + bufferFileName + ", error:" + errorStr
                                                       + ", nbytes:" + ToString(nbytes));
            LOG_ERROR(sLogger, ("error write encryption header", bufferFileName)("error", errorStr)("nbytes", nbytes));
            fclose(fout);
            return false;
        }
    }

    char* des;
    int32_t desLength;
    if (!FileEncryption::GetInstance()->Encrypt(dataPtr->mLogData.c_str(), dataPtr->mLogData.size(), des, desLength)) {
        fclose(fout);
        LOG_ERROR(sLogger, ("encrypt error, project_name", dataPtr->mProjectName));
        LogtailAlarm::GetInstance()->SendAlarm(ENCRYPT_DECRYPT_FAIL_ALARM,
                                               string("encrypt error, project_name:" + dataPtr->mProjectName));
        return false;
    }

    LogtailBufferMeta bufferMeta;
    bufferMeta.set_project(dataPtr->mProjectName);
    bufferMeta.set_endpoint(dataPtr->mRegion);
    bufferMeta.set_aliuid(dataPtr->mAliuid);
    bufferMeta.set_logstore(dataPtr->mLogstore);
    bufferMeta.set_datatype(int32_t(dataPtr->mDataType));
    bufferMeta.set_rawsize(dataPtr->mRawSize);
    bufferMeta.set_shardhashkey(dataPtr->mShardHashKey);
    string encodedInfo;
    bufferMeta.SerializeToString(&encodedInfo);

    EncryptionStateMeta meta;
    int32_t encodedInfoSize = encodedInfo.size();
    meta.mEncodedInfoSize = encodedInfoSize + BUFFER_META_BASE_SIZE;
    meta.mLogDataSize = dataPtr->mLogData.size();
    meta.mTimeStamp = time(NULL);
    meta.mHandled = 0;
    meta.mRetryTime = 0;
    meta.mEncryptionSize = desLength;
    char* buffer = new char[sizeof(meta) + encodedInfoSize + meta.mEncryptionSize];
    memcpy(buffer, (char*)&meta, sizeof(meta));
    memcpy(buffer + sizeof(meta), encodedInfo.c_str(), encodedInfoSize);
    memcpy(buffer + sizeof(meta) + encodedInfoSize, des, desLength);
    delete[] des;
    const auto bytesToWrite = sizeof(meta) + encodedInfoSize + meta.mEncryptionSize;
    auto nbytes = fwrite(buffer, 1, bytesToWrite, fout);
    if (nbytes != bytesToWrite) {
        string errorStr = ErrnoToString(GetErrno());
        LogtailAlarm::GetInstance()->SendAlarm(SECONDARY_READ_WRITE_ALARM,
                                               string("write file error:") + bufferFileName + ", error:" + errorStr
                                                   + ", nbytes:" + ToString(nbytes));
        LOG_ERROR(
            sLogger,
            ("write meta of buffer file", "fail")("filename", bufferFileName)("errorStr", errorStr)("nbytes", nbytes));
        delete[] buffer;
        fclose(fout);
        return false;
    }
    delete[] buffer;
    if (BOOL_FLAG(enable_mock_send))
        fflush(fout);
    if (ftell(fout) > AppConfig::GetInstance()->GetLocalFileSize())
        CreateNewFile();
    fclose(fout);
    LOG_DEBUG(sLogger, ("write buffer file", bufferFileName)("loglines", dataPtr->mLogLines));
    return true;
}

void Sender::FlowControl(int32_t dataSize, SEND_THREAD_TYPE type) {
    int64_t curTime = GetCurrentTimeInMicroSeconds();
    int32_t idx = int32_t(type);
    int32_t bps = (type == REALTIME_SEND_THREAD) ? AppConfig::GetInstance()->GetMaxBytePerSec()
                                                 : AppConfig::GetInstance()->GetBytePerSec();
    if (curTime < mSendLastTime[idx] || curTime - mSendLastTime[idx] >= 1000 * 1000) {
        mSendLastTime[idx] = curTime;
        mSendLastByte[idx] = dataSize;
    } else {
        if (mSendLastByte[idx] > bps) {
            usleep(1000 * 1000 - (curTime - mSendLastTime[idx]));
            mSendLastTime[idx] = GetCurrentTimeInMicroSeconds();
            mSendLastByte[idx] = dataSize;
        } else {
            mSendLastByte[idx] += dataSize;
        }
    }
}

bool Sender::IsValidToSend(const LogstoreFeedBackKey& logstoreKey) {
    return mSenderQueue.IsValidToPush(logstoreKey);
}

bool Sender::SendPb(Config* pConfig,
                    char* pbBuffer,
                    int32_t pbSize,
                    int32_t lines,
                    const std::string& logstore,
                    const std::string& shardHash) {
    // if logstore is specific, use this key, otherwise use pConfig->->mCategory
    LoggroupTimeValue* pData = new LoggroupTimeValue(pConfig->mProjectName,
                                                     logstore.empty() ? pConfig->mCategory : logstore,
                                                     pConfig->mConfigName,
                                                     pConfig->mFilePattern,
                                                     true,
                                                     pConfig->mAliuid,
                                                     pConfig->mRegion,
                                                     LOGGROUP_LZ4_COMPRESSED,
                                                     lines,
                                                     pbSize,
                                                     time(NULL),
                                                     shardHash,
                                                     pConfig->mLogstoreKey);
    //apsara::timing::TimeInNsec startT = apsara::timing::GetCurrentTimeInNanoSeconds();
    if (!CompressLz4(pbBuffer, pbSize, pData->mLogData)) {
        LOG_ERROR(sLogger,
                  ("compress data fail", "discard data")("projectName", pConfig->mProjectName)("logstore",
                                                                                               pConfig->mCategory));
        delete pData;
        return false;
    } else {
        //apsara::timing::TimeInNsec endT = apsara::timing::GetCurrentTimeInNanoSeconds();
        PutIntoBatchMap(pData);
        //apsara::timing::TimeInNsec endT2 = apsara::timing::GetCurrentTimeInNanoSeconds();
        //printf("compress and insert %d %d %d %d\n", (int)(endT - startT), (int)(endT2 - endT), (int)(pbSize), (int)(pData->mLogData.size()));
    }
    return true;
}

void Sender::AddEndpointEntry(const std::string& region, const std::string& endpoint, bool isDefault, bool isProxy) {
    LOG_DEBUG(sLogger,
              ("AddEndpointEntry, region", region)("endpoint", endpoint)("isDefault", isDefault)("isProxy", isProxy));
    PTScopedLock lock(mRegionEndpointEntryMapLock);
    std::unordered_map<std::string, RegionEndpointEntry*>::iterator iter = mRegionEndpointEntryMap.find(region);
    RegionEndpointEntry* entryPtr;
    if (iter == mRegionEndpointEntryMap.end()) {
        entryPtr = new RegionEndpointEntry();
        mRegionEndpointEntryMap.insert(std::make_pair(region, entryPtr));
    } else
        entryPtr = iter->second;

    if (isDefault)
        entryPtr->AddDefaultEndpoint(endpoint);
    else {
        entryPtr->AddEndpoint(endpoint, true, -1, isProxy);
    }
}

void Sender::TestNetwork() {
#ifdef LOGTAIL_RUNTIME_PLUGIN
    return;
#endif
    vector<std::string> unavaliableEndpoints;
    set<std::string> unavaliableRegions;
    int32_t lastCheckAllTime = 0;
    while (true) {
        unavaliableEndpoints.clear();
        unavaliableRegions.clear();
        {
            PTScopedLock lock(mRegionEndpointEntryMapLock);
            for (std::unordered_map<std::string, RegionEndpointEntry*>::iterator iter = mRegionEndpointEntryMap.begin();
                 iter != mRegionEndpointEntryMap.end();
                 ++iter) {
                bool unavaliable = true;
                for (std::unordered_map<std::string, EndpointDetail>::iterator epIter
                     = ((iter->second)->mEndpointDetailMap).begin();
                     epIter != ((iter->second)->mEndpointDetailMap).end();
                     ++epIter) {
                    if (!(epIter->second).mStatus) {
                        unavaliableEndpoints.push_back(iter->first);
                        unavaliableEndpoints.push_back(epIter->first);
                    } else
                        unavaliable = false;
                }
                if (unavaliable)
                    unavaliableRegions.insert(iter->first);
            }
        }
        if (unavaliableEndpoints.size() == 0) {
            sleep(INT32_FLAG(test_network_normal_interval));
            continue;
        }
        int32_t curTime = time(NULL);
        bool flag = false;
        bool wakeUp = false;
        for (size_t i = 0; i < unavaliableEndpoints.size(); i += 2) {
            const string& region = unavaliableEndpoints[i];
            const string& endpoint = unavaliableEndpoints[i + 1];
            if (unavaliableRegions.find(region) == unavaliableRegions.end()) {
                if (curTime - lastCheckAllTime >= 1800) {
                    TestEndpoint(region, endpoint);
                    flag = true;
                }
            } else {
                if (TestEndpoint(region, endpoint)) {
                    LOG_DEBUG(sLogger, ("Region recover success", "")(region, endpoint));
                    wakeUp = true;
                    mSenderQueue.OnRegionRecover(region);
                }
            }
        }
        if (flag)
            lastCheckAllTime = curTime;
        if (wakeUp && (!mIsSendingBuffer)) {
            mSenderQueue.Signal();
        }
        sleep(INT32_FLAG(test_unavailable_endpoint_interval));
    }
}

bool Sender::TestEndpoint(const std::string& region, const std::string& endpoint) {
    // if region status not ok, skip test endpoint
    if (!ConfigManager::GetInstance()->GetRegionStatus(region)) {
        return false;
    }
    if (mTestNetworkClient == NULL)
        return true;
    if (endpoint.size() == 0)
        return false;
    static LogGroup logGroup;
    mTestNetworkClient->SetSlsHost(endpoint);
    bool status = true;
    int64_t beginTime = GetCurrentTimeInMicroSeconds();
    try {
        if (BOOL_FLAG(enable_mock_send) && MockTestEndpoint) {
            string logData;
            MockTestEndpoint(
                "logtail-test-network-project", "logtail-test-network-logstore", logData, LOGGROUP_LZ4_COMPRESSED, 0);
        } else
            status = mTestNetworkClient->TestNetwork();
    } catch (sdk::LOGException& ex) {
        const string& errorCode = ex.GetErrorCode();
        LOG_DEBUG(sLogger, ("test network", "send fail")("errorCode", errorCode)("errorMessage", ex.GetMessage()));
        SendResult sendRst = ConvertErrorCode(errorCode);
        if (sendRst == SEND_NETWORK_ERROR)
            status = false;
    } catch (...) {
        LOG_ERROR(sLogger, ("test network", "send fail")("exception", "unknown"));
    }
    int64_t endTime = GetCurrentTimeInMicroSeconds();
    int32_t latency = int32_t((endTime - beginTime) / 1000); //ms
    LOG_DEBUG(sLogger, ("TestEndpoint, region", region)("endpoint", endpoint)("status", status)("latency", latency));
    SetNetworkStat(region, endpoint, status, latency);
    return status;
}

bool Sender::IsProfileData(const string& region, const std::string& project, const std::string& logstore) {
    if ((logstore == "shennong_log_profile" || logstore == "logtail_alarm" || logstore == "logtail_status_profile"
         || logstore == "logtail_suicide_profile")
        && (project == ConfigManager::GetInstance()->GetProfileProjectName(region) || region == ""))
        return true;
    else
        return false;
}

SendResult
Sender::SendBufferFileData(const LogtailBufferMeta& bufferMeta, const std::string& logData, std::string& errorCode) {
    FlowControl(bufferMeta.rawsize(), REPLAY_SEND_THREAD);
    string region = bufferMeta.endpoint();
    if (region.find("http://") == 0) //old buffer file which record the endpoint
        region = GetRegionFromEndpoint(region);

    sdk::Client* sendClient = GetSendClient(region, bufferMeta.aliuid());
    SendResult sendRes;
    const string& endpoint = sendClient->GetRawSlsHost();
    if (endpoint.empty())
        sendRes = SEND_NETWORK_ERROR;
    else {
        sendRes = SendToNetSync(sendClient, bufferMeta, logData, errorCode);
    }
    if (sendRes == SEND_NETWORK_ERROR) {
        SetNetworkStat(region, endpoint, false);
        Sender::Instance()->ResetSendClientEndpoint(bufferMeta.aliuid(), region, time(NULL));
        LOG_DEBUG(sLogger,
                  ("SendBufferFileData",
                   "SEND_NETWORK_ERROR")("region", region)("aliuid", bufferMeta.aliuid())("endpoint", endpoint));
    } else if (sendRes == SEND_UNAUTHORIZED) {
        int32_t lastUpdateTime;
        if (SLSControl::Instance()->SetSlsSendClientAuth(bufferMeta.aliuid(), false, sendClient, lastUpdateTime))
            sendRes = SendToNetSync(sendClient, bufferMeta, logData, errorCode);
    }
    return sendRes;
}

SendResult Sender::SendToNetSync(sdk::Client* sendClient,
                                 const LogtailBufferMeta& bufferMeta,
                                 const std::string& logData,
                                 std::string& errorCode) {
    int32_t retryTimes = 0;
    while (true) {
        ++retryTimes;
        try {
            if (BOOL_FLAG(enable_mock_send)) {
                if (MockSyncSend)
                    MockSyncSend(bufferMeta.project(),
                                 bufferMeta.logstore(),
                                 logData,
                                 (SEND_DATA_TYPE)bufferMeta.datatype(),
                                 bufferMeta.rawsize());
                else
                    LOG_ERROR(sLogger, ("MockSyncSend", "uninitialized"));
            } else if (bufferMeta.datatype() == LOGGROUP_LZ4_COMPRESSED) {
#ifdef LOGTAIL_RUNTIME_PLUGIN
                LogtailRuntimePlugin::GetInstance()->LogtailSendPb(bufferMeta.project(),
                                                                   bufferMeta.logstore(),
                                                                   logData.c_str(),
                                                                   logData.size(),
                                                                   bufferMeta.rawsize(),
                                                                   0);
                return SEND_OK;
#endif
                if (bufferMeta.has_shardhashkey() && !bufferMeta.shardhashkey().empty())
                    sendClient->PostLogStoreLogs(bufferMeta.project(),
                                                 bufferMeta.logstore(),
                                                 logData,
                                                 bufferMeta.rawsize(),
                                                 bufferMeta.shardhashkey());
                else
                    sendClient->PostLogStoreLogs(
                        bufferMeta.project(), bufferMeta.logstore(), logData, bufferMeta.rawsize());
            } else {
#ifdef LOGTAIL_RUNTIME_PLUGIN
                return SEND_OK;
#endif
                if (bufferMeta.has_shardhashkey() && !bufferMeta.shardhashkey().empty())
                    sendClient->PostLogStoreLogPackageList(
                        bufferMeta.project(), bufferMeta.logstore(), logData, bufferMeta.shardhashkey());
                else
                    sendClient->PostLogStoreLogPackageList(bufferMeta.project(), bufferMeta.logstore(), logData);
            }
            return SEND_OK;
        } catch (sdk::LOGException& ex) {
            errorCode = ex.GetErrorCode();
            SendResult sendRes = ConvertErrorCode(errorCode);
            if (sendRes == SEND_DISCARD_ERROR || sendRes == SEND_UNAUTHORIZED || sendRes == SEND_QUOTA_EXCEED
                || retryTimes >= INT32_FLAG(send_retrytimes)) {
                if (sendRes == SEND_QUOTA_EXCEED)
                    LogtailAlarm::GetInstance()->SendAlarm(SEND_QUOTA_EXCEED_ALARM,
                                                           "error_code: " + errorCode
                                                               + ", error_message: " + ex.GetMessage(),
                                                           bufferMeta.project(),
                                                           bufferMeta.logstore(),
                                                           "");
                // no region
                if (!IsProfileData("", bufferMeta.project(), bufferMeta.logstore()))
                    LOG_ERROR(sLogger,
                              ("send data to SLS fail, error_code", errorCode)("error_message", ex.GetMessage())(
                                  "endpoint", sendClient->GetRawSlsHost())("projectName", bufferMeta.project())(
                                  "logstore", bufferMeta.logstore())("RetryTimes", retryTimes)("rawsize",
                                                                                               bufferMeta.rawsize()));
                return sendRes;
            } else {
                LOG_DEBUG(
                    sLogger,
                    ("send data to SLS fail", "retry later")("error_code", errorCode)("error_message", ex.GetMessage())(
                        "endpoint", sendClient->GetRawSlsHost())("projectName", bufferMeta.project())(
                        "logstore", bufferMeta.logstore())("RetryTimes", retryTimes)("rawsize", bufferMeta.rawsize()));
                usleep(INT32_FLAG(send_retry_sleep_interval));
            }
        } catch (...) {
            if (retryTimes >= INT32_FLAG(send_retrytimes)) {
                LOG_ERROR(sLogger,
                          ("send data fail", "unknown excepiton")("endpoint", sendClient->GetRawSlsHost())(
                              "projectName", bufferMeta.project())("logstore", bufferMeta.logstore())(
                              "rawsize", bufferMeta.rawsize()));
                return SEND_DISCARD_ERROR;
            } else {
                LOG_DEBUG(sLogger,
                          ("send data fail", "unknown excepiton, retry later")("endpoint", sendClient->GetRawSlsHost())(
                              "projectName", bufferMeta.project())("logstore", bufferMeta.logstore())(
                              "rawsize", bufferMeta.rawsize()));
                usleep(INT32_FLAG(send_retry_sleep_interval));
            }
        }
    }
}

void Sender::SendToNetAsync(LoggroupTimeValue* dataPtr) {
    auto& exactlyOnceCpt = dataPtr->mLogGroupContext.mExactlyOnceCheckpoint;

    if (IsFlush()) // write local file avoid binary update fail
    {
        SubSendingBufferCount();
        if (!exactlyOnceCpt) {
            PutIntoSecondaryBuffer(dataPtr, 3);
        } else {
            LOG_INFO(sLogger,
                     ("no need to flush exactly once data to local",
                      exactlyOnceCpt->key)("checkpoint", exactlyOnceCpt->data.DebugString()));
        }
        OnSendDone(dataPtr, LogstoreSenderInfo::SendResult_Buffered);
        DescSendingCount();
        return;
    }

#ifdef LOGTAIL_RUNTIME_PLUGIN
    if (dataPtr->mDataType != LOG_PACKAGE_LIST) {
        LogtailRuntimePlugin::GetInstance()->LogtailSendPb(dataPtr->mProjectName,
                                                           dataPtr->mLogstore,
                                                           dataPtr->mLogData.c_str(),
                                                           dataPtr->mLogData.size(),
                                                           dataPtr->mRawSize,
                                                           dataPtr->mLogLines);
    }
    SubSendingBufferCount();
    OnSendDone(dataPtr, LogstoreSenderInfo::SendResult_OK);
    DescSendingCount();
    return;
#endif

    static int32_t lastResetEndpointTime = 0;
    sdk::Client* sendClient = GetSendClient(dataPtr->mRegion, dataPtr->mAliuid);
    int32_t curTime = time(NULL);

    dataPtr->mCurrentEndpoint = sendClient->GetRawSlsHost();
    if (dataPtr->mCurrentEndpoint.empty()) {
        if (curTime - lastResetEndpointTime >= 30) {
            ResetSendClientEndpoint(dataPtr->mAliuid, dataPtr->mRegion, curTime);
            dataPtr->mCurrentEndpoint = sendClient->GetRawSlsHost();
            lastResetEndpointTime = curTime;
        }
    }
    if (BOOL_FLAG(send_prefer_real_ip)) {
        if (curTime - sendClient->GetSlsRealIpUpdateTime() >= INT32_FLAG(send_check_real_ip_interval)) {
            UpdateSendClientRealIp(sendClient, dataPtr->mRegion);
        }
        dataPtr->mRealIpFlag = sendClient->GetRawSlsHostFlag();
    }

    SendClosure* sendClosure = new SendClosure;
    sendClosure->mDataPtr = dataPtr;
    LOG_DEBUG(sLogger,
              ("region", dataPtr->mRegion)("endpoint", dataPtr->mCurrentEndpoint)("project", dataPtr->mProjectName)(
                  "logstore", dataPtr->mLogstore)("LogLines", dataPtr->mLogLines)("bytes", dataPtr->mLogData.size()));
    if (BOOL_FLAG(enable_mock_send)) {
        if (MockAsyncSend && !MockIntegritySend)
            MockAsyncSend(dataPtr->mProjectName,
                          dataPtr->mLogstore,
                          dataPtr->mLogData,
                          dataPtr->mDataType,
                          dataPtr->mRawSize,
                          sendClosure);
        else if (MockIntegritySend)
            MockIntegritySend(dataPtr);
        else {
            LOG_ERROR(sLogger, ("MockAsyncSend", "uninitialized"));
            SubSendingBufferCount();
            DescSendingCount();
            delete sendClosure;
        }
    } else if (dataPtr->mDataType == LOGGROUP_LZ4_COMPRESSED) {
        const auto& hashKey = exactlyOnceCpt ? exactlyOnceCpt->data.hash_key() : dataPtr->mShardHashKey;
        if (hashKey.empty()) {
            sendClient->PostLogStoreLogs(
                dataPtr->mProjectName, dataPtr->mLogstore, dataPtr->mLogData, dataPtr->mRawSize, sendClosure);
        } else {
            int64_t hashKeySeqID = exactlyOnceCpt ? exactlyOnceCpt->data.sequence_id() : sdk::kInvalidHashKeySeqID;
            sendClient->PostLogStoreLogs(dataPtr->mProjectName,
                                         dataPtr->mLogstore,
                                         dataPtr->mLogData,
                                         dataPtr->mRawSize,
                                         sendClosure,
                                         hashKey,
                                         hashKeySeqID);
        }
    } else {
        if (dataPtr->mShardHashKey.empty())
            sendClient->PostLogStoreLogPackageList(
                dataPtr->mProjectName, dataPtr->mLogstore, dataPtr->mLogData, sendClosure);
        else
            sendClient->PostLogStoreLogPackageList(
                dataPtr->mProjectName, dataPtr->mLogstore, dataPtr->mLogData, sendClosure, dataPtr->mShardHashKey);
    }
}

// config NULL means Logtail Profiling data
bool Sender::Send(const std::string& projectName,
                  const std::string& sourceId,
                  LogGroup& logGroup,
                  const Config* config,
                  DATA_MERGE_TYPE mergeType,
                  const uint32_t logGroupSize,
                  const string& defaultRegion,
                  const string& filename,
                  const LogGroupContext& context) {
    static Aggregator* aggregator = Aggregator::GetInstance();
    return aggregator->Add(
        projectName, sourceId, logGroup, config, mergeType, logGroupSize, defaultRegion, filename, context);
}

bool Sender::SendInstantly(sls_logs::LogGroup& logGroup,
                           const std::string& aliuid,
                           const std::string& region,
                           const std::string& projectName,
                           const std::string& logstore) {
    int32_t logSize = (int32_t)logGroup.logs_size();
    if (logSize == 0)
        return true;

    if ((int32_t)logGroup.ByteSize() > INT32_FLAG(max_send_log_group_size)) {
        LOG_ERROR(sLogger, ("invalid log group size", logGroup.ByteSize()));
        return false;
    }

    // no merge
    time_t curTime = time(NULL);

    // shardHashKey is empty because log data config is NULL
    std::string shardHashKey = "";
    LogstoreFeedBackKey feedBackKey = GenerateLogstoreFeedBackKey(projectName, logstore);

    std::string oriData;
    logGroup.SerializeToString(&oriData);
    // filename is empty string
    LoggroupTimeValue* data = new LoggroupTimeValue(projectName,
                                                    logstore,
                                                    "",
                                                    "",
                                                    false,
                                                    aliuid,
                                                    region,
                                                    LOGGROUP_LZ4_COMPRESSED,
                                                    logSize,
                                                    oriData.size(),
                                                    curTime,
                                                    shardHashKey,
                                                    feedBackKey);

    if (!CompressLz4(oriData, data->mLogData)) {
        LOG_ERROR(sLogger, ("compress data fail", "discard data")("projectName", projectName)("logstore", logstore));
        LogtailAlarm::GetInstance()->SendAlarm(
            LZ4_COMPRESS_FAIL_ALARM, string("logs :") + ToString(logSize), projectName, logstore, region);
        delete data;
        return false;
    }

    PutIntoBatchMap(data);
    return true;
}

void Sender::SendLZ4Compressed(const std::string& projectName,
                               sls_logs::LogGroup& logGroup,
                               const std::vector<int32_t>& neededLogIndex,
                               const std::string& configName,
                               const std::string& aliuid,
                               const std::string& region,
                               const std::string& filename,
                               const LogGroupContext& context) {
    string oriData;
    auto lines = static_cast<int32_t>(logGroup.logs_size());
    auto const neededLogLines = static_cast<int32_t>(neededLogIndex.size());
    int32_t logTimeInMinute = 0;
    if (neededLogLines == lines) {
        logGroup.SerializeToString(&oriData);
        logTimeInMinute = logGroup.logs(0).time() - logGroup.logs(0).time() % 60;
    } else {
        sls_logs::LogGroup filteredLogGroup;
        filteredLogGroup.set_category(logGroup.category());
        filteredLogGroup.set_topic(logGroup.topic());
        filteredLogGroup.set_machineuuid(ConfigManager::GetInstance()->GetUUID());
        filteredLogGroup.set_source(logGroup.has_source() ? logGroup.source() : LogFileProfiler::mIpAddr);
        filteredLogGroup.mutable_logtags()->Swap(logGroup.mutable_logtags());

        auto mutableLogPtr = logGroup.mutable_logs()->mutable_data();
        int32_t needIdx = 0;
        for (int32_t logIdx = 0; logIdx < lines; ++logIdx) {
            if (!(needIdx < neededLogLines && logIdx == neededLogIndex[needIdx])) {
                continue;
            }
            filteredLogGroup.mutable_logs()->AddAllocated(*(mutableLogPtr + logIdx));
            needIdx++;
        }
        lines = neededLogLines;
        filteredLogGroup.SerializeToString(&oriData);
        logTimeInMinute = filteredLogGroup.logs(0).time() - filteredLogGroup.logs(0).time() % 60;
    }

    auto& cpt = context.mExactlyOnceCheckpoint;
    auto data = new LoggroupTimeValue(projectName,
                                      logGroup.category(),
                                      configName,
                                      filename,
                                      false,
                                      aliuid,
                                      region,
                                      LOGGROUP_LZ4_COMPRESSED,
                                      lines,
                                      oriData.size(),
                                      time(NULL),
                                      "",
                                      cpt->fbKey,
                                      context);
    data->mLogTimeInMinute = logTimeInMinute;
    data->mLogGroupContext.mSeqNum = ++mLogGroupContextSeq;

    if (!CompressLz4(oriData, data->mLogData)) {
        LOG_ERROR(sLogger,
                  ("compress data fail", "discard data")("projectName", projectName)("logstore", logGroup.category()));
        LogtailAlarm::GetInstance()->SendAlarm(
            LZ4_COMPRESS_FAIL_ALARM, string("logs :") + ToString(lines), projectName, logGroup.category(), region);
        delete data;
    } else {
        PutIntoBatchMap(data);
    }
}

void Sender::SendLZ4Compressed(std::vector<MergeItem*>& sendDataVec) {
    for (auto item : sendDataVec) {
        string oriData;
        item->mLogGroup.SerializeToString(&oriData);
        mLogGroupContextSeq++;
        auto& context = item->mLogGroupContext;
        auto& cpt = context.mExactlyOnceCheckpoint;
        context.mSeqNum = mLogGroupContextSeq;
        LoggroupTimeValue* data = new LoggroupTimeValue(item->mProjectName,
                                                        item->mLogGroup.category(),
                                                        item->mConfigName,
                                                        item->mFilename,
                                                        cpt ? false : item->mBufferOrNot,
                                                        item->mAliuid,
                                                        item->mRegion,
                                                        LOGGROUP_LZ4_COMPRESSED,
                                                        item->mLines,
                                                        oriData.size(),
                                                        item->mLastUpdateTime,
                                                        cpt ? "" : item->mShardHashKey,
                                                        cpt ? cpt->fbKey : item->mLogstoreKey,
                                                        context);
        data->mLogTimeInMinute = item->mLogTimeInMinute;

        if (!CompressLz4(oriData, data->mLogData)) {
            LOG_ERROR(sLogger,
                      ("compress data fail",
                       "discard data")("projectName", item->mProjectName)("logstore", item->mLogGroup.category()));
            LogtailAlarm::GetInstance()->SendAlarm(LZ4_COMPRESS_FAIL_ALARM,
                                                   string("logs :") + ToString(item->mLines),
                                                   item->mProjectName,
                                                   item->mLogGroup.category(),
                                                   item->mRegion);
            delete data;
        } else {
            if (data->mLogGroupContext.mMarkOffsetFlag) {
                LogFileCollectOffsetIndicator::GetInstance()->RecordFileOffset(data);
            }

            // raw logs are merged, then pushed into sendDataVec, here we record integrity info and put it into list
            LogIntegrity::GetInstance()->RecordIntegrityInfo(item);

            PutIntoBatchMap(data);
        }
        delete item;
    }
}

//all data in sendDataVec shoud have same key
void Sender::SendLogPackageList(std::vector<MergeItem*>& sendDataVec) {
    SlsLogPackageList logPackageList;
    int32_t bytes = 0;
    int32_t lines = 0;
    uint32_t totalLogGroupCount = sendDataVec.size();
    for (uint32_t idx = 0; idx < totalLogGroupCount; ++idx) {
        string compressedData;
        string oriData;
        sendDataVec[idx]->mLogGroup.SerializeToString(&oriData);
        if (!CompressLz4(oriData, compressedData)) {
            LOG_ERROR(sLogger,
                      ("compress data fail", "discard data")("projectName", sendDataVec[idx]->mProjectName)(
                          "logstore", sendDataVec[idx]->mLogGroup.category()));
            LogtailAlarm::GetInstance()->SendAlarm(LZ4_COMPRESS_FAIL_ALARM,
                                                   string("logs :") + ToString(sendDataVec[idx]->mLines),
                                                   sendDataVec[idx]->mProjectName,
                                                   sendDataVec[idx]->mLogGroup.category(),
                                                   sendDataVec[idx]->mRegion);
            delete sendDataVec[idx];
            continue;
        }
        SlsLogPackage* package = logPackageList.add_packages();
        package->set_data(compressedData);
        package->set_uncompress_size(oriData.size());
        lines += sendDataVec[idx]->mLines;
        bytes += sendDataVec[idx]->mRawBytes;
        if (bytes >= AppConfig::GetInstance()->GetMaxHoldedDataSize() || idx == totalLogGroupCount - 1) {
            mLogGroupContextSeq++;
            sendDataVec[idx]->mLogGroupContext.mSeqNum = mLogGroupContextSeq;
            LoggroupTimeValue* data = new LoggroupTimeValue(sendDataVec[idx]->mProjectName,
                                                            sendDataVec[idx]->mLogGroup.category(),
                                                            sendDataVec[idx]->mConfigName,
                                                            sendDataVec[idx]->mFilename,
                                                            sendDataVec[idx]->mBufferOrNot,
                                                            sendDataVec[idx]->mAliuid,
                                                            sendDataVec[idx]->mRegion,
                                                            LOG_PACKAGE_LIST,
                                                            lines,
                                                            bytes,
                                                            sendDataVec[idx]->mLastUpdateTime,
                                                            sendDataVec[idx]->mShardHashKey,
                                                            sendDataVec[idx]->mLogstoreKey,
                                                            sendDataVec[idx]->mLogGroupContext);
            data->mLogTimeInMinute = sendDataVec[idx]->mLogTimeInMinute;
            logPackageList.SerializeToString(&(data->mLogData));
            logPackageList.Clear();
            bytes = 0;
            lines = 0;

            if (data->mLogGroupContext.mMarkOffsetFlag) {
                LogFileCollectOffsetIndicator::GetInstance()->RecordFileOffset(data);
            }

            // raw logs are merged, then pushed into sendDataVec, here we record integrity info and put it into list
            LogIntegrity::GetInstance()->RecordIntegrityInfo(sendDataVec[idx]);

            PutIntoBatchMap(data);
        }
        delete sendDataVec[idx];
    }
}

void Sender::PutIntoBatchMap(LoggroupTimeValue* data) {
    int32_t tryTime = 0;
    while (tryTime < 1000) {
        if (mSenderQueue.PushItem(data->mLogstoreKey, data)) {
            return;
        }
        if (tryTime++ == 0) {
            LOG_WARNING(sLogger,
                        ("Push to sender buffer map fail, try again, data size",
                         ToString(data->mRawSize))(data->mProjectName, data->mLogstore));
        }
        usleep(10 * 1000);
    }
    LogtailAlarm::GetInstance()->SendAlarm(
        DISCARD_DATA_ALARM, "push file data into batch map fail", data->mProjectName, data->mLogstore, data->mRegion);
    LOG_ERROR(sLogger,
              ("push file data into batch map fail, discard log lines",
               data->mLogLines)("project", data->mProjectName)("logstore", data->mLogstore));
    delete data;
}

void Sender::ResetSendingCount() {
    mSendingLogGroupCount = 0;
}

void Sender::IncSendingCount(int32_t val) {
    mSendingLogGroupCount += val;
}

void Sender::DescSendingCount() {
    mSendingLogGroupCount--;
}

int32_t Sender::GetSendingCount() {
    return mSendingLogGroupCount;
}

void Sender::SetSendingBufferCount(int32_t count) {
    mSendingBufferCount = count;
}

void Sender::AddSendingBufferCount() {
    mSendingBufferCount++;
}

void Sender::SubSendingBufferCount() {
    mSendingBufferCount--;
}

int32_t Sender::GetSendingBufferCount() {
    return mSendingBufferCount;
}

bool Sender::IsFlush() {
    return mFlushLog;
}
void Sender::SetFlush() {
    ReadWriteBarrier();
    mFlushLog = true;
}
void Sender::ResetFlush() {
    ReadWriteBarrier();
    mFlushLog = false;
}

void Sender::SetQueueUrgent() {
    mSenderQueue.SetUrgent();
}


void Sender::ResetQueueUrgent() {
    mSenderQueue.ResetUrgent();
}

LogstoreSenderStatistics Sender::GetSenderStatistics(const LogstoreFeedBackKey& key) {
    return mSenderQueue.GetSenderStatistics(key);
}

void Sender::IncreaseRegionConcurrency(const std::string& region) {
    PTScopedLock lock(mRegionEndpointEntryMapLock);
    auto iter = mRegionEndpointEntryMap.find(region);
    if (mRegionEndpointEntryMap.end() == iter)
        return;

    auto regionInfo = iter->second;
    regionInfo->mContinuousErrorCount = 0;
    if (-1 == regionInfo->mConcurrency)
        return;
    if (++regionInfo->mConcurrency >= AppConfig::GetInstance()->GetSendRequestConcurrency()) {
        LOG_INFO(sLogger, ("Set region concurrency to unlimited", region));
        regionInfo->mConcurrency = -1;
    }
}

void Sender::ResetRegionConcurrency(const std::string& region) {
    PTScopedLock lock(mRegionEndpointEntryMapLock);
    auto iter = mRegionEndpointEntryMap.find(region);
    if (mRegionEndpointEntryMap.end() == iter)
        return;

    auto regionInfo = iter->second;
    if (++regionInfo->mContinuousErrorCount >= INT32_FLAG(reset_region_concurrency_error_count)) {
        auto oldConcurrency = regionInfo->mConcurrency;
        regionInfo->mConcurrency
            = AppConfig::GetInstance()->GetSendRequestConcurrency() / mRegionEndpointEntryMap.size();
        if (regionInfo->mConcurrency <= 0) {
            regionInfo->mConcurrency = 1;
        }
        if (oldConcurrency != regionInfo->mConcurrency) {
            LOG_INFO(sLogger,
                     ("Decrease region concurrency", region)("from", oldConcurrency)("to", regionInfo->mConcurrency));
        }
    }
}

bool Sender::FlushOut(int32_t time_interval_in_mili_seconds) {
    static Aggregator* aggregator = Aggregator::GetInstance();
    SetFlush();
    aggregator->FlushReadyBuffer();
    for (int i = 0; i < time_interval_in_mili_seconds / 100; ++i) {
        mSenderQueue.Signal();
        {
            WaitObject::Lock lock(mWriteSecondaryWait);
            mWriteSecondaryWait.signal();
        }
        if (IsFlush() == false) {
            // double check, fix bug #13758589
            aggregator->FlushReadyBuffer();
            if (aggregator->IsMergeMapEmpty() && IsBatchMapEmpty() && GetSendingCount() == 0
                && IsSecondaryBufferEmpty()) {
                return true;
            } else {
                SetFlush();
                continue;
            }
        }
        aggregator->FlushReadyBuffer();
        usleep(100 * 1000);
    }
    ResetFlush();
    return false;
}
Sender::~Sender() {
    if (!FlushOut(3 * 1000)) {
        LogtailAlarm::GetInstance()->SendAlarm(DISCARD_DATA_ALARM,
                                               "cannot flush data in 3 seconds when destruct Sender, discard data");
        LOG_ERROR(sLogger, ("cannot flush data in 3 seconds when destruct Sender", "discard data"));
    }
    RemoveSender();
    if (mTestNetworkClient) {
        delete mTestNetworkClient;
        mTestNetworkClient = NULL;
    }
}

double Sender::IncTotalSendStatistic(const std::string& projectName, const std::string& logstore, int32_t curTime) {
    return UpdateSendStatistic(projectName + "_" + logstore, curTime, false);
}

double
Sender::IncSendServerErrorStatistic(const std::string& projectName, const std::string& logstore, int32_t curTime) {
    return UpdateSendStatistic(projectName + "_" + logstore, curTime, true);
}

double Sender::UpdateSendStatistic(const std::string& key, int32_t curTime, bool serverError) {
    static int32_t WINDOW_COUNT = 6;
    static int32_t WINDOW_SIZE = 10; //seconds
    static int32_t STATISTIC_CYCLE = WINDOW_SIZE * WINDOW_COUNT;

    int32_t second = curTime % STATISTIC_CYCLE;
    int32_t window = second / WINDOW_SIZE;
    int32_t curBeginTime = curTime - second + window * WINDOW_SIZE;
    {
        PTScopedLock lock(mSendStatisticLock);
        std::unordered_map<std::string, std::vector<SendStatistic*> >::iterator iter = mSendStatisticMap.find(key);
        if (iter == mSendStatisticMap.end()) {
            vector<SendStatistic*> value;
            for (int32_t idx = 0; idx < WINDOW_COUNT; ++idx)
                value.push_back(new SendStatistic(0));
            iter = mSendStatisticMap.insert(iter, std::make_pair(key, value));
        }

        SendStatistic* curWindow = (iter->second)[window];
        if (serverError) {
            if (curWindow->mBeginTime < curBeginTime) {
                curWindow->mBeginTime = curBeginTime;
                curWindow->mServerErrorCount = 1;
                curWindow->mSendCount = 1;
            } else {
                curWindow->mServerErrorCount += 1;
                curWindow->mSendCount += 1;
            }

            int32_t totalCount = 0;
            int32_t serverErrorCount = 0;
            int32_t validBeginTime = curBeginTime - STATISTIC_CYCLE;
            for (int32_t idx = 0; idx < WINDOW_COUNT; ++idx) {
                if (((iter->second)[idx])->mBeginTime > validBeginTime) {
                    totalCount += ((iter->second)[idx])->mSendCount;
                    serverErrorCount += ((iter->second)[idx])->mServerErrorCount;
                }
            }
            return serverErrorCount / (DOUBLE_FLAG(send_server_error_ratio_smoothing_factor) + totalCount);
        } else {
            if (curWindow->mBeginTime < curBeginTime) {
                curWindow->mBeginTime = curBeginTime;
                curWindow->mServerErrorCount = 0;
                curWindow->mSendCount = 1;
            } else
                curWindow->mSendCount += 1;
            return 0;
        }
    }
}

void Sender::CleanTimeoutSendStatistic() {
    PTScopedLock lock(mSendStatisticLock);
    int32_t curTime = time(NULL);
    for (std::unordered_map<std::string, std::vector<SendStatistic*> >::iterator iter = mSendStatisticMap.begin();
         iter != mSendStatisticMap.end();) {
        bool timeout = true;
        for (vector<SendStatistic*>::iterator winIter = iter->second.begin(); winIter != iter->second.end();
             ++winIter) {
            if (curTime - (*winIter)->mBeginTime < INT32_FLAG(send_statistic_entry_timeout)) {
                timeout = false;
                break;
            }
        }
        if (timeout) {
            for (vector<SendStatistic*>::iterator winIter = iter->second.begin(); winIter != iter->second.end();
                 ++winIter)
                delete *winIter;
            iter = mSendStatisticMap.erase(iter);
        } else
            iter++;
    }
}


void Sender::SetLogstoreFlowControl(const LogstoreFeedBackKey& logstoreKey,
                                    int32_t maxSendBytesPerSecond,
                                    int32_t expireTime) {
    mSenderQueue.SetLogstoreFlowControl(logstoreKey, maxSendBytesPerSecond, expireTime);
}


SlsClientInfo::SlsClientInfo(sdk::Client* client, int32_t updateTime) {
    sendClient = client;
    lastUsedTime = updateTime;
}


void Sender::ForceUpdateRealIp(const std::string& region) {
    mRegionRealIpLock.lock();
    RegionRealIpInfoMap::iterator iter = mRegionRealIpMap.find(region);
    if (iter != mRegionRealIpMap.end()) {
        iter->second->mForceFlushFlag = true;
    }
    mRegionRealIpLock.unlock();
}

void Sender::UpdateSendClientRealIp(sdk::Client* client, const std::string& region) {
    string realIp;

    mRegionRealIpLock.lock();
    RealIpInfo* pInfo = NULL;
    RegionRealIpInfoMap::iterator iter = mRegionRealIpMap.find(region);
    if (iter != mRegionRealIpMap.end()) {
        pInfo = iter->second;
    } else {
        pInfo = new RealIpInfo;
        mRegionRealIpMap.insert(std::make_pair(region, pInfo));
    }
#ifdef LOGTAIL_DEBUG_FLAG
    LOG_DEBUG(sLogger,
              ("update real ip", pInfo->mRealIp)("region", region)("real ip update time", pInfo->mLastUpdateTime)(
                  "client update time", client->GetSlsRealIpUpdateTime()));
#endif
    realIp = pInfo->mRealIp;
    mRegionRealIpLock.unlock();

    if (!realIp.empty()) {
        client->SetSlsHost(realIp);
        client->SetSlsRealIpUpdateTime(time(NULL));
    } else if (pInfo->mLastUpdateTime >= client->GetSlsRealIpUpdateTime()) {
        const std::string& defaultEndpoint = GetRegionCurrentEndpoint(region);
        if (!defaultEndpoint.empty()) {
            client->SetSlsHost(defaultEndpoint);
            client->SetSlsRealIpUpdateTime(time(NULL));
        }
    }
}

void Sender::RealIpUpdateThread() {
    int32_t lastUpdateRealIpTime = 0;
    vector<string> regionEndpointArray;
    vector<string> regionArray;
    while (!mStopRealIpThread) {
        int32_t curTime = time(NULL);
        bool updateFlag = curTime - lastUpdateRealIpTime > INT32_FLAG(send_switch_real_ip_interval);
        {
            // check force update
            mRegionRealIpLock.lock();
            RegionRealIpInfoMap::iterator iter = mRegionRealIpMap.begin();
            for (; iter != mRegionRealIpMap.end(); ++iter) {
                if (iter->second->mForceFlushFlag) {
                    iter->second->mForceFlushFlag = false;
                    updateFlag = true;
                    LOG_INFO(sLogger, ("force update real ip", iter->first));
                }
            }
            mRegionRealIpLock.unlock();
        }
        if (updateFlag) {
            LOG_DEBUG(sLogger, ("start update real ip", ""));
            regionEndpointArray.clear();
            regionArray.clear();
            {
                PTScopedLock lock(mRegionEndpointEntryMapLock);
                std::unordered_map<std::string, RegionEndpointEntry*>::iterator iter = mRegionEndpointEntryMap.begin();
                for (; iter != mRegionEndpointEntryMap.end(); ++iter) {
                    regionEndpointArray.push_back((iter->second)->GetCurrentEndpoint());
                    regionArray.push_back(iter->first);
                }
            }
            for (size_t i = 0; i < regionEndpointArray.size(); ++i) {
                // no available endpoint
                if (regionEndpointArray[i].empty()) {
                    continue;
                }

                EndpointStatus status = UpdateRealIp(regionArray[i], regionEndpointArray[i]);
                if (status == STATUS_ERROR) {
                    SetNetworkStat(regionArray[i], regionEndpointArray[i], false);
                }
            }
            lastUpdateRealIpTime = time(NULL);
        }
        usleep(1000 * 1000);
    }
}

EndpointStatus Sender::UpdateRealIp(const std::string& region, const std::string& endpoint) {
    mUpdateRealIpClient->SetSlsHost(endpoint);
    EndpointStatus status = STATUS_ERROR;
    int64_t beginTime = GetCurrentTimeInMicroSeconds();
    try {
        sdk::GetRealIpResponse rsp;
        if (BOOL_FLAG(enable_mock_send) && MockGetRealIp)
            rsp = MockGetRealIp("logtail-test-network-project", "logtail-test-network-logstore");
        else
            rsp = mUpdateRealIpClient->GetRealIp();

        if (!rsp.realIp.empty()) {
            SetRealIp(region, rsp.realIp);
            status = STATUS_OK_WITH_IP;
        } else {
            status = STATUS_OK_WITH_ENDPOINT;
            static int32_t sUpdateRealIpWarningCount = 0;
            if (sUpdateRealIpWarningCount++ % 100 == 0) {
                sUpdateRealIpWarningCount %= 100;
                LOG_WARNING(sLogger,
                            ("get real ip request succeeded but server did not give real ip, region",
                             region)("endpoint", endpoint));
            }

            // we should set real ip to empty string if server did not give real ip
            SetRealIp(region, "");
        }
    }
    // GetRealIp's implement should not throw LOGException, but we catch it to hold implement changing
    catch (sdk::LOGException& ex) {
        const string& errorCode = ex.GetErrorCode();
        LOG_DEBUG(sLogger, ("get real ip", "send fail")("errorCode", errorCode)("errorMessage", ex.GetMessage()));
        SendResult sendRst = ConvertErrorCode(errorCode);
        if (sendRst == SEND_NETWORK_ERROR)
            status = STATUS_ERROR;
    } catch (...) {
        LOG_ERROR(sLogger, ("get real ip", "send fail")("exception", "unknown"));
    }
    int64_t endTime = GetCurrentTimeInMicroSeconds();
    int32_t latency = int32_t((endTime - beginTime) / 1000); //ms
    LOG_DEBUG(sLogger, ("Get real ip, region", region)("endpoint", endpoint)("status", status)("latency", latency));
    return status;
}

void Sender::SetRealIp(const std::string& region, const std::string& ip) {
    PTScopedLock lock(mRegionRealIpLock);
    RealIpInfo* pInfo = NULL;
    RegionRealIpInfoMap::iterator iter = mRegionRealIpMap.find(region);
    if (iter != mRegionRealIpMap.end()) {
        pInfo = iter->second;
    } else {
        pInfo = new RealIpInfo;
        mRegionRealIpMap.insert(std::make_pair(region, pInfo));
    }
    LOG_DEBUG(sLogger, ("set real ip, last", pInfo->mRealIp)("now", ip)("region", region));
    pInfo->SetRealIp(ip);
}

} // namespace logtail
