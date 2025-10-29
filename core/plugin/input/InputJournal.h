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
    // Helper methods for configuration parsing
    void parseBasicParams(const Json::Value& config);
    void parseArrayParams(const Json::Value& config);
    void parseStringArray(const Json::Value& config, const std::string& key, std::vector<std::string>& target);

    // Configuration options
    std::string mSeekPosition;
    std::string mCursorSeekFallback;
    std::vector<std::string> mUnits;
    bool mKernel;
    std::vector<std::string> mIdentifiers;
    std::vector<std::string> mJournalPaths;
    std::vector<std::string> mMatchPatterns;
    bool mParseSyslogFacility;
    bool mParsePriority;
    bool mUseJournalEventTime;

    // Threading
    std::atomic<bool> mShutdown;

    // Seek position constants
    static const std::string kSeekPositionCursor;
    static const std::string kSeekPositionHead;
    static const std::string kSeekPositionTail;
    static const std::string kSeekPositionDefault;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class InputJournalUnittest;
#endif
};

} // namespace logtail
