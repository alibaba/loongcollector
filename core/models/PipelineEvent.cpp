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

#include <sstream>

#include "logger/Logger.h"
#include "models/PipelineEventGroup.h"

namespace logtail {

StringView gEmptyStringView;

const std::string& PipelineEventTypeToString(PipelineEvent::Type t) {
    switch (t) {
        case PipelineEvent::Type::LOG:
            static std::string logname = "Log";
            return logname;
        case PipelineEvent::Type::METRIC:
            static std::string metricname = "Metric";
            return metricname;
        case PipelineEvent::Type::SPAN:
            static std::string spanname = "Span";
            return spanname;
        default:
            static std::string voidname = "";
            return voidname;
    }
}

PipelineEvent::PipelineEvent(Type type, PipelineEventGroup* ptr) : mType(type), mPipelineEventGroupPtr(ptr) {
}

std::shared_ptr<SourceBuffer>& PipelineEvent::GetSourceBuffer() {
    return mPipelineEventGroupPtr->GetSourceBuffer();
}

#ifdef APSARA_UNIT_TEST_MAIN
std::string PipelineEvent::ToJsonString(bool enableEventMeta) const {
    Json::Value root = ToJson(enableEventMeta);
    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    builder["indentation"] = "    ";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    std::ostringstream oss;
    writer->write(root, &oss);
    return oss.str();
}

bool PipelineEvent::FromJsonString(const std::string& inJson) {
    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    std::string errs;
    Json::Value root;
    if (!reader->parse(inJson.data(), inJson.data() + inJson.size(), &root, &errs)) {
        LOG_ERROR(sLogger, ("build PipelineEvent FromJsonString error", errs));
        return false;
    }
    return FromJson(root);
}
#endif

} // namespace logtail
