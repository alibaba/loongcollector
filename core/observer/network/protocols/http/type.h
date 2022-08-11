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

struct HTTPProtocolEventKey {
    uint64_t Hash() const {
        uint64_t hashValue = ConnKey.HashVal;
        hashValue = XXH32(this->Version.c_str(), this->Version.size(), hashValue);
        hashValue = XXH32(this->Method.c_str(), this->Method.size(), hashValue);
        hashValue = XXH32(this->URL.c_str(), this->URL.size(), hashValue);
        hashValue = XXH32(this->Host.c_str(), this->Host.size(), hashValue);
        hashValue = XXH32(this->RespCode.c_str(), this->RespCode.size(), hashValue);
        return hashValue;
    }

    void ToPB(sls_logs::Log* log) const {
        AddAnyLogContent(log, "version", Version);
        AddAnyLogContent(log, "host", Host);
        AddAnyLogContent(log, "method", Method);
        AddAnyLogContent(log, "url", URL);
        AddAnyLogContent(log, "resp_code", RespCode);
        ConnKey.ToPB(log, ProtocolType_HTTP);
    }

    friend std::ostream& operator<<(std::ostream& os, const HTTPProtocolEventKey& key) {
        os << "Version: " << key.Version << " Method: " << key.Method << " URL: " << key.URL << " Host: " << key.Host
           << " RespCode: " << key.RespCode << " ConnKey: " << key.ConnKey;
        return os;
    }

    std::string ToString() {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }

    std::string Version;
    std::string Method;
    std::string URL;
    std::string Host;
    std::string RespCode;
    CommonAggKey ConnKey;
};

typedef CommonProtocolEvent<HTTPProtocolEventKey> HTTPProtocolEvent;
typedef CommonProtocolEventAggItem<HTTPProtocolEventKey, CommonProtocolAggResult> HTTPProtocolEventAggItem;
typedef CommonProtocolPatternGenerator<HTTPProtocolEventKey> HTTPProtocolPatternGenerator;
typedef CommonProtocolEventAggItemManager<HTTPProtocolEventAggItem> HTTPProtocolEventAggItemManager;
typedef CommonProtocolEventAggregator<HTTPProtocolEventKey,
                                      HTTPProtocolEvent,
                                      HTTPProtocolEventAggItem,
                                      HTTPProtocolPatternGenerator,
                                      HTTPProtocolEventAggItemManager,
                                      ProtocolType_HTTP>
    HTTPProtocolEventAggregator;

} // namespace logtail
