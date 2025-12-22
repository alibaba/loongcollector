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

#include "MysqlParser.h"

#include <map>

#include "common/StringTools.h"
#include "ebpf/type/NetworkObserverEvent.h"
#include "ebpf/util/TraceId.h"
#include "logger/Logger.h"

namespace logtail::ebpf {
std::vector<std::shared_ptr<L7Record>>
MYSQLProtocolParser::Parse(struct conn_data_event_t* dataEvent,
                           const std::shared_ptr<Connection>& conn,
                           const std::shared_ptr<AppDetail>& appDetail,
                           const std::shared_ptr<AppConvergerManager>& converger) {
    auto record = std::make_shared<MysqlRecord>(conn, appDetail);
    record->SetEndTsNs(dataEvent->end_ts);
    record->SetStartTsNs(dataEvent->start_ts);
    auto spanId = GenerateSpanID();

    // slow request
    if (record->GetLatencyMs() > kSlowRequestThresholdMs || appDetail->mSampler->ShouldSample(spanId)) {
        record->MarkSample();
    }

    if (dataEvent->response_len > 0) {
        std::string_view buf(dataEvent->msg + dataEvent->request_len, dataEvent->response_len);
        ParseState state = mysql::ParseResponse(buf, record, true, false);
        if (state != ParseState::kSuccess) {
            LOG_DEBUG(sLogger, ("[MYSQLProtocolParser]: Parse MySQL response failed", int(state)));
            return {};
        }
    }

    if (dataEvent->request_len > 0) {
        std::string_view buf(dataEvent->msg, dataEvent->request_len);
        ParseState state = mysql::ParseRequest(buf, record, false);
        if (state != ParseState::kSuccess) {
            LOG_DEBUG(sLogger, ("[MYSQLProtocolParser]: Parse MySQL request failed", int(state)));
            return {};
        }
        if (converger) {
            converger->DoConverge(appDetail, ConvType::kSql, record->mSql);
        }
    }

    if (record->ShouldSample()) {
        record->SetSpanId(std::move(spanId));
        record->SetTraceId(GenerateTraceID());
    }

    return {record};
}

namespace mysql {


// See https://dev.mysql.com/doc/dev/mysql-server/9.4.0/page_protocol_basic_packets.html
constexpr int kPacketHeaderLength = 4;

constexpr size_t kMysqlErrIndicatorSize = 1;
constexpr size_t kMysqlErrCodeSize = 2;
constexpr size_t kMysqlSqlStateMarkerSize = 1;
constexpr size_t kMysqlSqlStateSize = 5;

constexpr size_t kMySqlStatMarkerOffset = kMysqlErrIndicatorSize + kMysqlErrCodeSize + kMysqlSqlStateMarkerSize;

// MySQL command type
constexpr uint8_t MYSQL_CMD_QUERY = 0x03;
// MySQL response status code
constexpr uint8_t MYSQL_RESPONSE_OK = 0x00;
constexpr uint8_t MYSQL_RESPONSE_ERR = 0xff;
constexpr uint8_t MYSQL_RESPONSE_EOF = 0xfe;
// Supported max SQL length
constexpr size_t kMaxSqlLength = 256;


std::string CommandName(uint32_t cmd) {
    if (cmd == MYSQL_CMD_QUERY)
        return "QUERY";
    return std::to_string(cmd);
}

uint32_t ParsePacketLen(std::string_view& buf) {
    // ref: https://dev.mysql.com/doc/dev/mysql-server/9.4.0/page_protocol_basic_packets.html
    return (uint32_t)(uint8_t)buf[0] | ((uint32_t)(uint8_t)buf[1] << 8) | ((uint32_t)(uint8_t)buf[2] << 16);
}


ParseState ParseRequest(std::string_view& buf, std::shared_ptr<MysqlRecord>& result, bool forceSample) {
    // 1 byte for Command Type
    if (buf.size() < kPacketHeaderLength + 1) {
        return ParseState::kNeedsMoreData;
    }

    uint32_t packetLen = ParsePacketLen(buf);
    uint8_t command = buf[kPacketHeaderLength];
    result->SetCommandName(CommandName(command));

    if (command != MYSQL_CMD_QUERY) {
        return ParseState::kUnknown;
    }

    if (packetLen == 0) {
        return ParseState::kInvalid;
    }

    size_t availableData = buf.size() - kPacketHeaderLength - 1;
    size_t sqlLen = std::min(static_cast<size_t>(packetLen - 1), kMaxSqlLength);
    sqlLen = std::min(sqlLen, availableData);

    std::string sql(buf.data() + kPacketHeaderLength + 1, sqlLen);
    result->SetSql(sql);
    return ParseState::kSuccess;
}

ParseState ParseResponse(std::string_view& buf, std::shared_ptr<MysqlRecord>& result, bool closed, bool forceSample) {
    // 1 byte for status code
    if (buf.size() < kPacketHeaderLength + 1) {
        return ParseState::kNeedsMoreData;
    }

    uint32_t packetLen = ParsePacketLen(buf);
    if (packetLen == 0) {
        return ParseState::kInvalid;
    }

    uint8_t statusCode = buf[4];
    if (statusCode != MYSQL_RESPONSE_OK && statusCode != MYSQL_RESPONSE_EOF && statusCode != MYSQL_RESPONSE_ERR) {
        return ParseState::kInvalid;
    }

    if (statusCode == MYSQL_RESPONSE_EOF) {
        statusCode = MYSQL_RESPONSE_OK;
    }

    result->SetStatusCode(statusCode);
    // See https://dev.mysql.com/doc/dev/mysql-server/9.4.0/page_protocol_basic_err_packet.html
    if (statusCode == MYSQL_RESPONSE_ERR) {
        size_t errorMsgMinStart = kMysqlErrIndicatorSize + kMysqlErrCodeSize;
        size_t errorMsgStart
            = kMysqlErrIndicatorSize + kMysqlErrCodeSize + kMysqlSqlStateMarkerSize + kMysqlSqlStateSize;

        if (packetLen <= errorMsgMinStart) {
            return ParseState::kNeedsMoreData;
        }

        if (buf[kPacketHeaderLength + kMySqlStatMarkerOffset] != '#') {
            // No SQL state code, error message starts directly from the 7th byte
            errorMsgStart = errorMsgMinStart;
        }

        std::string errorMsg(buf.data() + kPacketHeaderLength + errorMsgStart,
                             buf.size() - errorMsgStart - kPacketHeaderLength);
        result->SetErrorMessage(errorMsg);
    }
    return ParseState::kSuccess;
}

} // namespace mysql
} // namespace logtail::ebpf
