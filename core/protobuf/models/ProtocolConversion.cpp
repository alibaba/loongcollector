#include "protobuf/models/ProtocolConversion.h"

using namespace std;

namespace logtail {

bool TransferPBToPipelineEventGroup(const logtail::models::PipelineEventGroup& src, logtail::PipelineEventGroup& dst, std::string& errMsg) {
    // events
    switch (src.PipelineEvents_case())
    {
    case logtail::models::PipelineEventGroup::PipelineEventsCase::kLogs:
        if (src.logs().events_size() == 0) {
            errMsg = "error transfer PB to PipelineEventGroup: no log events";
            return false;
        }
        dst.MutableEvents().reserve(src.logs().events_size());
        for (auto& logSrc : src.logs().events()) {
            auto logDst = dst.AddLogEvent();
            if (!TransferPBToLogEvent(logSrc, *logDst, errMsg)) {
                return false;
            }
        }
        break;
    case logtail::models::PipelineEventGroup::PipelineEventsCase::kMetrics:
        if (src.metrics().events_size() == 0) {
            errMsg = "error transfer PB to PipelineEventGroup: no metric events";
            return false;
        }
        dst.MutableEvents().reserve(src.metrics().events_size());
        for (auto& metricSrc : src.metrics().events()) {
            auto metricDst = dst.AddMetricEvent();
            if (!TransferPBToMetricEvent(metricSrc, *metricDst, errMsg)) {
                return false;
            }
        }
        break;
    case logtail::models::PipelineEventGroup::PipelineEventsCase::kSpans:
        if (src.spans().events_size() == 0) {
            errMsg = "error transfer PB to PipelineEventGroup: no span events";
            return false;
        }
        dst.MutableEvents().reserve(src.spans().events_size());
        for (auto& spanSrc : src.spans().events()) {
            auto spanDst = dst.AddSpanEvent();
            if (!TransferPBToSpanEvent(spanSrc, *spanDst, errMsg)) {
                return false;
            }
        }
        break;
    default:
        errMsg = "error transfer PB to PipelineEventGroup: unsupported event type";
        return false;
    }

    // tags
    for (auto& tag : src.tags()) {
        dst.SetTag(tag.first, tag.second);
    }

    // TODO: transfer metadatas
    // for (auto& metaData : src.metadata()) {
    //     if (metaData.first == "source_id") {
    //         dst.SetMetadata(logtail::EventGroupMetaKey::SOURCE_ID, metaData.second);
    //     }
    // }

    return true;
}

bool TransferPBToLogEvent(const logtail::models::LogEvent& src, logtail::LogEvent& dst, std::string& errMsg) {
    // timestamp
    std::chrono::nanoseconds tns(src.timestamp()); 
    std::chrono::seconds ts = std::chrono::duration_cast<std::chrono::seconds>(tns); 
    dst.SetTimestamp(ts.count(), tns.count() - ts.count() * 1000000000);
    // contents
    for (auto& contentPair : src.contents()) {
        dst.SetContent(contentPair.key(), contentPair.value());
    }
    // level
    dst.SetLevel(src.level());
    // fileoffset and rawsize
    dst.SetPosition(src.fileoffset(), src.rawsize());
    return true;
}

bool TransferPBToMetricEvent(const logtail::models::MetricEvent& src, logtail::MetricEvent& dst, std::string& errMsg) {
    // timestamp
    std::chrono::nanoseconds tns(src.timestamp()); 
    std::chrono::seconds ts = std::chrono::duration_cast<std::chrono::seconds>(tns); 
    dst.SetTimestamp(ts.count(), tns.count() - ts.count() * 1000000000);
    // name
    dst.SetName(src.name());
    // value
    switch (src.Value_case()) {
    case logtail::models::MetricEvent::ValueCase::kUntypedSingleValue:
        dst.SetValue(logtail::UntypedSingleValue{src.untypedsinglevalue().value()});
        break;
    default:
        errMsg = "error transfer PB to MetricEvent: unsupported value type";
        return false;
    }
    // tags
    for (auto& tagPair : src.tags()) {
        dst.SetTag(tagPair.first, tagPair.second);
    }
    return true;
}

bool TransferPBToSpanEvent(const logtail::models::SpanEvent& src, logtail::SpanEvent& dst, std::string& errMsg) {
    // timestamp
    std::chrono::nanoseconds tns(src.timestamp()); 
    std::chrono::seconds ts = std::chrono::duration_cast<std::chrono::seconds>(tns); 
    dst.SetTimestamp(ts.count(), tns.count() - ts.count() * 1000000000);

    dst.SetTraceId(src.traceid());
    dst.SetSpanId(src.spanid());
    dst.SetTraceState(src.tracestate());
    dst.SetParentSpanId(src.parentspanid());
    dst.SetName(src.name());
    dst.SetKind(static_cast<logtail::SpanEvent::Kind>(src.kind()));
    dst.SetStartTimeNs(src.starttime());
    dst.SetEndTimeNs(src.endtime());

    // tags
    for (auto& tagPair : src.tags()) {
        dst.SetTag(tagPair.first, tagPair.second);
    }
    // inner events
    for (auto& event : src.events()) {
        auto dstEvent = dst.AddEvent();
        dstEvent->SetTimestampNs(event.timestamp());
        dstEvent->SetName(event.name());
        for (auto& tagPair : event.tags()) {
            dstEvent->SetTag(tagPair.first, tagPair.second);
        }
    }
    // span links
    for (auto& link : src.links()) {
        auto dstLink = dst.AddLink();
        dstLink->SetTraceId(link.traceid());
        dstLink->SetSpanId(link.spanid());
        dstLink->SetTraceState(link.tracestate());
        for (auto& tagPair : link.tags()) {
            dstLink->SetTag(tagPair.first, tagPair.second);
        }
    }

    dst.SetStatus(static_cast<logtail::SpanEvent::StatusCode>(src.status()));

    // scope tags
    for (auto& tagPair : src.scopetags()) {
        dst.SetScopeTag(tagPair.first, tagPair.second);
    }

    return true;
}

bool TransferPipelineEventGroupToPB(const logtail::PipelineEventGroup& src, logtail::models::PipelineEventGroup& dst, std::string& errMsg) {
    // events
    if (src.GetEvents().empty()) {
        errMsg = "error transfer PipelineEventGroup to PB: events empty";
        return false;
    }

    switch (src.GetEvents()[0]->GetType()) {
    case logtail::PipelineEvent::Type::LOG:
        dst.mutable_logs()->mutable_events()->Reserve(src.GetEvents().size());
        for (const auto& event : src.GetEvents()) {
            if (!event.Is<logtail::LogEvent>()) {
                errMsg = "error transfer PipelineEventGroup to PB: unsupport pipelineEventGroup with multi types of events";
                return false;
            }
            const auto& logSrc = event.Cast<logtail::LogEvent>();
            auto logDst = dst.mutable_logs()->add_events();
            if (!TransferLogEventToPB(logSrc, *logDst, errMsg)) {
                return false;
            }
        }
        break;
    case logtail::PipelineEvent::Type::METRIC:
        dst.mutable_metrics()->mutable_events()->Reserve(src.GetEvents().size());
        for (const auto& event : src.GetEvents()) {
            if (!event.Is<logtail::MetricEvent>()) {
                errMsg = "error transfer PipelineEventGroup to PB: unsupport pipelineEventGroup with multi types of events";
                return false;
            }
            const auto& metricSrc = event.Cast<logtail::MetricEvent>();
            auto metricDst = dst.mutable_metrics()->add_events();
            if (!TransferMetricEventToPB(metricSrc, *metricDst, errMsg)) {
                return false;
            }
        }
        break;
    case logtail::PipelineEvent::Type::SPAN:
        dst.mutable_spans()->mutable_events()->Reserve(src.GetEvents().size());
        for (const auto& event : src.GetEvents()) {
            if (!event.Is<logtail::SpanEvent>()) {
                errMsg = "error transfer PipelineEventGroup to PB: unsupport pipelineEventGroup with multi types of events";
                return false;
            }
            const auto& spanSrc = event.Cast<logtail::SpanEvent>();
            auto spanDst = dst.mutable_spans()->add_events();
            if (!TransferSpanEventToPB(spanSrc, *spanDst, errMsg)) {
                return false;
            }
        }
        break;
    default:
        errMsg = "error transfer PipelineEventGroup to PB: unsupported event type";
        return false;
    }

    // tags
    for (const auto& tag : src.GetTags()) {
        dst.mutable_tags()->insert({tag.first.to_string(), tag.second.to_string()});
    }

    // TODO: transfer metadatas
    // auto sourceId = src.GetMetadata(logtail::EventGroupMetaKey::SOURCE_ID);
    // if (!sourceId.empty()) {
    //     dst.mutable_metadata()->insert({"source_id", sourceId.to_string()});
    // }
    return true;
}

bool TransferLogEventToPB(const logtail::LogEvent& src, logtail::models::LogEvent& dst, std::string& errMsg) {
    // timestamp
    std::chrono::seconds ts(src.GetTimestamp()); 
    dst.set_timestamp(ts.count() * 1000000000 + src.GetTimestampNanosecond().value_or(0));

    // contents
    dst.mutable_contents()->Reserve(src.Size());
    for (auto iter = src.begin(); iter != src.end(); iter++) {
        auto content = dst.add_contents();
        content->set_key(iter->first.to_string());
        content->set_value(iter->second.to_string());
    }

    // level
    dst.set_level(src.GetLevel().to_string());

    // fileoffset and rawsize
    dst.set_fileoffset(src.GetPosition().first);
    dst.set_rawsize(src.GetPosition().second);

    return true;
}

bool TransferMetricEventToPB(const logtail::MetricEvent& src, logtail::models::MetricEvent& dst, std::string& errMsg) {
    // timestamp
    std::chrono::seconds ts(src.GetTimestamp()); 
    dst.set_timestamp(ts.count() * 1000000000 + src.GetTimestampNanosecond().value_or(0));

    // name
    dst.set_name(src.GetName().to_string());

    // value
    if (src.Is<logtail::UntypedSingleValue>()) {
        auto v = src.GetValue<logtail::UntypedSingleValue>();
        dst.mutable_untypedsinglevalue()->set_value(v->mValue);
    } else {
        errMsg = "error transfer MetricEvent to PB: unsupported value type";
        return false;
    }

    // tags
    for (auto iter = src.TagsBegin(); iter != src.TagsEnd(); iter++) {
        dst.mutable_tags()->insert({iter->first.to_string(), iter->second.to_string()});
    }

    return true;
}

bool TransferSpanEventToPB(const logtail::SpanEvent& src, logtail::models::SpanEvent& dst, std::string& errMsg) {
    // timestamp
    std::chrono::seconds ts(src.GetTimestamp()); 
    dst.set_timestamp(ts.count() * 1000000000 + src.GetTimestampNanosecond().value_or(0));

    dst.set_traceid(src.GetTraceId().to_string());
    dst.set_spanid(src.GetSpanId().to_string());
    dst.set_tracestate(src.GetTraceState().to_string());
    dst.set_parentspanid(src.GetParentSpanId().to_string());
    dst.set_name(src.GetName().to_string());
    dst.set_kind(static_cast<logtail::models::SpanEvent_SpanKind>(src.GetKind()));
    dst.set_starttime(src.GetStartTimeNs());
    dst.set_endtime(src.GetEndTimeNs());

    // tags
    for (auto iter = src.TagsBegin(); iter != src.TagsEnd(); iter++) {
        dst.mutable_tags()->insert({iter->first.to_string(), iter->second.to_string()});
    }

    // inner events
    for (const auto& event : src.GetEvents()) {
        auto dstEvent = dst.add_events();
        dstEvent->set_timestamp(event.GetTimestampNs());
        dstEvent->set_name(event.GetName().to_string());
        for (auto iter = event.TagsBegin(); iter != event.TagsEnd(); iter++) {
            dstEvent->mutable_tags()->insert({iter->first.to_string(), iter->second.to_string()});
        }
    }

    // span links
    for (const auto& link : src.GetLinks()) {
        auto dstLink = dst.add_links();
        dstLink->set_traceid(link.GetTraceId().to_string());
        dstLink->set_spanid(link.GetSpanId().to_string());
        dstLink->set_tracestate(link.GetTraceState().to_string());
        for (auto iter = link.TagsBegin(); iter != link.TagsEnd(); iter++) {
            dstLink->mutable_tags()->insert({iter->first.to_string(), iter->second.to_string()});
        }
    }

    // status
    dst.set_status(static_cast<logtail::models::SpanEvent::StatusCode>(src.GetStatus()));

    // scope tags
    for (auto iter = src.ScopeTagsBegin(); iter != src.ScopeTagsEnd(); iter++) {
        dst.mutable_scopetags()->insert({iter->first.to_string(), iter->second.to_string()});
    }

    return true;
}

} // namespace logtail
