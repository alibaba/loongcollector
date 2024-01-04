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

#include "spl/PipelineEventGroupOutput.h"

#include <unistd.h>

#include <boost/container_hash/hash.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "common/HashUtil.h"
#include "logger/Logger.h"
#include "spl/SplConstants.h"


namespace apsara::sls::spl {

void PipelineEventGroupOutput::setHeader(const IOHeader& header, std::string& err) {
    mIOHeader = &header;
    for (size_t i = 0; i < header.columnNames.size(); i++) {
        auto field = header.columnNames[i].ToString();
        auto length = field.length();
        if (length >= LENGTH_FIELD_PREFIX_TAG
            && field.compare(0, LENGTH_FIELD_PREFIX_TAG, FIELD_PREFIX_TAG) == 0) { // __tag__:*
            mTagsIdxs.push_back(i);
            mColumns.emplace_back(mLogGroup->GetSourceBuffer()->CopyString(
                StringView(mIOHeader->columnNames[i].mPtr, mIOHeader->columnNames[i].mLen)
                    .substr(LENGTH_FIELD_PREFIX_TAG)));
        } else { // content
            mContentsIdxs.push_back(i);
            mColumns.emplace_back(
                mLogGroup->GetSourceBuffer()->CopyString(header.columnNames[i].mPtr, header.columnNames[i].mLen));
        }
    }
}

void PipelineEventGroupOutput::addRow(const std::vector<SplStringPiece>& row,
                                      const uint32_t time,
                                      const uint32_t timeNsPart,
                                      const ErrorKV& errorKV,
                                      std::string& error) {
    std::unique_ptr<LogEvent> targetEvent = LogEvent::CreateEvent(mLogGroup->GetSourceBuffer());

    size_t tagStrHash = 0;
    for (const auto& idxTag : mTagsIdxs) {
        boost::hash_combine(tagStrHash, mIOHeader->columnNames[idxTag].hash());
        boost::hash_combine(tagStrHash, row[idxTag].hash());
    }

    if (mLogGroupList->empty() || tagStrHash != lastTagStrHash) {
        mLogGroupList->emplace_back(mLogGroup->GetSourceBuffer());
        mLogGroupList->back().SetAllMetadata(mLogGroup->GetAllMetadata());
    }

    PipelineEventGroup& current = mLogGroupList->back();
    lastTagStrHash = tagStrHash;

    targetEvent->SetTimestamp(time, timeNsPart);

    for (const auto& idxContent : mContentsIdxs) {
        if (!row[idxContent].hasValue()) {
            continue;
        }
        if (row[idxContent].isNull()) {
            targetEvent->SetContent(mColumns[idxContent], StringView(NULL_STR.c_str(), NULL_STR.length()));
        } else {
            targetEvent->SetContent(mColumns[idxContent], StringView(row[idxContent].mPtr, row[idxContent].mLen));
        }
    }

    for (const auto& idxTag : mTagsIdxs) {
        current.SetTag(mColumns[idxTag], StringView(row[idxTag].mPtr, row[idxTag].mLen));
    }

    current.AddEvent(std::move(targetEvent));
    if (!errorKV.second.empty()) {
        LOG_WARNING(sLogger,
                    ("__error__", errorKV.second)("project", mContext->GetProjectName())("logstore",
                                                                                         mContext->GetLogstoreName())(
                        "region", mContext->GetRegion())("configName", mContext->GetConfigName()));
    }
    mRowCount++;
}

bool PipelineEventGroupOutput::isColumnar() {
    return false;
}

} // namespace apsara::sls::spl
