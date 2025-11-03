//
// Created by shuyi on 2025/11/03.
//

#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <vector>

#include "ebpf/protocol/AbstractParser.h"
#include "ebpf/protocol/ParserRegistry.h"
#include "ebpf/type/NetworkObserverEvent.h"
#include "ebpf/util/Converger.h"
#include "ebpf/util/sampler/Sampler.h"

namespace logtail::ebpf {

// MySQL协议常量定义
constexpr uint8_t MYSQL_CMD_QUERY = 0x03;
constexpr uint8_t MYSQL_CMD_STMT_PREPARE = 0x16;
constexpr uint8_t MYSQL_RESPONSE_OK = 0x00;
constexpr uint8_t MYSQL_RESPONSE_ERR = 0xff;
constexpr uint8_t MYSQL_RESPONSE_EOF = 0xfe;

// MySQL最大包大小
constexpr size_t MYSQL_MAX_PACKET_SIZE = 0xffffff;

struct MysqlRequest {
    uint8_t mCommand = 0;
    std::string mQuery;
    uint32_t mStmtId = 0;
    std::vector<std::string> mParameters;
};

struct MysqlResponse {
    uint8_t mResponseType = 0;
    uint16_t mErrorCode = 0;
    std::string mErrorMessage;
    uint64_t mAffectedRows = 0;
    uint64_t mLastInsertId = 0;
    uint16_t mWarnings = 0;
};

enum class ParseState {
    kUnknown,

    // The parse failed: data is invalid.
    // Input buffer consumed is not consumed and parsed output element is invalid.
    kInvalid,

    // The parse is partial: data appears to be an incomplete message.
    // Input buffer may be partially consumed and the parsed output element is not fully populated.
    kNeedsMoreData,

    // The parse succeeded, but the data is ignored.
    // Input buffer is consumed, but the parsed output element is invalid.
    kIgnored,

    // The parse succeeded, but indicated the end-of-stream.
    // Input buffer is consumed, and the parsed output element is valid.
    // however, caller should stop parsing any future data on this stream, even if more data exists.
    // Use cases include messages that indicate a change in protocol (see HTTP status 101).
    kEOS,

    // The parse succeeded.
    // Input buffer is consumed, and the parsed output element is valid.
    kSuccess,
};

namespace mysql {

ParseState ParseRequest(std::string_view& buf, std::shared_ptr<MysqlRecord>& result, bool forceSample = false);

ParseState ParseRequestBody(std::string_view& buf, std::shared_ptr<MysqlRecord>& result);

ParseState ParseResponse(std::string_view& buf, std::shared_ptr<MysqlRecord>& result, bool closed, bool forceSample = false);

// 解析MySQL包头
bool ParsePacketHeader(std::string_view& buf, uint32_t& packetLen, uint8_t& seqId);

// 解析MySQL请求包
ParseState ParseMysqlRequestPacket(std::string_view& buf, MysqlRequest& request);

// 解析MySQL响应包
ParseState ParseMysqlResponsePacket(std::string_view& buf, MysqlResponse& response);

} // namespace mysql


class MYSQLProtocolParser : public AbstractProtocolParser {
public:
    std::shared_ptr<AbstractProtocolParser> Create() override { return std::make_shared<MYSQLProtocolParser>(); }

    std::vector<std::shared_ptr<L7Record>> Parse(struct conn_data_event_t* dataEvent,
                                                 const std::shared_ptr<Connection>& conn,
                                                 const std::shared_ptr<AppDetail>& appDetail,
                                                 const std::shared_ptr<AppConvergerManager>& converger) override;
};

REGISTER_PROTOCOL_PARSER(support_proto_e::ProtoMySQL, MYSQLProtocolParser)

} // namespace logtail::ebpf