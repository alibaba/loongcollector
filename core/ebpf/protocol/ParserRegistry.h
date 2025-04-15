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

#include <functional>
#include <memory>
#include <unordered_map>

#include "AbstractParser.h"
extern "C" {
#include <coolbpf/net.h>
}

namespace logtail::ebpf {

class ProtocolParserRegistry {
public:
    using CreatorFunc = std::function<std::shared_ptr<AbstractProtocolParser>()>;

    static ProtocolParserRegistry& GetInstance() {
        static ProtocolParserRegistry sRegistry;
        return sRegistry;
    }

    void RegisterParser(support_proto_e type, CreatorFunc creator) { mRegistry[type] = std::move(creator); }

    std::shared_ptr<AbstractProtocolParser> CreateParser(support_proto_e type) {
        if (mRegistry.find(type) != mRegistry.end()) {
            return mRegistry[type]();
        }
        return nullptr;
    }

private:
    ProtocolParserRegistry() = default;
    std::unordered_map<support_proto_e, CreatorFunc> mRegistry;
};

#define REGISTER_PROTOCOL_PARSER(type, className) \
    namespace { \
    struct className##AutoRegister { \
        className##AutoRegister() { \
            ProtocolParserRegistry::GetInstance().RegisterParser(type, \
                                                                 []() { return std::make_shared<className>(); }); \
        } \
    }; \
    static className##AutoRegister global_##className##AutoRegister_instance; \
    }
} // namespace logtail::ebpf
