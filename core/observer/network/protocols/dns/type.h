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

struct DNSProtocolEventKey {
    uint64_t Hash() {
        uint64_t hashValue = ConnKey.HashVal;
        hashValue = XXH32(this->IsSuccess.c_str(), this->IsSuccess.size(), hashValue);
        hashValue = XXH32(this->QueryRecord.c_str(), this->QueryRecord.size(), hashValue);
        hashValue = XXH32(this->QueryType.c_str(), this->QueryType.size(), hashValue);
        return hashValue;
    }

    void ToPB(sls_logs::Log* log) {
        AddAnyLogContent(log, "query_record", QueryRecord);
        AddAnyLogContent(log, "query_type", QueryType);
        AddAnyLogContent(log, "success", IsSuccess);
        ConnKey.ToPB(log, ProtocolType_DNS);
    }

    friend std::ostream& operator<<(std::ostream& os, const DNSProtocolEventKey& key) {
        os << "QueryRecord: " << key.QueryRecord << " QueryType: " << key.QueryType << " IsSuccess: " << key.IsSuccess
           << " ConnKey: " << key.ConnKey;
        return os;
    }

    std::string ToString() {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }

    std::string QueryRecord;
    std::string QueryType;
    std::string IsSuccess;
    CommonAggKey ConnKey;
};

typedef CommonProtocolEvent<DNSProtocolEventKey> DNSProtocolEvent;
typedef CommonProtocolEventAggItem<DNSProtocolEventKey, CommonProtocolAggResult> DNSProtocolEventAggItem;
typedef CommonProtocolPatternGenerator<DNSProtocolEventKey> DNSProtocolPatternGenerator;
typedef CommonProtocolEventAggItemManager<DNSProtocolEventAggItem> DNSProtocolEventAggItemManager;
typedef CommonProtocolEventAggregator<DNSProtocolEventKey,
                                      DNSProtocolEvent,
                                      DNSProtocolEventAggItem,
                                      DNSProtocolPatternGenerator,
                                      DNSProtocolEventAggItemManager,
                                      ProtocolType_DNS>
    DNSProtocolEventAggregator;

} // namespace logtail
