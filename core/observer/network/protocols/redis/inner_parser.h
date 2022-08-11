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

#include <vector>
#include <map>
#include <iostream>

#include "network/protocols/utils.h"

namespace logtail {
struct RedisData {
    std::vector<SlsStringPiece> data;
    bool isError;

    std::string GetCommands() {
        std::string cmd;
        for (auto iter = data.begin(); iter < data.end(); iter++) {
            if ((iter + 1) != data.end()) {
                cmd.append(iter->mPtr, iter->mLen).append(" ");
            } else {
                cmd.append(iter->mPtr, iter->mLen);
            }
        }
        return cmd;
    }
};

class RedisParser : public ProtoParser {
public:
    RedisParser(const char* payload, const size_t pktSize) : ProtoParser(payload, pktSize, true) {
        redisData.isError = false;
    }

    SlsStringPiece readUtilNewLine();

    void readData(std::vector<SlsStringPiece>& data);

    void parse();

    void print();

public:
    RedisData redisData;
};

} // end of namespace logtail