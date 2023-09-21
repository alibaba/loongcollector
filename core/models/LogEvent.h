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

using LogContents = std::map<StringView, StringView>;
class LogEvent : public PipelineEvent {
public:
    static std::unique_ptr<LogEvent> CreateEvent(std::shared_ptr<SourceBuffer>& sb);
    const LogContents& GetContents() const { return contents; }
    LogContents& MutableContents() { return contents; }
    void SetContent(const StringView& key, const StringView& val);
    void SetContent(const std::string& key, const std::string& val);
    void SetContentNoCopy(const StringBuffer& key, const StringBuffer& val);
    const StringView& GetContent(const StringView& key) const;
    bool HasContent(const StringView& key) const;
    void SetContentNoCopy(const StringView& key, const StringView& val);
    void DelContent(const StringView& key);

    // for debug and test
    Json::Value ToJson() const override;
    bool FromJson(const Json::Value&) override;
    uint64_t EventsSizeBytes() override;

private:
    LogEvent();

    LogContents contents;
};

} // namespace logtail