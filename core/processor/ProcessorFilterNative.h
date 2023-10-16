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

#include "plugin/interface/Processor.h"
#include "LogFilter.h"

namespace logtail {

class ProcessorFilterNative : public Processor {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const ComponentConfig& componentConfig) override;
    void Process(PipelineEventGroup& logGroup) override;
    ~ProcessorFilterNative();

protected:
    bool IsSupportedEvent(const PipelineEventPtr& e) const override;

private:
    enum Mode { BYPASS_MODE, EXPRESSION_MODE, RULE_MODE, GLOBAL_MODE };
    std::shared_ptr<LogFilterRule> mFilterRule;
    BaseFilterNodePtr mFilterExpressionRoot = nullptr;
    std::unordered_map<std::string, LogFilterRule*> mFilters;
    LogType mLogType;
    bool mDiscardNoneUtf8;
    Mode mFilterMode;

    CounterPtr mProcFilterInSizeBytes;
    CounterPtr mProcFilterOutSizeBytes;
    CounterPtr mProcFilterErrorTotal;
    CounterPtr mProcFilterRecordsTotal;

    bool LoadOldGlobalConfig(const PipelineConfig& componentConfig);
    bool FilterExpressionRoot(LogEvent& sourceEvent, const BaseFilterNodePtr& node);
    bool FilterFilterRule(LogEvent& sourceEvent, const LogFilterRule* filterRule);
    bool FilterGlobal(LogEvent& sourceEvent);

    bool ProcessEvent(PipelineEventPtr& e);
    bool IsMatched(const LogContents& contents, const LogFilterRule& rule);

    bool noneUtf8(StringView & strSrc, bool modify);
    bool CheckNoneUtf8(const StringView & strSrc);
    void FilterNoneUtf8(std::string& strSrc);

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessorFilterNativeUnittest;
#endif
};

} // namespace logtail