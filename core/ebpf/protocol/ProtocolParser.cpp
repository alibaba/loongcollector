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

#include "ProtocolParser.h"

#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "common/magic_enum.hpp"
#include "ebpf/plugin/network_observer/Connection.h"
#include "logger/Logger.h"

extern "C" {
#include <coolbpf/net.h>
}

namespace logtail::ebpf {

std::set<support_proto_e> ProtocolParserManager::AvaliableProtocolTypes() const {
    return {support_proto_e::ProtoHTTP};
}

support_proto_e ProtocolStringToEnum(std::string protocol) {
    std::transform(protocol.begin(), protocol.end(), protocol.begin(), [](unsigned char c) { return std::toupper(c); });
    if (protocol == "HTTP") {
        return support_proto_e::ProtoHTTP;
    }

    return support_proto_e::ProtoUnknown;
}

bool ProtocolParserManager::AddParser(const std::string& protocol) {
    auto pro = ProtocolStringToEnum(protocol);
    if (pro == support_proto_e::ProtoUnknown) {
        return false;
    }
    return AddParser(pro);
}

bool ProtocolParserManager::RemoveParser(const std::string& protocol) {
    auto pro = ProtocolStringToEnum(protocol);
    if (pro == support_proto_e::ProtoUnknown) {
        return false;
    }
    return RemoveParser(pro);
}

bool ProtocolParserManager::AddParser(support_proto_e type) {
    if (!AvaliableProtocolTypes().count(type)) {
        LOG_WARNING(sLogger, ("protocol not supported", magic_enum::enum_name(type)));
        return false;
    }
    WriteLock lock(mLock);
    auto parser = ProtocolParserRegistry::GetInstance().CreateParser(type);
    if (parser) {
        LOG_DEBUG(sLogger, ("add protocol parser", std::string(magic_enum::enum_name(type))));
        mParsers[type] = std::move(parser);
        return true;
    }
    LOG_ERROR(sLogger, ("No parser available for type ", magic_enum::enum_name(type)));

    return false;
}

bool ProtocolParserManager::RemoveParser(support_proto_e type) {
    WriteLock lock(mLock);
    if (mParsers.count(type)) {
        LOG_DEBUG(sLogger, ("remove protocol parser", std::string(magic_enum::enum_name(type))));
        mParsers.erase(type);
    } else {
        LOG_INFO(sLogger, ("No parser for type ", magic_enum::enum_name(type)));
    }

    return true;
}


std::vector<std::shared_ptr<AbstractRecord>> ProtocolParserManager::Parse(support_proto_e type,
                                                                          const std::shared_ptr<Connection>& conn,
                                                                          struct conn_data_event_t* data,
                                                                          const std::shared_ptr<Sampler>& sampler) {
    ReadLock lock(mLock);
    if (mParsers.find(type) != mParsers.end()) {
        return mParsers[type]->Parse(data, conn, sampler);
    }

    LOG_ERROR(sLogger, ("No parser found for given protocol type", std::string(magic_enum::enum_name(type))));
    return std::vector<std::shared_ptr<AbstractRecord>>();
}

} // namespace logtail::ebpf
