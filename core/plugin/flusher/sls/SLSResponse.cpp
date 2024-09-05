// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "plugin/flusher/sls/SLSResponse.h"

#include "common/ErrorUtil.h"
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "logger/Logger.h"
#include "sdk/Common.h"
#include "sdk/Exception.h"
#include "sdk/Result.h"

using namespace std;

namespace logtail {

bool SLSResponse::Parse(const HttpResponse& response) {
    const auto iter = response.mHeader.find(sdk::X_LOG_REQUEST_ID);
    if (iter != response.mHeader.end()) {
        mRequestId = iter->second;
    }
    mStatusCode = response.mStatusCode;

    if (mStatusCode == 0) {
        mErrorCode = sdk::LOG_REQUEST_TIMEOUT;
        mErrorMsg = "Request timeout";
    } else if (mStatusCode != 200) {
        try {
            sdk::ErrorCheck(response.mBody, mRequestId, response.mStatusCode);
        } catch (sdk::LOGException& e) {
            mErrorCode = e.GetErrorCode();
            mErrorMsg = e.GetMessage_();
        }
    }
    return true;
}

bool IsSLSResponse(const HttpResponse& response) {
    const auto iter = response.mHeader.find(sdk::X_LOG_REQUEST_ID);
    if (iter == response.mHeader.end()) {
        return false;
    }
    return !iter->second.empty();
}

time_t GetServerTime(const HttpResponse& response) {
#define METHOD_LOG_PATTERN ("method", "GetServerTimeFromHeader")
    // Priority: x-log-time -> Date
    string errMsg;
    try {
        const auto iter = response.mHeader.find("x-log-time");
        if (iter != response.mHeader.end()) {
            return StringTo<time_t>(iter->second);
        }
    } catch (const exception& e) {
        errMsg = e.what();
    } catch (...) {
        errMsg = "unknown";
    }
    if (!errMsg.empty()) {
        LOG_ERROR(sLogger, METHOD_LOG_PATTERN("parse x-log-time error", errMsg));
    }

    const auto iter = response.mHeader.find("Date");
    if (iter == response.mHeader.end()) {
        LOG_WARNING(sLogger, METHOD_LOG_PATTERN("no Date in HTTP header", ""));
        return 0;
    }
    // Date sample: Thu, 18 Feb 2021 03:09:29 GMT
    LogtailTime ts;
    int nanosecondLength;
    if (Strptime(iter->second.c_str(), "%a, %d %b %Y %H:%M:%S", &ts, nanosecondLength) == NULL) {
        LOG_ERROR(sLogger, METHOD_LOG_PATTERN("parse Date error", ErrnoToString(GetErrno()))("value", iter->second));
        return 0;
    }
    time_t serverTime = ts.tv_sec;
    static auto sOffset = GetLocalTimeZoneOffsetSecond();
    return serverTime + sOffset;
#undef METHOD_LOG_PATTERN
}

} // namespace logtail
