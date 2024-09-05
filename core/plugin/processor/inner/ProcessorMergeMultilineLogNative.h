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

#include <vector>

#include "file_server/MultilineOptions.h"
#include "pipeline/plugin/interface/Processor.h"

namespace logtail {

class ProcessorMergeMultilineLogNative : public Processor {
public:
    enum class MergeType { BY_REGEX, BY_FLAG };

    static const std::string PartLogFlag;
    static const std::string sName;

    std::string mSourceKey = DEFAULT_CONTENT_KEY;
    MergeType mMergeType = MergeType::BY_REGEX;
    MultilineOptions mMultiline;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config) override;
    void Process(PipelineEventGroup& logGroup) override;

protected:
    bool IsSupportedEvent(const PipelineEventPtr& e) const override;

private:
    void MergeLogsByRegex(PipelineEventGroup& logGroup);
    void MergeLogsByFlag(PipelineEventGroup& logGroup);

    void HandleUnmatchLogs(
        std::vector<PipelineEventPtr>& logEvents, size_t& newSize, size_t begin, size_t end, StringView logPath);

    void MergeEvents(std::vector<LogEvent*>& logEvents, bool insertLineBreak = true);

    CounterPtr mProcMergedEventsCnt; // 成功合并了多少条日志
    // CounterPtr mProcMergedEventsBytes; // 成功合并了多少字节的日志
    CounterPtr mProcUnmatchedEventsCnt; // 未成功合并的日志条数
    // CounterPtr mProcUnmatchedEventsBytes; // 未成功合并的日志字节数

    int* mSplitLines = nullptr;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessorMergeMultilineLogNativeUnittest;
    friend class ProcessorMergeMultilineLogDisacardUnmatchUnittest;
    friend class ProcessorMergeMultilineLogKeepUnmatchUnittest;
    friend class ProcessorParseContainerLogNativeUnittest;
#endif
};

} // namespace logtail
