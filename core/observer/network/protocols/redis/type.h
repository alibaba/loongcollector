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

#include "interface/protocol.h"
#include "network/protocols/common.h"
#include <string>
#include <ostream>
#include "common/xxhash/xxhash.h"

namespace logtail {

struct RedisProtocolEventKey {
    uint64_t Hash() {
        uint64_t hashValue = ConnKey.HashVal;
        hashValue = XXH32(this->Cmd.c_str(), this->Cmd.size(), hashValue);
        hashValue = XXH32(this->OK.c_str(), this->OK.size(), hashValue);
        return hashValue;
    }

    void ToPB(sls_logs::Log* log) {
        AddAnyLogContent(log, "cmd", Cmd);
        AddAnyLogContent(log, "success", OK);
        ConnKey.ToPB(log, ProtocolType_PgSQL);
    }

    friend std::ostream& operator<<(std::ostream& os, const RedisProtocolEventKey& key) {
        os << "Cmd: " << key.Cmd << " OK: " << key.OK << " ConnKey: " << key.ConnKey;
        return os;
    }

    std::string ToString() {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }

    std::string Cmd;
    std::string OK;
    CommonAggKey ConnKey;
};

typedef CommonProtocolEvent<RedisProtocolEventKey> RedisProtocolEvent;
typedef CommonProtocolEventAggItem<RedisProtocolEventKey, CommonProtocolAggResult> RedisProtocolEventAggItem;
typedef CommonProtocolPatternGenerator<RedisProtocolEventKey> RedisProtocolPatternGenerator;
typedef CommonProtocolEventAggItemManager<RedisProtocolEventAggItem> RedisProtocolEventAggItemManager;
typedef CommonProtocolEventAggregator<RedisProtocolEventKey,
                                      RedisProtocolEvent,
                                      RedisProtocolEventAggItem,
                                      RedisProtocolPatternGenerator,
                                      RedisProtocolEventAggItemManager,
                                      ProtocolType_Redis>
    RedisProtocolEventAggregator;
} // namespace logtail
