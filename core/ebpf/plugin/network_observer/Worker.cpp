//
// Created by qianlu on 2024/5/21.
//

#include "Worker.h"
#include "ebpf/protocol/ProtocolParser.h"
#include "logger/Logger.h"
#include "common/magic_enum.hpp"

extern "C" {
#include <net.h>
};

namespace logtail {
namespace ebpf {

int count_ = 0;
void NetDataHandler::operator()(std::unique_ptr<NetDataEvent>& evt, ResultQueue& resQueue) {
    LOG_DEBUG(sLogger, ("[NetDataHandler] begin to handle data event ... total count", ++count_));

    // get protocol
    ProtocolType protocol = static_cast<ProtocolType>(evt->protocol);
    if (ProtocolType::UNKNOWN == protocol) {
        LOG_DEBUG(sLogger, ("[NetDataHandler] protocol is unknown, skip parse", ""));
        evt.reset();
        return;
    }

    LOG_DEBUG(sLogger, ("[NetDataHandler] begin parse, protocol is", std::string(magic_enum::enum_name(protocol))));

    std::vector<std::unique_ptr<AbstractRecord>> records =
        ProtocolParserManager::GetInstance().Parse(protocol, std::move(evt));
    
    // add records to span/event generate queue
    for (auto& record : records) {
        resQueue.enqueue(std::move(record));
    }
}

// template class WorkerPool<std::unique_ptr<NetDataEvent>, std::unique_ptr<AbstractRecord>>;
}
}


