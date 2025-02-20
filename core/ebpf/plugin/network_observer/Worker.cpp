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

#include "Worker.h"

#include "common/magic_enum.hpp"
#include "ebpf/protocol/ProtocolParser.h"
#include "logger/Logger.h"

extern "C" {
#include <net.h>
};

namespace logtail {
namespace ebpf {

int count_ = 0;
void NetDataHandler::operator()(std::unique_ptr<NetDataEvent>& evt, ResultQueue& resQueue) {
    if (!evt) {
        return;
    }
    LOG_DEBUG(sLogger, ("[NetDataHandler] begin to handle data event ... total count", ++count_));

    // get protocol
    ProtocolType protocol = static_cast<ProtocolType>(evt->mProtocol);
    if (ProtocolType::UNKNOWN == protocol) {
        LOG_DEBUG(sLogger, ("[NetDataHandler] protocol is unknown, skip parse", ""));
        evt.reset();
        return;
    }

    LOG_DEBUG(sLogger, ("[NetDataHandler] begin parse, protocol is", std::string(magic_enum::enum_name(protocol))));

    std::vector<std::unique_ptr<AbstractRecord>> records
        = ProtocolParserManager::GetInstance().Parse(protocol, std::move(evt));

    // add records to span/event generate queue
    for (auto& record : records) {
        resQueue.enqueue(std::move(record));
    }
}

} // namespace ebpf
} // namespace logtail
