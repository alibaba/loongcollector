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

struct PgSQLProtocolEventKey {
    uint64_t Hash() {
        uint64_t hashValue = ConnKey.HashVal;
        hashValue = XXH32(this->SQL.c_str(), this->SQL.size(), hashValue);
        hashValue = XXH32(this->OK.c_str(), this->OK.size(), hashValue);
        return hashValue;
    }

    void ToPB(sls_logs::Log* log) {
        AddAnyLogContent(log, "sql", SQL);
        AddAnyLogContent(log, "success", OK);
        ConnKey.ToPB(log, ProtocolType_PgSQL);
    }

    friend std::ostream& operator<<(std::ostream& os, const PgSQLProtocolEventKey& key) {
        os << "SQL: " << key.SQL << " OK: " << key.OK << " ConnKey: " << key.ConnKey;
        return os;
    }

    std::string ToString() {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }

    std::string SQL;
    std::string OK;
    CommonAggKey ConnKey;
};

typedef CommonProtocolEvent<PgSQLProtocolEventKey> PgSQLProtocolEvent;
typedef CommonProtocolEventAggItem<PgSQLProtocolEventKey, CommonProtocolAggResult> PgSQLProtocolEventAggItem;
typedef CommonProtocolPatternGenerator<PgSQLProtocolEventKey> PgSQLProtocolPatternGenerator;
typedef CommonProtocolEventAggItemManager<PgSQLProtocolEventAggItem> PgSQLProtocolEventAggItemManager;
typedef CommonProtocolEventAggregator<PgSQLProtocolEventKey,
                                      PgSQLProtocolEvent,
                                      PgSQLProtocolEventAggItem,
                                      PgSQLProtocolPatternGenerator,
                                      PgSQLProtocolEventAggItemManager,
                                      ProtocolType_PgSQL>
    PgSQLProtocolEventAggregator;
} // namespace logtail
