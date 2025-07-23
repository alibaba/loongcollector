// Copyright 2025 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "AbstractParser.h"
#include "ParserRegistry.h"
#include "common/Lock.h"
#include "ebpf/type/NetworkObserverEvent.h"
#include "ebpf/util/sampler/Sampler.h"
#include "http/HttpParser.h"

extern "C" {
#include <coolbpf/net.h>
}

namespace logtail::ebpf {

class ProtocolParserManager {
public:
    ProtocolParserManager(const ProtocolParserManager&) = delete;
    ProtocolParserManager& operator=(const ProtocolParserManager&) = delete;

    // singleton
    static ProtocolParserManager& GetInstance() {
        static ProtocolParserManager sInstance;
        return sInstance;
    }

    bool AddParser(const std::string& protocol);
    bool RemoveParser(const std::string& protocol);
    bool AddParser(support_proto_e type);
    bool RemoveParser(support_proto_e type);
    std::set<support_proto_e> AvaliableProtocolTypes() const;

    std::vector<std::shared_ptr<AbstractRecord>> Parse(support_proto_e type,
                                                       const std::shared_ptr<Connection>& conn,
                                                       struct conn_data_event_t* data,
                                                       const std::shared_ptr<Sampler>& sampler = nullptr);

private:
    ProtocolParserManager() {}
    ReadWriteLock mLock;
    std::unordered_map<support_proto_e, std::shared_ptr<AbstractProtocolParser>> mParsers;
#ifdef APSARA_UNIT_TEST_MAIN
    friend class NetworkObserverManagerUnittest;
#endif
};

} // namespace logtail::ebpf
