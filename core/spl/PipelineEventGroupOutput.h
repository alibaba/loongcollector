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

#include <spl/rw/IO.h>

#include "models/LogEvent.h"
#include "models/PipelineEventGroup.h"
#include "pipeline/PipelineContext.h"

using namespace logtail;

namespace apsara::sls::spl {

class PipelineEventGroupOutput : public Output {
public:
    PipelineEventGroupOutput(PipelineEventGroup& logGroup,
                             std::vector<PipelineEventGroup>& logGroups,
                             const PipelineContext& context,
                             const std::string& taskLabel = "",
                             bool withSleep = false)
        : mLogGroup(&logGroup),
          mLogGroupList(&logGroups),
          mContext(&context),
          mTaskLabel(taskLabel),
          mWithSleep(withSleep) {}
    virtual ~PipelineEventGroupOutput() {}
    virtual void setHeader(const IOHeader& header, std::string& err);
    virtual void addRow(const std::vector<SplStringPiece>& row,
                        const uint32_t time,
                        const uint32_t timeNsPart,
                        const ErrorKV& errorKV,
                        std::string& error);
    virtual bool isColumnar();

private:
    int32_t mRowCount;

    const IOHeader* mIOHeader;

    PipelineEventGroup* mLogGroup = nullptr;
    std::vector<PipelineEventGroup>* mLogGroupList;
    const PipelineContext* mContext;

    std::string mTaskLabel;
    bool mWithSleep;


    int32_t mTimeIdx = -1;
    int32_t mTimeNSIdx = -1;
    std::vector<int32_t> mTagsIdxs;
    std::vector<int32_t> mContentsIdxs;
    size_t lastTagStrHash = -1;

    std::vector<StringBuffer> mColumns;
};

using PipelineEventGroupOutputPtr = std::shared_ptr<PipelineEventGroupOutput>;

} // namespace apsara::sls::spl
