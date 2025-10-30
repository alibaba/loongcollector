/*
 * Copyright 2024 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <atomic>
#include <string>
#include <vector>

#include "json/json.h"

#include "collection_pipeline/plugin/interface/Input.h"

namespace logtail {

// Forward declarations
class JournalEntry;

class InputJournal : public Input {
public:
    static const std::string sName;

    InputJournal();
    ~InputJournal();

    // Delete copy and move operations
    InputJournal(const InputJournal&) = delete;
    InputJournal& operator=(const InputJournal&) = delete;
    InputJournal(InputJournal&&) = delete;
    InputJournal& operator=(InputJournal&&) = delete;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) override;
    bool Start() override;
    bool Stop(bool isPipelineRemoving) override;
    bool SupportAck() const override { return true; }

private:
    // 原始 JSON 配置（用于在 Start() 时使用 JournalConfig::ParseFromJson() 解析）
    Json::Value mConfigJson;

    // Threading
    std::atomic<bool> mShutdown;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class InputJournalUnittest;
#endif
};

} // namespace logtail
