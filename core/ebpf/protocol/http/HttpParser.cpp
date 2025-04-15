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

#include "HttpParser.h"

#include <map>

#include "common/StringTools.h"
#include "ebpf/type/NetworkObserverEvent.h"
#include "ebpf/util/TraceId.h"
#include "logger/Logger.h"

namespace logtail::ebpf {

inline constexpr char kContentLength[] = "Content-Length";
inline constexpr char kTransferEncoding[] = "Transfer-Encoding";
inline constexpr char kUpgrade[] = "Upgrade";

std::vector<std::shared_ptr<AbstractRecord>> HTTPProtocolParser::Parse(struct conn_data_event_t* dataEvent,
                                                                       const std::shared_ptr<Connection>& conn,
                                                                       const std::shared_ptr<Sampler>& sampler) {
    auto record = std::make_shared<HttpRecord>(conn);
    record->SetEndTsNs(dataEvent->end_ts);
    record->SetStartTsNs(dataEvent->start_ts);
    auto spanId = GenerateSpanID();
    // slow request
    if (record->GetLatencyMs() > 500 || sampler->ShouldSample(spanId)) {
        record->MarkSample();
    }

    // ParseResponse may set SAMPLE flag, depending on HTTP status code ...
    if (dataEvent->response_len > 0) {
        std::string_view buf(dataEvent->msg + dataEvent->request_len, dataEvent->response_len);
        ParseState state = http::ParseResponse(buf, record, true, false);
        if (state != ParseState::kSuccess) {
            LOG_DEBUG(sLogger, ("[HTTPProtocolParser]: Parse HTTP response failed", int(state)));
            return {};
        }
    }

    if (dataEvent->request_len > 0) {
        std::string_view buf(dataEvent->msg, dataEvent->request_len);
        ParseState state = http::ParseRequest(buf, record, false);
        if (state != ParseState::kSuccess) {
            LOG_DEBUG(sLogger, ("[HTTPProtocolParser]: Parse HTTP request failed", int(state)));
            return {};
        }
    }

    if (record->ShouldSample()) {
        record->SetSpanId(std::move(spanId));
        record->SetTraceId(GenerateTraceID());
    }

    return {record};
}

namespace http {
HeadersMap GetHTTPHeadersMap(const phr_header* headers, size_t numHeaders) {
    HeadersMap result;
    for (size_t i = 0; i < numHeaders; i++) {
        std::string name(headers[i].name, headers[i].name_len);
        std::string value(headers[i].value, headers[i].value_len);
        result.emplace(std::move(name), std::move(value));
    }
    return result;
}

int ParseHttpRequest(std::string_view& buf, HTTPRequest& result) {
    return phr_parse_request(buf.data(),
                             buf.size(),
                             &result.mMethod,
                             &result.mMethodLen,
                             &result.mPath,
                             &result.mPathLen,
                             &result.mMinorVersion,
                             result.mHeaders,
                             &result.mNumHeaders,
                             /*last_len*/ 0);
}

const std::string kRootPath = "/";
const char kQuestionMark = '?';
const std::string kHttP1Prefix = "http1.";

ParseState ParseRequest(std::string_view& buf, std::shared_ptr<HttpRecord>& result, bool forceSample) {
    HTTPRequest req;
    int retval = http::ParseHttpRequest(buf, req);
    if (retval >= 0) {
        buf.remove_prefix(retval);

        auto orginPath = std::string(req.mPath, req.mPathLen);
        auto trimPath = TrimString(orginPath);
        std::size_t pos = trimPath.find(kQuestionMark);

        if (trimPath.empty() || (pos != std::string::npos && pos == 0)) {
            result->SetPath(kRootPath);
            result->SetRealPath(kRootPath);
        } else if (pos != std::string::npos) {
            result->SetPath(trimPath.substr(0, pos));
        } else {
            result->SetPath(trimPath);
            result->SetRealPath(trimPath);
        }

        if (result->ShouldSample() || forceSample) {
            result->SetProtocolVersion(kHttP1Prefix + std::to_string(req.mMinorVersion));
            result->SetMethod(std::string(req.mMethod, req.mMethodLen));
            result->SetReqHeaderMap(http::GetHTTPHeadersMap(req.mHeaders, req.mNumHeaders));
            return ParseRequestBody(buf, result);
        }
        return ParseState::kSuccess;
    }
    if (retval == -2) {
        return ParseState::kNeedsMoreData;
    }

    return ParseState::kInvalid;
}

ParseState PicoParseChunked(std::string_view& data, size_t bodySizeLimitBytes, std::string& result, size_t& bodySize) {
    // Make a copy of the data because phr_decode_chunked mutates the input,
    // and if the original parse fails due to a lack of data, we need the original
    // state to be preserved.
    std::string dataCopy(data);

    phr_chunked_decoder chunkDecoder = {};
    chunkDecoder.consume_trailer = 1;
    char* buf = dataCopy.data();
    size_t bufSize = dataCopy.size();
    ssize_t retval = phr_decode_chunked(&chunkDecoder, buf, &bufSize);

    if (retval == -1) {
        // Parse failed.
        return ParseState::kInvalid;
    }
    if (retval == -2) {
        // Incomplete message.
        return ParseState::kNeedsMoreData;
    }
    if (retval >= 0) {
        // Found a complete message.
        dataCopy.resize(std::min(bufSize, bodySizeLimitBytes));
        // data_copy.resize(buf_size);
        dataCopy.shrink_to_fit();
        result = std::move(dataCopy);
        bodySize = bufSize;

        // phr_decode_chunked rewrites the buffer in place, removing chunked-encoding headers.
        // So we cannot simply remove the prefix, but rather have to shorten the buffer too.
        // This is done via retval, which specifies how many unprocessed bytes are left.
        data.remove_prefix(data.size() - retval);

        return ParseState::kSuccess;
    }

    return ParseState::kUnknown;
}


ParseState ParseChunked(std::string_view& data, size_t bodySizeLimitBytes, std::string& result, size_t& bodySize) {
    return PicoParseChunked(data, bodySizeLimitBytes, result, bodySize);
}

ParseState ParseRequestBody(std::string_view& buf, std::shared_ptr<HttpRecord>& result) {
    // Case 1: Content-Length
    const auto contentLengthIter = result->GetReqHeaderMap().find(kContentLength);
    if (contentLengthIter != result->GetReqHeaderMap().end()) {
        std::string_view contentLenStr = contentLengthIter->second;
        auto r = ParseContent(contentLenStr, buf, 256, result->mReqBody, result->mReqBodySize);
        return r;
    }

    // Case 2: Chunked transfer.
    const auto transferEncodingIter = result->GetReqHeaderMap().find(kTransferEncoding);
    if (transferEncodingIter != result->GetReqHeaderMap().end() && transferEncodingIter->second == "chunked") {
        auto s = ParseChunked(buf, 256, result->mReqBody, result->mReqBodySize);

        return s;
    }

    // Case 3: Message has no Content-Length or Transfer-Encoding.
    // An HTTP request with no Content-Length and no Transfer-Encoding should not have a body when
    // no Content-Length or Transfer-Encoding is set:
    // "A user agent SHOULD NOT send a Content-Length header field when the request message does
    // not contain a payload body and the method semantics do not anticipate such a body."
    //
    // We apply this to all methods, since we have no better strategy in other cases.
    result->mReqBody = "";
    return ParseState::kSuccess;
}


int ParseHttpResponse(std::string_view buf, HTTPResponse* result) {
    return phr_parse_response(buf.data(),
                              buf.size(),
                              &result->mMinorVersion,
                              &result->mStatus,
                              &result->mMsg,
                              &result->mMsgLen,
                              result->mHeaders,
                              &result->mNumHeaders,
                              /*last_len*/ 0);
}

bool ParseContentLength(const std::string_view& contentLenStr, size_t* len) {
    if (len == nullptr) {
        return false;
    }

    try {
        size_t pos;
        std::stoull(contentLenStr.data());
        *len = std::stoull(std::string(contentLenStr), &pos);
        if (pos != contentLenStr.size()) {
            return false;
        }
    } catch (const std::exception& e) {
        return false;
    }

    return true;
}

ParseState ParseContent(std::string_view& contentLenStr,
                        std::string_view& data,
                        size_t bodySizeLimitBytes,
                        std::string& result,
                        size_t& bodySize) {
    size_t len;
    if (!ParseContentLength(contentLenStr, &len)) {
        return ParseState::kInvalid;
    }
    if (data.size() < len) {
        return ParseState::kNeedsMoreData;
    }

    result = data.substr(0, std::min(len, bodySizeLimitBytes));
    // *result = data->substr(0, len);

    bodySize = len;
    data.remove_prefix(std::min(len, data.size()));
    return ParseState::kSuccess;
}

bool StartsWithHttp(const std::string_view& buf) {
    if (buf.empty()) {
        return false;
    }
    static const std::string_view kPrefix = "HTTP";
    return buf.size() >= kPrefix.size() && buf.substr(0, kPrefix.size()) == kPrefix;
}

ParseState ParseResponseBody(std::string_view& buf, std::shared_ptr<HttpRecord>& result, bool closed) {
    HTTPResponse r;
    bool adjacentResp = StartsWithHttp(buf) && (ParseHttpResponse(buf, &r) > 0);

    if (adjacentResp || (buf.empty() && closed)) {
        return ParseState::kSuccess;
    }

    // Case 1: Content-Length
    const auto contentLengthIter = result->GetRespHeaderMap().find(kContentLength);
    if (contentLengthIter != result->GetRespHeaderMap().end()) {
        std::string_view contentLenStr = contentLengthIter->second;
        auto s = ParseContent(contentLenStr, buf, 256, result->mRespBody, result->mRespBodySize);
        // CTX_DCHECK_LE(result->body.size(), FLAGS_http_body_limit_bytes);
        return s;
    }

    // Case 2: Chunked transfer.
    const auto transferEncodingIter = result->GetRespHeaderMap().find(kTransferEncoding);
    if (transferEncodingIter != result->GetRespHeaderMap().end() && transferEncodingIter->second == "chunked") {
        auto s = ParseChunked(buf, 256, result->mRespBody, result->mRespBodySize);
        // CTX_DCHECK_LE(result->body.size(), FLAGS_http_body_limit_bytes);
        return s;
    }

    // Case 3: Responses where we can assume no body.
    // The status codes below MUST not have a body, according to the spec.
    // See: https://tools.ietf.org/html/rfc2616#section-4.4
    if ((result->mCode >= 100 && result->mCode < 200) || result->mCode == 204 || result->mCode == 304) {
        result->mRespBody = "";

        // Status 101 is an even more special case.
        if (result->mCode == 101) {
            const auto upgradeIter = result->GetRespHeaderMap().find(kUpgrade);
            if (upgradeIter == result->GetRespHeaderMap().end()) {
            }

            return ParseState::kEOS;
        }

        return ParseState::kSuccess;
    }

    // Case 4: Response where we can't assume no body, but where no Content-Length or
    // Transfer-Encoding is provided. In these cases we should wait for close().
    // According to HTTP/1.1 standard:
    // https://www.w3.org/Protocols/HTTP/1.0/draft-ietf-http-spec.html#BodyLength
    // such messages are terminated by the close of the connection.
    // TODO(yzhao): For now we just accumulate messages, let probe_close() submit a message to
    // perf buffer, so that we can terminate such messages.
    result->mRespBody = buf;
    buf.remove_prefix(buf.size());

    return ParseState::kSuccess;
}

ParseState ParseResponse(std::string_view& buf, std::shared_ptr<HttpRecord>& result, bool closed, bool forceSample) {
    HTTPResponse resp;
    int retval = ParseHttpResponse(buf, &resp);

    if (retval >= 0) {
        buf.remove_prefix(retval);
        result->SetStatusCode(resp.mStatus);
        // for 4xx 5xx
        if (result->GetStatusCode() >= 400) {
            result->MarkSample();
        }

        if (result->ShouldSample() || forceSample) {
            result->SetRespHeaderMap(http::GetHTTPHeadersMap(resp.mHeaders, resp.mNumHeaders));
            result->SetRespMsg(std::string(resp.mMsg, resp.mMsgLen));
            return ParseResponseBody(buf, result, closed);
        }
        return ParseState::kSuccess;
    }
    if (retval == -2) {
        return ParseState::kNeedsMoreData;
    }
    return ParseState::kInvalid;
}
} // namespace http
} // namespace logtail::ebpf
