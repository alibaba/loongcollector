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
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <functional>

#include "collection_pipeline/plugin/interface/Input.h"
#include "json/json.h"

namespace logtail {

// Forward declarations
class JournalEntry;

class InputJournal : public Input {
public:
    static const std::string sName;

    InputJournal();
    ~InputJournal();

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) override;
    bool Start() override;
    bool Stop(bool isPipelineRemoving) override;
    bool SupportAck() const override { return true; }

private:
    // Configuration options
    std::string mSeekPosition;
    int mCursorFlushPeriodMs;
    std::string mCursorSeekFallback;
    std::vector<std::string> mUnits;
    bool mKernel;
    std::vector<std::string> mIdentifiers;
    std::vector<std::string> mJournalPaths;
    std::vector<std::string> mMatchPatterns;
    bool mParseSyslogFacility;
    bool mParsePriority;
    bool mUseJournalEventTime;
    int mResetIntervalSecond;

    // Runtime state
    // 不再需要 JournalReader，JournalServer 会处理所有数据
    
    // Threading
    std::atomic<bool> mShutdown;
    // 不再需要线程管理，JournalServer 会处理所有数据

    // Constants
    static constexpr int DEFAULT_RESET_INTERVAL = 3600; // 1 hour
    static constexpr int DEFAULT_CURSOR_FLUSH_PERIOD_MS = 5000; // 5 seconds

    // 不再需要这些辅助方法，JournalServer 会处理所有 journal 操作
    
    // Seek position constants
    static const std::string SEEK_POSITION_CURSOR;
    static const std::string SEEK_POSITION_HEAD;
    static const std::string SEEK_POSITION_TAIL;
    static const std::string SEEK_POSITION_DEFAULT;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class InputJournalUnittest;
#endif
};

} // namespace logtail 