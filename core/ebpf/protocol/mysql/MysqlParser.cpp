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
    if (record->GetLatencyMs() > 500 || appDetail->mSampler->ShouldSample(spanId)) {
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

// MySQL协议解析相关函数实现
ParseState ParseRequest(std::string_view& buf, std::shared_ptr<MysqlRecord>& result, bool forceSample) {
    if (buf.size() < 5) { // MySQL包头至少5字节
        return ParseState::kNeedsMoreData;
    }

    uint32_t packetLen = (uint8_t)buf[0] | ((uint8_t)buf[1] << 8) | ((uint8_t)buf[2] << 16);
    // 包序号（1字节）
    uint8_t seqId = buf[3];
    // 命令类型（1字节）
    uint8_t command = buf[4];
    result->SetSeqId(seqId);
    result->SetPacketLen(packetLen);
    result->SetCommand(command);

    // 根据命令类型解析具体内容
    switch (command) {
        case MYSQL_CMD_QUERY:
            if (packetLen > 1) {
                size_t sqlLen = std::min(static_cast<size_t>(packetLen - 1), static_cast<size_t>(256));
                sqlLen = std::min(sqlLen, buf.size() - 5);
                std::string sql(buf.data() + 5, sqlLen);
                result->SetSql(sql);
            }
            break;
        default:
            // 未知命令类型
            break;
    }
    return ParseState::kSuccess;
}

ParseState ParseResponse(std::string_view& buf, std::shared_ptr<MysqlRecord>& result, bool closed, bool forceSample) {
    if (buf.size() < 4) {
        return ParseState::kNeedsMoreData;
    }

    // 解析MySQL包长度（3字节）
    uint32_t packetLen = (uint8_t)buf[0] | (uint8_t)(buf[1] << 8) | (uint8_t)(buf[2] << 16);

    uint8_t seqId = buf[3];
    result->SetSeqId(seqId);
    result->SetPacketLen(packetLen);

    // 根据响应内容设置状态码等信息
    if (packetLen > 0) {
        uint8_t firstByte = buf[4];

        if (firstByte == MYSQL_RESPONSE_OK) {
            // OK packet
            result->SetStatusCode(0); // OK
        } else if (firstByte == MYSQL_RESPONSE_ERR) {
            // ERR packet
            result->SetStatusCode(1); // Error
            if (packetLen >= 9) {
                uint16_t errorCode = buf[5] | (buf[6] << 8);
                result->SetErrorCode(errorCode);

                // 提取错误消息
                // ERR packet格式: header(4) + ERR byte(1) + error code(2) + SQL state marker('#')(1) + SQL state(5) +
                // error message
                if (buf.size() >= 13) { // 确保有足够的数据
                    size_t errorMsgStart = 13; // 4(header) + 1(ERR) + 2(error code) + 1(marker) + 5(SQL state)
                    if (buf[12] == '#') { // 检查SQL状态标记
                        // 跳过SQL状态码
                        errorMsgStart = 13;
                    } else {
                        // 没有SQL状态码，错误消息直接从第7字节开始
                        errorMsgStart = 7; // 4(header) + 1(ERR) + 2(error code)
                    }

                    if (errorMsgStart < buf.size()) {
                        std::string errorMsg(buf.data() + errorMsgStart, buf.size() - errorMsgStart);
                        result->SetErrorMessage(errorMsg); // 需要在MysqlRecord中添加此方法
                    }
                }
            }
        } else if (firstByte == MYSQL_RESPONSE_EOF) {
            // EOF packet
            result->SetStatusCode(2); // EOF
        }
        // 其他类型的响应包...
    }
    return ParseState::kSuccess;
}

} // namespace mysql
} // namespace logtail::ebpf
