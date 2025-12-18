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

#include "collection_pipeline/serializer/SLSSerializer.h"

#include <array>
#include <vector>

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "collection_pipeline/serializer/JsonSerializer.h"
#include "common/Flags.h"
#include "common/StringTools.h"
#include "common/compression/CompressType.h"
#include "constants/Constants.h"
#include "constants/SpanConstants.h"
#include "logger/Logger.h"
#include "models/MetricValue.h"
#include "plugin/flusher/sls/FlusherSLS.h"

DECLARE_FLAG_INT32(max_send_log_group_size);

using namespace std;

namespace logtail {

// Maximum size threshold for thread_local buffer before reallocation
constexpr size_t kMaxThreadLocalBufferSize = 1024 * 1024;

void SerializeSpanLinksToString(const SpanEvent& event, std::string& result) {
    if (event.GetLinks().empty()) {
        result.clear();
        return;
    }

    // reuse thread_local buffer to avoid repeated memory allocation
    thread_local rapidjson::StringBuffer buffer;
    if (buffer.GetSize() > kMaxThreadLocalBufferSize) {
        buffer = rapidjson::StringBuffer(); // reallocate to avoid holding too much memory
    }
    buffer.Clear();
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

    writer.StartArray();

    for (const auto& link : event.GetLinks()) {
        writer.StartObject();

        writer.Key(DEFAULT_TRACE_TAG_TRACE_ID.data(), DEFAULT_TRACE_TAG_TRACE_ID.size());
        writer.String(link.GetTraceId().data(), link.GetTraceId().size());

        writer.Key(DEFAULT_TRACE_TAG_SPAN_ID.data(), DEFAULT_TRACE_TAG_SPAN_ID.size());
        writer.String(link.GetSpanId().data(), link.GetSpanId().size());

        if (!link.GetTraceState().empty()) {
            writer.Key(DEFAULT_TRACE_TAG_TRACE_STATE.data(), DEFAULT_TRACE_TAG_TRACE_STATE.size());
            writer.String(link.GetTraceState().data(), link.GetTraceState().size());
        }

        if (link.TagsSize() > 0) {
            writer.Key(DEFAULT_TRACE_TAG_ATTRIBUTES.data(), DEFAULT_TRACE_TAG_ATTRIBUTES.size());
            writer.StartObject();
            for (auto it = link.TagsBegin(); it != link.TagsEnd(); ++it) {
                writer.Key(it->first.data(), it->first.size());
                writer.String(it->second.data(), it->second.size());
            }
            writer.EndObject();
        }

        writer.EndObject();
    }

    writer.EndArray();
    result.assign(buffer.GetString(), buffer.GetSize());
}

void SerializeSpanEventsToString(const SpanEvent& event, std::string& result) {
    if (event.GetEvents().empty()) {
        result.clear();
        return;
    }

    // reuse thread_local buffer to avoid repeated memory allocation
    thread_local rapidjson::StringBuffer buffer;
    if (buffer.GetSize() > kMaxThreadLocalBufferSize) {
        buffer = rapidjson::StringBuffer(); // reallocate to avoid holding too much memory
    }
    buffer.Clear();
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

    writer.StartArray();

    for (const auto& evt : event.GetEvents()) {
        writer.StartObject();

        writer.Key(DEFAULT_TRACE_TAG_SPAN_EVENT_NAME.data(), DEFAULT_TRACE_TAG_SPAN_EVENT_NAME.size());
        writer.String(evt.GetName().data(), evt.GetName().size());

        writer.Key(DEFAULT_TRACE_TAG_TIMESTAMP.data(), DEFAULT_TRACE_TAG_TIMESTAMP.size());
        writer.Uint64(evt.GetTimestampNs());

        if (evt.TagsSize() > 0) {
            writer.Key(DEFAULT_TRACE_TAG_ATTRIBUTES.data(), DEFAULT_TRACE_TAG_ATTRIBUTES.size());
            writer.StartObject();
            for (auto it = evt.TagsBegin(); it != evt.TagsEnd(); ++it) {
                writer.Key(it->first.data(), it->first.size());
                writer.String(it->second.data(), it->second.size());
            }
            writer.EndObject();
        }

        writer.EndObject();
    }

    writer.EndArray();
    result.assign(buffer.GetString(), buffer.GetSize());
}

void SerializeSpanAttributesToString(const SpanEvent& event, std::string& result) {
    if (event.TagsSize() == 0 && event.ScopeTagsSize() == 0) {
        result.clear();
        return;
    }

    // reuse thread_local buffer to avoid repeated memory allocation
    thread_local rapidjson::StringBuffer buffer;
    if (buffer.GetSize() > kMaxThreadLocalBufferSize) {
        buffer = rapidjson::StringBuffer(); // reallocate to avoid holding too much memory
    }
    buffer.Clear();
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

    writer.StartObject();

    for (auto it = event.TagsBegin(); it != event.TagsEnd(); ++it) {
        writer.Key(it->first.data(), it->first.size());
        writer.String(it->second.data(), it->second.size());
    }

    for (auto it = event.ScopeTagsBegin(); it != event.ScopeTagsEnd(); ++it) {
        writer.Key(it->first.data(), it->first.size());
        writer.String(it->second.data(), it->second.size());
    }

    writer.EndObject();

    result.assign(buffer.GetString(), buffer.GetSize());
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
    vector<MetricEventContentCacheItem> metricEventContentCache(group.mEvents.size());
    vector<array<string, 6>> spanEventContentCache(group.mEvents.size());
    size_t logGroupSZ = 0;
    switch (eventType) {
        case PipelineEvent::Type::LOG: {
            CalculateLogEventSize(group, logGroupSZ, logSZ, enableNs);
            break;
        }
        case PipelineEvent::Type::METRIC: {
            CalculateMetricEventSize(group, logGroupSZ, metricEventContentCache, logSZ);
            break;
        }
        case PipelineEvent::Type::SPAN:
            CalculateSpanEventSize(group, logGroupSZ, spanEventContentCache, logSZ);
            break;
        case PipelineEvent::Type::RAW:
            CalculateRawEventSize(group, logGroupSZ, logSZ, enableNs);
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
            SerializeLogEvent(serializer, group, logSZ, enableNs);
            break;
        case PipelineEvent::Type::METRIC:
            SerializeMetricEvent(serializer, group, metricEventContentCache, logSZ);
            break;
        case PipelineEvent::Type::SPAN:
            SerializeSpanEvent(serializer, group, spanEventContentCache, logSZ);
            break;
        case PipelineEvent::Type::RAW:
            SerializeRawEvent(serializer, group, logSZ, enableNs);
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

void SLSEventGroupSerializer::CalculateLogEventSize(const BatchedEvents& group,
                                                    size_t& logGroupSZ,
                                                    std::vector<size_t>& logSZ,
                                                    bool enableNs) const {
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
}

void SLSEventGroupSerializer::CalculateMetricEventSize(
    const BatchedEvents& group,
    size_t& logGroupSZ,
    std::vector<MetricEventContentCacheItem>& metricEventContentCache,
    std::vector<size_t>& logSZ) const {
    for (size_t i = 0; i < group.mEvents.size(); ++i) {
        const auto& e = group.mEvents[i].Cast<MetricEvent>();
        if (e.GetTimestamp() < 1e9) {
            LOG_WARNING(sLogger,
                        ("metric event timestamp is less than 1e9", "discard event")("timestamp", e.GetTimestamp())(
                            "config", mFlusher->GetContext().GetConfigName()));
            continue;
        }
        if (e.Is<UntypedSingleValue>()) {
            metricEventContentCache[i].mMetricEventContentCache.push_back(
                DoubleToString(e.GetValue<UntypedSingleValue>()->mValue));
            metricEventContentCache[i].mLabelSize = GetMetricLabelSize(e);
            size_t contentSZ = 0;
            contentSZ += GetLogContentSize(METRIC_RESERVED_KEY_NAME.size(), e.GetName().size());
            contentSZ += GetLogContentSize(METRIC_RESERVED_KEY_VALUE.size(),
                                           metricEventContentCache[i].mMetricEventContentCache[0].size());
            contentSZ
                += GetLogContentSize(METRIC_RESERVED_KEY_TIME_NANO.size(), e.GetTimestampNanosecond() ? 19U : 10U);
            contentSZ += GetLogContentSize(METRIC_RESERVED_KEY_LABELS.size(), metricEventContentCache[i].mLabelSize);
            // Add metadata
            for (auto it = e.MetadataBegin(); it != e.MetadataEnd(); it++) {
                contentSZ += GetLogContentSize(it->first.size(), it->second.size());
            }
            logGroupSZ += GetLogSize(contentSZ, false, logSZ[i]);
        } else if (e.Is<UntypedMultiDoubleValues>()) {
            if (e.GetValue<UntypedMultiDoubleValues>()->ValuesSize() == 0) {
                LOG_WARNING(sLogger,
                            ("metric event multi value is empty",
                             "discard event")("config", mFlusher->GetContext().GetConfigName()));
                continue;
            }
            size_t contentSZ = 0;
            contentSZ
                += GetLogContentSize(METRIC_RESERVED_KEY_TIME_NANO.size(), e.GetTimestampNanosecond() ? 19U : 10U);
            for (auto it = e.TagsBegin(); it != e.TagsEnd(); ++it) {
                // the tag of multi value is serialized in the content
                contentSZ += GetLogContentSize(it->first.size(), it->second.size());
            }
            const auto* const multiValue = e.GetValue<UntypedMultiDoubleValues>();
            for (auto it = multiValue->ValuesBegin(); it != multiValue->ValuesEnd(); ++it) {
                string valueStr = DoubleToString(it->second.Value);
                metricEventContentCache[i].mMetricEventContentCache.push_back(valueStr); // value
                contentSZ += GetLogContentSize(it->first.size(), valueStr.size());
            }
            logGroupSZ += GetLogSize(contentSZ, false, logSZ[i]);
        } else {
            LOG_WARNING(
                sLogger,
                ("invalid metric event type", "discard event")("config", mFlusher->GetContext().GetConfigName()));
            continue;
        }
    }
}

void SLSEventGroupSerializer::CalculateSpanEventSize(const BatchedEvents& group,
                                                     size_t& logGroupSZ,
                                                     std::vector<std::array<std::string, 6>>& spanEventContentCache,
                                                     std::vector<size_t>& logSZ) const {
    for (size_t i = 0; i < group.mEvents.size(); ++i) {
        const auto& e = group.mEvents[i].Cast<SpanEvent>();
        size_t contentSZ = 0;
        contentSZ += GetLogContentSize(DEFAULT_TRACE_TAG_TRACE_ID.size(), e.GetTraceId().size());
        contentSZ += GetLogContentSize(DEFAULT_TRACE_TAG_SPAN_ID.size(), e.GetSpanId().size());
        contentSZ += GetLogContentSize(DEFAULT_TRACE_TAG_PARENT_ID.size(), e.GetParentSpanId().size());
        contentSZ += GetLogContentSize(DEFAULT_TRACE_TAG_SPAN_NAME.size(), e.GetName().size());
        contentSZ += GetLogContentSize(DEFAULT_TRACE_TAG_SPAN_KIND.size(), GetKindString(e.GetKind()).size());
        contentSZ += GetLogContentSize(DEFAULT_TRACE_TAG_STATUS_CODE.size(), GetStatusString(e.GetStatus()).size());
        contentSZ += GetLogContentSize(DEFAULT_TRACE_TAG_TRACE_STATE.size(), e.GetTraceState().size());

        SerializeSpanAttributesToString(e, spanEventContentCache[i][0]);
        contentSZ += GetLogContentSize(DEFAULT_TRACE_TAG_ATTRIBUTES.size(), spanEventContentCache[i][0].size());
        SerializeSpanLinksToString(e, spanEventContentCache[i][1]);
        contentSZ += GetLogContentSize(DEFAULT_TRACE_TAG_LINKS.size(), spanEventContentCache[i][1].size());
        SerializeSpanEventsToString(e, spanEventContentCache[i][2]);
        contentSZ += GetLogContentSize(DEFAULT_TRACE_TAG_EVENTS.size(), spanEventContentCache[i][2].size());

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
}

void SLSEventGroupSerializer::CalculateRawEventSize(const BatchedEvents& group,
                                                    size_t& logGroupSZ,
                                                    std::vector<size_t>& logSZ,
                                                    bool enableNs) const {
    for (size_t i = 0; i < group.mEvents.size(); ++i) {
        const auto& e = group.mEvents[i].Cast<RawEvent>();
        size_t contentSZ = GetLogContentSize(DEFAULT_CONTENT_KEY.size(), e.GetContent().size());
        logGroupSZ += GetLogSize(contentSZ, enableNs && e.GetTimestampNanosecond(), logSZ[i]);
    }
}

void SLSEventGroupSerializer::SerializeLogEvent(LogGroupSerializer& serializer,
                                                const BatchedEvents& group,
                                                std::vector<size_t>& logSZ,
                                                bool enableNs) const {
    for (size_t i = 0; i < group.mEvents.size(); ++i) {
        const auto& e = group.mEvents[i].Cast<LogEvent>();
        if (e.Empty()) {
            continue;
        }
        serializer.StartToAddLog(logSZ[i]);
        serializer.AddLogTime(e.GetTimestamp());
        for (const auto& kv : e) {
            serializer.AddLogContent(kv.first, kv.second);
        }
        if (enableNs && e.GetTimestampNanosecond()) {
            serializer.AddLogTimeNs(e.GetTimestampNanosecond().value());
        }
    }
}

// SingleValue Metric
// event: {"labels": {"label1": "value1", "label2": "value2"}, "value": 123}
// result:
//   __time__: 1234567890
//   content:
//      __label__: label1#$#value1|label2#$#value2
//      __time_nano__: 1234567890
//      __name__: value
//      __value__: 123
// MultiValue Metric
// event: {"labels": {"label1": "value1", "label2": "value2"}, "values": {"value1": 123, "value2": 456}}
// result:
//   __time__: 1234567890
//   content:
//      __time_nano__: 1234567890
//      label1: value1
//      label2: value2
//      value1: 123
//      value2: 456
// Metric with __apm_metric_type__ in Metadata
// event: {"labels": {"label1": "value1", "label2": "value2"}, "values": {"value1": 123, "value2": 456}, "metadata":
// {"__apm_metric_type__": "app"}} result:
//   __time__: 1234567890
//   content:
//      __label__: label1#$#value1|label2#$#value2
//      __time_nano__: 1234567890
//      __name__: value
//      __value__: 123
//      __apm_metric_type__: app
void SLSEventGroupSerializer::SerializeMetricEvent(LogGroupSerializer& serializer,
                                                   BatchedEvents& group,
                                                   std::vector<MetricEventContentCacheItem>& metricEventContentCache,
                                                   std::vector<size_t>& logSZ) const {
    for (size_t i = 0; i < group.mEvents.size(); ++i) {
        auto& e = group.mEvents[i].Cast<MetricEvent>();
        if (e.GetTimestamp() < 1e9) {
            continue;
        }
        if (e.Is<UntypedSingleValue>()) {
            if (metricEventContentCache[i].mMetricEventContentCache.empty()) {
                LOG_ERROR(sLogger,
                          ("metric event single value size mismatch", "should never happen")(
                              "config", mFlusher->GetContext().GetConfigName())("expected", 1)("actual", 0));
                continue;
            }
            serializer.StartToAddLog(logSZ[i]);
            serializer.AddLogTime(e.GetTimestamp());
            e.SortTags();
            serializer.AddLogContentMetricLabel(e, metricEventContentCache[i].mLabelSize);
            serializer.AddLogContentMetricTimeNano(e);
            serializer.AddLogContent(METRIC_RESERVED_KEY_VALUE, metricEventContentCache[i].mMetricEventContentCache[0]);
            serializer.AddLogContent(METRIC_RESERVED_KEY_NAME, e.GetName());
            for (auto it = e.MetadataBegin(); it != e.MetadataEnd(); it++) {
                serializer.AddLogContent(it->first, it->second);
            }
        } else if (e.Is<UntypedMultiDoubleValues>()) {
            const auto* const multiValue = e.GetValue<UntypedMultiDoubleValues>();
            if (metricEventContentCache[i].mMetricEventContentCache.size() != multiValue->ValuesSize()) {
                LOG_ERROR(sLogger,
                          ("metric event multi value size mismatch", "should never happen")(
                              "config", mFlusher->GetContext().GetConfigName())("expected", multiValue->ValuesSize())(
                              "actual", metricEventContentCache[i].mMetricEventContentCache.size()));
                continue;
            }
            serializer.StartToAddLog(logSZ[i]);
            serializer.AddLogTime(e.GetTimestamp());
            serializer.AddLogContentMetricTimeNano(e);
            for (auto it = e.TagsBegin(); it != e.TagsEnd(); ++it) {
                serializer.AddLogContent(it->first, it->second);
            }
            size_t currentValueIdx = 0;
            for (auto it = multiValue->ValuesBegin(); it != multiValue->ValuesEnd(); ++it) {
                serializer.AddLogContent(it->first,
                                         metricEventContentCache[i].mMetricEventContentCache[currentValueIdx]);
                ++currentValueIdx;
            }
        } else {
            continue;
        }
    }
}

void SLSEventGroupSerializer::SerializeSpanEvent(LogGroupSerializer& serializer,
                                                 const BatchedEvents& group,
                                                 std::vector<std::array<std::string, 6>>& spanEventContentCache,
                                                 std::vector<size_t>& logSZ) const {
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
}

void SLSEventGroupSerializer::SerializeRawEvent(LogGroupSerializer& serializer,
                                                const BatchedEvents& group,
                                                std::vector<size_t>& logSZ,
                                                bool enableNs) const {
    for (size_t i = 0; i < group.mEvents.size(); ++i) {
        const auto& e = group.mEvents[i].Cast<RawEvent>();
        serializer.StartToAddLog(logSZ[i]);
        serializer.AddLogTime(e.GetTimestamp());
        serializer.AddLogContent(DEFAULT_CONTENT_KEY, e.GetContent());
        if (enableNs && e.GetTimestampNanosecond()) {
            serializer.AddLogTimeNs(e.GetTimestampNanosecond().value());
        }
    }
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
