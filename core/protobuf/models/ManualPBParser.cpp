/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "protobuf/models/ManualPBParser.h"

#include <cstring>

#include "logger/Logger.h"
#include "models/LogEvent.h"
#include "models/MetricEvent.h"
#include "models/MetricValue.h"
#include "models/PipelineEventGroup.h"
#include "models/SpanEvent.h"

namespace logtail {

// Protobuf wire types
enum WireType {
    kVarint = 0,
    kFixed64 = 1,
    kLengthDelimited = 2,
    kStartGroup = 3, // deprecated
    kEndGroup = 4, // deprecated
    kFixed32 = 5
};

// PipelineEventGroup field numbers
enum PipelineEventGroupField { kMetadataField = 1, kTagsField = 2, kLogsField = 3, kMetricsField = 4, kSpansField = 5 };

// LogEvent field numbers
enum LogEventField {
    kLogTimestampField = 1,
    kLogContentsField = 2,
    kLogLevelField = 3,
    kLogFileOffsetField = 4,
    kLogRawSizeField = 5
};

// LogEvent Content field numbers
enum LogContentField { kLogContentKeyField = 1, kLogContentValueField = 2 };

// MetricEvent field numbers
enum MetricEventField {
    kMetricTimestampField = 1,
    kMetricNameField = 2,
    kMetricTagsField = 3,
    kMetricUntypedSingleValueField = 4
};

// SpanEvent field numbers
enum SpanEventField {
    kSpanTimestampField = 1,
    kSpanTraceIdField = 2,
    kSpanSpanIdField = 3,
    kSpanTraceStateField = 4,
    kSpanParentSpanIdField = 5,
    kSpanNameField = 6,
    kSpanKindField = 7,
    kSpanStartTimeField = 8,
    kSpanEndTimeField = 9,
    kSpanTagsField = 10,
    kSpanEventsField = 11,
    kSpanLinksField = 12,
    kSpanStatusField = 13,
    kSpanScopeTagsField = 14
};

// SpanEvent::InnerEvent field numbers
enum SpanInnerEventField { kInnerEventTimestampField = 1, kInnerEventNameField = 2, kInnerEventTagsField = 3 };

// SpanEvent::SpanLink field numbers
enum SpanLinkField {
    kSpanLinkTraceIdField = 1,
    kSpanLinkSpanIdField = 2,
    kSpanLinkTraceStateField = 3,
    kSpanLinkTagsField = 4
};

ManualPBParser::ManualPBParser(const uint8_t* data, size_t size, bool replaceSpanTags)
    : mData(data), mPos(data), mEnd(data + size), mSize(size), mReplaceSpanTags(replaceSpanTags) {
}

bool ManualPBParser::ParsePipelineEventGroup(PipelineEventGroup& eventGroup, std::string& errMsg) {
    mLastError.clear();
    mPos = mData;

    if (!mData || mSize == 0) {
        errMsg = "Empty or null input data";
        return false;
    }

    while (hasMoreData()) {
        uint32_t tag = 0;
        if (!readVarint32(tag)) {
            errMsg = "Failed to read field tag: " + mLastError;
            return false;
        }

        uint32_t fieldNumber = tag >> 3;
        uint32_t wireType = tag & 0x7;

        switch (fieldNumber) {
            case kMetadataField:
                if (wireType != kLengthDelimited) {
                    errMsg = "Invalid wire type for Metadata field";
                    return false;
                }
                if (!parseMetadata(eventGroup)) {
                    errMsg = "Failed to parse Metadata: " + mLastError;
                    return false;
                }
                break;

            case kTagsField:
                if (wireType != kLengthDelimited) {
                    errMsg = "Invalid wire type for Tags field";
                    return false;
                }
                if (!parseTags(eventGroup)) {
                    errMsg = "Failed to parse Tags: " + mLastError;
                    return false;
                }
                break;

            case kLogsField:
                if (wireType != kLengthDelimited) {
                    errMsg = "Invalid wire type for Logs field";
                    return false;
                }
                if (!parseLogEvents(eventGroup)) {
                    errMsg = "Failed to parse LogEvents: " + mLastError;
                    return false;
                }
                break;

            case kMetricsField:
                if (wireType != kLengthDelimited) {
                    errMsg = "Invalid wire type for Metrics field";
                    return false;
                }
                if (!parseMetricEvents(eventGroup)) {
                    errMsg = "Failed to parse MetricEvents: " + mLastError;
                    return false;
                }
                break;

            case kSpansField:
                if (wireType != kLengthDelimited) {
                    errMsg = "Invalid wire type for Spans field";
                    return false;
                }
                if (!parseSpanEvents(eventGroup)) {
                    errMsg = "Failed to parse SpanEvents: " + mLastError;
                    return false;
                }
                break;

            default:
                if (!skipField(wireType)) {
                    errMsg = "Failed to skip unknown field: " + mLastError;
                    return false;
                }
                break;
        }
    }

    return true;
}

bool ManualPBParser::readVarint32(uint32_t& value) {
    value = 0;
    int shift = 0;

    while (hasMoreData() && shift < 32) {
        uint8_t byte = *mPos++;
        value |= (static_cast<uint32_t>(byte & 0x7F) << shift);

        if ((byte & 0x80) == 0) {
            return true;
        }
        shift += 7;
    }

    setError("Invalid varint32 encoding");
    return false;
}

bool ManualPBParser::readVarint64(uint64_t& value) {
    value = 0;
    int shift = 0;

    while (hasMoreData() && shift < 64) {
        uint8_t byte = *mPos++;
        value |= (static_cast<uint64_t>(byte & 0x7F) << shift);

        if ((byte & 0x80) == 0) {
            return true;
        }
        shift += 7;
    }

    setError("Invalid varint64 encoding");
    return false;
}

bool ManualPBParser::readFixed32(uint32_t& value) {
    if (!checkBounds(4)) {
        setError("Not enough data for fixed32");
        return false;
    }

    // Little endian
    value = static_cast<uint32_t>(mPos[0]) | (static_cast<uint32_t>(mPos[1]) << 8)
        | (static_cast<uint32_t>(mPos[2]) << 16) | (static_cast<uint32_t>(mPos[3]) << 24);
    mPos += 4;
    return true;
}

bool ManualPBParser::readFixed64(uint64_t& value) {
    if (!checkBounds(8)) {
        setError("Not enough data for fixed64");
        return false;
    }

    // Little endian
    value = static_cast<uint64_t>(mPos[0]) | (static_cast<uint64_t>(mPos[1]) << 8)
        | (static_cast<uint64_t>(mPos[2]) << 16) | (static_cast<uint64_t>(mPos[3]) << 24)
        | (static_cast<uint64_t>(mPos[4]) << 32) | (static_cast<uint64_t>(mPos[5]) << 40)
        | (static_cast<uint64_t>(mPos[6]) << 48) | (static_cast<uint64_t>(mPos[7]) << 56);
    mPos += 8;
    return true;
}

bool ManualPBParser::readLengthDelimited(const uint8_t*& data, size_t& length) {
    uint32_t len = 0;
    if (!readVarint32(len)) {
        return false;
    }

    if (!checkBounds(len)) {
        setError("Not enough data for length-delimited field");
        return false;
    }

    data = mPos;
    length = len;
    mPos += len;
    return true;
}

bool ManualPBParser::readString(std::string& str) {
    const uint8_t* data = nullptr;
    size_t length = 0;
    if (!readLengthDelimited(data, length)) {
        return false;
    }

    str.assign(reinterpret_cast<const char*>(data), length);
    return true;
}

bool ManualPBParser::readBytes(const uint8_t*& data, size_t& length) {
    return readLengthDelimited(data, length);
}

bool ManualPBParser::skipField(uint32_t wireType) {
    LOG_ERROR(sLogger, ("ManualPBParser encountered unknown wire type", std::to_string(wireType)));
    switch (wireType) {
        case kVarint: {
            uint64_t dummy = 0;
            return readVarint64(dummy);
        }
        case kFixed64: {
            uint64_t dummy = 0;
            return readFixed64(dummy);
        }
        case kLengthDelimited: {
            const uint8_t* dummyData = nullptr;
            size_t dummyLength = 0;
            return readLengthDelimited(dummyData, dummyLength);
        }
        case kFixed32: {
            uint32_t dummy = 0;
            return readFixed32(dummy);
        }
        default:
            setError("Unknown wire type: " + std::to_string(wireType));
            return false;
    }
}

bool ManualPBParser::parseMetadata(PipelineEventGroup& eventGroup) {
    const uint8_t* mapData = nullptr;
    size_t mapLength = 0;
    if (!readLengthDelimited(mapData, mapLength)) {
        return false;
    }

    // Save current state and parse map entry
    ParseState savedState = saveState();
    mPos = mapData;
    mEnd = mapData + mapLength;

    std::string key;
    std::string value;
    bool hasKey = false;
    bool hasValue = false;

    while (hasMoreData()) {
        uint32_t tag = 0;
        if (!readVarint32(tag)) {
            restoreState(savedState);
            return false;
        }

        uint32_t fieldNumber = tag >> 3;
        uint32_t wireType = tag & 0x7;

        switch (fieldNumber) {
            case 1: // key
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for map key");
                    return false;
                }
                if (!readString(key)) {
                    restoreState(savedState);
                    return false;
                }
                hasKey = true;
                break;

            case 2: // value
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for map value");
                    return false;
                }
                if (!readString(value)) {
                    restoreState(savedState);
                    return false;
                }
                hasValue = true;
                break;

            default:
                if (!skipField(wireType)) {
                    restoreState(savedState);
                    return false;
                }
                break;
        }
    }

    // Restore parser state
    restoreState(savedState);
    mPos = mapData + mapLength;

    // Set metadata if both key and value are present
    if (hasKey && hasValue) {
        eventGroup.SetTag(key, value);
    }

    return true;
}

bool ManualPBParser::parseTags(PipelineEventGroup& eventGroup) {
    const uint8_t* mapData = nullptr;
    size_t mapLength = 0;
    if (!readLengthDelimited(mapData, mapLength)) {
        return false;
    }

    // Save current state and parse map entry
    ParseState savedState = saveState();
    mPos = mapData;
    mEnd = mapData + mapLength;

    std::string key;
    std::string value;
    bool hasKey = false;
    bool hasValue = false;

    while (hasMoreData()) {
        uint32_t tag = 0;
        if (!readVarint32(tag)) {
            restoreState(savedState);
            return false;
        }

        uint32_t fieldNumber = tag >> 3;
        uint32_t wireType = tag & 0x7;

        switch (fieldNumber) {
            case 1: // key
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for map key");
                    return false;
                }
                if (!readString(key)) {
                    restoreState(savedState);
                    return false;
                }
                hasKey = true;
                break;

            case 2: // value
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for map value");
                    return false;
                }
                if (!readString(value)) {
                    restoreState(savedState);
                    return false;
                }
                hasValue = true;
                break;

            default:
                if (!skipField(wireType)) {
                    restoreState(savedState);
                    return false;
                }
                break;
        }
    }

    // Restore parser state
    restoreState(savedState);
    mPos = mapData + mapLength;

    // Set tag if both key and value are present
    if (hasKey && hasValue) {
        eventGroup.SetTag(key, value);
    }

    return true;
}

bool ManualPBParser::parseLogEvents(PipelineEventGroup& eventGroup) {
    const uint8_t* eventsData = nullptr;
    size_t eventsLength = 0;
    if (!readLengthDelimited(eventsData, eventsLength)) {
        return false;
    }

    // Save current state and parse LogEvents message
    ParseState savedState = saveState();
    mPos = eventsData;
    mEnd = eventsData + eventsLength;

    while (hasMoreData()) {
        uint32_t tag = 0;
        if (!readVarint32(tag)) {
            restoreState(savedState);
            return false;
        }

        uint32_t fieldNumber = tag >> 3;
        uint32_t wireType = tag & 0x7;

        switch (fieldNumber) {
            case 1: // Events field
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for LogEvent");
                    return false;
                }
                if (!parseLogEvent(eventGroup)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            default:
                if (!skipField(wireType)) {
                    restoreState(savedState);
                    return false;
                }
                break;
        }
    }

    // Restore parser state
    restoreState(savedState);
    mPos = eventsData + eventsLength;

    return true;
}

bool ManualPBParser::parseMetricEvents(PipelineEventGroup& eventGroup) {
    const uint8_t* eventsData = nullptr;
    size_t eventsLength = 0;
    if (!readLengthDelimited(eventsData, eventsLength)) {
        return false;
    }

    // Save current state and parse MetricEvents message
    ParseState savedState = saveState();
    mPos = eventsData;
    mEnd = eventsData + eventsLength;

    while (hasMoreData()) {
        uint32_t tag = 0;
        if (!readVarint32(tag)) {
            restoreState(savedState);
            return false;
        }

        uint32_t fieldNumber = tag >> 3;
        uint32_t wireType = tag & 0x7;

        switch (fieldNumber) {
            case 1: // Events field
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for MetricEvent");
                    return false;
                }
                if (!parseMetricEvent(eventGroup)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            default:
                if (!skipField(wireType)) {
                    restoreState(savedState);
                    return false;
                }
                break;
        }
    }

    // Restore parser state
    restoreState(savedState);
    mPos = eventsData + eventsLength;

    return true;
}

bool ManualPBParser::parseSpanEvents(PipelineEventGroup& eventGroup) {
    const uint8_t* eventsData = nullptr;
    size_t eventsLength = 0;
    if (!readLengthDelimited(eventsData, eventsLength)) {
        return false;
    }

    // Save current state and parse SpanEvents message
    ParseState savedState = saveState();
    mPos = eventsData;
    mEnd = eventsData + eventsLength;

    while (hasMoreData()) {
        uint32_t tag = 0;
        if (!readVarint32(tag)) {
            restoreState(savedState);
            return false;
        }

        uint32_t fieldNumber = tag >> 3;
        uint32_t wireType = tag & 0x7;

        switch (fieldNumber) {
            case 1: // Events field
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for SpanEvent");
                    return false;
                }
                if (!parseSpanEvent(eventGroup)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            default:
                if (!skipField(wireType)) {
                    restoreState(savedState);
                    return false;
                }
                break;
        }
    }

    // Restore parser state
    restoreState(savedState);
    mPos = eventsData + eventsLength;

    return true;
}

bool ManualPBParser::parseLogEvent(PipelineEventGroup& eventGroup) {
    const uint8_t* eventData = nullptr;
    size_t eventLength = 0;
    if (!readLengthDelimited(eventData, eventLength)) {
        return false;
    }

    // Create new log event
    LogEvent* logEvent = eventGroup.AddLogEvent();
    if (!logEvent) {
        setError("Failed to create LogEvent");
        return false;
    }

    // Save current state and parse LogEvent message
    ParseState savedState = saveState();
    mPos = eventData;
    mEnd = eventData + eventLength;

    uint64_t timestamp = 0;
    uint64_t fileOffset = 0;
    uint64_t rawSize = 0;
    std::string level;

    while (hasMoreData()) {
        uint32_t tag = 0;
        if (!readVarint32(tag)) {
            restoreState(savedState);
            return false;
        }

        uint32_t fieldNumber = tag >> 3;
        uint32_t wireType = tag & 0x7;

        switch (fieldNumber) {
            case kLogTimestampField:
                if (wireType != kVarint) {
                    restoreState(savedState);
                    setError("Invalid wire type for timestamp");
                    return false;
                }
                if (!readVarint64(timestamp)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            case kLogContentsField:
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for contents");
                    return false;
                }
                if (!parseLogContent(eventGroup, logEvent)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            case kLogLevelField:
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for level");
                    return false;
                }
                if (!readString(level)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            case kLogFileOffsetField:
                if (wireType != kVarint) {
                    restoreState(savedState);
                    setError("Invalid wire type for file offset");
                    return false;
                }
                if (!readVarint64(fileOffset)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            case kLogRawSizeField:
                if (wireType != kVarint) {
                    restoreState(savedState);
                    setError("Invalid wire type for raw size");
                    return false;
                }
                if (!readVarint64(rawSize)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            default:
                if (!skipField(wireType)) {
                    restoreState(savedState);
                    return false;
                }
                break;
        }
    }

    // Restore parser state
    restoreState(savedState);
    mPos = eventData + eventLength;

    // Set parsed values
    logEvent->SetTimestamp(timestamp);
    logEvent->SetPosition(fileOffset, rawSize);
    if (!level.empty()) {
        logEvent->SetLevel(level);
    }

    return true;
}

bool ManualPBParser::parseMetricEvent(PipelineEventGroup& eventGroup) {
    const uint8_t* eventData = nullptr;
    size_t eventLength = 0;
    if (!readLengthDelimited(eventData, eventLength)) {
        return false;
    }

    // Create new metric event
    MetricEvent* metricEvent = eventGroup.AddMetricEvent();
    if (!metricEvent) {
        setError("Failed to create MetricEvent");
        return false;
    }

    // Save current state and parse MetricEvent message
    ParseState savedState = saveState();
    mPos = eventData;
    mEnd = eventData + eventLength;

    uint64_t timestamp = 0;
    std::string name;

    while (hasMoreData()) {
        uint32_t tag = 0;
        if (!readVarint32(tag)) {
            restoreState(savedState);
            return false;
        }

        uint32_t fieldNumber = tag >> 3;
        uint32_t wireType = tag & 0x7;

        switch (fieldNumber) {
            case kMetricTimestampField:
                if (wireType != kVarint) {
                    restoreState(savedState);
                    setError("Invalid wire type for timestamp");
                    return false;
                }
                if (!readVarint64(timestamp)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            case kMetricNameField:
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for name");
                    return false;
                }
                if (!readString(name)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            case kMetricTagsField:
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for tags");
                    return false;
                }
                if (!parseMetricTags(eventGroup, metricEvent)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            case kMetricUntypedSingleValueField: {
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for untyped single value");
                    return false;
                }

                const uint8_t* valueData = nullptr;
                size_t valueLength = 0;
                if (!readLengthDelimited(valueData, valueLength)) {
                    restoreState(savedState);
                    return false;
                }

                // Parse UntypedSingleValue message
                ParseState valueState = saveState();
                mPos = valueData;
                mEnd = valueData + valueLength;

                double value = 0.0;
                while (hasMoreData()) {
                    uint32_t valueTag = 0;
                    if (!readVarint32(valueTag)) {
                        restoreState(savedState);
                        return false;
                    }

                    uint32_t valueFieldNumber = valueTag >> 3;
                    uint32_t valueWireType = valueTag & 0x7;

                    if (valueFieldNumber == 1 && valueWireType == kFixed64) {
                        uint64_t doubleBytes = 0;
                        if (!readFixed64(doubleBytes)) {
                            restoreState(savedState);
                            return false;
                        }
                        // Convert uint64 to double (IEEE 754)
                        memcpy(&value, &doubleBytes, sizeof(double));
                    } else {
                        if (!skipField(valueWireType)) {
                            restoreState(savedState);
                            return false;
                        }
                    }
                }

                restoreState(valueState);
                mPos = valueData + valueLength;

                // Set the metric value
                metricEvent->SetValue(UntypedSingleValue{value});
                break;
            }

            default:
                if (!skipField(wireType)) {
                    restoreState(savedState);
                    return false;
                }
                break;
        }
    }

    // Restore parser state
    restoreState(savedState);
    mPos = eventData + eventLength;

    // Set parsed values
    metricEvent->SetTimestamp(timestamp);
    if (!name.empty()) {
        metricEvent->SetName(name);
    }

    return true;
}

bool ManualPBParser::parseSpanEvent(PipelineEventGroup& eventGroup) {
    const uint8_t* eventData = nullptr;
    size_t eventLength = 0;
    if (!readLengthDelimited(eventData, eventLength)) {
        return false;
    }

    // Create new span event
    SpanEvent* spanEvent = eventGroup.AddSpanEvent();
    if (!spanEvent) {
        setError("Failed to create SpanEvent");
        return false;
    }

    // Save current state and parse SpanEvent message
    ParseState savedState = saveState();
    mPos = eventData;
    mEnd = eventData + eventLength;

    uint64_t timestamp = 0;
    uint64_t startTime = 0;
    uint64_t endTime = 0;
    std::string traceId;
    std::string spanId;
    std::string traceState;
    std::string parentSpanId;
    std::string name;
    uint32_t kind = 0;

    while (hasMoreData()) {
        uint32_t tag = 0;
        if (!readVarint32(tag)) {
            restoreState(savedState);
            return false;
        }

        uint32_t fieldNumber = tag >> 3;
        uint32_t wireType = tag & 0x7;

        switch (fieldNumber) {
            case kSpanTimestampField:
                if (wireType != kVarint) {
                    restoreState(savedState);
                    setError("Invalid wire type for timestamp");
                    return false;
                }
                if (!readVarint64(timestamp)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            case kSpanTraceIdField:
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for trace ID");
                    return false;
                }
                if (!readString(traceId)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            case kSpanSpanIdField:
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for span ID");
                    return false;
                }
                if (!readString(spanId)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            case kSpanTraceStateField:
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for trace state");
                    return false;
                }
                if (!readString(traceState)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            case kSpanParentSpanIdField:
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for parent span ID");
                    return false;
                }
                if (!readString(parentSpanId)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            case kSpanNameField:
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for name");
                    return false;
                }
                if (!readString(name)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            case kSpanKindField:
                if (wireType != kVarint) {
                    restoreState(savedState);
                    setError("Invalid wire type for kind");
                    return false;
                }
                if (!readVarint32(kind)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            case kSpanStartTimeField:
                if (wireType != kVarint) {
                    restoreState(savedState);
                    setError("Invalid wire type for start time");
                    return false;
                }
                if (!readVarint64(startTime)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            case kSpanEndTimeField:
                if (wireType != kVarint) {
                    restoreState(savedState);
                    setError("Invalid wire type for end time");
                    return false;
                }
                if (!readVarint64(endTime)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            case kSpanTagsField:
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for tags");
                    return false;
                }
                if (!parseSpanTags(eventGroup, spanEvent)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            case kSpanEventsField:
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for events");
                    return false;
                }
                if (!parseSpanInnerEvent(eventGroup, spanEvent)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            case kSpanLinksField:
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for links");
                    return false;
                }
                if (!parseSpanLink(eventGroup, spanEvent)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            case kSpanStatusField: {
                if (wireType != kVarint) {
                    restoreState(savedState);
                    setError("Invalid wire type for status");
                    return false;
                }
                uint32_t status = 0;
                if (!readVarint32(status)) {
                    restoreState(savedState);
                    return false;
                }
                // Set status (convert from protobuf enum to SpanEvent::StatusCode)
                switch (status) {
                    case 0:
                        spanEvent->SetStatus(SpanEvent::StatusCode::Unset);
                        break;
                    case 1:
                        spanEvent->SetStatus(SpanEvent::StatusCode::Ok);
                        break;
                    case 2:
                        spanEvent->SetStatus(SpanEvent::StatusCode::Error);
                        break;
                    default:
                        spanEvent->SetStatus(SpanEvent::StatusCode::Unset);
                        break;
                }
                break;
            }

            case kSpanScopeTagsField:
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for scope tags");
                    return false;
                }
                if (!parseSpanScopeTags(eventGroup, spanEvent)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            default:
                if (!skipField(wireType)) {
                    restoreState(savedState);
                    return false;
                }
                break;
        }
    }

    // Restore parser state
    restoreState(savedState);
    mPos = eventData + eventLength;

    // Set parsed values
    spanEvent->SetTimestamp(timestamp);
    spanEvent->SetStartTimeNs(startTime);
    spanEvent->SetEndTimeNs(endTime);

    if (!traceId.empty()) {
        spanEvent->SetTraceId(traceId);
    }
    if (!spanId.empty()) {
        spanEvent->SetSpanId(spanId);
    }
    if (!traceState.empty()) {
        spanEvent->SetTraceState(traceState);
    }
    if (!parentSpanId.empty()) {
        spanEvent->SetParentSpanId(parentSpanId);
    }
    if (!name.empty()) {
        spanEvent->SetName(name);
    }

    // Set kind (convert from protobuf enum to SpanEvent::Kind)
    switch (kind) {
        case 0:
            spanEvent->SetKind(SpanEvent::Kind::Unspecified);
            break;
        case 1:
            spanEvent->SetKind(SpanEvent::Kind::Internal);
            break;
        case 2:
            spanEvent->SetKind(SpanEvent::Kind::Server);
            break;
        case 3:
            spanEvent->SetKind(SpanEvent::Kind::Client);
            break;
        case 4:
            spanEvent->SetKind(SpanEvent::Kind::Producer);
            break;
        case 5:
            spanEvent->SetKind(SpanEvent::Kind::Consumer);
            break;
        default:
            spanEvent->SetKind(SpanEvent::Kind::Unspecified);
            break;
    }

    return true;
}

bool ManualPBParser::parseLogContent(PipelineEventGroup& /* eventGroup */, LogEvent* logEvent) {
    const uint8_t* contentData = nullptr;
    size_t contentLength = 0;
    if (!readLengthDelimited(contentData, contentLength)) {
        return false;
    }

    // Save current state and parse Content message
    ParseState savedState = saveState();
    mPos = contentData;
    mEnd = contentData + contentLength;

    std::string key;
    std::string value;
    bool hasKey = false;
    bool hasValue = false;

    while (hasMoreData()) {
        uint32_t tag = 0;
        if (!readVarint32(tag)) {
            restoreState(savedState);
            return false;
        }

        uint32_t fieldNumber = tag >> 3;
        uint32_t wireType = tag & 0x7;

        switch (fieldNumber) {
            case kLogContentKeyField:
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for content key");
                    return false;
                }
                if (!readString(key)) {
                    restoreState(savedState);
                    return false;
                }
                hasKey = true;
                break;

            case kLogContentValueField:
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for content value");
                    return false;
                }
                if (!readString(value)) {
                    restoreState(savedState);
                    return false;
                }
                hasValue = true;
                break;

            default:
                if (!skipField(wireType)) {
                    restoreState(savedState);
                    return false;
                }
                break;
        }
    }

    // Restore parser state
    restoreState(savedState);
    mPos = contentData + contentLength;

    // Set content if both key and value are present
    if (hasKey && hasValue) {
        logEvent->SetContent(key, value);
    }

    return true;
}

bool ManualPBParser::parseMetricTags(PipelineEventGroup& /* eventGroup */, MetricEvent* metricEvent) {
    const uint8_t* mapData = nullptr;
    size_t mapLength = 0;
    if (!readLengthDelimited(mapData, mapLength)) {
        return false;
    }

    // Save current state and parse map entry
    ParseState savedState = saveState();
    mPos = mapData;
    mEnd = mapData + mapLength;

    std::string key;
    std::string value;
    bool hasKey = false;
    bool hasValue = false;

    while (hasMoreData()) {
        uint32_t tag = 0;
        if (!readVarint32(tag)) {
            restoreState(savedState);
            return false;
        }

        uint32_t fieldNumber = tag >> 3;
        uint32_t wireType = tag & 0x7;

        switch (fieldNumber) {
            case 1: // key
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for map key");
                    return false;
                }
                if (!readString(key)) {
                    restoreState(savedState);
                    return false;
                }
                hasKey = true;
                break;

            case 2: // value
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for map value");
                    return false;
                }
                if (!readString(value)) {
                    restoreState(savedState);
                    return false;
                }
                hasValue = true;
                break;

            default:
                if (!skipField(wireType)) {
                    restoreState(savedState);
                    return false;
                }
                break;
        }
    }

    // Restore parser state
    restoreState(savedState);
    mPos = mapData + mapLength;

    // Set tag if both key and value are present
    if (hasKey && hasValue) {
        metricEvent->SetTag(key, value);
    }

    return true;
}

bool ManualPBParser::parseSpanTags(PipelineEventGroup& /* eventGroup */, SpanEvent* spanEvent) {
    const uint8_t* mapData = nullptr;
    size_t mapLength = 0;
    if (!readLengthDelimited(mapData, mapLength)) {
        return false;
    }

    // Save current state and parse map entry
    ParseState savedState = saveState();
    mPos = mapData;
    mEnd = mapData + mapLength;

    std::string key;
    std::string value;
    bool hasKey = false;
    bool hasValue = false;

    while (hasMoreData()) {
        uint32_t tag = 0;
        if (!readVarint32(tag)) {
            restoreState(savedState);
            return false;
        }

        uint32_t fieldNumber = tag >> 3;
        uint32_t wireType = tag & 0x7;

        switch (fieldNumber) {
            case 1: // key
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for map key");
                    return false;
                }
                if (!readString(key)) {
                    restoreState(savedState);
                    return false;
                }
                hasKey = true;
                break;

            case 2: // value
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for map value");
                    return false;
                }
                if (!readString(value)) {
                    restoreState(savedState);
                    return false;
                }
                hasValue = true;
                break;

            default:
                if (!skipField(wireType)) {
                    restoreState(savedState);
                    return false;
                }
                break;
        }
    }

    // Restore parser state
    restoreState(savedState);
    mPos = mapData + mapLength;

    // Set tag if both key and value are present
    if (hasKey && hasValue) {
        spanEvent->IngestTag(key, value);
    }

    return true;
}

bool ManualPBParser::parseSpanScopeTags(PipelineEventGroup& /* eventGroup */, SpanEvent* spanEvent) {
    const uint8_t* mapData = nullptr;
    size_t mapLength = 0;
    if (!readLengthDelimited(mapData, mapLength)) {
        return false;
    }

    // Save current state and parse map entry
    ParseState savedState = saveState();
    mPos = mapData;
    mEnd = mapData + mapLength;

    std::string key;
    std::string value;
    bool hasKey = false;
    bool hasValue = false;

    while (hasMoreData()) {
        uint32_t tag = 0;
        if (!readVarint32(tag)) {
            restoreState(savedState);
            return false;
        }

        uint32_t fieldNumber = tag >> 3;
        uint32_t wireType = tag & 0x7;

        switch (fieldNumber) {
            case 1: // key
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for map key");
                    return false;
                }
                if (!readString(key)) {
                    restoreState(savedState);
                    return false;
                }
                hasKey = true;
                break;

            case 2: // value
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for map value");
                    return false;
                }
                if (!readString(value)) {
                    restoreState(savedState);
                    return false;
                }
                hasValue = true;
                break;

            default:
                if (!skipField(wireType)) {
                    restoreState(savedState);
                    return false;
                }
                break;
        }
    }

    // Restore parser state
    restoreState(savedState);
    mPos = mapData + mapLength;

    // Set scope tag if both key and value are present
    if (hasKey && hasValue) {
        spanEvent->SetScopeTag(key, value);
    }

    return true;
}

bool ManualPBParser::parseSpanInnerEvent(PipelineEventGroup& eventGroup, SpanEvent* spanEvent) {
    const uint8_t* eventData = nullptr;
    size_t eventLength = 0;
    if (!readLengthDelimited(eventData, eventLength)) {
        return false;
    }

    // Add new inner event
    SpanEvent::InnerEvent* innerEvent = spanEvent->AddEvent();
    if (!innerEvent) {
        setError("Failed to create InnerEvent");
        return false;
    }

    // Save current state and parse InnerEvent message
    ParseState savedState = saveState();
    mPos = eventData;
    mEnd = eventData + eventLength;

    uint64_t timestamp = 0;
    std::string name;

    while (hasMoreData()) {
        uint32_t tag = 0;
        if (!readVarint32(tag)) {
            restoreState(savedState);
            return false;
        }

        uint32_t fieldNumber = tag >> 3;
        uint32_t wireType = tag & 0x7;

        switch (fieldNumber) {
            case kInnerEventTimestampField: // Timestamp
                if (wireType != kVarint) {
                    restoreState(savedState);
                    setError("Invalid wire type for inner event timestamp");
                    return false;
                }
                if (!readVarint64(timestamp)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            case kInnerEventNameField: // Name
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for inner event name");
                    return false;
                }
                if (!readString(name)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            case kInnerEventTagsField: // Tags
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for inner event tags");
                    return false;
                }
                if (!parseSpanInnerEventTags(eventGroup, spanEvent, innerEvent)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            default:
                if (!skipField(wireType)) {
                    restoreState(savedState);
                    return false;
                }
                break;
        }
    }

    // Restore parser state
    restoreState(savedState);
    mPos = eventData + eventLength;

    // Set parsed values
    innerEvent->SetTimestampNs(timestamp);
    if (!name.empty()) {
        innerEvent->SetName(name);
    }

    return true;
}

bool ManualPBParser::parseSpanLink(PipelineEventGroup& eventGroup, SpanEvent* spanEvent) {
    const uint8_t* linkData = nullptr;
    size_t linkLength = 0;
    if (!readLengthDelimited(linkData, linkLength)) {
        return false;
    }

    // Add new span link
    SpanEvent::SpanLink* spanLink = spanEvent->AddLink();
    if (!spanLink) {
        setError("Failed to create SpanLink");
        return false;
    }

    // Save current state and parse SpanLink message
    ParseState savedState = saveState();
    mPos = linkData;
    mEnd = linkData + linkLength;

    std::string traceId;
    std::string spanId;
    std::string traceState;

    while (hasMoreData()) {
        uint32_t tag = 0;
        if (!readVarint32(tag)) {
            restoreState(savedState);
            return false;
        }

        uint32_t fieldNumber = tag >> 3;
        uint32_t wireType = tag & 0x7;

        switch (fieldNumber) {
            case kSpanLinkTraceIdField: // TraceID
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for span link trace ID");
                    return false;
                }
                if (!readString(traceId)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            case kSpanLinkSpanIdField: // SpanID
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for span link span ID");
                    return false;
                }
                if (!readString(spanId)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            case kSpanLinkTraceStateField: // TraceState
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for span link trace state");
                    return false;
                }
                if (!readString(traceState)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            case kSpanLinkTagsField: // Tags
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for span link tags");
                    return false;
                }
                if (!parseSpanLinkTags(eventGroup, spanEvent, spanLink)) {
                    restoreState(savedState);
                    return false;
                }
                break;

            default:
                if (!skipField(wireType)) {
                    restoreState(savedState);
                    return false;
                }
                break;
        }
    }

    // Restore parser state
    restoreState(savedState);
    mPos = linkData + linkLength;

    // Set parsed values
    if (!traceId.empty()) {
        spanLink->SetTraceId(traceId);
    }
    if (!spanId.empty()) {
        spanLink->SetSpanId(spanId);
    }
    if (!traceState.empty()) {
        spanLink->SetTraceState(traceState);
    }

    return true;
}

bool ManualPBParser::parseSpanInnerEventTags(PipelineEventGroup& /* eventGroup */,
                                             SpanEvent* /* spanEvent */,
                                             SpanEvent::InnerEvent* innerEvent) {
    const uint8_t* mapData = nullptr;
    size_t mapLength = 0;
    if (!readLengthDelimited(mapData, mapLength)) {
        return false;
    }

    // Save current state and parse map entry
    ParseState savedState = saveState();
    mPos = mapData;
    mEnd = mapData + mapLength;

    std::string key;
    std::string value;
    bool hasKey = false;
    bool hasValue = false;

    while (hasMoreData()) {
        uint32_t tag = 0;
        if (!readVarint32(tag)) {
            restoreState(savedState);
            return false;
        }

        uint32_t fieldNumber = tag >> 3;
        uint32_t wireType = tag & 0x7;

        switch (fieldNumber) {
            case 1: // key
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for map key");
                    return false;
                }
                if (!readString(key)) {
                    restoreState(savedState);
                    return false;
                }
                hasKey = true;
                break;

            case 2: // value
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for map value");
                    return false;
                }
                if (!readString(value)) {
                    restoreState(savedState);
                    return false;
                }
                hasValue = true;
                break;

            default:
                if (!skipField(wireType)) {
                    restoreState(savedState);
                    return false;
                }
                break;
        }
    }

    // Restore parser state
    restoreState(savedState);
    mPos = mapData + mapLength;

    // Set tag if both key and value are present
    if (hasKey && hasValue) {
        innerEvent->IngestTag(key, value);
    }

    return true;
}

bool ManualPBParser::parseSpanLinkTags(PipelineEventGroup& /* eventGroup */,
                                       SpanEvent* /* spanEvent */,
                                       SpanEvent::SpanLink* spanLink) {
    const uint8_t* mapData = nullptr;
    size_t mapLength = 0;
    if (!readLengthDelimited(mapData, mapLength)) {
        return false;
    }

    // Save current state and parse map entry
    ParseState savedState = saveState();
    mPos = mapData;
    mEnd = mapData + mapLength;

    std::string key;
    std::string value;
    bool hasKey = false;
    bool hasValue = false;

    while (hasMoreData()) {
        uint32_t tag = 0;
        if (!readVarint32(tag)) {
            restoreState(savedState);
            return false;
        }

        uint32_t fieldNumber = tag >> 3;
        uint32_t wireType = tag & 0x7;

        switch (fieldNumber) {
            case 1: // key
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for map key");
                    return false;
                }
                if (!readString(key)) {
                    restoreState(savedState);
                    return false;
                }
                hasKey = true;
                break;

            case 2: // value
                if (wireType != kLengthDelimited) {
                    restoreState(savedState);
                    setError("Invalid wire type for map value");
                    return false;
                }
                if (!readString(value)) {
                    restoreState(savedState);
                    return false;
                }
                hasValue = true;
                break;

            default:
                if (!skipField(wireType)) {
                    restoreState(savedState);
                    return false;
                }
                break;
        }
    }

    // Restore parser state
    restoreState(savedState);
    mPos = mapData + mapLength;

    // Set tag if both key and value are present
    if (hasKey && hasValue) {
        spanLink->IngestTag(key, value);
    }

    return true;
}

void ManualPBParser::setError(const std::string& msg) {
    mLastError = msg;
}

ManualPBParser::ParseState ManualPBParser::saveState() const {
    return {mPos, mEnd};
}

void ManualPBParser::restoreState(const ParseState& state) {
    mPos = state.pos;
    mEnd = state.end;
}

} // namespace logtail
