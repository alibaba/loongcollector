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

#include "RedisParser.h"

#include <cctype>

#include <algorithm>
#include <map>

#include "common/StringTools.h"
#include "ebpf/type/NetworkObserverEvent.h"
#include "ebpf/util/TraceId.h"
#include "logger/Logger.h"

namespace logtail::ebpf {

std::vector<std::shared_ptr<L7Record>>
REDISProtocolParser::Parse(struct conn_data_event_t* dataEvent,
                           const std::shared_ptr<Connection>& conn,
                           const std::shared_ptr<AppDetail>& appDetail,
                           const std::shared_ptr<AppConvergerManager>& converger) {
    auto record = std::make_shared<RedisRecord>(conn, appDetail);
    record->SetEndTsNs(dataEvent->end_ts);
    record->SetStartTsNs(dataEvent->start_ts);
    auto spanId = GenerateSpanID();

    // Mark as sample if it's a slow request (> 500ms) or selected by sampler
    if (record->GetLatencyMs() > 500 || appDetail->mSampler->ShouldSample(spanId)) {
        record->MarkSample();
    }

    if (dataEvent->response_len > 0) {
        std::string_view buf(dataEvent->msg + dataEvent->request_len, dataEvent->response_len);
        ParseState state = redis::ParseResponse(buf, record, true, false);
        if (state != ParseState::kSuccess) {
            LOG_DEBUG(sLogger, ("[REDISProtocolParser]: Parse REDIS response failed", int(state)));
            return {};
        }
    }

    if (dataEvent->request_len > 0) {
        std::string_view buf(dataEvent->msg, dataEvent->request_len);
        ParseState state = redis::ParseRequest(buf, record, false);
        if (state != ParseState::kSuccess) {
            LOG_DEBUG(sLogger, ("[REDISProtocolParser]: Parse REDIS request failed", int(state)));
            return {};
        }
        if (converger) {
            std::string sql = record->GetSql();
            converger->DoConverge(appDetail, ConvType::kNoSql, sql);
        }
    }

    if (record->ShouldSample()) {
        record->SetSpanId(std::move(spanId));
        record->SetTraceId(GenerateTraceID());
    }

    return {record};
}

namespace redis {

// Redis RESP protocol constants
constexpr size_t kMaxCommandLength = 256;
constexpr char kRespArrayType = '*';
constexpr char kRespBulkStringType = '$';
constexpr char kRespSimpleStringType = '+';
constexpr char kRespErrorType = '-';
constexpr char kRespIntegerType = ':';
constexpr char kCRLF[] = "\r\n";

// Helper function: Find CRLF (\r\n) in buffer
static size_t FindCRLF(const std::string_view& buf, size_t start = 0) {
    for (size_t i = start; i + 1 < buf.size(); ++i) {
        if (buf[i] == '\r' && buf[i + 1] == '\n') {
            return i;
        }
    }
    return std::string_view::npos;
}

// Helper function: Parse integer from string_view
static bool ParseInteger(const std::string_view& str, int64_t& result) {
    if (str.empty()) {
        return false;
    }
    try {
        size_t pos;
        result = std::stoll(std::string(str), &pos);
        return pos == str.size();
    } catch (...) {
        return false;
    }
}

// Parse RESP Bulk String: $<length>\r\n<data>\r\n
static ParseState ParseBulkString(std::string_view& buf, std::string& result, size_t maxLen) {
    if (buf.empty() || buf[0] != kRespBulkStringType) {
        return ParseState::kInvalid;
    }

    // Find CRLF after length
    size_t lengthEnd = FindCRLF(buf, 1);
    if (lengthEnd == std::string_view::npos) {
        return ParseState::kNeedsMoreData;
    }

    // Parse length
    int64_t length;
    if (!ParseInteger(buf.substr(1, lengthEnd - 1), length)) {
        return ParseState::kInvalid;
    }

    // Null bulk string: $-1\r\n
    if (length == -1) {
        buf.remove_prefix(lengthEnd + 2);
        result = "";
        return ParseState::kSuccess;
    }

    if (length < 0) {
        return ParseState::kInvalid;
    }

    // Check if we have enough data
    size_t requiredSize = lengthEnd + 2 + length + 2; // $len\r\ndata\r\n
    if (buf.size() < requiredSize) {
        return ParseState::kNeedsMoreData;
    }

    // Verify trailing CRLF
    if (buf[requiredSize - 2] != '\r' || buf[requiredSize - 1] != '\n') {
        return ParseState::kInvalid;
    }

    // Extract data (limit to maxLen)
    size_t dataStart = lengthEnd + 2;
    size_t dataLen = std::min(static_cast<size_t>(length), maxLen);
    result = std::string(buf.substr(dataStart, dataLen));

    buf.remove_prefix(requiredSize);
    return ParseState::kSuccess;
}

// Parse Redis RESP request
ParseState ParseRequest(std::string_view& buf, std::shared_ptr<RedisRecord>& result, bool forceSample) {
    if (buf.empty()) {
        return ParseState::kNeedsMoreData;
    }

    // Parse Redis RESP protocol request
    // Standard format: *<arg_count>\r\n$<arg1_len>\r\n<arg1>\r\n$<arg2_len>\r\n<arg2>\r\n...
    if (buf[0] == kRespArrayType) {
        // Array type request (standard Redis command format)
        size_t arrayCountEnd = FindCRLF(buf, 1);
        if (arrayCountEnd == std::string_view::npos) {
            return ParseState::kNeedsMoreData;
        }

        // Parse argument count
        int64_t arrayCount;
        if (!ParseInteger(buf.substr(1, arrayCountEnd - 1), arrayCount)) {
            return ParseState::kInvalid;
        }

        if (arrayCount <= 0) {
            return ParseState::kInvalid;
        }

        // Remove array header
        buf.remove_prefix(arrayCountEnd + 2);

        // Parse first argument (command name)
        std::string commandName;
        ParseState state = ParseBulkString(buf, commandName, kMaxCommandLength);
        if (state != ParseState::kSuccess) {
            return state;
        }

        // Convert command name to uppercase
        std::transform(commandName.begin(), commandName.end(), commandName.begin(), [](unsigned char c) {
            return std::toupper(c);
        });
        result->SetCommandName(commandName);

        // Build complete SQL string (command + arguments)
        std::string sql = commandName;
        for (int64_t i = 1; i < arrayCount && i < 10; ++i) { // Parse at most 10 arguments
            std::string arg;
            state = ParseBulkString(buf, arg, kMaxCommandLength);
            if (state != ParseState::kSuccess) {
                // Keep already parsed command even if subsequent arguments fail
                break;
            }
            sql += " " + arg;
        }

        // Limit SQL length
        if (sql.size() > kMaxCommandLength) {
            sql = sql.substr(0, kMaxCommandLength);
        }
        result->SetSql(sql);

        return ParseState::kSuccess;
    } else if (buf[0] == kRespBulkStringType || buf[0] == kRespSimpleStringType) {
        // Simple command or single bulk string (inline command, less common)
        size_t endPos = FindCRLF(buf);
        if (endPos == std::string_view::npos) {
            return ParseState::kNeedsMoreData;
        }
        std::string cmd = std::string(buf.substr(0, std::min(endPos, kMaxCommandLength)));
        result->SetSql(cmd);
        result->SetCommandName(cmd);
        return ParseState::kSuccess;
    } else {
        // Unknown format
        return ParseState::kInvalid;
    }
}

ParseState ParseResponse(std::string_view& buf, std::shared_ptr<RedisRecord>& result, bool closed, bool forceSample) {
    if (buf.empty()) {
        return ParseState::kNeedsMoreData;
    }

    // Parse Redis RESP protocol response
    char responseType = buf[0];

    switch (responseType) {
        case kRespSimpleStringType: { // Simple string: +OK\r\n
            size_t endPos = FindCRLF(buf);
            if (endPos == std::string_view::npos) {
                return ParseState::kNeedsMoreData;
            }
            result->SetStatusCode(0); // Success
            buf.remove_prefix(endPos + 2);
            return ParseState::kSuccess;
        }
        case kRespErrorType: { // Error: -ERR message\r\n
            size_t endPos = FindCRLF(buf);
            if (endPos == std::string_view::npos) {
                return ParseState::kNeedsMoreData;
            }
            result->SetStatusCode(1); // Error
            if (endPos > 1) {
                std::string errorMsg = std::string(buf.substr(1, endPos - 1));
                result->SetErrorMessage(errorMsg);
                // Force sampling for error responses
                result->MarkSample();
            }
            buf.remove_prefix(endPos + 2);
            return ParseState::kSuccess;
        }
        case kRespIntegerType: { // Integer: :1000\r\n
            size_t endPos = FindCRLF(buf);
            if (endPos == std::string_view::npos) {
                return ParseState::kNeedsMoreData;
            }
            result->SetStatusCode(0); // Success
            buf.remove_prefix(endPos + 2);
            return ParseState::kSuccess;
        }
        case kRespBulkStringType: { // Bulk string: $len\r\ndata\r\n
            size_t lengthEnd = FindCRLF(buf, 1);
            if (lengthEnd == std::string_view::npos) {
                return ParseState::kNeedsMoreData;
            }

            int64_t length;
            if (!ParseInteger(buf.substr(1, lengthEnd - 1), length)) {
                return ParseState::kInvalid;
            }

            // Null bulk string: $-1\r\n (NULL)
            if (length == -1) {
                result->SetStatusCode(0);
                buf.remove_prefix(lengthEnd + 2);
                return ParseState::kSuccess;
            }

            if (length < 0) {
                return ParseState::kInvalid;
            }

            // Check if data is complete
            size_t requiredSize = lengthEnd + 2 + length + 2;
            if (buf.size() < requiredSize) {
                return ParseState::kNeedsMoreData;
            }

            // Verify trailing CRLF
            if (buf[requiredSize - 2] != '\r' || buf[requiredSize - 1] != '\n') {
                return ParseState::kInvalid;
            }

            result->SetStatusCode(0); // Success
            buf.remove_prefix(requiredSize);
            return ParseState::kSuccess;
        }
        case kRespArrayType: { // Array: *count\r\n...
            size_t arrayCountEnd = FindCRLF(buf, 1);
            if (arrayCountEnd == std::string_view::npos) {
                return ParseState::kNeedsMoreData;
            }

            int64_t arrayCount;
            if (!ParseInteger(buf.substr(1, arrayCountEnd - 1), arrayCount)) {
                return ParseState::kInvalid;
            }

            // Empty or null array: *-1\r\n or *0\r\n
            if (arrayCount <= 0) {
                result->SetStatusCode(0);
                buf.remove_prefix(arrayCountEnd + 2);
                return ParseState::kSuccess;
            }

            // Simplified handling for array responses: only validate format, not parsing all elements
            // This avoids complex recursive parsing and improves performance
            result->SetStatusCode(0); // Success
            buf.remove_prefix(arrayCountEnd + 2);
            return ParseState::kSuccess;
        }
        default:
            // Unknown response type
            result->SetStatusCode(-1); // Unknown
            return ParseState::kInvalid;
    }
}

} // namespace redis
} // namespace logtail::ebpf
