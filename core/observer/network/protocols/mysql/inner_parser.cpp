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

#include <iostream>
#include <string>

#include "inner_parser.h"
#include "Logger.h"

namespace logtail {

uint64_t MySQLParser::readLengthCodedInt() {
    uint8_t v = readUint8();

    if (v < 251) {
        return v;
    } else if (v == 252) {
        return readUint16();
    } else if (v == 253) {
        // mysql报文为小端模式
        auto data = readChar(false);
        positionCommit(3);
        return data[0] + (data[1] << 8) + (data[2] << 16);
    } else if (v == 254) {
        return readUint64();
    } else { // 251
        return 0;
    }
}

SlsStringPiece MySQLParser::readLeft() {
    uint32_t leftSize = getLeftSize();

    const char* s = readChar(true);

    positionCommit(leftSize - 1);
    SlsStringPiece value(s, leftSize);
    return value;
}

SlsStringPiece MySQLParser::readFixSizeString(int32_t size) {
    const char* s = readChar(true);
    positionCommit(size - 1);
    SlsStringPiece value(s, size);
    return value;
}


SlsStringPiece MySQLParser::readLengthCodedString() {
    uint64_t counter = readLengthCodedInt();
    return readFixSizeString(counter);
}

uint32_t MySQLParser::readPacketLength() {
    auto data = readChar(false);
    positionCommit(3);
    return data[0] + (data[1] << 8) + (data[2] << 16);
}

uint8_t MySQLParser::readPacketNum() {
    return readUint8();
}


void MySQLParser::parseGreetingPacket() {
    //    uint8_t protocol = readUint8();
    //    SlsStringPiece version = readUntil('\0');
    //    uint32_t threadId = readUint32();
    //    const char * slatStart = readChar(true);
    //    this->positionCommit(8);
    //    SlsStringPiece salt(slatStart, 8);
    //    uint16_t cap = readUint16();
    //    uint8_t chCoding = readUint8();
    //    uint16_t serverStatus = readUint16();
    //    uint16_t extCap = readUint16();
    //    uint8_t authPluinLen = readUint8();
    //
    //    // 10个填充字符
    //    this->positionCommit(10);
    //    SlsStringPiece salt2 = readUntil('\0');
    //    SlsStringPiece autPluinName = readUntil('\0');
}

void MySQLParser::parseLoginPacket() {
    //    uint16_t clientCap = readUint16();
    //    uint16_t clientCapExt = readUint16();
    //    uint32_t maxPacketSize = readUint32();
    //    uint8_t clientCharset = readUint8();
    //
    //    // skip 23个填充字符
    //    positionCommit(23);
    // do nothing for username, database because which are optional fields.
}

void MySQLParser::parseQueryPacket(MySQLCommand command) {
    // for com_query only collect command type to avoid plain sql influence aggregator performance
    if (command == MySQLCommand::comQuery) {
        SlsStringPiece val = readUntil(' ', true);
        mysqlPacketQuery.sql = val;
    } else {
        SlsStringPiece val = readLeft();
        mysqlPacketQuery.sql = val;
    }
}

void MySQLParser::parseOKPacket() {
    //    uint8_t code = readUint8();
    //    uint32_t affected = readLengthCodedInt();
    //    uint32_t lastId = readLengthCodedInt();
    //    uint32_t serverStatus = readUint16();
    //    uint32_t warnings = readUint16();
}

void MySQLParser::parseErrPacket() {
    //    uint8_t code = readUint8();
    //    uint8_t errCode = readUint16();
    //    uint8_t serverStateFlag = readUint8();
    //    SlsStringPiece serverState = readFixSizeString(5);
    //    SlsStringPiece errorMsg = readLeft();
}

void MySQLParser::parseEOFPacket() {
    //    uint8_t eofVal = readUint8();
    //    uint16_t warnings = readUint16();
    //    uint16_t serverStatus = readUint16();
}

bool MySQLParser::packetLengthCheckSkip(uint32_t startPosition, uint32_t packetLen) {
    if ((currPostion - startPosition) != (packetLen)) {
        uint32_t left = packetLen - (currPostion - startPosition);
        if (left < 0) {
            setParseFail("bad packet found");
            return false;
        }
        positionCommit(left);
    }
    return true;
}

uint32_t MySQLParser::parseFieldHeaderPacket() {
    uint32_t startPosition = currPostion;
    uint32_t fieldCount = readLengthCodedInt();
    if (!packetLengthCheckSkip(startPosition, packet.packetLen)) {
        return 0;
    }
    return fieldCount;
}

void MySQLParser::parseFieldPacket() {
    //    uint32_t startPosition = currPostion;
    //    SlsStringPiece catalog = readLengthCodedString();
    //    SlsStringPiece database = readLengthCodedString();
    //    SlsStringPiece table = readLengthCodedString();
    //    SlsStringPiece tableOriName = readLengthCodedString();
    //    SlsStringPiece fieldName = readLengthCodedString();
    //    SlsStringPiece fieldOriName = readLengthCodedString();
    //
    //    // 跳过一位填充值
    //    positionCommit(1);
    //    uint16_t fieldCharset = readUint16();
    //    uint32_t fieldLength = readUint32();
    //    uint8_t fieldType = readUint8();
    //    uint16_t fieldFlag = readUint16();
    //    uint8_t decimal = readUint8();
    //    packetLengthCheckSkip(startPosition, packet.packetLen);
}

void MySQLParser::parseRowPacket(int fieldCount) {
    //    for(int i=0; i<fieldCount; i++) {
    //        SlsStringPiece val = readLengthCodedString();
    //    }
}


bool MySQLParser::isEofPacket() {
    const uint8_t v = readUint8(false);
    if (v == 0xfe && packet.packetLen <= 9) {
        return true;
    }
    return false;
}


bool MySQLParser::isOKPacket() {
    const uint8_t v = readUint8(false);
    if (v == 0 && packet.packetLen >= 7) {
        return true;
    }
    return false;
}

bool MySQLParser::isErrPacket() {
    const uint8_t v = readUint8(false);
    if (v == 0xff && packet.packetLen >= 7) {
        return true;
    }
    return false;
}

void MySQLParser::parsePacketHeader() {
    packet.packetLen = readPacketLength();
    packet.packetNum = readPacketNum();
}

void MySQLParser::parseResultSetPacket() {
    //    uint32_t fieldCount = parseFieldHeaderPacket();
    //    for(int32_t i=0; i<fieldCount;  i++){
    //        parsePacketHeader();
    //        parseFieldPacket();
    //    }

    // TODO: For a lot of data, row set is too large or cannot be parsed.
    // for mysql 8.x there is no eof packet between header and row packet
    //    parsePacketHeader();
    //    parseEOFPacket();
    //
    //    while(!isNextEof()) {
    //
    //        parsePacketHeader();
    //        if(isEofPacket() || isParseFail ){
    //            break;
    //        }
    //
    //        parseRowPacket(fieldCount);
    //    }
}

void MySQLParser::parse() {
    parsePacketHeader();
    LOG_TRACE(sLogger, ("[MYSQL META] length", this->packet.packetLen)("num", (int)this->packet.packetNum));
    if (packet.packetNum == 0) {
        // Server Greeting | Query
        uint8_t v = readUint8(false);
        if (v == 0x0a && packet.packetLen > 60) {
            LOG_TRACE(sLogger, ("[MYSQL TYPE] ", "Greeting"));
            mysqlPacketType = MySQLPacketTypeServerGreeting;
            parseGreetingPacket();
        } else {
            auto com = static_cast<MySQLCommand>(readUint8());
            LOG_TRACE(sLogger, ("[MYSQL TYPE] request comm", (int)com));
            switch (com) {
                case MySQLCommand::comInitDB:
                case MySQLCommand::comFieldList:
                case MySQLCommand::comRefresh:
                case MySQLCommand::comShutdown:
                case MySQLCommand::comStatistics:
                case MySQLCommand::comProcessInfo:
                case MySQLCommand::comConnect:
                case MySQLCommand::comProcessKill:
                case MySQLCommand::comDebug:
                case MySQLCommand::comPing:
                case MySQLCommand::comTime:
                case MySQLCommand::comDelayedInsert:
                case MySQLCommand::comChangeUser:
                case MySQLCommand::comBinlogDump:
                case MySQLCommand::comTableDump:
                case MySQLCommand::comConnectOut:
                case MySQLCommand::comRegisterSlave:
                case MySQLCommand::comStmtExecute:
                case MySQLCommand::comStmtReset:
                case MySQLCommand::comSetOption:
                case MySQLCommand::comStmtFetch: {
                    mysqlPacketType = MySQLPacketIgnoreStatement;
                    break;
                }
                case MySQLCommand::comStmtSendLongData:
                case MySQLCommand::comStmtClose:
                case MySQLCommand::comQuit: {
                    mysqlPacketType = MySQLPacketNoResponseStatement;
                    break;
                }
                case MySQLCommand::comStmtPrepare:
                case MySQLCommand::comCreateDB:
                case MySQLCommand::comDropDB:
                case MySQLCommand::comQuery: {
                    mysqlPacketType = MySQLPacketTypeCollectStatement;
                    parseQueryPacket(com);
                    break;
                }
                default: {
                    setParseFail("got unknown packet");
                }
            }
        }
    } else {
        if (packet.packetLen == 1 && packet.packetNum == 1 && pktSize > 5) {
            LOG_TRACE(sLogger, ("[MYSQL TYPE]", "Result response"));
            mysqlPacketType = MySQLPacketTypeResponse;
            mysqlPacketResponse.ok = true;
            // parseResultSetPacket();
        } else {
            uint8_t v = readUint8(false);
            if (packet.packetNum == 1 && v == 0x00) {
                LOG_TRACE(sLogger, ("[MYSQL TYPE]", "ok response"));
                mysqlPacketType = MySQLPacketTypeResponse;
                mysqlPacketResponse.ok = true;
                // parseOKPacket();
            } else if (packet.packetNum == 1 && v == 0xff) {
                LOG_TRACE(sLogger, ("[MYSQL TYPE]", "error response"));
                mysqlPacketType = MySQLPacketTypeResponse;
                mysqlPacketResponse.ok = false;
                // parseErrPacket();
            } else if (packet.packetLen > 31 && pktSize > 35 && payload[13] == '\0'
                       && payload[35] == '\0') { // 40 = 36 + 4, 通过抽样payload判断
                LOG_TRACE(sLogger, ("[MYSQL TYPE]", "login Request"));
                mysqlPacketType = MySQLPacketTypeClientLogin;
                // parseLoginPacket();
            } else {
                LOG_TRACE(sLogger, ("[MYSQL TYPE]", "unknown packet"));
                setParseFail("got unknown packet");
            }
        }
    }
}

void MySQLParser::print() {
    std::cout << "===================================================" << std::endl;
    std::cout << "print" << std::endl;
    std::cout << "type:" << std::endl;
    std::cout << this->mysqlPacketType << std::endl;
    std::cout << "resp:" << std::endl;
    std::cout << this->mysqlPacketResponse.ok << std::endl;
    std::cout << "sql:" + this->mysqlPacketQuery.sql.ToString() << std::endl;
    std::cout << "===================================================" << std::endl;
}

} // end of namespace logtail
