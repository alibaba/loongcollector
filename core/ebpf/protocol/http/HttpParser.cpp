// Copyright 2023 iLogtail Authors
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

#include <atomic>
#include <map>

#include "ebpf/type/NetworkObserverEvent.h"
#include "ebpf/util/TraceId.h"
#include "logger/Logger.h"

namespace logtail {
namespace ebpf {

inline constexpr char kContentLength[] = "Content-Length";
inline constexpr char kTransferEncoding[] = "Transfer-Encoding";
inline constexpr char kUpgrade[] = "Upgrade";

std::vector<std::shared_ptr<AbstractRecord>> HTTPProtocolParser::Parse(struct conn_data_event_t* dataEvent,
                                                                       const std::shared_ptr<Connection>& conn,
                                                                       const std::shared_ptr<Sampler>& sampler) {
    // 处理 HTTP 协议数据
    std::vector<std::shared_ptr<AbstractRecord>> records;
    records.reserve(1);
    auto record = std::make_shared<HttpRecord>(conn);
    // record->SetEndTs(dataEvent->mEndTs);
    // record->SetStartTs(dataEvent->mStartTs);
    record->SetEndTs(dataEvent->end_ts);
    record->SetStartTs(dataEvent->start_ts);
    // bool isSample = false;
    auto spanId = GenerateSpanID();
    bool isSample = sampler->ShouldSample(spanId);
    if (isSample) {
        // LOG
        auto traceId = GenerateTraceID();
        LOG_DEBUG(sLogger, ("spanId", FromSpanId(spanId))("traceId", FromTraceId(traceId)));
        record->SetSpanId(FromSpanId(spanId));
        record->SetTraceId(FromTraceId(traceId));
    } else {
        LOG_DEBUG(sLogger, ("sampler", "reject"));
    }

    if (dataEvent->request_len) {
        // Message result;
        std::string_view buf(dataEvent->msg, dataEvent->request_len);
        ParseState state = http::ParseRequest(&buf, record, isSample);
        if (state != ParseState::kSuccess) {
            LOG_WARNING(sLogger, ("[HTTPProtocolParser]: Parse HTTP request failed", int(state)));
        }
    }

    if (dataEvent->response_len > 0) {
        // Message result;
        std::string_view buf(dataEvent->msg + dataEvent->request_len, dataEvent->response_len);
        ParseState state = http::ParseResponse(&buf, record, true, isSample);
        if (state != ParseState::kSuccess) {
            LOG_WARNING(sLogger, ("[HTTPProtocolParser]: Parse HTTP response failed", int(state)));
        }
    }
    records.emplace_back(std::move(record));
    return records;
}

namespace http {
HeadersMap GetHTTPHeadersMap(const phr_header* headers, size_t num_headers) {
    HeadersMap result;
    for (size_t i = 0; i < num_headers; i++) {
        std::string name(headers[i].name, headers[i].name_len);
        std::string value(headers[i].value, headers[i].value_len);
        result.emplace(std::move(name), std::move(value));
    }
    return result;
}

int ParseHttpRequest(std::string_view buf, HTTPRequest* result) {
    return phr_parse_request(buf.data(),
                             buf.size(),
                             &result->method,
                             &result->method_len,
                             &result->path,
                             &result->path_len,
                             &result->minor_version,
                             result->headers,
                             &result->num_headers,
                             /*last_len*/ 0);
}

const std::string ROOT_PATH = "/";
const char QUESTION_MARK = '?';
const std::string HTTP1_PREFIX = "http1.";

ParseState ParseRequest(std::string_view* buf, std::shared_ptr<HttpRecord>& result, bool sample) {
    HTTPRequest req;
    int retval = http::ParseHttpRequest(*buf, &req);
    if (retval >= 0) {
        buf->remove_prefix(retval);

        if (req.path_len == 0) {
            result->SetPath(ROOT_PATH);
            result->SetRealPath(ROOT_PATH);
        } else {
            auto path = std::string(req.path, req.path_len);
            result->SetRealPath(path);
            std::size_t pos = result->GetRealPath().find(QUESTION_MARK);
            if (pos != std::string::npos) {
                result->SetPath(path.substr(0, pos));
            } else {
                result->SetPath(path);
            }
        }

        if (sample) {
            result->SetProtocolVersion(HTTP1_PREFIX + std::to_string(req.minor_version));
            result->SetMethod(std::string(req.method, req.method_len));
            result->SetReqHeaderMap(http::GetHTTPHeadersMap(req.headers, req.num_headers));
            return ParseRequestBody(buf, result);
        } else {
            return ParseState::kSuccess;
        }
    } else if (retval == -2) {
        return ParseState::kNeedsMoreData;
    } else {
        return ParseState::kInvalid;
    }
}

ParseState
PicoParseChunked(std::string_view* data, size_t body_size_limit_bytes, std::string* result, size_t* body_size) {
    // Make a copy of the data because phr_decode_chunked mutates the input,
    // and if the original parse fails due to a lack of data, we need the original
    // state to be preserved.
    std::string data_copy(*data);

    phr_chunked_decoder chunk_decoder = {};
    chunk_decoder.consume_trailer = 1;
    char* buf = data_copy.data();
    size_t buf_size = data_copy.size();
    ssize_t retval = phr_decode_chunked(&chunk_decoder, buf, &buf_size);

    if (retval == -1) {
        // Parse failed.
        return ParseState::kInvalid;
    } else if (retval == -2) {
        // Incomplete message.
        return ParseState::kNeedsMoreData;
    } else if (retval >= 0) {
        // Found a complete message.
        // data_copy.resize(std::min(buf_size, body_size_limit_bytes));
        data_copy.resize(buf_size);
        data_copy.shrink_to_fit();
        *result = std::move(data_copy);
        *body_size = buf_size;

        // phr_decode_chunked rewrites the buffer in place, removing chunked-encoding headers.
        // So we cannot simply remove the prefix, but rather have to shorten the buffer too.
        // This is done via retval, which specifies how many unprocessed bytes are left.
        data->remove_prefix(data->size() - retval);

        return ParseState::kSuccess;
    }

    return ParseState::kUnknown;
}


ParseState ParseChunked(std::string_view* data, size_t body_size_limit_bytes, std::string* result, size_t* body_size) {
    return PicoParseChunked(data, body_size_limit_bytes, result, body_size);
}

ParseState ParseRequestBody(std::string_view* buf, std::shared_ptr<HttpRecord>& result) {
    // Case 1: Content-Length
    const auto content_length_iter = result->GetReqHeaderMap().find(kContentLength);
    if (content_length_iter != result->GetReqHeaderMap().end()) {
        std::string_view content_len_str = content_length_iter->second;
        auto r = ParseContent(content_len_str, buf, 256, &result->mReqBody, &result->mReqBodySize);
        return r;
    }

    // Case 2: Chunked transfer.
    const auto transfer_encoding_iter = result->GetReqHeaderMap().find(kTransferEncoding);
    if (transfer_encoding_iter != result->GetReqHeaderMap().end() && transfer_encoding_iter->second == "chunked") {
        auto s = ParseChunked(buf, 256, &result->mReqBody, &result->mReqBodySize);

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
                              &result->minor_version,
                              &result->status,
                              &result->msg,
                              &result->msg_len,
                              result->headers,
                              &result->num_headers,
                              /*last_len*/ 0);
}

bool ParseContentLength(const std::string_view& content_len_str, size_t* len) {
    if (len == nullptr) {
        return false;
    }

    try {
        size_t pos;
        std::stoull(content_len_str.data());
        *len = std::stoull(std::string(content_len_str), &pos);
        if (pos != content_len_str.size()) {
            return false;
        }
    } catch (const std::exception& e) {
        return false;
    }

    return true;
}

ParseState ParseContent(std::string_view content_len_str,
                        std::string_view* data,
                        size_t body_size_limit_bytes,
                        std::string* result,
                        size_t* body_size) {
    size_t len;
    if (!ParseContentLength(content_len_str, &len)) {
        return ParseState::kInvalid;
    }
    if (data->size() < len) {
        return ParseState::kNeedsMoreData;
    }

    // *result = data->substr(0, std::min(len, body_size_limit_bytes));
    *result = data->substr(0, len);

    *body_size = len;
    data->remove_prefix(std::min(len, data->size()));
    return ParseState::kSuccess;
}

bool starts_with_http(const std::string_view* buf) {
    if (buf == nullptr) {
        return false;
    }
    std::string_view prefix = "HTTP";
    return buf->size() >= prefix.size() && buf->substr(0, prefix.size()) == prefix;
}

ParseState ParseResponseBody(std::string_view* buf, std::shared_ptr<HttpRecord>& result, bool closed) {
    HTTPResponse r;
    bool adjacent_resp = starts_with_http(buf) && (ParseHttpResponse(*buf, &r) > 0);

    if (adjacent_resp || (buf->empty() && closed)) {
        // result->mRespBody = "";
        return ParseState::kSuccess;
    }

    // Case 1: Content-Length
    const auto content_length_iter = result->GetRespHeaderMap().find(kContentLength);
    if (content_length_iter != result->GetRespHeaderMap().end()) {
        std::string_view content_len_str = content_length_iter->second;
        auto s = ParseContent(content_len_str, buf, 256, &result->mRespBody, &result->mRespBodySize);
        // CTX_DCHECK_LE(result->body.size(), FLAGS_http_body_limit_bytes);
        return s;
    }

    // Case 2: Chunked transfer.
    const auto transfer_encoding_iter = result->GetRespHeaderMap().find(kTransferEncoding);
    if (transfer_encoding_iter != result->GetRespHeaderMap().end() && transfer_encoding_iter->second == "chunked") {
        auto s = ParseChunked(buf, 256, &result->mRespBody, &result->mRespBodySize);
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
            const auto upgrade_iter = result->GetRespHeaderMap().find(kUpgrade);
            if (upgrade_iter == result->GetRespHeaderMap().end()) {
                //    LOG(WARNING) << "Expected an Upgrade header with HTTP status 101";
            }

            //  LOG(WARNING) << "HTTP upgrades are not yet supported";
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
    result->mRespBody = *buf;
    buf->remove_prefix(buf->size());

    return ParseState::kSuccess;
}

ParseState ParseResponse(std::string_view* buf, std::shared_ptr<HttpRecord>& result, bool closed, bool sample) {
    HTTPResponse resp;
    int retval = ParseHttpResponse(*buf, &resp);

    if (retval >= 0) {
        buf->remove_prefix(retval);
        result->SetStatusCode(resp.status);

        if (!sample) {
            return ParseState::kSuccess;
        } else {
            result->SetRespHeaderMap(http::GetHTTPHeadersMap(resp.headers, resp.num_headers));
            result->SetRespMsg(std::string(resp.msg, resp.msg_len));
            return ParseResponseBody(buf, result, closed);
        }
        // result->minor_version = resp.minor_version;
        // result->headers = http::GetHTTPHeadersMap(resp.headers, resp.num_headers);
        // result->resp_status = resp.status;
        // result->resp_message = std::string(resp.msg, resp.msg_len);
        // result->headers_byte_size = retval;

        // return ParseResponseBody(buf, result, closed);
    }
    if (retval == -2) {
        return ParseState::kNeedsMoreData;
    }
    return ParseState::kInvalid;
}
} // namespace http
} // namespace ebpf
} // namespace logtail
