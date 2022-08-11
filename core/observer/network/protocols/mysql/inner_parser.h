/*
 * Copyright 2022 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include <iostream>

#include "network/protocols/utils.h"

namespace logtail {
struct MySQLPacket {
    uint32_t packetLen;
    uint8_t packetNum;
};

enum MySQLPacketType {
    MySQLPacketTypeServerGreeting = 1,
    MySQLPacketTypeClientLogin = 2,
    MySQLPacketTypeResponse = 3,
    MySQLPacketTypeCollectStatement = 4,
    MySQLPacketIgnoreStatement = 5,
    MySQLPacketNoResponseStatement = 6,
};

inline std::string MySQLPacketTypeToString(enum MySQLPacketType packetType) {
    switch (packetType) {
        case MySQLPacketTypeServerGreeting:
            return "server_greeting";
        case MySQLPacketTypeClientLogin:
            return "client_login";
        case MySQLPacketTypeResponse:
            return "response";
        case MySQLPacketTypeCollectStatement:
            return "collect_statment";
        case MySQLPacketIgnoreStatement:
            return "ignore_statement";
        case MySQLPacketNoResponseStatement:
            return "no_response_statement";
        default:
            return "unknown";
    }
}

// https://dev.mysql.com/doc/internals/en/command-phase.html
enum class MySQLCommand {
    comSleep = 0,
    comQuit = 1,
    comInitDB = 2,
    comQuery = 3,
    comFieldList = 4,
    comCreateDB = 5,
    comDropDB = 6,
    comRefresh = 7,
    comShutdown = 8,
    comStatistics = 9,
    comProcessInfo = 10,
    comConnect = 11,
    comProcessKill = 12,
    comDebug = 13,
    comPing = 14,
    comTime = 15,
    comDelayedInsert = 16,
    comChangeUser = 17,
    comBinlogDump = 18,
    comTableDump = 19,
    comConnectOut = 20,
    comRegisterSlave = 21,
    comStmtPrepare = 22,
    comStmtExecute = 23,
    comStmtSendLongData = 24,
    comStmtClose = 25,
    comStmtReset = 26,
    comSetOption = 27,
    comStmtFetch = 28,
};

struct MySQLPacketQuery {
    SlsStringPiece sql;
};

struct MySQLPacketResponse {
    bool ok;
};

class MySQLParser : public ProtoParser {
public:
    MySQLParser(const char* payload, const size_t pktSize) : ProtoParser(payload, pktSize, false) {}

    void parsePacketHeader();

    void parseGreetingPacket();

    void parseLoginPacket();

    void parseQueryPacket(MySQLCommand command);

    void parseOKPacket();

    void parseErrPacket();

    void parseEOFPacket();

    void parseRowPacket(int fieldCount);

    bool isEofPacket();

    bool isOKPacket();

    bool isErrPacket();

    uint32_t parseFieldHeaderPacket(); // return field count

    void parseFieldPacket();

    bool packetLengthCheckSkip(uint32_t startPosition, uint32_t packetLen);

    void parseResultSetPacket();

    uint64_t readLengthCodedInt();

    SlsStringPiece readLeft();

    SlsStringPiece readFixSizeString(int32_t size);

    SlsStringPiece readLengthCodedString();

    uint32_t readPacketLength();

    uint8_t readPacketNum();

    void readData(std::vector<SlsStringPiece>& data);

    void parse();

    void print();

private:
    MySQLPacket packet;

public:
    MySQLPacketType mysqlPacketType;
    MySQLPacketQuery mysqlPacketQuery;
    MySQLPacketResponse mysqlPacketResponse;
};

} // end of namespace logtail