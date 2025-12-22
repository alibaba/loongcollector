//
// Created by shuyi on 2025/11/03.
//

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

namespace mysql {

ParseState ParseRequest(std::string_view& buf, std::shared_ptr<MysqlRecord>& result, bool forceSample = false);

ParseState
ParseResponse(std::string_view& buf, std::shared_ptr<MysqlRecord>& result, bool closed, bool forceSample = false);
} // namespace mysql


class MYSQLProtocolParser : public AbstractProtocolParser {
public:
    std::shared_ptr<AbstractProtocolParser> Create() override { return std::make_shared<MYSQLProtocolParser>(); }

    std::vector<std::shared_ptr<L7Record>> Parse(struct conn_data_event_t* dataEvent,
                                                 const std::shared_ptr<Connection>& conn,
                                                 const std::shared_ptr<AppDetail>& appDetail,
                                                 const std::shared_ptr<AppConvergerManager>& converger) override;
};

REGISTER_PROTOCOL_PARSER(support_proto_e::ProtoMySQL, MYSQLProtocolParser)

} // namespace logtail::ebpf
