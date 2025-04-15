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
#include <vector>

#include "ebpf/type/NetworkObserverEvent.h"
#include "ebpf/util/sampler/Sampler.h"

namespace logtail::ebpf {

class AbstractProtocolParser {
public:
    virtual ~AbstractProtocolParser() = default;
    virtual std::shared_ptr<AbstractProtocolParser> Create() = 0;
    virtual std::vector<std::shared_ptr<AbstractRecord>> Parse(struct conn_data_event_t* dataEvent,
                                                               const std::shared_ptr<Connection>& conn,
                                                               const std::shared_ptr<Sampler>& sampler = nullptr)
        = 0;
};

} // namespace logtail::ebpf
