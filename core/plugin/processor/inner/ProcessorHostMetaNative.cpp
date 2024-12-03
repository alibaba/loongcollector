/*
 * Copyright 2024 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ProcessorHostMetaNative.h"

#include <algorithm>
#include <memory>
#include <numeric>
#include <utility>

#include "Common.h"
#include "LogEvent.h"
#include "Logger.h"
#include "MachineInfoUtil.h"
#include "PipelineEventGroup.h"
#include "StringTools.h"
#include "constants/EntityConstants.h"

namespace logtail {

const std::string ProcessorHostMetaNative::sName = "processor_host_meta_native";

bool ProcessorHostMetaNative::Init(const Json::Value& config) {
    auto hostType = ToString(getenv(DEFAULT_ENV_KEY_HOST_TYPE.c_str()));
    std::ostringstream oss;
    if (hostType == DEFAULT_ENV_VALUE_ECS) {
        oss << DEFAULT_CONTENT_VALUE_DOMAIN_ACS << "." << DEFAULT_ENV_VALUE_ECS << "."
            << DEFAULT_CONTENT_VALUE_ENTITY_TYPE_PROCESS;
        mDomain = DEFAULT_CONTENT_VALUE_DOMAIN_ACS;
        mHostEntityID = FetchHostId();
    } else {
        oss << DEFAULT_CONTENT_VALUE_DOMAIN_INFRA << "." << DEFAULT_ENV_VALUE_HOST << "."
            << DEFAULT_CONTENT_VALUE_ENTITY_TYPE_PROCESS;
        mDomain = DEFAULT_CONTENT_VALUE_DOMAIN_INFRA;
        mHostEntityID = GetHostIp();
    }
    mEntityType = oss.str();
    return true;
}

void ProcessorHostMetaNative::Process(PipelineEventGroup& group) {
    EventsContainer& events = group.MutableEvents();
    EventsContainer newEvents;

    for (auto& event : events) {
        ProcessEvent(group, std::move(event), newEvents);
    }
    events.swap(newEvents);
}

bool ProcessorHostMetaNative::IsSupportedEvent(const PipelineEventPtr& event) const {
    return event.Is<LogEvent>();
}

void ProcessorHostMetaNative::ProcessEvent(PipelineEventGroup& group,
                                           PipelineEventPtr&& e,
                                           EventsContainer& newEvents) {
    if (!IsSupportedEvent(e)) {
        newEvents.emplace_back(std::move(e));
        return;
    }

    auto& sourceEvent = e.Cast<LogEvent>();
    std::unique_ptr<LogEvent> targetEvent = group.CreateLogEvent(true);
    targetEvent->SetTimestamp(sourceEvent.GetTimestamp());

    // TODO: support host entity
    targetEvent->SetContent(DEFAULT_CONTENT_KEY_ENTITY_TYPE, mEntityType);
    targetEvent->SetContent(DEFAULT_CONTENT_KEY_ENTITY_ID,
                            GetProcessEntityID(sourceEvent.GetContent(DEFAULT_CONTENT_KEY_PROCESS_PID),
                                               sourceEvent.GetContent(DEFAULT_CONTENT_KEY_PROCESS_CREATE_TIME)));
    targetEvent->SetContent(DEFAULT_CONTENT_KEY_DOMAIN, mDomain);
    targetEvent->SetContent(DEFAULT_CONTENT_KEY_FIRST_OBSERVED_TIME,
                            sourceEvent.GetContent(DEFAULT_CONTENT_KEY_PROCESS_CREATE_TIME));
    targetEvent->SetContent(DEFAULT_CONTENT_KEY_LAST_OBSERVED_TIME, group.GetMetadata(EventGroupMetaKey::COLLECT_TIME));
    targetEvent->SetContent(DEFAULT_CONTENT_KEY_KEEP_ALIVE_SECONDS, "30");
    // TODO: support delete event
    targetEvent->SetContent(DEFAULT_CONTENT_KEY_METHOD, DEFAULT_CONTENT_VALUE_METHOD_UPDATE);

    targetEvent->SetContent("pid", sourceEvent.GetContent(DEFAULT_CONTENT_KEY_PROCESS_PID));
    targetEvent->SetContent("ppid", sourceEvent.GetContent(DEFAULT_CONTENT_KEY_PROCESS_PPID));
    targetEvent->SetContent("user", sourceEvent.GetContent(DEFAULT_CONTENT_KEY_PROCESS_USER));
    targetEvent->SetContent("comm", sourceEvent.GetContent(DEFAULT_CONTENT_KEY_PROCESS_COMM));
    targetEvent->SetContent("create_time", sourceEvent.GetContent(DEFAULT_CONTENT_KEY_PROCESS_CREATE_TIME));
    targetEvent->SetContent("cwd", sourceEvent.GetContent(DEFAULT_CONTENT_KEY_PROCESS_CWD));
    targetEvent->SetContent("binary", sourceEvent.GetContent(DEFAULT_CONTENT_KEY_PROCESS_BINARY));
    targetEvent->SetContent("arguments", sourceEvent.GetContent(DEFAULT_CONTENT_KEY_PROCESS_ARGUMENTS));
    targetEvent->SetContent("language", sourceEvent.GetContent(DEFAULT_CONTENT_KEY_PROCESS_LANGUAGE));
    targetEvent->SetContent("containerID", sourceEvent.GetContent(DEFAULT_CONTENT_KEY_PROCESS_CONTAINER_ID));
    newEvents.emplace_back(std::move(targetEvent), true, nullptr);
}


const std::string ProcessorHostMetaNative::GetProcessEntityID(StringView pid, StringView createTime) {
    std::ostringstream oss;
    oss << mHostEntityID << pid << createTime;
    auto bigID = sdk::CalcMD5(oss.str());
    std::transform(bigID.begin(), bigID.end(), bigID.begin(), ::tolower);
    return bigID;
}

} // namespace logtail
