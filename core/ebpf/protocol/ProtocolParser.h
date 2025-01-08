// Copyright 2023 iLogtail Authors
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

#include <vector>
#include <memory>
#include <unordered_map>

#include "ebpf/type/NetworkObserverEvent.h"
#include "AbstractParser.h"
#include "ParserRegistry.h"
#include "http/HttpParser.h"


namespace logtail {
namespace ebpf {

class ParseResult {
    int status;
    std::vector<std::unique_ptr<AbstractRecord>> records;
};

class ProtocolParserManager {
public:
    ProtocolParserManager(const ProtocolParserManager&) = delete;
    ProtocolParserManager& operator=(const ProtocolParserManager&) = delete;

    // singleton
    static ProtocolParserManager& GetInstance() {
        static ProtocolParserManager instance;
        return instance;
    }

    void Init();

    std::vector<std::unique_ptr<AbstractRecord>> Parse(ProtocolType type, std::unique_ptr<NetDataEvent> data);

private:
    ProtocolParserManager() {}
    std::unordered_map<ProtocolType, std::shared_ptr<AbstractProtocolParser>> parsers_;
};

}
}

