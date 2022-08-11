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
#include "interface/type.h"

namespace logtail {
enum class PgSQLPacketType {
    Unknown,
    Query,
    Response,
    IgnoreQuery,
};

struct PgSQLPacketQuery {
    SlsStringPiece sql;
};

struct PgSQLPacketResponse {
    bool ok;
};

class PgSQLParser : public ProtoParser {
public:
    PgSQLParser(const char* payload, const size_t pktSize) : ProtoParser(payload, pktSize, true) {}


    void parse(MessageType messageType);

    PgSQLPacketQuery query;
    PgSQLPacketResponse resp{};
    PgSQLPacketType type;

private:
    bool parseStartUpMsg();

    void parseRegularMsg(MessageType messageType);

    bool parseRegularResponseMsg(bool& foundRowDesc, bool& foundCmdCmpl, bool& foundParseCmpl, bool& foundError);
};

} // namespace logtail