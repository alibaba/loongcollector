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

#include <rapidjson/document.h>

#include "app_config/AppConfig.h"
#include "common/ErrorUtil.h"
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "logger/Logger.h"
#include "plugin/flusher/sls/SLSConstant.h"
#include "Exception.h"

using namespace std;
using namespace logtail::sdk;

namespace logtail {

void ExtractJsonResult(const string& response, rapidjson::Document& document) {
    document.Parse(response.c_str());
    if (document.HasParseError()) {
        throw JsonException("ParseException", "Fail to parse from json string");
    }
}

void JsonMemberCheck(const rapidjson::Value& value, const char* name) {
    if (!value.IsObject()) {
        throw JsonException("InvalidObjectException", "response is not valid JSON object");
    }
    if (!value.HasMember(name)) {
        throw JsonException("NoMemberException", string("Member ") + name + " does not exist");
    }
}

void ExtractJsonResult(const rapidjson::Value& value, const char* name, string& dst) {
    JsonMemberCheck(value, name);
    if (value[name].IsString()) {
        dst = value[name].GetString();
    } else {
        throw JsonException("ValueTypeException", string("Member ") + name + " is not string type");
    }
}

void ErrorCheck(const string& response, const string& requestId, const int32_t httpCode) {
    rapidjson::Document document;
    try {
        ExtractJsonResult(response, document);

        string errorCode;
        ExtractJsonResult(document, LOG_ERROR_CODE.c_str(), errorCode);

        string errorMessage;
        ExtractJsonResult(document, LOG_ERROR_MESSAGE.c_str(), errorMessage);

        throw LOGException(errorCode, errorMessage, requestId, httpCode);
    } catch (JsonException& e) {
        if (httpCode >= 500) {
            throw LOGException(LOGE_INTERNAL_SERVER_ERROR, response, requestId, httpCode);
        } else {
            throw LOGException(LOGE_BAD_RESPONSE, string("Unextractable error:") + response, requestId, httpCode);
        }
    }
}

bool SLSResponse::Parse(const HttpResponse& response) {
    const auto iter = response.GetHeader().find(X_LOG_REQUEST_ID);
    if (iter != response.GetHeader().end()) {
        mRequestId = iter->second;
    }
    mStatusCode = response.GetStatusCode();

    if (mStatusCode == 0) {
        mErrorCode = LOGE_REQUEST_TIMEOUT;
        mErrorMsg = "Request timeout";
    } else if (mStatusCode != 200) {
        try {
            ErrorCheck(*response.GetBody<string>(), mRequestId, response.GetStatusCode());
        } catch (sdk::LOGException& e) {
            mErrorCode = e.GetErrorCode();
            mErrorMsg = e.GetMessage_();
        }
    }
    return true;
}

SLSResponse ParseHttpResponse(const HttpResponse& response) {
    SLSResponse slsResponse;
    if (AppConfig::GetInstance()->IsResponseVerificationEnabled() && !IsSLSResponse(response)) {
        slsResponse.mStatusCode = 0;
        slsResponse.mErrorCode = LOGE_REQUEST_ERROR;
        slsResponse.mErrorMsg = "invalid response body";
    } else {
        slsResponse.Parse(response);

        if (AppConfig::GetInstance()->EnableLogTimeAutoAdjust()) {
            static uint32_t sCount = 0;
            if (sCount++ % 10000 == 0 || slsResponse.mErrorCode == LOGE_REQUEST_TIME_EXPIRED) {
                time_t serverTime = GetServerTime(response);
                if (serverTime > 0) {
                    UpdateTimeDelta(serverTime);
                }
            }
        }
    }
    return slsResponse;
}

bool IsSLSResponse(const HttpResponse& response) {
    const auto iter = response.GetHeader().find(X_LOG_REQUEST_ID);
    if (iter == response.GetHeader().end()) {
        return false;
    }
    return !iter->second.empty();
}

time_t GetServerTime(const HttpResponse& response) {
#define METHOD_LOG_PATTERN ("method", "GetServerTimeFromHeader")
    // Priority: x-log-time -> Date
    string errMsg;
    try {
        const auto iter = response.GetHeader().find("x-log-time");
        if (iter != response.GetHeader().end()) {
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

    const auto iter = response.GetHeader().find("Date");
    if (iter == response.GetHeader().end()) {
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
