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

#pragma once

#include <cstdint>

#include <string>

#include "models/SpanEvent.h"

namespace logtail {

class PipelineEventGroup;

/**
 * @brief High-performance manual protobuf parser for PipelineEventGroup
 *
 * This parser directly parses protobuf wire format to avoid the overhead
 * of standard protobuf library, including memory allocations and object
 * creation. It's optimized for zero-copy parsing where possible.
 */
class ManualPBParser {
public:
    /**
     * @brief Construct parser with binary data
     * @param data Pointer to protobuf binary data
     * @param size Size of the binary data
     */
    ManualPBParser(const uint8_t* data, size_t size, bool replaceSpanTags = true);

    /**
     * @brief Parse PipelineEventGroup from binary data
     * @param eventGroup Output event group to populate
     * @param errMsg Error message if parsing fails
     * @return true if parsing succeeds, false otherwise
     */
    bool ParsePipelineEventGroup(PipelineEventGroup& eventGroup, std::string& errMsg);

private:
    // Wire format parsing utilities
    bool readVarint32(uint32_t& value);
    bool readVarint64(uint64_t& value);
    bool readFixed32(uint32_t& value);
    bool readFixed64(uint64_t& value);
    bool readLengthDelimited(const uint8_t*& data, size_t& length);
    bool readString(std::string& str);
    bool readBytes(const uint8_t*& data, size_t& length);

    // Skip unknown fields
    bool skipField(uint32_t wireType);

    // Parse specific message types
    bool parseMetadata(PipelineEventGroup& eventGroup);
    bool parseTags(PipelineEventGroup& eventGroup);
    bool parseLogEvents(PipelineEventGroup& eventGroup);
    bool parseMetricEvents(PipelineEventGroup& eventGroup);
    bool parseSpanEvents(PipelineEventGroup& eventGroup);

    // Parse individual event types
    bool parseLogEvent(PipelineEventGroup& eventGroup);
    bool parseMetricEvent(PipelineEventGroup& eventGroup);
    bool parseSpanEvent(PipelineEventGroup& eventGroup);

    // Parse nested structures
    bool parseLogContent(PipelineEventGroup& eventGroup, class LogEvent* logEvent);
    bool parseMetricTags(PipelineEventGroup& eventGroup, class MetricEvent* metricEvent);
    bool parseMetricMetadata(PipelineEventGroup& eventGroup, class MetricEvent* metricEvent);
    bool parseSpanTags(PipelineEventGroup& eventGroup, class SpanEvent* spanEvent);
    bool parseSpanScopeTags(PipelineEventGroup& eventGroup, class SpanEvent* spanEvent);
    bool parseSpanInnerEvent(PipelineEventGroup& eventGroup, class SpanEvent* spanEvent);
    bool parseSpanInnerEventTags(PipelineEventGroup& eventGroup,
                                 class SpanEvent* spanEvent,
                                 class SpanEvent::InnerEvent* innerEvent);
    bool parseSpanLink(PipelineEventGroup& eventGroup, class SpanEvent* spanEvent);
    bool
    parseSpanLinkTags(PipelineEventGroup& eventGroup, class SpanEvent* spanEvent, class SpanEvent::SpanLink* spanLink);

    // Utility functions
    inline bool hasMoreData() const { return mPos < mEnd; }
    inline bool checkBounds(size_t bytes) const { return (mEnd - mPos) >= static_cast<ptrdiff_t>(bytes); }
    void setError(const std::string& msg);

    // State management for nested parsing
    struct ParseState {
        const uint8_t* pos;
        const uint8_t* end;
    };

    ParseState saveState() const;
    void restoreState(const ParseState& state);

    // Parser state
    const uint8_t* mData;
    const uint8_t* mPos;
    const uint8_t* mEnd;
    size_t mSize;
    std::string mLastError;
    bool mReplaceSpanTags = true;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ManualPBParserUnittest;
    friend class ManualPBParserTestHelper;
#endif
};

} // namespace logtail
