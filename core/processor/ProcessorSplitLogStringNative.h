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

#include "plugin/interface/Processor.h"
#include <string>
#include <boost/regex.hpp>

namespace logtail {

class ProcessorSplitLogStringNative : public Processor {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const ComponentConfig& componentConfig) override;
    void Process(PipelineEventGroup& logGroup) override;

protected:
    bool IsSupportedEvent(const PipelineEventPtr& e) const override;

private:
    void ProcessEvent(PipelineEventGroup& logGroup, const PipelineEventPtr& e, EventsContainer& newEvents);
    void LogSplit(const char* buffer, int32_t size, int32_t& lineFeed, std::vector<StringView>& logIndex);
    int* mFeedLines = nullptr;
    int* mSplitLines = nullptr;
    std::string mSplitKey;
    char mSplitChar = '\n';
    bool mEnableLogPositionMeta = false;
#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessorRegexStringNativeUnittest;
#endif
};

} // namespace logtail