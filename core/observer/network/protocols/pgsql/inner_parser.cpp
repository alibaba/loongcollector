// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "inner_parser.h"
#include "network/protocols/infer.h"
#include "Logger.h"

namespace logtail {

// length[int32],major[int16],major[int16]
bool PgSQLParser::parseStartUpMsg() {
    auto c = static_cast<PostgreSqlTag>(*readChar(false));
    if (c == PostgreSqlTag::Unknown) {
        this->type = PgSQLPacketType::IgnoreQuery;
        return true;
    }
    return false;
}

void PgSQLParser::parseRegularMsg(MessageType messageType) {
    if (messageType == MessageType_Request) {
        auto c = readChar(true);
        auto t = static_cast<PostgreSqlTag>(*c);
        switch (t) {
            case PostgreSqlTag::Query: {
                uint32_t len = readUint32();
                if (this->isParseFail || pktSize - currPostion + 4 < len) {
                    setParseFail("parese pgsql Parse fail");
                    return;
                }
                this->query.sql = readUntil(' ');
                this->type = PgSQLPacketType::Query;
                break;
            }
            case PostgreSqlTag::Parse: {
                uint32_t len = readUint32();
                if (this->isParseFail || pktSize - currPostion + 4 < len) {
                    setParseFail("parese pgsql Parse fail");
                    return;
                }
                readUntil('\0'); // statement name
                this->query.sql = readUntil('\0');
                this->type = PgSQLPacketType::Query;
                break;
            }
            default: {
                this->type = PgSQLPacketType::IgnoreQuery;
                LOG_DEBUG(sLogger, ("pgsql response tag has not been supported to parsed", *c));
                break;
            }
        }
    } else if (messageType == MessageType_Response) {
        bool foundRowDesc = false, foundCmdCmpl = false, foundParseCmpl = false, foundError = false;
        bool res = parseRegularResponseMsg(foundRowDesc, foundCmdCmpl, foundParseCmpl, foundError);
        if (!res) {
            this->type = PgSQLPacketType::Unknown;
            return;
        }
        // check simple query result
        if (foundRowDesc) {
            if (foundCmdCmpl) {
                this->resp.ok = true;
                this->type = PgSQLPacketType::Response;
                return;
            } else if (foundError) {
                this->resp.ok = false;
                this->type = PgSQLPacketType::Response;
                return;
            }
        }
        // check parse statement result
        if (foundParseCmpl) {
            this->resp.ok = true;
            this->type = PgSQLPacketType::Response;
            return;
        } else if (foundError) {
            this->resp.ok = false;
            this->type = PgSQLPacketType::Response;
            return;
        }
        this->type = PgSQLPacketType::Unknown;
    }
}

void PgSQLParser::parse(MessageType messageType) {
    if (!parseStartUpMsg()) {
        parseRegularMsg(messageType);
    }
}

bool PgSQLParser::parseRegularResponseMsg(bool& foundRowDesc,
                                          bool& foundCmdCmpl,
                                          bool& foundParseCmpl,
                                          bool& foundError) {
    auto c = readChar(true);
    auto t = static_cast<PostgreSqlTag>(*c);
    uint32_t len = readUint32(false);
    uint32_t nextPos = currPostion + len;
    if (isParseFail || nextPos > pktSize) {
        LOG_DEBUG(sLogger, ("pgsql request tag has not been supported to parsed", *c));
        return false;
    }
    switch (t) {
        case PostgreSqlTag::CmdComplete: {
            foundCmdCmpl = true;
            break;
        }
        case PostgreSqlTag::RowDesc: {
            foundRowDesc = true;
            break;
        }
        case PostgreSqlTag::ParseComplete: {
            foundParseCmpl = true;
            break;
        }
        case PostgreSqlTag::ErrResp: {
            foundError = true;
            break;
        }
        default: {
        }
    }
    if (nextPos == pktSize) {
        return true;
    }
    positionCommit(len);
    return parseRegularResponseMsg(foundRowDesc, foundCmdCmpl, foundParseCmpl, foundError);
}
} // namespace logtail