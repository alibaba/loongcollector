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
    // 实现MySQL请求解析逻辑
    // 这里需要根据MySQL协议规范解析数据包
    
    // 示例实现（需要根据实际MySQL协议完善）
    if (buf.size() < 5) { // MySQL包头至少5字节
        return ParseState::kNeedsMoreData;
    }
    
    // 解析MySQL包长度（3字节）
    uint32_t packetLen = buf[0] | (buf[1] << 8) | (buf[2] << 16);
    
    if (buf.size() < 5 + packetLen) {
        return ParseState::kNeedsMoreData;
    }
    
    // 包序号（1字节）
    uint8_t seqId = buf[3];
    
    // 命令类型（1字节）
    uint8_t command = buf[4];
    
    // 根据命令类型解析具体内容
    switch (command) {
        case 0x03: // COM_QUERY
            if (packetLen > 1) {
                std::string sql(buf.data() + 5, packetLen - 1);
                result->SetSql(sql);
            }
            break;
        case 0x04: // COM_FIELD_LIST
            // 处理字段列表命令
            break;
        // 其他命令...
        default:
            // 未知命令类型
            break;
    }
    
    buf.remove_prefix(5 + packetLen);
    
    if (result->ShouldSample() || forceSample) {
        // 解析详细信息
        return ParseState::kSuccess;
    }
    
    return ParseState::kSuccess;
}

ParseState ParseResponse(std::string_view& buf, std::shared_ptr<MysqlRecord>& result, bool closed, bool forceSample) {
    // 实现MySQL响应解析逻辑
    // 这里需要根据MySQL协议规范解析响应数据包
    
    // 示例实现（需要根据实际MySQL协议完善）
    if (buf.size() < 4) {
        return ParseState::kNeedsMoreData;
    }
    
    // 解析MySQL包长度（3字节）
    uint32_t packetLen = buf[0] | (buf[1] << 8) | (buf[2] << 16);
    
    if (buf.size() < 4 + packetLen) {
        return ParseState::kNeedsMoreData;
    }
    
    // 包序号（1字节）
    uint8_t seqId = buf[3];
    
    // 根据响应内容设置状态码等信息
    if (packetLen > 0) {
        uint8_t firstByte = buf[4];
        
        if (firstByte == 0x00) {
            // OK packet
            result->SetStatusCode(0); // OK
        } else if (firstByte == 0xff) {
            // ERR packet
            result->SetStatusCode(1); // Error
            if (packetLen >= 9) {
                uint16_t errorCode = buf[5] | (buf[6] << 8);
                result->SetErrorCode(errorCode);
            }
        } else if (firstByte == 0xfe) {
            // EOF packet
            result->SetStatusCode(2); // EOF
        }
        // 其他类型的响应包...
    }
    
    buf.remove_prefix(4 + packetLen);
    
    if (result->ShouldSample() || forceSample) {
        // 解析详细信息
        return ParseState::kSuccess;
    }
    
    return ParseState::kSuccess;
}

} // namespace mysql
} // namespace logtail::ebpf