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

#include "LogstoreSenderQueue.h"
#include "common/Flags.h"
#include "common/StringTools.h"
#include "profiler/LogtailAlarm.h"


DEFINE_FLAG_INT32(max_client_send_error_count, "set disabled if last send is all failed", 60);
DEFINE_FLAG_INT32(client_disable_send_retry_interval, "disabled client can retry to send after seconds", 300);
DEFINE_FLAG_INT32(client_disable_send_retry_interval_max,
                  "max : disabled client can retry to send after seconds",
                  3600);
DEFINE_FLAG_DOUBLE(client_disable_send_retry_interval_scale, "", 1.5);
DEFINE_FLAG_INT32(max_client_quota_exceed_count, "set disabled if last send is all quota exceed", 1);
DEFINE_FLAG_INT32(client_quota_send_retry_interval, "quota exceed client can retry to send after seconds", 3);
DEFINE_FLAG_INT32(client_quota_send_retry_interval_max,
                  "max : quota exceed client can retry to send after seconds",
                  60);
DEFINE_FLAG_INT32(client_quota_send_concurrency_min, "quota exceed client's send concurrency after recover", 1);
DEFINE_FLAG_INT32(client_send_concurrency_trigger,
                  "trigger send if now consurrency <= this value (client_send_concurrency_max/2)",
                  256);
DEFINE_FLAG_INT32(client_send_concurrency_max, "max concurrency of one client", 512);
DEFINE_FLAG_INT32(client_send_concurrency_max_update_time, "max update time seconds", 300);
DEFINE_FLAG_DOUBLE(client_quota_send_retry_interval_scale, "", 2.0);

namespace logtail {

LogstoreSenderInfo::LogstoreSenderInfo()
    : mLastNetworkErrorCount(0),
      mLastQuotaExceedCount(0),
      mLastNetworkErrorTime(0),
      mLastQuotaExceedTime(0),
      mNetworkRetryInterval((double)INT32_FLAG(client_disable_send_retry_interval)),
      mQuotaRetryInterval((double)INT32_FLAG(client_quota_send_retry_interval)),
      mNetworkValidFlag(true),
      mQuotaValidFlag(true),
      mSendConcurrency(INT32_FLAG(client_send_concurrency_max)) {
    mSendConcurrencyUpdateTime = time(NULL);
}

bool LogstoreSenderInfo::CanSend(int32_t curTime) {
    if (!mNetworkValidFlag) {
        if (curTime - mLastNetworkErrorTime >= (int32_t)mNetworkRetryInterval) {
            LOG_INFO(sLogger,
                     ("Network fail timeout,  try send data again",
                      this->mRegion)("last error", mLastNetworkErrorTime)("retry interval", mNetworkRetryInterval));
            mNetworkRetryInterval *= DOUBLE_FLAG(client_disable_send_retry_interval_scale);
            if (mNetworkRetryInterval > (double)INT32_FLAG(client_disable_send_retry_interval_max)) {
                mNetworkRetryInterval = (double)INT32_FLAG(client_disable_send_retry_interval_max);
            }

            // set lastErrorTime to curTime, make sure client can only try once between client_disable_send_retry_interval seconds
            mLastNetworkErrorTime = curTime;
            // can try INT32_FLAG(max_client_send_error_count) times
            mLastNetworkErrorCount = 0;
            mNetworkValidFlag = true;
        } else {
            return false;
        }
    } else if (!mQuotaValidFlag) {
        if (curTime - mLastQuotaExceedTime >= (int32_t)mQuotaRetryInterval) {
            LOG_INFO(sLogger,
                     ("Quota fail timeout,  try send data again",
                      this->mRegion)("last error", mLastQuotaExceedTime)("retry interval", mQuotaRetryInterval));
            mQuotaRetryInterval *= DOUBLE_FLAG(client_quota_send_retry_interval_scale);
            if (mQuotaRetryInterval > (double)INT32_FLAG(client_quota_send_retry_interval_max)) {
                mQuotaRetryInterval = (double)INT32_FLAG(client_quota_send_retry_interval_max);
            }

            mLastQuotaExceedTime = curTime;
            mLastQuotaExceedCount = 0;
            mQuotaValidFlag = true;
        } else {
            return false;
        }
    }
    return true;
}

bool LogstoreSenderInfo::RecordSendResult(SendResult rst, LogstoreSenderStatistics& statisticsItem) {
    switch (rst) {
        case LogstoreSenderInfo::SendResult_OK:
            ++statisticsItem.mSendSuccessCount;
            mLastNetworkErrorCount = 0;
            mLastQuotaExceedCount = 0;
            mNetworkValidFlag = true;
            mQuotaValidFlag = true;
            mNetworkRetryInterval = (double)INT32_FLAG(client_disable_send_retry_interval);
            mQuotaRetryInterval = (double)INT32_FLAG(client_quota_send_retry_interval) + rand() % 5;
            mSendConcurrency += 2;
            if (mSendConcurrency > INT32_FLAG(client_send_concurrency_max)) {
                mSendConcurrency = INT32_FLAG(client_send_concurrency_max);
            } else {
                mSendConcurrencyUpdateTime = time(NULL);
            }
            // return true only if concurrency < client_send_concurrency_trigger
            return mSendConcurrency < INT32_FLAG(client_send_concurrency_trigger);
            break;
        case LogstoreSenderInfo::SendResult_DiscardFail:
            ++statisticsItem.mSendDiscardErrorCount;
            mLastNetworkErrorCount = 0;
            mLastQuotaExceedCount = 0;
            mNetworkValidFlag = true;
            mQuotaValidFlag = true;
            mNetworkRetryInterval = (double)INT32_FLAG(client_disable_send_retry_interval);
            mQuotaRetryInterval = (double)INT32_FLAG(client_quota_send_retry_interval) + rand() % 5;
            // only if send success or discard, mSendConcurrency inc 2
            mSendConcurrency += 2;
            if (mSendConcurrency > INT32_FLAG(client_send_concurrency_max)) {
                mSendConcurrency = INT32_FLAG(client_send_concurrency_max);
            } else {
                mSendConcurrencyUpdateTime = time(NULL);
            }
            // return true only if concurrency < client_send_concurrency_trigger
            return mSendConcurrency < INT32_FLAG(client_send_concurrency_trigger);
            break;
        case LogstoreSenderInfo::SendResult_NetworkFail:
            ++statisticsItem.mSendNetWorkErrorCount;
            mLastNetworkErrorTime = time(NULL);
            // only if send success or discard, mSendConcurrency inc 2
            ++mSendConcurrency;
            if (mSendConcurrency > INT32_FLAG(client_send_concurrency_max)) {
                mSendConcurrency = INT32_FLAG(client_send_concurrency_max);
            } else {
                mSendConcurrencyUpdateTime = mLastNetworkErrorTime;
            }
            if (++mLastNetworkErrorCount >= INT32_FLAG(max_client_send_error_count)) {
                mLastNetworkErrorCount = INT32_FLAG(max_client_send_error_count);
                mNetworkValidFlag = false;
                LOG_WARNING(sLogger,
                            ("Network fail, disable ", this->mRegion)("retry interval", mNetworkRetryInterval));
            }
            break;
        case LogstoreSenderInfo::SendResult_QuotaFail:
            ++statisticsItem.mSendQuotaErrorCount;
            mLastQuotaExceedTime = time(NULL);
            // if quota fail, set mSendConcurrency to min
            mSendConcurrency = INT32_FLAG(client_quota_send_concurrency_min);
            mSendConcurrencyUpdateTime = mLastQuotaExceedTime;
            if (++mLastQuotaExceedCount >= INT32_FLAG(max_client_quota_exceed_count)) {
                mLastQuotaExceedCount = INT32_FLAG(max_client_quota_exceed_count);
                mQuotaValidFlag = false;
                LOG_WARNING(sLogger, ("QuotaF fail, disable ", this->mRegion)("retry interval", mQuotaRetryInterval));
            }
            break;
        default:
            // only if send success or discard, mSendConcurrency inc 2
            ++mSendConcurrency;
            if (mSendConcurrency > INT32_FLAG(client_send_concurrency_max)) {
                mSendConcurrency = INT32_FLAG(client_send_concurrency_max);
            } else {
                mSendConcurrencyUpdateTime = time(NULL);
            }
            break;
    }
    return false;
}

bool LogstoreSenderInfo::OnRegionRecover(const std::string& region) {
    if (region == mRegion) {
        bool rst = !(mNetworkValidFlag && mQuotaValidFlag);
        LogstoreSenderStatistics senderStatistics;
        RecordSendResult(LogstoreSenderInfo::SendResult_OK, senderStatistics);
        LOG_DEBUG(sLogger, ("On region recover  ", this->mRegion));
        return rst;
    }
    return false;
}


void LogstoreSenderInfo::SetRegion(const std::string& region) {
    if (!mRegion.empty()) {
        return;
    }
    mRegion = region;
}

bool LogstoreSenderInfo::ConcurrencyValid() {
    if (mSendConcurrency <= 0) {
        // check consurrency update time
        int32_t nowTime = time(NULL);
        if (nowTime - mSendConcurrencyUpdateTime > INT32_FLAG(client_send_concurrency_max_update_time)) {
            mSendConcurrency = INT32_FLAG(client_quota_send_concurrency_min);
            mSendConcurrencyUpdateTime = time(NULL);
        }
    }
    return mSendConcurrency > 0;
}

LogstoreSenderStatistics::LogstoreSenderStatistics() {
    Reset();
}

void LogstoreSenderStatistics::Reset() {
    mMaxUnsendTime = 0;
    mMinUnsendTime = 0;
    mMaxSendSuccessTime = 0;
    mSendQueueSize = 0;
    mSendNetWorkErrorCount = 0;
    mSendQuotaErrorCount = 0;
    mSendDiscardErrorCount = 0;
    mSendSuccessCount = 0;
    mSendBlockFlag = false;
    mValidToSendFlag = false;
}

} // namespace logtail
