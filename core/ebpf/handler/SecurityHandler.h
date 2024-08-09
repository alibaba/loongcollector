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
#include <string>

#include "ebpf/handler/AbstractHandler.h"
#include "ebpf/include/export.h"

namespace logtail {
namespace ebpf {

class SecurityHandler : public AbstractHandler {
public:
    SecurityHandler(const logtail::PipelineContext* ctx, logtail::QueueKey key, uint32_t idx);
    void handle(std::vector<std::unique_ptr<AbstractSecurityEvent>>&& events);
private:
    // TODO 后续这两个 key 需要移到 group 的 metadata 里，在 processortagnative 中转成tag
    std::string mHostIp;
    std::string mHostName;
};

}
}
