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

#include <atomic>
#include <fstream>
#include <string>

#include "sls_control/SLSControl.h"
#if defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#endif
#include "app_config/AppConfig.h"
#include "application/Application.h"
#include "common/CompressTools.h"
#include "common/Constants.h"
#include "common/EndpointUtil.h"
#include "common/ErrorUtil.h"
#include "common/ExceptionBase.h"
#include "common/FileEncryption.h"
#include "common/FileSystemUtil.h"
#include "common/HashUtil.h"
#include "common/LogtailCommonFlags.h"
#include "common/RandomUtil.h"
#include "common/RuntimeUtil.h"
#include "common/SlidingWindowCounter.h"
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "config_manager/ConfigManager.h"
#include "fuse/UlogfsHandler.h"
#include "monitor/LogFileProfiler.h"
#include "monitor/LogtailAlarm.h"
#include "monitor/Monitor.h"
#include "processor/daemon/LogProcess.h"
#include "sdk/Client.h"
#include "sdk/Exception.h"
#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#endif
#include "flusher/FlusherSLS.h"
#include "pipeline/PipelineManager.h"
#include "queue/QueueKeyManager.h"
#include "queue/SLSSenderQueueItem.h"
#include "queue/SenderQueueManager.h"
#include "sdk/CurlAsynInstance.h"
#include "sender/PackIdManager.h"
#include "sender/SLSClientManager.h"
#include "common/MemoryBarrier.h"

using namespace std;
using namespace sls_logs;

DEFINE_FLAG_INT32(buffer_check_period, "check logtail local storage buffer period", 60);
DEFINE_FLAG_INT32(quota_exceed_wait_interval, "when daemon buffer thread get quotaExceed error, sleep 5 seconds", 5);
DEFINE_FLAG_INT32(secondary_buffer_count_limit, "data ready for write buffer file", 20);
DEFINE_FLAG_INT32(buffer_file_alive_interval, "the max alive time of a bufferfile, 5 minutes", 300);
DEFINE_FLAG_INT32(write_secondary_wait_timeout, "interval of dump seconary buffer from memory to file, seconds", 2);
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
DEFINE_FLAG_BOOL(dump_reduced_send_result, "for performance test", false);
DEFINE_FLAG_INT32(discard_send_fail_interval, "discard data when send fail after 6 * 3600 seconds", 6 * 3600);
DEFINE_FLAG_BOOL(global_network_success, "global network success flag, default false", false);
DEFINE_FLAG_INT32(reset_region_concurrency_error_count,
                  "reset region concurrency if the number of continuous error exceeds, 5",
                  5);
DEFINE_FLAG_INT32(unknow_error_try_max, "discard data when try times > this value", 5);
static const int SEND_BLOCK_COST_TIME_ALARM_INTERVAL_SECOND = 3;
static const int LOG_GROUP_WAIT_IN_QUEUE_ALARM_INTERVAL_SECOND = 6;
static const int ON_FAIL_LOG_WARNING_INTERVAL_SECOND = 10;
DEFINE_FLAG_INT32(log_expire_time, "log expire time", 24 * 3600);

DECLARE_FLAG_STRING(default_access_key_id);
DECLARE_FLAG_STRING(default_access_key);
DECLARE_FLAG_BOOL(send_prefer_real_ip);

namespace logtail {
const string Sender::BUFFER_FILE_NAME_PREFIX = "logtail_buffer_file_";
const int32_t Sender::BUFFER_META_BASE_SIZE = 65536;

std::atomic_int gNetworkErrorCount{0};

void SendClosure::OnSuccess(sdk::Response* response) {
    BOOL_FLAG(global_network_success) = true;
    Sender::Instance()->SubSendingBufferCount();
    Sender::Instance()->DescSendingCount();

    if (mDataPtr->mFlusher->Name() == "flusher_sls") {
        auto data = static_cast<SLSSenderQueueItem*>(mDataPtr);
        auto flusher = static_cast<const FlusherSLS*>(data->mFlusher);
        string configName = flusher->HasContext() ? flusher->GetContext().GetConfigName() : "";

        if (sLogger->should_log(spdlog::level::debug)) {
            time_t curTime = time(NULL);
            bool isProfileData = Sender::IsProfileData(flusher->mRegion, flusher->mProject, data->mLogstore);
            LOG_DEBUG(
                sLogger,
                ("SendSucess", "OK")("RequestId", response->requestId)("StatusCode", response->statusCode)(
                    "ResponseTime", curTime - data->mLastSendTime)("Region", flusher->mRegion)(
                    "Project", flusher->mProject)("Logstore", data->mLogstore)("Config", configName)(
                    "RetryTimes", data->mSendRetryTimes)("TotalSendCost", curTime - data->mEnqueTime)(
                    "Bytes", data->mData.size())("Endpoint", data->mCurrentEndpoint)("IsProfileData", isProfileData));
        }

        auto& cpt = data->mExactlyOnceCheckpoint;
        if (cpt) {
            cpt->Commit();
            cpt->IncreaseSequenceID();
            LOG_DEBUG(sLogger, ("increase sequence id", cpt->key)("checkpoint", cpt->data.DebugString()));
        }

        // FlusherSLS::sRegionConcurrencyLimiter.PostPop(flusher->mRegion);
        Sender::Instance()->IncTotalSendStatistic(flusher->mProject, data->mLogstore, time(NULL));
    }
    Sender::Instance()->OnSendDone(mDataPtr, GroupSendResult::SendResult_OK); // mDataPtr is released here

    delete this;
}

static const char* GetOperationString(OperationOnFail op) {
    switch (op) {
        case RETRY_ASYNC_WHEN_FAIL:
            return "retry now";
        case RECORD_ERROR_WHEN_FAIL:
            return "retry later";
        case DISCARD_WHEN_FAIL:
        default:
            return "discard data";
    }
}

/*
 * @brief OnFail callback if send failed
 * There are 3 possible outcomes:
 * 1. RETRY_ASYNC_WHEN_FAIL: Resend the item immediately. It will be kept in the sender queue with its mStatus Sending.
 * All RETRY_ASYNC_WHEN_FAIL must fall to RECORD_ERROR_WHEN_FAIL after several retries.
 * 2. RECORD_ERROR_WHEN_FAIL: Resend the item later. It will be kept in the sender queue with its mStatus reseting to
 * Idle. The item will be fetched on the next round when its sender queue is visited. resend later
 * 3. DISCARD_WHEN_FAIL: Won't resend the item and delete it in the sender queue.
 * @param response response from server (maybe empty)
 * @param errorCode defined in sdk/Common.cpp
 * @param errorMessage error message from server
 *
 */
void SendClosure::OnFail(sdk::Response* response, const string& errorCode, const string& errorMessage) {
    if (mDataPtr->mFlusher->Name() == "flusher_sls") {
        auto data = static_cast<SLSSenderQueueItem*>(mDataPtr);
        auto flusher = static_cast<const FlusherSLS*>(data->mFlusher);
        string configName = flusher->HasContext() ? flusher->GetContext().GetConfigName() : "";

        data->mSendRetryTimes++;
        int32_t curTime = time(NULL);
        OperationOnFail operation;
        GroupSendResult recordRst = GroupSendResult::SendResult_OtherFail;
        SendResult sendResult = ConvertErrorCode(errorCode);
        std::ostringstream failDetail, suggestion;
        std::string failEndpoint = data->mCurrentEndpoint;
        if (sendResult == SEND_NETWORK_ERROR || sendResult == SEND_SERVER_ERROR) {
            if (SEND_NETWORK_ERROR == sendResult) {
                gNetworkErrorCount++;
            }

            if (sendResult == SEND_NETWORK_ERROR) {
                failDetail << "network error";
            } else {
                failDetail << "server error";
            }
            suggestion << "check network connection to endpoint";
            if (BOOL_FLAG(send_prefer_real_ip) && data->mRealIpFlag) {
                // connect refused, use vip directly
                failDetail << ", real ip may be stale, force update";
                // just set force update flag
                // mDataPtr->mRealIpFlag = false;
                // Sender::Instance()->ResetSendClientEndpoint(mDataPtr->mAliuid, mDataPtr->mRegion, curTime);
                SLSClientManager::GetInstance()->ForceUpdateRealIp(flusher->mRegion);
            }
            double serverErrorRatio
                = Sender::Instance()->IncSendServerErrorStatistic(flusher->mProject, data->mLogstore, curTime);
            if (serverErrorRatio < DOUBLE_FLAG(send_server_error_retry_ratio)
                && data->mSendRetryTimes < static_cast<uint32_t>(INT32_FLAG(send_retrytimes))) {
                operation = RETRY_ASYNC_WHEN_FAIL;
            } else {
                if (sendResult == SEND_NETWORK_ERROR) {
                    // only set network stat when no real ip
                    if (!BOOL_FLAG(send_prefer_real_ip) || !data->mRealIpFlag) {
                        SLSClientManager::GetInstance()->UpdateEndpointStatus(
                            flusher->mRegion, data->mCurrentEndpoint, false);
                        recordRst = GroupSendResult::SendResult_NetworkFail;
                        if (SLSClientManager::GetInstance()->GetServerSwitchPolicy()
                            == SLSClientManager::EndpointSwitchPolicy::DESIGNATED_FIRST) {
                            SLSClientManager::GetInstance()->ResetClientEndpoint(
                                flusher->mAliuid, flusher->mRegion, curTime);
                        }
                    }
                }
                operation = data->mBufferOrNot ? RECORD_ERROR_WHEN_FAIL : DISCARD_WHEN_FAIL;
            }
            // FlusherSLS::sRegionConcurrencyLimiter.Reset(flusher->mRegion);
        } else if (sendResult == SEND_QUOTA_EXCEED) {
            BOOL_FLAG(global_network_success) = true;
            if (errorCode == sdk::LOGE_SHARD_WRITE_QUOTA_EXCEED) {
                failDetail << "shard write quota exceed";
                suggestion << "Split logstore shards. https://help.aliyun.com/zh/sls/user-guide/expansion-of-resources";
            } else {
                failDetail << "project write quota exceed";
                suggestion << "Submit quota modification request. "
                              "https://help.aliyun.com/zh/sls/user-guide/expansion-of-resources";
            }
            Sender::Instance()->IncTotalSendStatistic(flusher->mProject, data->mLogstore, curTime);
            LogtailAlarm::GetInstance()->SendAlarm(SEND_QUOTA_EXCEED_ALARM,
                                                   "error_code: " + errorCode + ", error_message: " + errorMessage
                                                       + ", request_id:" + response->requestId,
                                                   flusher->mProject,
                                                   data->mLogstore,
                                                   flusher->mRegion);
            operation = RECORD_ERROR_WHEN_FAIL;
            recordRst = GroupSendResult::SendResult_QuotaFail;
        } else if (sendResult == SEND_UNAUTHORIZED) {
            failDetail << "write unauthorized";
            suggestion << "check https connection to endpoint or access keys provided";
            Sender::Instance()->IncTotalSendStatistic(flusher->mProject, data->mLogstore, curTime);
            if (data->mSendRetryTimes > static_cast<uint32_t>(INT32_FLAG(unauthorized_send_retrytimes))) {
                operation = DISCARD_WHEN_FAIL;
            } else {
                BOOL_FLAG(global_network_success) = true;
#ifdef __ENTERPRISE__
                if (flusher->mAliuid.empty() && !EnterpriseConfigProvider::GetInstance()->IsPubRegion()) {
                    operation = RETRY_ASYNC_WHEN_FAIL;
                } else {
#endif
                    int32_t lastUpdateTime;
                    sdk::Client* sendClient
                        = SLSClientManager::GetInstance()->GetClient(flusher->mRegion, flusher->mAliuid);
                    if (SLSControl::GetInstance()->SetSlsSendClientAuth(
                            flusher->mAliuid, false, sendClient, lastUpdateTime))
                        operation = RETRY_ASYNC_WHEN_FAIL;
                    else if (curTime - lastUpdateTime < INT32_FLAG(unauthorized_allowed_delay_after_reset))
                        operation = RECORD_ERROR_WHEN_FAIL;
                    else
                        operation = DISCARD_WHEN_FAIL;
#ifdef __ENTERPRISE__
                }
#endif
            }
        } else if (sendResult == SEND_PARAMETER_INVALID) {
            Sender::Instance()->IncTotalSendStatistic(flusher->mProject, data->mLogstore, curTime);
            failDetail << "invalid paramters";
            suggestion << "check input parameters";
            operation = DefaultOperation();
        } else if (sendResult == SEND_INVALID_SEQUENCE_ID) {
            failDetail << "invalid exactly-once sequence id";
            Sender::Instance()->IncTotalSendStatistic(flusher->mProject, data->mLogstore, curTime);
            do {
                auto& cpt = data->mExactlyOnceCheckpoint;
                if (!cpt) {
                    failDetail << ", unexpected result when exactly once checkpoint is not found";
                    suggestion << "report bug";
                    LogtailAlarm::GetInstance()->SendAlarm(
                        EXACTLY_ONCE_ALARM,
                        "drop exactly once log group because of invalid sequence ID, request id:" + response->requestId,
                        flusher->mProject,
                        data->mLogstore,
                        flusher->mRegion);
                    operation = DISCARD_WHEN_FAIL;
                    break;
                }

                // Because hash key is generated by UUID library, we consider that
                //  the possibility of hash key conflict is very low, so data is
                //  dropped here.
                cpt->Commit();
                failDetail << ", drop exactly once log group and commit checkpoint" << " checkpointKey:" << cpt->key
                           << " checkpoint:" << cpt->data.DebugString();
                suggestion << "no suggestion";
                LogtailAlarm::GetInstance()->SendAlarm(
                    EXACTLY_ONCE_ALARM,
                    "drop exactly once log group because of invalid sequence ID, cpt:" + cpt->key
                        + ", data:" + cpt->data.DebugString() + "request id:" + response->requestId,
                    flusher->mProject,
                    data->mLogstore,
                    flusher->mRegion);
                operation = DISCARD_WHEN_FAIL;
                cpt->IncreaseSequenceID();
            } while (0);
        } else if (AppConfig::GetInstance()->EnableLogTimeAutoAdjust() && sdk::LOGE_REQUEST_TIME_EXPIRED == errorCode) {
            failDetail << "write request expired, will retry";
            suggestion << "check local system time";
            Sender::Instance()->IncTotalSendStatistic(flusher->mProject, data->mLogstore, curTime);
            operation = RETRY_ASYNC_WHEN_FAIL;
        } else {
            failDetail << "other error";
            suggestion << "no suggestion";
            Sender::Instance()->IncTotalSendStatistic(flusher->mProject, data->mLogstore, curTime);
            // when unknown error such as SignatureNotMatch happens, we should retry several times
            // first time, we will retry immediately
            // then we record error and retry latter
            // when retry times > unknow_error_try_max, we will drop this data
            operation = DefaultOperation();
        }
        if (curTime - data->mEnqueTime > INT32_FLAG(discard_send_fail_interval)) {
            operation = DISCARD_WHEN_FAIL;
        }
        bool isProfileData = Sender::IsProfileData(flusher->mRegion, flusher->mProject, data->mLogstore);
        if (isProfileData && data->mSendRetryTimes >= static_cast<uint32_t>(INT32_FLAG(profile_data_send_retrytimes))) {
            operation = DISCARD_WHEN_FAIL;
        }

#define LOG_PATTERN \
    ("SendFail", failDetail.str())("Operation", GetOperationString(operation))("Suggestion", suggestion.str())( \
        "RequestId", response->requestId)("StatusCode", response->statusCode)("ErrorCode", errorCode)( \
        "ErrorMessage", errorMessage)("ResponseTime", curTime - data->mLastSendTime)("Region", flusher->mRegion)( \
        "Project", flusher->mProject)("Logstore", data->mLogstore)("Config", configName)( \
        "RetryTimes", data->mSendRetryTimes)("TotalSendCost", curTime - data->mEnqueTime)( \
        "Bytes", data->mData.size())("Endpoint", data->mCurrentEndpoint)("IsProfileData", isProfileData)

        // Log warning if retry for too long or will discard data
        switch (operation) {
            case RETRY_ASYNC_WHEN_FAIL:
                if (errorCode == sdk::LOGE_REQUEST_TIMEOUT) {
                    // retry on network timeout should be recorded, because this may lead to data duplication
                    LOG_WARNING(sLogger, LOG_PATTERN);
                }
                Sender::Instance()->SendToNetAsync(mDataPtr);
                break;
            case RECORD_ERROR_WHEN_FAIL:
                if (errorCode == sdk::LOGE_REQUEST_TIMEOUT
                    || curTime - data->mLastLogWarningTime > ON_FAIL_LOG_WARNING_INTERVAL_SECOND) {
                    LOG_WARNING(sLogger, LOG_PATTERN);
                    data->mLastLogWarningTime = curTime;
                }
                // Sender::Instance()->PutIntoSecondaryBuffer(mDataPtr, 10);
                Sender::Instance()->SubSendingBufferCount();
                // record error
                Sender::Instance()->OnSendDone(mDataPtr, recordRst);
                Sender::Instance()->DescSendingCount();
                break;
            case DISCARD_WHEN_FAIL:
            default:
                LOG_WARNING(sLogger, LOG_PATTERN);
                if (!isProfileData) {
                    LogtailAlarm::GetInstance()->SendAlarm(SEND_DATA_FAIL_ALARM,
                                                           "errorCode:" + errorCode + ", errorMessage:" + errorMessage
                                                               + ", requestId:" + response->requestId + ", endpoint:"
                                                               + data->mCurrentEndpoint + ", config:" + configName,
                                                           flusher->mProject,
                                                           data->mLogstore,
                                                           flusher->mRegion);
                }
                Sender::Instance()->SubSendingBufferCount();
                // set ok to delete data
                Sender::Instance()->OnSendDone(mDataPtr, GroupSendResult::SendResult_DiscardFail);
                Sender::Instance()->DescSendingCount();
        }
#undef LOG_PATTERN
    } else {
        Sender::Instance()->SubSendingBufferCount();
        Sender::Instance()->OnSendDone(mDataPtr, GroupSendResult::SendResult_DiscardFail);
        Sender::Instance()->DescSendingCount();
    }
    delete this;
}

OperationOnFail SendClosure::DefaultOperation() {
    if (mDataPtr->mSendRetryTimes == 1) {
        return RETRY_ASYNC_WHEN_FAIL;
    } else if (mDataPtr->mSendRetryTimes > static_cast<uint32_t>(INT32_FLAG(unknow_error_try_max))) {
        return DISCARD_WHEN_FAIL;
    } else {
        return RECORD_ERROR_WHEN_FAIL;
    }
}

SendResult ConvertErrorCode(const std::string& errorCode) {
    if (errorCode == sdk::LOGE_REQUEST_ERROR || errorCode == sdk::LOGE_CLIENT_OPERATION_TIMEOUT
        || errorCode == sdk::LOGE_REQUEST_TIMEOUT) {
        return SEND_NETWORK_ERROR;
    } else if (errorCode == sdk::LOGE_SERVER_BUSY || errorCode == sdk::LOGE_INTERNAL_SERVER_ERROR) {
        return SEND_SERVER_ERROR;
    } else if (errorCode == sdk::LOGE_WRITE_QUOTA_EXCEED || errorCode == sdk::LOGE_SHARD_WRITE_QUOTA_EXCEED) {
        return SEND_QUOTA_EXCEED;
    } else if (errorCode == sdk::LOGE_UNAUTHORIZED) {
        return SEND_UNAUTHORIZED;
    } else if (errorCode == sdk::LOGE_INVALID_SEQUENCE_ID) {
        return SEND_INVALID_SEQUENCE_ID;
    } else if (errorCode == sdk::LOGE_PARAMETER_INVALID) {
        return SEND_PARAMETER_INVALID;
    } else {
        return SEND_DISCARD_ERROR;
    }
}

Sender::Sender() : mDefaultRegion(STRING_FLAG(default_region_name)) {
    srand(time(NULL));
    mFlushLog = false;
    SetBufferFilePath(AppConfig::GetInstance()->GetBufferFilePath());
    SetSendingBufferCount(0);
#ifndef APSARA_UNIT_TEST_MAIN
    new Thread(bind(&Sender::DaemonSender, this)); // be careful: this thread will not stop until process exit
    new Thread(bind(&Sender::WriteSecondary, this)); // be careful: this thread will not stop until process exit
#endif
}

Sender* Sender::Instance() {
    static Sender* senderPtr = new Sender();
    return senderPtr;
}

bool Sender::Init(void) {
    SLSControl::GetInstance()->Init();

    SetBufferFilePath(AppConfig::GetInstance()->GetBufferFilePath());
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

void Sender::OnSendDone(SenderQueueItem* mDataPtr, GroupSendResult sendRst) {
    if (sendRst != GroupSendResult::SendResult_OK && sendRst != GroupSendResult::SendResult_Buffered
        && sendRst != GroupSendResult::SendResult_DiscardFail) {
        mDataPtr->mStatus = SendingStatus::IDLE;
        return;
    }
    SenderQueueManager::GetInstance()->RemoveItem(mDataPtr->mQueueKey, mDataPtr);
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
    while ((ent = dir.ReadNext())) {
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
        bufferMeta.set_endpoint(GetDefaultRegion()); // new mode
        bufferMeta.set_aliuid("");
    }
    if (!bufferMeta.has_compresstype()) {
        bufferMeta.set_compresstype(SlsCompressType::SLS_CMP_LZ4);
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
        if (!SLSClientManager::GetInstance()->HasNetworkAvailable()) {
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
                            SEND_COMPRESS_FAIL_ALARM,
                            string("projectName is:" + bufferMeta.project() + ", fileName is:" + filename));
                    } else {
                        bufferMeta.set_logstore(logGroup.category());
                        bufferMeta.set_datatype(int(RawDataType::EVENT_GROUP));
                        bufferMeta.set_rawsize(meta.mLogDataSize);
                        bufferMeta.set_compresstype(sls_logs::SLS_CMP_LZ4);
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
    // mSenderQueue.Signal();
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
        // mSenderQueue.RemoveAll();
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
    vector<SenderQueueItem*> logGroupToDump;
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
            for (vector<SenderQueueItem*>::iterator itr = logGroupToDump.begin(); itr != logGroupToDump.end(); ++itr) {
                SendToBufferFile(*itr);
                delete *itr;
            }
            logGroupToDump.clear();
        }
    }
    LOG_INFO(sLogger, ("DumpSecondaryThread", "exit"));
}

bool Sender::IsBatchMapEmpty() {
    return SenderQueueManager::GetInstance()->IsAllQueueEmpty();
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
    size_t sendNetBodyBytes = 0;
    mLastDaemonRunTime = lastUpdateMetricTime;
    mLastSendDataTime = lastUpdateMetricTime;
    while (true) {
        vector<SenderQueueItem*> logGroupToSend;
        SenderQueueManager::GetInstance()->Wait(1000);

        uint32_t bufferPackageCount = 0;
        bool singleBatchMapFull = false;
        int32_t curTime = time(NULL);
        if (Application::GetInstance()->IsExiting()) {
            SenderQueueManager::GetInstance()->GetAllAvailableItems(logGroupToSend, false);
        } else {
            SenderQueueManager::GetInstance()->GetAllAvailableItems(logGroupToSend);
        }
        mLastDaemonRunTime = curTime;
        IncSendingCount((int32_t)logGroupToSend.size());
        bufferPackageCount = logGroupToSend.size();

        sendBufferCount += logGroupToSend.size();
        if (curTime - lastUpdateMetricTime >= 40) {
            static auto sMonitor = LogtailMonitor::GetInstance();

            sMonitor->UpdateMetric("send_tps", 1.0 * sendBufferCount / (curTime - lastUpdateMetricTime));
            sMonitor->UpdateMetric("send_bytes_ps", 1.0 * sendBufferBytes / (curTime - lastUpdateMetricTime));
            sMonitor->UpdateMetric("send_net_bytes_ps", 1.0 * sendNetBodyBytes / (curTime - lastUpdateMetricTime));
            lastUpdateMetricTime = curTime;
            sendBufferCount = 0;
            sendNetBodyBytes = 0;
            sendBufferBytes = 0;

            // update process queue status
            // int32_t invalidCount = 0;
            // int32_t invalidSenderCount = 0;
            // int32_t totalCount = 0;
            // int32_t eoInvalidCount = 0;
            // int32_t eoInvalidSenderCount = 0;
            // int32_t eoTotalCount = 0;
            // mSenderQueue.GetStatus(
            //     invalidCount, invalidSenderCount, totalCount, eoInvalidCount, eoInvalidSenderCount, eoTotalCount);
            // sMonitor->UpdateMetric("send_queue_full", invalidCount);
            // sMonitor->UpdateMetric("send_queue_total", totalCount);
            // sMonitor->UpdateMetric("sender_invalid", invalidSenderCount);
            // if (eoTotalCount > 0) {
            //     sMonitor->UpdateMetric("eo_send_queue_full", eoInvalidCount);
            //     sMonitor->UpdateMetric("eo_send_queue_total", eoTotalCount);
            //     sMonitor->UpdateMetric("eo_sender_invalid", eoInvalidSenderCount);
            // }

            // Collect at most 15 stats, similar to Linux load 1,5,15.
            static SlidingWindowCounter sNetErrCounter = CreateLoadCounter();
            sMonitor->UpdateMetric("net_err_stat", sNetErrCounter.Add(gNetworkErrorCount.exchange(0)));
        }

        ///////////////////////////////////////
        // smoothing send tps, walk around webserver load burst
        if (!Application::GetInstance()->IsExiting() && AppConfig::GetInstance()->IsSendRandomSleep()) {
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

        for (vector<SenderQueueItem*>::iterator itr = logGroupToSend.begin(); itr != logGroupToSend.end(); ++itr) {
            int32_t logGroupWaitTime = curTime - (*itr)->mEnqueTime;

            if (logGroupWaitTime > LOG_GROUP_WAIT_IN_QUEUE_ALARM_INTERVAL_SECOND) {
                LOG_WARNING(sLogger,
                            ("log group wait in queue for too long, may blocked by concurrency or quota",
                             "")("log group wait time", logGroupWaitTime));
                LogtailAlarm::GetInstance()->SendAlarm(LOG_GROUP_WAIT_TOO_LONG_ALARM,
                                                       "log group wait in queue for too long, log group wait time "
                                                           + ToString(logGroupWaitTime));
            }

            mLastSendDataTime = curTime;
#ifdef __ENTERPRISE__
            if (BOOST_UNLIKELY(EnterpriseConfigProvider::GetInstance()->IsDebugMode())) {
                // DumpDebugFile(*itr);
                // OnSendDone(*itr, GroupSendResult::SendResult_OK);
                // DescSendingCount();
            } else {
#endif
                if (!Application::GetInstance()->IsExiting() && AppConfig::GetInstance()->IsSendFlowControl()) {
                    FlowControl((*itr)->mRawSize, REALTIME_SEND_THREAD);
                }

                int32_t beforeSleepTime = time(NULL);
                while (!Application::GetInstance()->IsExiting()
                       && GetSendingBufferCount() >= AppConfig::GetInstance()->GetSendRequestConcurrency()) {
                    usleep(10 * 1000);
                }
                int32_t afterSleepTime = time(NULL);
                int32_t blockCostTime = afterSleepTime - beforeSleepTime;
                if (blockCostTime > SEND_BLOCK_COST_TIME_ALARM_INTERVAL_SECOND) {
                    LOG_WARNING(sLogger,
                                ("sending log group blocked too long because send concurrency reached limit. current "
                                 "concurrency used",
                                 GetSendingBufferCount())("max concurrency",
                                                          AppConfig::GetInstance()->GetSendRequestConcurrency())(
                                    "blocked time", blockCostTime));
                    LogtailAlarm::GetInstance()->SendAlarm(SENDING_COSTS_TOO_MUCH_TIME_ALARM,
                                                           "sending log group blocked for too much time, cost "
                                                               + ToString(blockCostTime));
                }

                AddSendingBufferCount();
                sendBufferBytes += (*itr)->mRawSize;
                sendNetBodyBytes += (*itr)->mData.size();
                SendToNetAsync(*itr);
#ifdef __ENTERPRISE__
            }
#endif
        }
        logGroupToSend.clear();

        if ((time(NULL) - mLastCheckSendClientTime) > INT32_FLAG(check_send_client_timeout_interval)) {
            SLSClientManager::GetInstance()->CleanTimeoutClient();
            CleanTimeoutSendStatistic();
            PackIdManager::GetInstance()->CleanTimeoutEntry();
            mLastCheckSendClientTime = time(NULL);
        }

        if (IsFlush() && IsBatchMapEmpty() && GetSendingCount() == 0 && IsSecondaryBufferEmpty())
            ResetFlush();
    }
    LOG_INFO(sLogger, ("SendThread", "exit"));
}

void Sender::PutIntoSecondaryBuffer(SenderQueueItem* dataPtr, int32_t retryTimes) {
    auto data = static_cast<SLSSenderQueueItem*>(dataPtr);
    auto flusher = static_cast<const FlusherSLS*>(data->mFlusher);
    int32_t retry = 0;
    bool writeDone = false;
    while (true) {
        ++retry;
        {
            PTScopedLock lock(mSecondaryMutexLock);
            if (Application::GetInstance()->IsExiting()
                || (mSecondaryBuffer.size() < (uint32_t)INT32_FLAG(secondary_buffer_count_limit))) {
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
        LOG_ERROR(
            sLogger,
            ("write to secondary buffer fail", "discard data")("projectName", flusher->mProject)(
                "logstore", data->mLogstore)("RetryTimes", dataPtr->mSendRetryTimes)("bytes", dataPtr->mData.size()));
        LogtailAlarm::GetInstance()->SendAlarm(
            DISCARD_DATA_ALARM, "write to buffer file fail", flusher->mProject, data->mLogstore, flusher->mRegion);
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

bool Sender::SendToBufferFile(SenderQueueItem* dataPtr) {
    auto data = static_cast<SLSSenderQueueItem*>(dataPtr);
    auto flusher = static_cast<const FlusherSLS*>(data->mFlusher);
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
    if (!FileEncryption::GetInstance()->Encrypt(data->mData.c_str(), data->mData.size(), des, desLength)) {
        fclose(fout);
        LOG_ERROR(sLogger, ("encrypt error, project_name", flusher->mProject));
        LogtailAlarm::GetInstance()->SendAlarm(ENCRYPT_DECRYPT_FAIL_ALARM,
                                               string("encrypt error, project_name:" + flusher->mProject));
        return false;
    }

    LogtailBufferMeta bufferMeta;
    bufferMeta.set_project(flusher->mProject);
    bufferMeta.set_endpoint(flusher->mRegion);
    bufferMeta.set_aliuid(flusher->mAliuid);
    bufferMeta.set_logstore(data->mLogstore);
    bufferMeta.set_datatype(int32_t(data->mType));
    bufferMeta.set_rawsize(data->mRawSize);
    bufferMeta.set_shardhashkey(data->mShardHashKey);
    bufferMeta.set_compresstype(ConvertCompressType(flusher->GetCompressType()));
    string encodedInfo;
    bufferMeta.SerializeToString(&encodedInfo);

    EncryptionStateMeta meta;
    int32_t encodedInfoSize = encodedInfo.size();
    meta.mEncodedInfoSize = encodedInfoSize + BUFFER_META_BASE_SIZE;
    meta.mLogDataSize = data->mData.size();
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
    if (ftell(fout) > AppConfig::GetInstance()->GetLocalFileSize())
        CreateNewFile();
    fclose(fout);
    LOG_DEBUG(sLogger, ("write buffer file", bufferFileName));
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

bool Sender::IsProfileData(const string& region, const std::string& project, const std::string& logstore) {
    if ((logstore == "shennong_log_profile" || logstore == "logtail_alarm" || logstore == "logtail_status_profile"
         || logstore == "logtail_suicide_profile")
        && (project == ProfileSender::GetInstance()->GetProfileProjectName(region) || region == ""))
        return true;
    else
        return false;
}

SendResult
Sender::SendBufferFileData(const LogtailBufferMeta& bufferMeta, const std::string& logData, std::string& errorCode) {
    FlowControl(bufferMeta.rawsize(), REPLAY_SEND_THREAD);
    string region = bufferMeta.endpoint();
    if (region.find("http://") == 0) // old buffer file which record the endpoint
        region = SLSClientManager::GetInstance()->GetRegionFromEndpoint(region);

    sdk::Client* sendClient = SLSClientManager::GetInstance()->GetClient(region, bufferMeta.aliuid());
    SendResult sendRes;
    const string& endpoint = sendClient->GetRawSlsHost();
    if (endpoint.empty())
        sendRes = SEND_NETWORK_ERROR;
    else {
        sendRes = SendToNetSync(sendClient, bufferMeta, logData, errorCode);
    }
    if (sendRes == SEND_NETWORK_ERROR) {
        SLSClientManager::GetInstance()->UpdateEndpointStatus(region, endpoint, false);
        SLSClientManager::GetInstance()->ResetClientEndpoint(bufferMeta.aliuid(), region, time(NULL));
        LOG_DEBUG(sLogger,
                  ("SendBufferFileData",
                   "SEND_NETWORK_ERROR")("region", region)("aliuid", bufferMeta.aliuid())("endpoint", endpoint));
    } else if (sendRes == SEND_UNAUTHORIZED) {
        int32_t lastUpdateTime;
        if (SLSControl::GetInstance()->SetSlsSendClientAuth(bufferMeta.aliuid(), false, sendClient, lastUpdateTime))
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
            if (bufferMeta.datatype() == int(RawDataType::EVENT_GROUP)) {
                if (bufferMeta.has_shardhashkey() && !bufferMeta.shardhashkey().empty())
                    sendClient->PostLogStoreLogs(bufferMeta.project(),
                                                 bufferMeta.logstore(),
                                                 bufferMeta.compresstype(),
                                                 logData,
                                                 bufferMeta.rawsize(),
                                                 bufferMeta.shardhashkey());
                else
                    sendClient->PostLogStoreLogs(bufferMeta.project(),
                                                 bufferMeta.logstore(),
                                                 bufferMeta.compresstype(),
                                                 logData,
                                                 bufferMeta.rawsize());
            } else {
                if (bufferMeta.has_shardhashkey() && !bufferMeta.shardhashkey().empty())
                    sendClient->PostLogStoreLogPackageList(bufferMeta.project(),
                                                           bufferMeta.logstore(),
                                                           bufferMeta.compresstype(),
                                                           logData,
                                                           bufferMeta.shardhashkey());
                else
                    sendClient->PostLogStoreLogPackageList(
                        bufferMeta.project(), bufferMeta.logstore(), bufferMeta.compresstype(), logData);
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

void Sender::SendToNetAsync(SenderQueueItem* dataPtr) {
    if (dataPtr->mFlusher->Name() == "flusher_sls") {
        auto& exactlyOnceCpt = static_cast<SLSSenderQueueItem*>(dataPtr)->mExactlyOnceCheckpoint;
        if (!BOOL_FLAG(enable_full_drain_mode)
            && Application::GetInstance()->IsExiting()) // write local file avoid binary update fail
        {
            SubSendingBufferCount();
            if (!exactlyOnceCpt) {
                // explicitly clone the data to avoid dataPtr be destructed by queue
                auto copy = dataPtr->Clone();
                PutIntoSecondaryBuffer(copy, 3);
            } else {
                LOG_INFO(sLogger,
                         ("no need to flush exactly once data to local",
                          exactlyOnceCpt->key)("checkpoint", exactlyOnceCpt->data.DebugString()));
            }
            OnSendDone(dataPtr, GroupSendResult::SendResult_Buffered);
            DescSendingCount();
            return;
        }
    } else {
        if (!BOOL_FLAG(enable_full_drain_mode) && Application::GetInstance()->IsExiting()) {
            SubSendingBufferCount();
            OnSendDone(dataPtr, GroupSendResult::SendResult_DiscardFail);
            DescSendingCount();
            return;
        }
    }
    sdk::AsynRequest* req = dataPtr->mFlusher->BuildRequest(dataPtr);
    dataPtr->mLastSendTime = time(nullptr);
    sdk::CurlAsynInstance::GetInstance()->AddRequest(req);
}

bool Sender::SendInstantly(sls_logs::LogGroup& logGroup,
                           const std::string& aliuid,
                           const std::string& region,
                           const std::string& projectName,
                           const std::string& logstore) {
    // int32_t logSize = (int32_t)logGroup.logs_size();
    // if (logSize == 0)
    //     return true;

    // auto logGroupSize = logGroup.ByteSize();
    // if ((int32_t)logGroupSize > INT32_FLAG(max_send_log_group_size)) {
    //     LOG_ERROR(sLogger,
    //               ("log group size exceed limit. actual size", logGroupSize)("size limit",
    //                                                                          INT32_FLAG(max_send_log_group_size)));
    //     return false;
    // }

    // // no merge
    // time_t curTime = time(NULL);

    // // shardHashKey is empty because log data config is NULL
    // std::string shardHashKey = "";
    // LogstoreFeedBackKey feedBackKey = GenerateLogstoreFeedBackKey(projectName, logstore);

    // std::string oriData;
    // logGroup.SerializeToString(&oriData);
    // filename is empty string
    // SenderQueueItem* data = new SenderQueueItem(projectName,
    //                                             logstore,
    //                                             "",
    //                                             false,
    //                                             aliuid,
    //                                             region,
    //                                             LOGGROUP_COMPRESSED,
    //                                             oriData.size(),
    //                                             curTime,
    //                                             shardHashKey,
    //                                             feedBackKey);

    // if (!CompressData(data->mLogGroupContext.mCompressType, oriData, data->mLogData)) {
    //     LOG_ERROR(sLogger, ("compress data fail", "discard data")("projectName", projectName)("logstore", logstore));
    //     LogtailAlarm::GetInstance()->SendAlarm(
    //         SEND_COMPRESS_FAIL_ALARM, string("lines :") + ToString(logSize), projectName, logstore, region);
    //     delete data;
    //     return false;
    // }

    // PutIntoBatchMap(data);
    return true;
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

bool Sender::FlushOut(int32_t time_interval_in_mili_seconds) {
    SetFlush();
    for (int i = 0; i < time_interval_in_mili_seconds / 100; ++i) {
        // mSenderQueue.Signal();
        {
            WaitObject::Lock lock(mWriteSecondaryWait);
            mWriteSecondaryWait.signal();
        }
        if (!IsFlush()) {
            // double check, fix bug #13758589
            // TODO: this is not necessary, the task of checking whether all data has been flushed should be done in
            // this func, not in Sender thread
            if (IsBatchMapEmpty() && GetSendingCount() == 0 && IsSecondaryBufferEmpty()) {
                return true;
            } else {
                SetFlush();
                continue;
            }
        }
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
    static int32_t WINDOW_SIZE = 10; // seconds
    static int32_t STATISTIC_CYCLE = WINDOW_SIZE * WINDOW_COUNT;

    int32_t second = curTime % STATISTIC_CYCLE;
    int32_t window = second / WINDOW_SIZE;
    int32_t curBeginTime = curTime - second + window * WINDOW_SIZE; // normalize to 10s
    {
        PTScopedLock lock(mSendStatisticLock);
        std::unordered_map<std::string, std::vector<SendStatistic*>>::iterator iter = mSendStatisticMap.find(key);
        if (iter == mSendStatisticMap.end()) {
            std::vector<SendStatistic*> value;
            for (int32_t idx = 0; idx < WINDOW_COUNT; ++idx) {
                value.push_back(new SendStatistic(0));
            }
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
    for (std::unordered_map<std::string, std::vector<SendStatistic*>>::iterator iter = mSendStatisticMap.begin();
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

std::string Sender::GetAllProjects() {
    string result;
    ScopedSpinLock lock(mProjectRefCntMapLock);
    for (auto iter = mProjectRefCntMap.cbegin(); iter != mProjectRefCntMap.cend(); ++iter) {
        result.append(iter->first).append(" ");
    }
    return result;
}

void Sender::IncreaseProjectReferenceCnt(const std::string& project) {
    ScopedSpinLock lock(mProjectRefCntMapLock);
    ++mProjectRefCntMap[project];
}

void Sender::DecreaseProjectReferenceCnt(const std::string& project) {
    ScopedSpinLock lock(mProjectRefCntMapLock);
    auto iter = mProjectRefCntMap.find(project);
    if (iter == mProjectRefCntMap.end()) {
        // should not happen
        return;
    }
    if (--iter->second == 0) {
        mProjectRefCntMap.erase(iter);
    }
}

bool Sender::IsRegionContainingConfig(const std::string& region) const {
    ScopedSpinLock lock(mRegionRefCntMapLock);
    return mRegionRefCntMap.find(region) != mRegionRefCntMap.end();
}

void Sender::IncreaseRegionReferenceCnt(const std::string& region) {
    ScopedSpinLock lock(mRegionRefCntMapLock);
    ++mRegionRefCntMap[region];
}

void Sender::DecreaseRegionReferenceCnt(const std::string& region) {
    ScopedSpinLock lock(mRegionRefCntMapLock);
    auto iter = mRegionRefCntMap.find(region);
    if (iter == mRegionRefCntMap.end()) {
        // should not happen
        return;
    }
    if (--iter->second == 0) {
        mRegionRefCntMap.erase(iter);
    }
}

void Sender::UpdateRegionStatus(const string& region, bool status) {
    LOG_DEBUG(sLogger, ("update region status, region", region)("is network in good condition", ToString(status)));
    ScopedSpinLock lock(mRegionStatusLock);
    mAllRegionStatus[region] = status;
}

bool Sender::GetRegionStatus(const string& region) {
    ScopedSpinLock lock(mRegionStatusLock);

    decltype(mAllRegionStatus.begin()) rst = mAllRegionStatus.find(region);
    if (rst == mAllRegionStatus.end()) {
        // if no region status, return true
        return true;
    } else {
        return rst->second;
    }
}

const string& Sender::GetDefaultRegion() const {
    ScopedSpinLock lock(mDefaultRegionLock);
    return mDefaultRegion;
}

void Sender::SetDefaultRegion(const string& region) {
    ScopedSpinLock lock(mDefaultRegionLock);
    mDefaultRegion = region;
}

} // namespace logtail
