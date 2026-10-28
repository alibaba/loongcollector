// Copyright 2025 iLogtail Authors
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

#include "ebpf/type/NetworkObserverEvent.h"

#include "models/LogEvent.h"
#include "models/SpanEvent.h"

namespace logtail::ebpf {

// HttpRecord

void HttpRecord::FillProtocolSpecificLogFields(LogEvent* logEvent) {
    logEvent->SetContent(kHTTPMethod.LogKey(), mHttpMethod);
    logEvent->SetContent(kHTTPPath.LogKey(), mRealPath.size() ? mRealPath : mPath);
    logEvent->SetContent(kHTTPVersion.LogKey(), mProtocolVersion);
    logEvent->SetContent(kStatusCode.LogKey(), std::to_string(mCode));
    logEvent->SetContent(kHTTPReqBody.LogKey(), mReqBody);
    logEvent->SetContent(kHTTPRespBody.LogKey(), mRespBody);
}

void HttpRecord::FillProtocolSpecificSpanFields(SpanEvent* spanEvent, bool isServer) {
    spanEvent->SetTag(kRpc.SpanKey(), GetConvSpanName());
    if (!isServer) {
        spanEvent->SetTag(kEndpoint.SpanKey(), GetConvSpanName());
    }
    spanEvent->SetTag(kHTTPReqBody.SpanKey(), mReqBody);
    spanEvent->SetTag(kHTTPRespBody.SpanKey(), mRespBody);
    spanEvent->SetTag(kHTTPReqBodySize.SpanKey(), std::to_string(mReqBodySize));
    spanEvent->SetTag(kHTTPRespBodySize.SpanKey(), std::to_string(mRespBodySize));
    spanEvent->SetTag(kHTTPVersion.SpanKey(), mProtocolVersion);
}

// MysqlRecord

void MysqlRecord::FillProtocolSpecificLogFields(LogEvent* logEvent) {
    logEvent->SetContent(kDBSystemName.LogKey(), "mysql");
    logEvent->SetContent(kDBResponseStatusCode.LogKey(), std::to_string(mCode));
    logEvent->SetContent(kDBStatement.LogKey(), mSql);
}

void MysqlRecord::FillProtocolSpecificSpanFields(SpanEvent* spanEvent, [[maybe_unused]] bool isServer) {
    spanEvent->SetTag(kDBSystemName.SpanKey(), "mysql");
    spanEvent->SetTag(kDBResponseStatusCode.SpanKey(), std::to_string(mCode));
    spanEvent->SetTag(kDBStatement.SpanKey(), mSql);
}

// RedisRecord

void RedisRecord::FillProtocolSpecificLogFields(LogEvent* logEvent) {
    logEvent->SetContent(kDBSystemName.LogKey(), "redis");
    logEvent->SetContent(kDBResponseStatusCode.LogKey(), std::to_string(mCode));
    logEvent->SetContent(kDBStatement.LogKey(), mSql);
    if (!mErrorMsg.empty()) {
        logEvent->SetContent(kDBResponseMessage.LogKey(), mErrorMsg);
    }
}

void RedisRecord::FillProtocolSpecificSpanFields(SpanEvent* spanEvent, [[maybe_unused]] bool isServer) {
    spanEvent->SetTag(kDBSystemName.SpanKey(), "redis");
    spanEvent->SetTag(kDBResponseStatusCode.SpanKey(), std::to_string(mCode));
    spanEvent->SetTag(kDBStatement.SpanKey(), mSql);
    if (!mErrorMsg.empty()) {
        spanEvent->SetTag(kDBResponseMessage.SpanKey(), mErrorMsg);
    }
}

} // namespace logtail::ebpf
