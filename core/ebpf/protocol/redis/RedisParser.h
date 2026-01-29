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

#include <iostream>
#include <map>
#include <memory>
#include <vector>

#include "ebpf/protocol/AbstractParser.h"
#include "ebpf/protocol/ParserRegistry.h"
#include "ebpf/type/NetworkObserverEvent.h"
#include "ebpf/util/Converger.h"
#include "ebpf/util/sampler/Sampler.h"

namespace logtail::ebpf {

namespace redis {

ParseState ParseRequest(std::string_view& buf, std::shared_ptr<RedisRecord>& result, bool forceSample = false);

ParseState ParseRequestBody(std::string_view& buf, std::shared_ptr<RedisRecord>& result);

ParseState
ParseResponse(std::string_view& buf, std::shared_ptr<RedisRecord>& result, bool closed, bool forceSample = false);
} // namespace redis


class REDISProtocolParser : public AbstractProtocolParser {
public:
    std::shared_ptr<AbstractProtocolParser> Create() override { return std::make_shared<REDISProtocolParser>(); }

    std::vector<std::shared_ptr<L7Record>> Parse(struct conn_data_event_t* dataEvent,
                                                 const std::shared_ptr<Connection>& conn,
                                                 const std::shared_ptr<AppDetail>& appDetail,
                                                 const std::shared_ptr<AppConvergerManager>& converger) override;
};

REGISTER_PROTOCOL_PARSER(support_proto_e::ProtoRedis, REDISProtocolParser)

} // namespace logtail::ebpf
