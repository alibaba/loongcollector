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

#include "ebpf/handler/SecurityHandler.h"
#include "logger/Logger.h"
#include "pipeline/PipelineContext.h"
#include "common/RuntimeUtil.h"
#include "ebpf/SourceManager.h"
#include "models/SpanEvent.h"
#include "models/PipelineEventGroup.h"
#include "models/PipelineEvent.h"
#include "logger/Logger.h"
#include "pipeline/queue/ProcessQueueManager.h"
#include "pipeline/queue/ProcessQueueItem.h"
#include "common/MachineInfoUtil.h"

namespace logtail {
namespace ebpf {

SecurityHandler::SecurityHandler(const logtail::PipelineContext* ctx, logtail::QueueKey key, uint32_t idx) 
    : AbstractHandler(ctx, key, idx) {
    mHostName = GetHostName();
    mHostIp = GetHostIp();
}

void SecurityHandler::handle(std::vector<std::unique_ptr<AbstractSecurityEvent>>& events) {
    if (events.empty()) {
        return ;
    }

    std::shared_ptr<SourceBuffer> source_buffer = std::make_shared<SourceBuffer>();;
    PipelineEventGroup event_group(source_buffer);
    // aggregate to pipeline event group
    // set host ips
    // TODO 后续这两个 key 需要移到 group 的 metadata 里，在 processortagnative 中转成tag
    const static std::string host_ip_key = "host.ip";
    const static std::string host_name_key = "host.name";
    event_group.SetTag(host_ip_key, mHostIp);
    event_group.SetTag(host_name_key, mHostName);
    for (const auto& x : events) {
        auto event = event_group.AddLogEvent();
        for (const auto& tag : x->GetAllTags()) {
            event->SetContent(tag.first, tag.second);
        }
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::nanoseconds(x->GetTimestamp()));
        event->SetTimestamp(seconds.count(), x->GetTimestamp());
    }
    mProcessTotalCnt+= events.size();
#ifdef APSARA_UNIT_TEST_MAIN
    return;
#endif
    std::unique_ptr<ProcessQueueItem> item = 
            std::unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(event_group), mPluginIdx));
    
    if (ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item))) {
        LOG_WARNING(sLogger, ("configName", mCtx->GetConfigName())("pluginIdx",mPluginIdx)("Push queue failed!", events.size()));
    }
}

}
}
