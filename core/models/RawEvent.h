/*
 * Copyright 2023 iLogtail Authors
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

#pragma once

#include "models/PipelineEvent.h"

namespace logtail {

class RawEvent : public PipelineEvent {
    friend class PipelineEventGroup;
    friend class EventPool;

public:
    std::unique_ptr<PipelineEvent> Copy() const override;
    void Reset() override;

    StringView GetContent() const { return mContent; }
    void SetContent(const std::string& content);
    void SetContentNoCopy(StringView content);
    void SetContentNoCopy(const StringBuffer& content);

    size_t DataSize() const override;

#ifdef APSARA_UNIT_TEST_MAIN
    Json::Value ToJson(bool enableEventMeta = false) const override;
    bool FromJson(const Json::Value&) override;
#endif

private:
    RawEvent(PipelineEventGroup* ptr);

    StringView mContent;
};

} // namespace logtail
