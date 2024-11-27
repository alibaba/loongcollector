// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "pipeline/serializer/SLSSerializer.h"

#include <json/json.h>

#include <array>

#include "common/Flags.h"
#include "common/compression/CompressType.h"
#include "constants/SpanConstants.h"
#include "plugin/flusher/sls/FlusherSLS.h"
#include "protobuf/sls/LogGroupSerializer.h"

DECLARE_FLAG_INT32(max_send_log_group_size);

using namespace std;

namespace logtail {

std::string SerializeSpanLinksToString(const SpanEvent& event) {
    if (event.GetLinks().empty()) {
        return "";
    }
    Json::Value jsonLinks(Json::arrayValue);
    for (const auto& link : event.GetLinks()) {
        jsonLinks.append(link.ToJson());
    }
    Json::StreamWriterBuilder writer;
    return Json::writeString(writer, jsonLinks);
}
std::string SerializeSpanEventsToString(const SpanEvent& event) {
    if (event.GetEvents().empty()) {
        return "";
    }
    Json::Value jsonEvents(Json::arrayValue);
    for (const auto& event : event.GetEvents()) {
        jsonEvents.append(event.ToJson());
    }
    Json::StreamWriterBuilder writer;
    return Json::writeString(writer, jsonEvents);
}

template <>
bool Serializer<vector<CompressedLogGroup>>::DoSerialize(vector<CompressedLogGroup>&& p,
                                                         std::string& output,
                                                         std::string& errorMsg) {
    auto inputSize = 0;
    for (auto& item : p) {
        inputSize += item.mData.size();
    }
    mInItemsTotal->Add(1);
    mInItemSizeBytes->Add(inputSize);

    auto before = std::chrono::system_clock::now();
    auto res = Serialize(std::move(p), output, errorMsg);
    mTotalProcessMs->Add(std::chrono::system_clock::now() - before);

    if (res) {
        mOutItemsTotal->Add(1);
        mOutItemSizeBytes->Add(output.size());
    } else {
        mDiscardedItemsTotal->Add(1);
        mDiscardedItemSizeBytes->Add(inputSize);
    }
    return res;
}

bool SLSEventGroupSerializer::Serialize(BatchedEvents&& group, string& res, string& errorMsg) {
    if (group.mEvents.empty()) {
        errorMsg = "empty event group";
        return false;
    }

    PipelineEvent::Type eventType = group.mEvents[0]->GetType();
    if (eventType == PipelineEvent::Type::NONE) {
        // should not happen
        errorMsg = "unsupported event type in event group";
        return false;
    }

    bool enableNs = mFlusher->GetContext().GetGlobalConfig().mEnableTimestampNanosecond;

    // caculate serialized logGroup size first, where some critical results can be cached
    vector<size_t> logSZ(group.mEvents.size());
    vector<pair<string, size_t>> metricEventContentCache(group.mEvents.size());
    vector<array<string, 6>> spanEventContentCache(group.mEvents.size());
    size_t logGroupSZ = 0;
    switch (eventType) {
        case PipelineEvent::Type::LOG: {
            for (size_t i = 0; i < group.mEvents.size(); ++i) {
                const auto& e = group.mEvents[i].Cast<LogEvent>();
                if (e.Empty()) {
                    continue;
                }
                size_t contentSZ = 0;
                for (const auto& kv : e) {
                    contentSZ += GetLogContentSize(kv.first.size(), kv.second.size());
                }
                logGroupSZ += GetLogSize(contentSZ, enableNs && e.GetTimestampNanosecond(), logSZ[i]);
            }
            break;
        }
        case PipelineEvent::Type::METRIC: {
            for (size_t i = 0; i < group.mEvents.size(); ++i) {
                const auto& e = group.mEvents[i].Cast<MetricEvent>();
                if (e.Is<UntypedSingleValue>()) {
                    metricEventContentCache[i].first = to_string(e.GetValue<UntypedSingleValue>()->mValue);
                } else {
                    // should not happen
                    LOG_ERROR(sLogger,
                              ("unexpected error",
                               "invalid metric event type")("config", mFlusher->GetContext().GetConfigName()));
                    continue;
                }
                metricEventContentCache[i].second = GetMetricLabelSize(e);

                size_t contentSZ = 0;
                contentSZ += GetLogContentSize(METRIC_RESERVED_KEY_NAME.size(), e.GetName().size());
                contentSZ
                    += GetLogContentSize(METRIC_RESERVED_KEY_VALUE.size(), metricEventContentCache[i].first.size());
                contentSZ
                    += GetLogContentSize(METRIC_RESERVED_KEY_TIME_NANO.size(), e.GetTimestampNanosecond() ? 19U : 10U);
                contentSZ += GetLogContentSize(METRIC_RESERVED_KEY_LABELS.size(), metricEventContentCache[i].second);
                logGroupSZ += GetLogSize(contentSZ, false, logSZ[i]);
            }
            break;
        }
        case PipelineEvent::Type::SPAN:
            for (size_t i = 0; i < group.mEvents.size(); ++i) {
                const auto& e = group.mEvents[i].Cast<SpanEvent>();
                size_t contentSZ = 0;
                contentSZ += GetLogContentSize(DEFAULT_TRACE_TAG_TRACE_ID.size(), e.GetTraceId().size());
                contentSZ += GetLogContentSize(DEFAULT_TRACE_TAG_SPAN_ID.size(), e.GetSpanId().size());
                contentSZ += GetLogContentSize(DEFAULT_TRACE_TAG_PARENT_ID.size(), e.GetParentSpanId().size());
                contentSZ += GetLogContentSize(DEFAULT_TRACE_TAG_SPAN_NAME.size(), e.GetName().size());
                contentSZ += GetLogContentSize(DEFAULT_TRACE_TAG_SPAN_KIND.size(), GetKindString(e.GetKind()).size());
                contentSZ
                    += GetLogContentSize(DEFAULT_TRACE_TAG_STATUS_CODE.size(), GetStatusString(e.GetStatus()).size());
                contentSZ += GetLogContentSize(DEFAULT_TRACE_TAG_TRACE_STATE.size(), e.GetTraceState().size());

                // set tags and scope tags
                Json::Value jsonVal;
                for (auto it = e.TagsBegin(); it != e.TagsEnd(); ++it) {
                    jsonVal[it->first.to_string()] = it->second.to_string();
                }
                for (auto it = e.ScopeTagsBegin(); it != e.ScopeTagsEnd(); ++it) {
                    jsonVal[it->first.to_string()] = it->second.to_string();
                }
                Json::StreamWriterBuilder writer;
                std::string attrString = Json::writeString(writer, jsonVal);
                contentSZ += GetLogContentSize(DEFAULT_TRACE_TAG_ATTRIBUTES.size(), attrString.size());
                spanEventContentCache[i][0] = std::move(attrString);
                auto linkString = SerializeSpanLinksToString(e);
                contentSZ += GetLogContentSize(DEFAULT_TRACE_TAG_LINKS.size(), linkString.size());
                spanEventContentCache[i][1] = std::move(linkString);
                auto eventString = SerializeSpanEventsToString(e);
                contentSZ += GetLogContentSize(DEFAULT_TRACE_TAG_EVENTS.size(), eventString.size());
                spanEventContentCache[i][2] = std::move(eventString);

                // time related
                auto startTsNs = std::to_string(e.GetStartTimeNs());
                contentSZ += GetLogContentSize(DEFAULT_TRACE_TAG_START_TIME_NANO.size(), startTsNs.size());
                spanEventContentCache[i][3] = std::move(startTsNs);
                auto endTsNs = std::to_string(e.GetEndTimeNs());
                contentSZ += GetLogContentSize(DEFAULT_TRACE_TAG_END_TIME_NANO.size(), endTsNs.size());
                spanEventContentCache[i][4] = std::move(endTsNs);
                auto durationNs = std::to_string(e.GetEndTimeNs() - e.GetStartTimeNs());
                contentSZ += GetLogContentSize(DEFAULT_TRACE_TAG_DURATION.size(), durationNs.size());
                spanEventContentCache[i][5] = std::move(durationNs);
                logGroupSZ += GetLogSize(contentSZ, false, logSZ[i]);
            }
            break;
        case PipelineEvent::Type::RAW:
            for (size_t i = 0; i < group.mEvents.size(); ++i) {
                const auto& e = group.mEvents[i].Cast<RawEvent>();
                if (e.GetContent().empty()) {
                    continue;
                }
                size_t contentSZ = GetLogContentSize(DEFAULT_CONTENT_KEY.size(), e.GetContent().size());
                logGroupSZ += GetLogSize(contentSZ, enableNs && e.GetTimestampNanosecond(), logSZ[i]);
            }
            break;
        default:
            break;
    }
    if (logGroupSZ == 0) {
        errorMsg = "all empty logs";
        return false;
    }

    // loggroup.category is deprecated, no need to set
    for (const auto& tag : group.mTags.mInner) {
        if (tag.first == LOG_RESERVED_KEY_TOPIC || tag.first == LOG_RESERVED_KEY_SOURCE
            || tag.first == LOG_RESERVED_KEY_MACHINE_UUID) {
            logGroupSZ += GetStringSize(tag.second.size());
        } else {
            logGroupSZ += GetLogTagSize(tag.first.size(), tag.second.size());
        }
    }

    if (static_cast<int32_t>(logGroupSZ) > INT32_FLAG(max_send_log_group_size)) {
        errorMsg = "log group exceeds size limit\tgroup size: " + ToString(logGroupSZ)
            + "\tsize limit: " + ToString(INT32_FLAG(max_send_log_group_size));
        return false;
    }

    thread_local LogGroupSerializer serializer;
    serializer.Prepare(logGroupSZ);
    switch (eventType) {
        case PipelineEvent::Type::LOG:
            for (size_t i = 0; i < group.mEvents.size(); ++i) {
                const auto& e = group.mEvents[i].Cast<LogEvent>();
                serializer.StartToAddLog(logSZ[i]);
                serializer.AddLogTime(e.GetTimestamp());
                for (const auto& kv : e) {
                    serializer.AddLogContent(kv.first, kv.second);
                }
                if (enableNs && e.GetTimestampNanosecond()) {
                    serializer.AddLogTimeNs(e.GetTimestampNanosecond().value());
                }
            }
            break;
        case PipelineEvent::Type::METRIC:
            for (size_t i = 0; i < group.mEvents.size(); ++i) {
                const auto& e = group.mEvents[i].Cast<MetricEvent>();
                if (e.Is<std::monostate>()) {
                    continue;
                }
                serializer.StartToAddLog(logSZ[i]);
                serializer.AddLogTime(e.GetTimestamp());
                serializer.AddLogContentMetricLabel(e, metricEventContentCache[i].second);
                serializer.AddLogContentMetricTimeNano(e);
                serializer.AddLogContent(METRIC_RESERVED_KEY_VALUE, metricEventContentCache[i].first);
                serializer.AddLogContent(METRIC_RESERVED_KEY_NAME, e.GetName());
            }
            break;
        case PipelineEvent::Type::SPAN:
            for (size_t i = 0; i < group.mEvents.size(); ++i) {
                const auto& spanEvent = group.mEvents[i].Cast<SpanEvent>();

                serializer.StartToAddLog(logSZ[i]);
                serializer.AddLogTime(spanEvent.GetTimestamp());
                // set trace_id span_id span_kind status etc
                serializer.AddLogContent(DEFAULT_TRACE_TAG_TRACE_ID, spanEvent.GetTraceId());
                serializer.AddLogContent(DEFAULT_TRACE_TAG_SPAN_ID, spanEvent.GetSpanId());
                serializer.AddLogContent(DEFAULT_TRACE_TAG_PARENT_ID, spanEvent.GetParentSpanId());
                // span_name
                serializer.AddLogContent(DEFAULT_TRACE_TAG_SPAN_NAME, spanEvent.GetName());
                // span_kind
                serializer.AddLogContent(DEFAULT_TRACE_TAG_SPAN_KIND, GetKindString(spanEvent.GetKind()));
                // status_code
                serializer.AddLogContent(DEFAULT_TRACE_TAG_STATUS_CODE, GetStatusString(spanEvent.GetStatus()));
                // trace state
                serializer.AddLogContent(DEFAULT_TRACE_TAG_TRACE_STATE, spanEvent.GetTraceState());

                serializer.AddLogContent(DEFAULT_TRACE_TAG_ATTRIBUTES, spanEventContentCache[i][0]);

                serializer.AddLogContent(DEFAULT_TRACE_TAG_LINKS, spanEventContentCache[i][1]);
                serializer.AddLogContent(DEFAULT_TRACE_TAG_EVENTS, spanEventContentCache[i][2]);

                // start_time
                serializer.AddLogContent(DEFAULT_TRACE_TAG_START_TIME_NANO, spanEventContentCache[i][3]);
                // end_time
                serializer.AddLogContent(DEFAULT_TRACE_TAG_END_TIME_NANO, spanEventContentCache[i][4]);
                // duration
                serializer.AddLogContent(DEFAULT_TRACE_TAG_DURATION, spanEventContentCache[i][5]);
            }
            break;
        case PipelineEvent::Type::RAW:
            for (size_t i = 0; i < group.mEvents.size(); ++i) {
                const auto& e = group.mEvents[i].Cast<RawEvent>();
                serializer.StartToAddLog(logSZ[i]);
                serializer.AddLogTime(e.GetTimestamp());
                serializer.AddLogContent(DEFAULT_CONTENT_KEY, e.GetContent());
                if (enableNs && e.GetTimestampNanosecond()) {
                    serializer.AddLogTimeNs(e.GetTimestampNanosecond().value());
                }
            }
            break;
        default:
            break;
    }
    for (const auto& tag : group.mTags.mInner) {
        if (tag.first == LOG_RESERVED_KEY_TOPIC) {
            serializer.AddTopic(tag.second);
        } else if (tag.first == LOG_RESERVED_KEY_SOURCE) {
            serializer.AddSource(tag.second);
        } else if (tag.first == LOG_RESERVED_KEY_MACHINE_UUID) {
            serializer.AddMachineUUID(tag.second);
        } else {
            serializer.AddLogTag(tag.first, tag.second);
        }
    }
    res = std::move(serializer.GetResult());
    return true;
}

bool SLSEventGroupListSerializer::Serialize(vector<CompressedLogGroup>&& v, string& res, string& errorMsg) {
    sls_logs::SlsLogPackageList logPackageList;
    for (const auto& item : v) {
        auto package = logPackageList.add_packages();
        package->set_data(item.mData);
        package->set_uncompress_size(item.mRawSize);

        CompressType compressType = static_cast<const FlusherSLS*>(mFlusher)->GetCompressType();
        sls_logs::SlsCompressType slsCompressType = sls_logs::SLS_CMP_LZ4;
        if (compressType == CompressType::NONE) {
            slsCompressType = sls_logs::SLS_CMP_NONE;
        } else if (compressType == CompressType::ZSTD) {
            slsCompressType = sls_logs::SLS_CMP_ZSTD;
        }
        package->set_compress_type(slsCompressType);
    }
    res = logPackageList.SerializeAsString();
    return true;
}

} // namespace logtail
