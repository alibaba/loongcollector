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

#include "models/LogEvent.h"
#include "models/PipelineEventGroup.h"
#include "pipeline/PipelineContext.h"
#include "spl/rw/IO.h"

using namespace logtail;

namespace apsara::sls::spl {

class PipelineEventGroupInput : public Input {
public:
    PipelineEventGroupInput(const std::vector<std::string> columnNames,
                            const PipelineEventGroup& logGroup,
                            const PipelineContext& context)
        : mColumnNames(columnNames), mLogGroup(&logGroup), mContext(&context) {}

    ~PipelineEventGroupInput() {}

    virtual void getHeader(IOHeader& header, std::string& err);
    virtual void getColumn(const int32_t colIndex, std::vector<SplStringPiece>& values, std::string& err);
    virtual void getTimeColumns(std::vector<uint32_t>& times, std::vector<uint32_t>& timeNanos, std::string& err);
    virtual bool isColumnar();
    virtual bool hasTime();


private:
    std::vector<std::string> mColumnNames;

    std::vector<std::string> mTmpTags;
    const PipelineEventGroup* mLogGroup;
    const PipelineContext* mContext;
};

using PipelineEventGroupInputPtr = std::shared_ptr<PipelineEventGroupInput>;

} // namespace apsara::sls::spl
